#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""从 brand.ini 生成品牌注入文件。

产物（都是自动生成，不要手改）：
  src/common/nb_brand.h       —— C 侧宏（产品名/公司/版本/目录/文件名）
  src/common/brand_res.h      —— 资源脚本用的图标路径
  src/common/brand_rc.inc     —— 版本信息资源块（VERSIONINFO）
  src/common/brand_setup.ins  —— 安装程序的 SETUP.INS 品牌字段（可选）

用法：
  python tools/mkbrand.py [brand.ini] [--out DIR]
"""

import io
import os
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


# --------------------------------------------------------------------------
# 一个够用的 INI 读取器（不依赖 configparser 的插值行为）
# --------------------------------------------------------------------------
def read_ini(path):
    data = {}
    sec = None
    with io.open(path, "r", encoding="utf-8-sig") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith(";") or line.startswith("#"):
                continue
            if line.startswith("[") and line.endswith("]"):
                sec = line[1:-1].strip().lower()
                data.setdefault(sec, {})
                continue
            if "=" not in line or sec is None:
                continue
            k, v = line.split("=", 1)
            # 去掉行尾注释（前面有空格的 ; ）
            v = v.strip()
            data[sec][k.strip().lower()] = v
    return data


def get(ini, sec, key, default=""):
    return ini.get(sec, {}).get(key, default) or default


def cstr(s):
    """C 字符串字面量（转义反斜杠与引号）"""
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def rcstr(s):
    return '"' + s.replace('"', "'") + '"'


def parse_tuple(s):
    parts = [p.strip() for p in s.split(",")]
    if len(parts) != 4 or not all(p.isdigit() for p in parts):
        raise SystemExit("brand.ini: version_tuple 必须是 4 个数字，例如 3,0,0,2003（现在是 %r）" % s)
    return [int(p) for p in parts]


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    brand_path = args[0] if args else os.path.join(ROOT, "brand.ini")
    out_dir = os.path.join(ROOT, "src", "common")
    for i, a in enumerate(sys.argv):
        if a == "--out" and i + 1 < len(sys.argv):
            out_dir = sys.argv[i + 1]

    if not os.path.exists(brand_path):
        raise SystemExit("找不到品牌文件: " + brand_path)

    ini = read_ini(brand_path)

    name = get(ini, "product", "name", "Axiom NeuroBench 2003")
    name_en = get(ini, "product", "name_en", name)
    short = get(ini, "product", "short", name)
    short_en = get(ini, "product", "short_en", short)
    nickname = get(ini, "product", "nickname", short)
    nickname_en = get(ini, "product", "nickname_en", short_en)
    desc = get(ini, "product", "description", "")
    desc_en = get(ini, "product", "description_en", desc)
    ver = get(ini, "product", "version", "1.0.0.0")
    vt = parse_tuple(get(ini, "product", "version_tuple", "1,0,0,0"))

    comp = get(ini, "company", "name", "Axiom Software")
    comp_en = get(ini, "company", "name_en", comp)
    copyr = get(ini, "company", "copyright", "Copyright (C) " + comp)
    site = get(ini, "company", "website", "")

    exe = get(ini, "layout", "exe", "APP")
    ini_name = get(ini, "layout", "ini_name", "APP.INI")
    reg_root = get(ini, "layout", "reg_root", "Software\\" + comp + "\\" + short)
    subdir = get(ini, "layout", "install_subdir", comp + "\\" + short)
    menu = get(ini, "layout", "menu_group", subdir)
    slug = get(ini, "layout", "slug", exe.upper())
    iso1 = get(ini, "layout", "iso_label", exe.upper())
    iso2 = get(ini, "layout", "iso_label_tools", (exe.upper() + "TOOLS")[:32])

    ico_agent = get(ini, "icons", "agent", "")
    ico_setup = get(ini, "icons", "setup", "")
    ico_keygen = get(ini, "icons", "keygen", "")
    ico_uninst = get(ini, "icons", "uninst", "")

    vlang = get(ini, "versioninfo", "lang", "080404b0")
    vcp = get(ini, "versioninfo", "codepage", "1200")
    vcomments = get(ini, "versioninfo", "comments", "")

    stamp = time.strftime("%Y-%m-%d %H:%M:%S")
    src_name = os.path.basename(brand_path)

    # ---------------- nb_brand.h ----------------
    h = []
    h.append("/* 自动生成，请勿手改。源: %s   生成时间: %s */" % (src_name, stamp))
    h.append("/* 由 tools/mkbrand.py 生成。改品牌请编辑 brand.ini 后重新编译。 */")
    h.append("#ifndef NB_BRAND_H")
    h.append("#define NB_BRAND_H")
    h.append("")
    h.append("#define SM_PRODUCT_NAME      %s" % cstr(name))
    h.append("#define SM_PRODUCT_NAME_EN   %s" % cstr(name_en))
    h.append("#define SM_SHORT_NAME        %s" % cstr(short))
    h.append("#define SM_SHORT_NAME_EN     %s" % cstr(short_en))
    h.append("#define SM_NICKNAME          %s" % cstr(nickname))
    h.append("#define SM_NICKNAME_EN       %s" % cstr(nickname_en))
    h.append("#define SM_DESCRIPTION       %s" % cstr(desc))
    h.append("#define SM_DESCRIPTION_EN    %s" % cstr(desc_en))
    h.append("#define SM_COMPANY           %s" % cstr(comp))
    h.append("#define SM_COMPANY_EN        %s" % cstr(comp_en))
    h.append("#define SM_COPYRIGHT         %s" % cstr(copyr))
    h.append("#define SM_WEBSITE           %s" % cstr(site))
    h.append("#define SM_VERSION_STR       %s" % cstr(ver))
    h.append("#define SM_VER_MAJOR         %d" % vt[0])
    h.append("#define SM_VER_MINOR         %d" % vt[1])
    h.append("#define SM_VER_BUILD         %d" % vt[3])   # 界面上显示的构建号
    h.append("#define SM_VER_PATCH         %d" % vt[2])
    h.append("#define SM_VER_TUPLE         %d,%d,%d,%d" % tuple(vt))
    h.append("#define SM_EXE_NAME          %s" % cstr(exe + ".EXE"))
    h.append("#define SM_EXE_BASE          %s" % cstr(exe))
    h.append("#define SM_INI_NAME          %s" % cstr(ini_name))
    h.append("#define SM_REG_ROOT          %s" % cstr(reg_root))
    h.append("#define SM_INSTALL_SUBDIR    %s" % cstr(subdir))
    h.append("#define SM_MENU_GROUP        %s" % cstr(menu))
    h.append("#define SM_ISO_LABEL         %s" % cstr(iso1))
    h.append("#define SM_ISO_LABEL_TOOLS   %s" % cstr(iso2))
    h.append("")
    h.append("#endif /* NB_BRAND_H */")
    h.append("")

    # ---------------- brand_res.h（给 .rc 用） ----------------
    r = []
    r.append("/* 自动生成，请勿手改。源: %s */" % src_name)
    r.append("#ifndef BRAND_RES_H")
    r.append("#define BRAND_RES_H")
    for tag, val in (("BRAND_ICON_AGENT", ico_agent), ("BRAND_ICON_SETUP", ico_setup),
                     ("BRAND_ICON_KEYGEN", ico_keygen), ("BRAND_ICON_UNINST", ico_uninst)):
        if val:
            ab = os.path.abspath(os.path.join(ROOT, val))
            r.append('#define %-18s "%s"' % (tag, ab.replace("\\", "\\\\")))
        else:
            r.append("#undef %s" % tag)
    r.append("#endif /* BRAND_RES_H */")
    r.append("")

    # ---------------- brand_rc.inc（版本信息） ----------------
    v = []
    v.append("/* 自动生成，请勿手改。源: %s */" % src_name)
    v.append("/* 使用前请先 #define BRAND_INTERNAL / BRAND_ORIGNAME / BRAND_FILEDESC */")
    v.append("#ifndef BRAND_INTERNAL")
    v.append('#define BRAND_INTERNAL "%s"' % exe)
    v.append("#endif")
    v.append("#ifndef BRAND_ORIGNAME")
    v.append('#define BRAND_ORIGNAME "%s.EXE"' % exe)
    v.append("#endif")
    v.append("#ifndef BRAND_FILEDESC")
    v.append('#define BRAND_FILEDESC "%s"' % (desc_en or name_en))
    v.append("#endif")
    v.append("")
    v.append("VS_VERSION_INFO VERSIONINFO")
    v.append(" FILEVERSION    %d,%d,%d,%d" % tuple(vt))
    v.append(" PRODUCTVERSION %d,%d,%d,%d" % tuple(vt))
    v.append(" FILEFLAGSMASK  0x3fL")
    v.append(" FILEFLAGS      0x0L")
    v.append(" FILEOS         VOS__WINDOWS32")
    v.append(" FILETYPE       VFT_APP")
    v.append(" FILESUBTYPE    VFT2_UNKNOWN")
    v.append("BEGIN")
    v.append('    BLOCK "StringFileInfo"')
    v.append("    BEGIN")
    v.append('        BLOCK "%s"' % vlang)
    v.append("        BEGIN")
    v.append('            VALUE "CompanyName",      %s' % rcstr(comp_en))
    v.append('            VALUE "FileDescription",  BRAND_FILEDESC')
    v.append('            VALUE "FileVersion",      %s' % rcstr(ver))
    v.append('            VALUE "InternalName",     BRAND_INTERNAL')
    v.append('            VALUE "LegalCopyright",   %s' % rcstr(copyr))
    v.append('            VALUE "OriginalFilename", BRAND_ORIGNAME')
    v.append('            VALUE "ProductName",      %s' % rcstr(name_en))
    v.append('            VALUE "ProductVersion",   %s' % rcstr(ver))
    if vcomments:
        v.append('            VALUE "Comments",         %s' % rcstr(vcomments))
    v.append("        END")
    v.append("    END")
    v.append('    BLOCK "VarFileInfo"')
    v.append("    BEGIN")
    v.append('        VALUE "Translation", 0x%x, %s' % (int(vlang[:4], 16), vcp))
    v.append("    END")
    v.append("END")
    v.append("")

    jobs = (("nb_brand.h", "\n".join(h), "utf-8"),
            ("brand_res.h", "\n".join(r), "gbk"),
            ("brand_rc.inc", "\n".join(v), "gbk"))
    for fn, content, enc in jobs:
        path = os.path.join(out_dir, fn)
        with io.open(path, "w", encoding=enc, newline="\n") as f:
            f.write(content)
        print("  生成 %s (%s)" % (os.path.relpath(path, ROOT), enc))

    # 给 build.ps1 用的机器可读摘要
    summary = {
        "product": name, "short": short, "company": comp, "version": ver,
        "exe": exe, "slug": slug, "iso_label": iso1, "iso_label_tools": iso2,
        "install_subdir": subdir, "menu_group": menu,
    }
    with io.open(os.path.join(out_dir, "brand_summary.json"), "w", encoding="utf-8") as f:
        import json
        json.dump(summary, f, ensure_ascii=False, indent=2)

    print("  品牌: %s / %s / %s / v%s" % (name, comp, exe + ".EXE", ver))


if __name__ == "__main__":
    main()
