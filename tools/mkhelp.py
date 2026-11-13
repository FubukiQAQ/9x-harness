#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Axiom NeuroBench 2003 - 帮助文件源准备工具

帮助页面的源文件（中文 GBK 编码、英文 CP1252 编码的 HTML）存放在 src/help/ 下，
可以直接用记事本编辑。本工具负责：
  1. 检查必备页面是否齐全；
  2. 把 src/help 复制到 build/help；
  3. 重新生成目录 (toc.hhc) 与索引 (index.hhk)（中英两个分支）。

然后用 mkchm.py 编译成 NEUROBENCH.CHM：
  py mkchm.py --src build/help --out build/out/NEUROBENCH.CHM \
      --title "Axiom NeuroBench 2003 Help" --default index.htm \
      --toc toc.hhc --index index.hhk

用法: py mkhelp.py [--check]
"""
import io
import os
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src", "help")
OUT = os.path.join(ROOT, "build", "help")
ENC = "gbk"

# (分支标题, [(文件名, 标题), ...])  —— 目录里按分支显示
TOC_GROUPS = [
    ("中文帮助", [
        ("index.htm",      "Axiom NeuroBench 2003 帮助"),
        ("quickstart.htm", "快速安装与使用"),
        ("settings.htm",   "配置服务器与密钥"),
        ("tools.htm",      "工具调用说明"),
        ("workspace.htm",  "工作区与文件"),
        ("activate.htm",   "激活与试用"),
        ("editor.htm",     "内置文本编辑器"),
        ("faq.htm",        "常见问题"),
        ("history.htm",    "版本历史"),
        ("about.htm",      "关于本软件"),
    ]),
    ("English help", [
        ("en_index.htm",      "Axiom NeuroBench 2003 Help"),
        ("en_quickstart.htm", "Quick start"),
        ("en_settings.htm",   "Server and key settings"),
        ("en_tools.htm",      "Tool calling"),
        ("en_workspace.htm",  "Workspaces and files"),
        ("en_activate.htm",   "Activation and trial"),
        ("en_editor.htm",     "The built-in text editor"),
        ("en_faq.htm",        "Troubleshooting"),
        ("en_history.htm",    "Version history"),
        ("en_about.htm",      "About"),
    ]),
]

KEYWORDS = [
    ("安装", "quickstart.htm"),
    ("激活", "activate.htm"),
    ("产品序列号", "activate.htm"),
    ("试用版", "activate.htm"),
    ("服务器地址", "settings.htm"),
    ("API 密钥", "settings.htm"),
    ("密钥", "settings.htm"),
    ("工具调用", "tools.htm"),
    ("确认对话框", "tools.htm"),
    ("工作区", "workspace.htm"),
    ("会话文件", "workspace.htm"),
    ("文本编辑器", "editor.htm"),
    ("编码", "tools.htm"),
    ("乱码", "faq.htm"),
    ("证书", "settings.htm"),
    ("根证书", "settings.htm"),
    ("TLS", "settings.htm"),
    ("常见问题", "faq.htm"),
    ("版本历史", "history.htm"),
    ("卸载", "faq.htm"),
    ("workspace", "en_workspace.htm"),
    ("tool calling", "en_tools.htm"),
    ("activation", "en_activate.htm"),
    ("troubleshooting", "en_faq.htm"),
    ("server URL", "en_settings.htm"),
]


def all_pages():
    for _, pages in TOC_GROUPS:
        for f, t in pages:
            yield f, t


def _brand(text):
    """目录/索引里的品牌名跟 brand.ini 走。"""
    import json
    path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                        "src", "common", "brand_summary.json")
    if not os.path.exists(path):
        return text
    with io.open(path, encoding="utf-8") as f:
        b = json.load(f)
    short = b.get("short", "NeuroBench 2003")
    out = text.replace("Axiom NeuroBench 2003", b.get("product", "Axiom NeuroBench 2003"))
    out = out.replace("NeuroBench 2003", short)
    out = out.replace("NeuroBench", short)
    return out


def write_toc():
    t = ['<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">',
         '<HTML><HEAD><meta http-equiv="Content-Type" content="text/html; charset=gb2312">',
         '<meta name="GENERATOR" content="Microsoft&reg; HTML Help Workshop 4.1">',
         '</HEAD><BODY>',
         '<OBJECT type="text/site properties">',
         '<param name="Window Styles" value="0x800025">',
         '<param name="ImageType" value="Folder">',
         '</OBJECT>',
         '<UL>']
    for group, pages in TOC_GROUPS:
        t.append('<LI><OBJECT type="text/sitemap">')
        t.append('<param name="Name" value="%s">' % group)
        t.append('</OBJECT>')
        t.append('<UL>')
        for fname, title in pages:
            t.append('<LI><OBJECT type="text/sitemap">')
            t.append('<param name="Name" value="%s">' % title)
            t.append('<param name="Local" value="%s">' % fname)
            t.append('</OBJECT>')
        t.append('</UL>')
    t.append('</UL>')
    t.append('</BODY></HTML>')
    with open(os.path.join(OUT, "toc.hhc"), "wb") as f:
        f.write(_brand("\r\n".join(t)).encode(ENC, "replace"))


def write_index():
    t = ['<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">',
         '<HTML><HEAD><meta http-equiv="Content-Type" content="text/html; charset=gb2312">',
         '<meta name="GENERATOR" content="Microsoft&reg; HTML Help Workshop 4.1">',
         '</HEAD><BODY>', '<UL>']
    for kw, target in KEYWORDS:
        t.append('<LI><OBJECT type="text/sitemap">')
        t.append('<param name="Name" value="%s">' % kw)
        t.append('<param name="Name" value="%s">' % kw)
        t.append('<param name="Local" value="%s">' % target)
        t.append('</OBJECT>')
    t.append('</UL>')
    t.append('</BODY></HTML>')
    with open(os.path.join(OUT, "index.hhk"), "wb") as f:
        f.write(_brand("\r\n".join(t)).encode(ENC, "replace"))


def main():
    pages = list(all_pages())
    missing = [f for f, _ in pages if not os.path.exists(os.path.join(SRC, f))]
    if missing:
        sys.stderr.write("缺少帮助页面: %s\n" % ", ".join(missing))
        return 1
    if "--check" in sys.argv:
        print("帮助源检查通过：%d 个页面（2 个语言分支）" % len(pages))
        return 0

    os.makedirs(OUT, exist_ok=True)
    for fname, _ in pages:
        shutil.copy(os.path.join(SRC, fname), os.path.join(OUT, fname))
    write_toc()
    write_index()
    print("帮助源已准备：%d 个页面 + toc.hhc + index.hhk -> %s" % (len(pages), OUT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
