#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""给附加工具（CDKEY / UNINST / CERTUPD / SYSINFO / NB-SP1）加上双语界面。

做法：把列出的中文字面量替换成 TS("中文", "English")。
TS() 在 nb_lang.c 中实现，按当前界面语言返回其中之一。
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
T = os.path.join(ROOT, "src", "tools")

M = {
    "keygen.c": [
        ("NeuroBench 2003 产品序列号生成器 [OEM 内部工具]",
         "NeuroBench 2003 Product Key Generator [OEM internal]"),
        ("生成参数(&P)", "Generation &parameters"),
        ("产品版本(&V)：", "&Edition:"),
        ("标准版 (1 用户)", "Standard (1 seat)"),
        ("专业版", "Professional"),
        ("企业版", "Enterprise"),
        ("用户数(&S)：", "&Seats:"),
        ("有效天数(&D)：", "Valid &days:"),
        ("（0 表示永久授权，最大 511）", "(0 = perpetual, max 511)"),
        ("生成数量(&N)：", "&Quantity:"),
        ("（一次最多 200 个）", "(max 200 at a time)"),
        ("起始序号(&I)：", "Starting &serial:"),
        ("（仅用于内部记录，不参与校验）", "(internal record only, not validated)"),
        ("生成结果(&R)：", "&Results:"),
        ("生成(&G)", "&Generate"),
        ("保存到文件(&F)...", "Save to &file..."),
        ("复制(&C)", "&Copy"),
        ("退出(&X)", "E&xit"),
        ("内部工具 · 请勿外传 · (C) 1993-2003 Axiom Software",
         "Internal tool - do not distribute - (C) 1993-2003 Axiom Software"),
        ("生成器", "Key generator"),
        ("写入文件失败。", "Could not write the file."),
        ("保存序列号清单", "Save key list"),
        ("文本文件 (*.txt)", "Text files (*.txt)"),
        ("所有文件 (*.*)", "All files (*.*)"),
        ("; NeuroBench 2003 - 产品序列号清单\\r\\n",
         "; NeuroBench 2003 - product key list\\r\\n"),
        ("; 生成时间: %s\\r\\n", "; Generated: %s\\r\\n"),
        ("; 版本: %s   用户数: %d   有效天数: %d\\r\\n",
         "; Edition: %s   Seats: %d   Valid days: %d\\r\\n"),
        ("; 数量: %d\\r\\n\\r\\n", "; Quantity: %d\\r\\n\\r\\n"),
    ],
    "UNINST.c": [
        ("NeuroBench 2003 卸载", "Uninstall NeuroBench 2003"),
        ("SmartMate 智能伴侣 2000 卸载程序", "Axiom NeuroBench 2003 Uninstall"),
        ("将删除本软件在您计算机上安装的全部文件",
         "Removes every file this program installed on your computer"),
        ("卸载(&U)", "&Uninstall"),
        ("退出(&X)", "E&xit"),
        ("正在准备...", "Preparing..."),
        ("卸载尚未完成，确实要退出吗？", "Uninstall is not finished. Really quit?"),
        ("找不到卸载记录 UNINST.LST。\\r\\n\\r\\n"
         "无法自动卸载，请手工删除安装文件夹。",
         "UNINST.LST was not found.\\r\\n\\r\\nAutomatic removal is not possible; "
         "delete the installation folder by hand."),
        ("正在删除：%s", "Removing: %s"),
        ("NeuroBench 智能伴侣 2000 已从您的计算机上删除。\\r\\n\\r\\n"
         "谢谢使用本软件。",
         "Axiom NeuroBench 2003 has been removed from your computer.\\r\\n\\r\\n"
         "Thank you for using it."),
    ],
    "CERTUPD.c": [
        ("根证书更新程序", "Root Certificate Update"),
        ("NeuroBench 2003 附加组件", "Axiom NeuroBench 2003 add-on"),
        ("从文件导入(&I)...", "&Import from file..."),
        ("测试连接(&T)", "&Test connection"),
        ("打开证书文件夹(&O)", "&Open certificates folder"),
        ("关闭", "Close"),
        ("选择根证书包", "Select a root certificate bundle"),
        ("证书文件 (*.pem;*.crt;*.cer)", "Certificate files (*.pem;*.crt;*.cer)"),
        ("所有文件 (*.*)", "All files (*.*)"),
        ("无法读取所选文件。", "Could not read the selected file."),
        ("根证书包已更新。\\r\\n\\r\\n请重新启动主程序后再试。",
         "The root certificate bundle has been updated.\\r\\n\\r\\n"
         "Restart the main program and try again."),
        ("写入证书文件失败，请检查磁盘空间或权限。",
         "Could not write the certificate file. Check disk space and permissions."),
        ("连接测试结果", "Connection test"),
        ("正在测试与服务器的安全连接，请稍候...\\r\\n",
         "Testing the secure connection to the server, please wait...\\r\\n"),
    ],
    "SYSINFO.c": [
        ("系统信息报告", "System Information Report"),
        ("复制(&C)", "&Copy"),
        ("另存为(&S)...", "Save &As..."),
        ("关闭", "Close"),
        ("文本文件 (*.txt)", "Text files (*.txt)"),
        ("所有文件 (*.*)", "All files (*.*)"),
        ("写入文件失败。", "Could not write the file."),
    ],
    "NB-SP1.c": [
        ("NeuroBench 2003 服务包 1 安装程序", "Axiom NeuroBench 2003 Service Pack 1 Setup"),
        ("NeuroBench 2003 服务包 1", "Axiom NeuroBench 2003 Service Pack 1"),
        ("安装目录(&D)：", "Install &folder:"),
        ("浏览(&B)...", "&Browse..."),
        ("安装(&I)", "&Install"),
        ("退出(&X)", "E&xit"),
        ("等待开始...", "Ready to install..."),
        ("正在更新：%s", "Updating: %s"),
        ("服务包尚未安装，确实要退出吗？", "Service Pack 1 is not installed. Really quit?"),
        ("请先指定 NeuroBench 2003 的安装目录。",
         "Please specify the NeuroBench 2003 installation folder first."),
        ("请选择 NeuroBench 2003 的安装文件夹",
         "Select the NeuroBench 2003 installation folder"),
        ("服务包", "Service Pack"),
    ],
}


def wrap(text, zh, en):
    needle = '"%s"' % zh
    if needle not in text:
        return text, False
    return text.replace(needle, 'TS("%s",\n           "%s")' % (zh, en)), True


def main():
    total_miss = 0
    for fname, pairs in M.items():
        p = os.path.join(ROOT, "src", "keygen", fname) if fname == "keygen.c" else os.path.join(T, fname)
        src = open(p, encoding="utf-8").read()
        # 加入头文件与语言初始化
        if "nb_lang.h" not in src:
            src = src.replace('#include "../common/sm.h"',
                              '#include "../common/sm.h"\n#include "../common/nb_lang.h"')
        for zh, en in pairs:
            src, ok = wrap(src, zh, en)
            if not ok:
                print("  MISS %-12s %s" % (fname, zh[:40]))
                total_miss += 1
        open(p, "w", encoding="utf-8").write(src)
        print("%-12s ok" % fname)
    print("未命中 %d 条" % total_miss)
    return 0


if __name__ == "__main__":
    sys.exit(main())
