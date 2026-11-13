#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""CHM 结构对比工具（按 chmlib 的字段顺序解析）。

用法： python tools/chmdump.py a.chm [b.chm ...]
"""

import io
import os
import struct
import sys


def u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def u64(b, o):
    return struct.unpack_from("<Q", b, o)[0]


def dump(path):
    d = io.open(path, "rb").read()
    if d[:4] != b"ITSF":
        print("不是 CHM: %s" % path)
        return
    print("== %s  (%d 字节) ==" % (os.path.basename(path), len(d)))
    print("  ITSF ver=%d hdrlen=0x%X unk=0x%X ts=0x%08X lang=0x%04X"
          % (u32(d, 4), u32(d, 8), u32(d, 0x0C), u32(d, 0x10), u32(d, 0x14)))
    print("  GUID1=%s" % d[0x18:0x28].hex())
    print("  GUID2=%s" % d[0x28:0x38].hex())
    s1_off, s1_len = u64(d, 0x38), u64(d, 0x40)
    dn_off, dn_len = u64(d, 0x48), u64(d, 0x50)
    print("  section1 off=0x%X len=0x%X" % (s1_off, s1_len))
    print("  content  off=0x%X len=0x%X  (+0x58=0x%X)"
          % (dn_off, dn_len, u64(d, 0x58) if len(d) >= 0x60 else 0))

    # ITSP：直接从 content 偏移读
    p = dn_off
    sig = d[p:p + 4]
    print("  ITSP sig=%r ver=%d hdrlen=0x%X unk=0x%X"
          % (sig, u32(d, p + 4), u32(d, p + 8), u32(d, p + 0x0C)))
    chunk = u32(d, p + 0x10)
    density = u32(d, p + 0x14)
    depth = u32(d, p + 0x18)
    root = u32(d, p + 0x1C)
    first = u32(d, p + 0x20)
    last = u32(d, p + 0x24)
    print("    chunk=0x%X density=%d depth=%d root=%d first=%d last=%d"
          % (chunk, density, depth, root, first, last))

    # 找所有 PMGL 块（从 content 开始扫描）
    pos = p
    found = []
    while pos + 20 <= len(d):
        i = d.find(b"PMGL", pos)
        if i < 0:
            break
        found.append(i)
        pos = i + 4
        if len(found) > 300:
            break
    print("    PMGL 块数: %d  首个偏移=0x%X" % (len(found), found[0] if found else -1))

    if found:
        q = found[0]
        free = u32(d, q + 4)
        prev = u32(d, q + 8)
        nxt = u32(d, 0x0C + q)
        print("    PMGL[0] free=0x%X prev=%d next=%d" % (free, prev, nxt))
        end = q + chunk - free
        r = q + 20
        n = 0
        while r + 16 <= end and n < 20:
            nl = u32(d, r)
            if nl == 0 or nl > 200:
                print("    !! 文件名长度异常 %d @0x%X" % (nl, r))
                break
            name = d[r + 4:r + 4 + nl].decode("latin1")
            sec = u32(d, r + 4 + nl)
            off = u32(d, r + 8 + nl)
            size = u32(d, r + 12 + nl)
            print("      %-26s sec=%d off=0x%-6X size=%d" % (name, sec, off, size))
            r += 16 + nl
            n += 1

    # #SYSTEM 粗查
    i = d.find(b"#SYSTEM")
    print("  #SYSTEM 出现于 0x%X" % i if i >= 0 else "  没有 #SYSTEM")
    print()


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    for p in sys.argv[1:]:
        dump(p)
    return 0


if __name__ == "__main__":
    sys.exit(main())
