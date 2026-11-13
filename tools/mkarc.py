#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把安装目录打包成 InstallShield 风格的 DATA1.CAB（SMARC 格式），
同时生成供人阅读的 SETUP.PKG 文件清单。

用法:
  py mkarc.py --src <安装文件目录> --out DATA1.CAB [--pkg SETUP.PKG] [--label SMARTMATE]

格式（小端，pack(1)）:
  头 20 字节:  magic[8]="SMARC1\\0\\0", uint32 version, uint32 count, uint32 data_off
  目录项 112 字节/个: name[100], uint32 size, uint32 crc32, uint32 attrib
  数据区: 按目录顺序拼接的文件内容
"""
import argparse
import os
import struct
import sys
import zlib

MAGIC = b"SMARC1\0\0"
HDR = struct.Struct("<8sIII")
ENT = struct.Struct("<100sIII")
ENC = "gbk"


def collect(src):
    items = []
    for root, dirs, files in os.walk(src):
        dirs.sort()
        for d in sorted(dirs):
            full = os.path.join(root, d)
            rel = os.path.relpath(full, src).replace("/", "\\")
            items.append((rel, None, 0x10))
        for f in sorted(files):
            full = os.path.join(root, f)
            rel = os.path.relpath(full, src).replace("/", "\\")
            items.append((rel, full, 0x20))
    items.sort(key=lambda it: (it[0].count("\\"), it[0].upper()))
    return items


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--pkg", default=None)
    ap.add_argument("--label", default="SMARTMATE")
    ap.add_argument("--version", default="2.00.0871")
    args = ap.parse_args()

    items = collect(args.src)
    if not items:
        print("错误: 源目录为空", file=sys.stderr)
        return 2
    if len(items) > 4000:
        print("错误: 文件太多 (SMARC 上限 4000)", file=sys.stderr)
        return 2

    data_off = HDR.size + ENT.size * len(items)
    blobs = []
    entries = []
    for rel, full, attrib in items:
        data = b""
        if full is not None:
            with open(full, "rb") as f:
                data = f.read()
        crc = zlib.crc32(data) & 0xFFFFFFFF
        name = rel.encode(ENC, "replace")[:99]
        entries.append(ENT.pack(name, len(data), crc, attrib))
        blobs.append(data)

    with open(args.out, "wb") as f:
        f.write(HDR.pack(MAGIC, 1, len(items), data_off))
        for e in entries:
            f.write(e)
        for b in blobs:
            f.write(b)

    total = sum(len(b) for b in blobs)
    print("DATA1.CAB: %d 项, 数据 %d 字节, 总计 %d 字节"
          % (len(items), total, os.path.getsize(args.out)))

    if args.pkg:
        lines = []
        lines.append("; ------------------------------------------------------------")
        lines.append(";  %s  安装文件清单 (SETUP.PKG)" % args.label)
        lines.append(";  版本 %s" % args.version)
        lines.append(";  本文件由安装程序制作工具自动生成，请勿手工修改。")
        lines.append("; ------------------------------------------------------------")
        lines.append(";")
        lines.append("; [Product]")
        lines.append("Name=%s" % args.label)
        lines.append("Version=%s" % args.version)
        lines.append("FileCount=%d" % len(items))
        lines.append("TotalBytes=%d" % total)
        lines.append(";")
        lines.append("; [Files]  序号  大小  属性  文件名")
        for i, ((rel, full, attrib), blob) in enumerate(zip(items, blobs)):
            kind = "DIR " if attrib == 0x10 else "FILE"
            lines.append("%4d  %10d  %s  %s" % (i + 1, len(blob), kind, rel))
        lines.append("; ---- 清单结束 ----")
        with open(args.pkg, "wb") as f:
            f.write("\r\n".join(lines).encode(ENC, "replace") + b"\r\n")
        print("SETUP.PKG: %d 行" % len(lines))
    return 0


if __name__ == "__main__":
    sys.exit(main())
