#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""严格校验 ISO 9660 + Joliet 镜像（按 ECMA-119 / Windows CDFS 的挑剔点）。

用法: py isocheck.py <x.iso>
退出码 0 = 通过，1 = 有问题（会逐条列出）。
"""
import struct
import sys

SS = 2048
problems = []
notes = []


def bad(msg):
    problems.append(msg)


def u16le(b, o): return struct.unpack_from('<H', b, o)[0]
def u32le(b, o): return struct.unpack_from('<I', b, o)[0]
def u16be(b, o): return struct.unpack_from('>H', b, o)[0]
def u32be(b, o): return struct.unpack_from('>I', b, o)[0]


def both32(b, o, what):
    le, be = u32le(b, o), u32be(b, o + 4)
    if le != be:
        bad("%s 的 LE(%d)/BE(%d) 不一致" % (what, le, be))
    return le


def both16(b, o, what):
    le, be = u16le(b, o), u16be(b, o + 2)
    if le != be:
        bad("%s 的 LE(%d)/BE(%d) 不一致" % (what, le, be))
    return le


class Rec:
    pass


def parse_record(buf, off, end):
    """解析一个目录记录；返回 Rec 或 None（结束标记）"""
    if off >= end:
        return None
    ln = buf[off]
    if ln == 0:
        return None
    if off + ln > end:
        bad("目录记录越界（off=%d len=%d end=%d）" % (off, ln, end))
        return None
    if ln % 2:
        bad("目录记录长度不是偶数（%d）" % ln)
    r = Rec()
    r.len = ln
    r.xattr = buf[off + 1]
    r.extent = both32(buf, off + 2, "目录记录 extent")
    r.size = both32(buf, off + 10, "目录记录 size")
    r.flags = buf[off + 25]
    r.idlen = buf[off + 32]
    r.ident = buf[off + 33:off + 33 + r.idlen]
    return r


def walk(d, sector, size, label, visited, depth=0, joliet=False):
    if depth > 8:
        bad("%s: 目录层数超过 8" % label)
        return []
    if (sector, size) in visited:
        bad("%s: 目录环（sector=%d）" % (label, sector))
        return []
    visited.add((sector, size))
    if sector * SS + size > len(d):
        bad("%s: 目录区超出文件范围" % label)
        return []
    out = []
    # 逐扇区解析，记录不得跨扇区
    pos = sector * SS
    end = pos + size
    first = True
    while pos < end:
        sect_end = min(((pos // SS) + 1) * SS, end)
        off = pos
        while off < sect_end:
            ln = d[off]
            if ln == 0:
                break
            if off + ln > sect_end:
                bad("%s: 目录记录跨越扇区边界（off=%d len=%d 扇区末=%d）"
                    % (label, off, ln, sect_end))
                break
            r = parse_record(d, off, sect_end)
            if r is None:
                break
            if first:
                if r.ident != b'\x00':
                    bad("%s: 首条记录不是 '.'（id=%r）" % (label, r.ident))
                first = False
            out.append(r)
            off += ln
        pos = sect_end
    # '..' 必须存在
    if not any(r.ident == b'\x01' for r in out):
        bad("%s: 缺少 '..' 记录" % label)
    return out


def name_of(r, joliet):
    if r.ident == b'\x00':
        return "."
    if r.ident == b'\x01':
        return ".."
    if joliet:
        try:
            return r.ident.decode('utf-16-be')
        except Exception:
            bad("Joliet 名字不是合法 UCS-2BE: %r" % r.ident)
            return repr(r.ident)
    return r.ident.decode('latin1')


def main():
    path = sys.argv[1]
    d = open(path, 'rb').read()
    if len(d) % SS:
        bad("文件长度 %d 不是 2048 的整数倍" % len(d))
    total_sectors = len(d) // SS

    # ---- 卷描述符 ----
    pvd = svd = None
    s16 = d[16 * SS:17 * SS]
    if s16[1:6] != b'CD001' or s16[6] != 1:
        bad("扇区 16 不是合法的 PVD")
        print_report()
        return 1
    if s16[0] != 1:
        bad("扇区 16 的卷描述符类型不是 1（是 %d）" % s16[0])
    pvd = s16
    n = 17
    while n < 40:
        s = d[n * SS:(n + 1) * SS]
        if s[1:6] != b'CD001':
            bad("扇区 %d 不是卷描述符（缺少 CD001）" % n)
            break
        t = s[0]
        if t == 2 and svd is None:
            svd = s
        if t == 255:
            notes.append("卷描述符结束于扇区 %d" % n)
            break
        n += 1
    if svd is None:
        bad("没有 Joliet 补充卷描述符（SVD）")

    # ---- PVD 字段 ----
    for name, vd, joliet in (("PVD", pvd, False), ("SVD", svd, True)):
        if vd is None:
            continue
        vss = both32(vd, 80, name + " volume space size")
        if vss != total_sectors:
            bad("%s volume_space_size=%d 但实际扇区数=%d" % (name, vss, total_sectors))
        both16(vd, 120, name + " volume set size")
        both16(vd, 124, name + " volume sequence number")
        lbs = both16(vd, 128, name + " logical block size")
        if lbs != SS:
            bad("%s logical_block_size=%d（应为 2048）" % (name, lbs))
        pts = both32(vd, 132, name + " path table size")
        lpt = u32le(vd, 140)
        mpt = u32be(vd, 148)
        if lpt == 0 or lpt >= total_sectors:
            bad("%s Type-L 路径表位置非法：%d" % (name, lpt))
        if mpt == 0 or mpt >= total_sectors:
            bad("%s Type-M 路径表位置非法：%d" % (name, mpt))
        # 路径表内容
        for tbl, loc, big, tname in ((0, lpt, False, 'L'), (0, mpt, True, 'M')):
            off = loc * SS
            if off + pts > len(d):
                bad("%s 路径表 %s 超出文件" % (name, tname))
                continue
            buf = d[off:off + pts]
            p = 0
            cnt = 0
            while p < pts:
                idl = buf[p]
                if idl == 0:
                    bad("%s 路径表 %s 出现 id_len=0" % (name, tname))
                    break
                ext = u32be(buf, p + 2) if big else u32le(buf, p + 2)
                par = u16be(buf, p + 6) if big else u16le(buf, p + 6)
                if ext >= total_sectors:
                    bad("%s 路径表 %s 第 %d 项 extent=%d 越界" % (name, tname, cnt, ext))
                p += 8 + idl + (idl & 1)
                cnt += 1
            if cnt == 0:
                bad("%s 路径表 %s 为空" % (name, tname))
            elif p != pts:
                notes.append("%s 路径表 %s 解析长度 %d / 声明 %d" % (name, tname, p, pts))
        # 根目录记录
        rl = vd[156]
        if rl != 34:
            bad("%s 根目录记录长度=%d（应为 34）" % (name, rl))
        root_extent = both32(vd, 158, name + " 根目录 extent")
        root_size = both32(vd, 166, name + " 根目录 size")
        if not (vd[156 + 25] & 0x02):
            bad("%s 根目录记录未设置“目录”标志" % name)
        # 根目录本身
        recs = walk(d, root_extent, root_size, name + " 根目录", set(), 0, joliet)
        if recs:
            dot = recs[0]
            if dot.extent != root_extent or dot.size != root_size:
                bad("%s 根目录 '.' 记录与卷描述符不一致" % name)

    # ---- Joliet 转义序列 ----
    if svd:
        esc = svd[88:120]
        if not (esc.startswith(b'%/@') or esc.startswith(b'%/C') or esc.startswith(b'%/E')):
            bad("Joliet 转义序列非法: %r" % esc[:8])
        try:
            svd[40:72].decode('utf-16-be')
        except Exception:
            bad("SVD 卷标不是合法的 UCS-2BE")

    # ---- 完整目录遍历：PVD 树 + Joliet 树，检查文件名与 extent ----
    def full(which, vd, joliet):
        rl = vd[156]
        root_extent = both32(vd, 158, which + " 根 extent")
        root_size = both32(vd, 166, which + " 根 size")
        seen_names = {}
        files = []

        def rec_walk(sector, size, prefix, depth):
            recs = walk(d, sector, size, which + " " + (prefix or "/"), set(), depth, joliet)
            for r in recs:
                nm = name_of(r, joliet)
                if nm in ('.', '..'):
                    continue
                full_name = (prefix + '/' + nm) if prefix else nm
                if r.flags & 0x02:
                    rec_walk(r.extent, r.size, full_name, depth + 1)
                else:
                    files.append((full_name, r.extent, r.size, r.flags))
                # 同目录重名检查
                key = (sector, nm.upper() if not joliet else nm)
                if key in seen_names:
                    bad("%s 目录内重名: %s" % (which, full_name))
                seen_names[key] = 1
                # 文件名规则
                if not joliet:
                    if not r.ident.endswith(b';1'):
                        bad("%s 文件标识缺少 ';1': %r" % (which, r.ident))
                    base = r.ident[:-2]
                    if b'.' in base:
                        b1, b2 = base.split(b'.', 1)
                    else:
                        b1, b2 = base, b''
                    if len(b1) > 8 or len(b2) > 3:
                        bad("%s 文件名超过 8.3: %r" % (which, r.ident))
                    for ch in b1 + b2:
                        if not (65 <= ch <= 90 or 48 <= ch <= 57 or ch == 95):
                            bad("%s 文件名含非法字符 %r: %r" % (which, chr(ch), r.ident))
                else:
                    if len(r.ident) > 128:
                        bad("%s Joliet 名字过长" % which)
                # 数据区不得越界
                if r.size and r.extent * SS + r.size > len(d):
                    bad("%s 文件数据越界: %s" % (which, full_name))
        rec_walk(root_extent, root_size, '', 0)
        return files

    pf = full("PVD", pvd, False)
    jf = full("SVD", svd, True) if svd else []
    notes.append("PVD 文件数 %d，Joliet 文件数 %d" % (len(pf), len(jf)))

    # 两棵树应指向同一批 extent/size
    pset = sorted((e, s) for _, e, s, _ in pf)
    jset = sorted((e, s) for _, e, s, _ in jf)
    if pset != jset:
        bad("PVD 与 Joliet 指向的数据区不一致（PVD %d 项 / Joliet %d 项）"
            % (len(pset), len(jset)))

    # 数据区不应重叠
    spans = sorted((e, e * SS + s) for e, s in pset if s)
    for i in range(1, len(spans)):
        if spans[i][0] * SS < spans[i - 1][1]:
            bad("文件数据区重叠：扇区 %d 与 %d" % (spans[i - 1][0], spans[i][0]))

    print_report()
    return 0 if not problems else 1


def print_report():
    print("=== 备注 ===")
    for x in notes:
        print("  ", x)
    if problems:
        print("=== 问题 %d 条 ===" % len(problems))
        for x in problems:
            print("  !", x)
        print("RESULT: FAIL")
    else:
        print("=== 问题 0 条 ===")
        print("RESULT: PASS")


if __name__ == "__main__":
    sys.exit(main())
