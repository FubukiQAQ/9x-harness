#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把安装程序里的中文字面量包成 TS("中文","English")，实现双语安装向导。

只替换 TRANS 表中列出的字面量；每条替换都会统计命中次数，命中 0 次的会
被报告出来（说明源文本已经变了，需要更新本表）。

用法: py i18n_setup.py [--dry]
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TARGET = os.path.join(ROOT, "src", "setup", "setup.c")

# 中文原文 -> 英文译文
TRANS = [
    ("安装程序", "Setup"),
    ("完成(&F)", "&Finish"),
    ("下一步(&N) >", "&Next >"),
    ("上一步(&B) <", "< &Back"),
    ("取消", "Cancel"),
    ("欢迎使用 NeuroBench 2003 安装向导", "Welcome to the NeuroBench 2003 Setup Wizard"),
    ("欢迎使用 Axiom NeuroBench 2003 安装向导。\\r\\n\\r\\n",
     "Welcome to the Axiom NeuroBench 2003 Setup Wizard.\\r\\n\\r\\n"),
    ("本向导将指导您完成软件的安装。安装完成后，您可以从",
     "This wizard will guide you through the installation. When it finishes you can"),
    ("「开始」菜单或桌面启动本程序。\\r\\n\\r\\n",
     "start the program from the Start menu or from the desktop.\\r\\n\\r\\n"),
    ("安装前请关闭其他正在运行的程序。\\r\\n\\r\\n",
     "Please close any other running programs before continuing.\\r\\n\\r\\n"),
    ("本程序适用于 Windows 95 / 98 / NT 4.0 / 2000 / XP，",
     "This program runs on Windows 95 / 98 / NT 4.0 / 2000 / XP"),
    ("需要至少 16 MB 硬盘空间。", "and needs at least 16 MB of free disk space."),
    ("警告：本计算机程序受著作权法和国际公约的保护。未经授权擅自复制或",
     "Warning: this computer program is protected by copyright law and international"),
    ("传播本程序或其中任何部分，将承担相应的法律责任。",
     "treaties. Unauthorised reproduction or distribution may lead to legal action."),
    ("许可证协议", "License Agreement"),
    ("请仔细阅读下列许可证协议。您必须接受协议的全部条款才能继续安装。",
     "Please read the following license agreement. You must accept all of its terms to continue."),
    ("我接受上述许可证协议的全部条款(&A)", "I &accept the terms of this license agreement"),
    ("我不接受上述许可证协议(&D)", "I do &not accept the agreement"),
    ("用户信息", "User Information"),
    ("请填写您的姓名和单位。安装程序会把它们记录在软件的“关于”对话框中。",
     "Enter your name and company. Setup stores them in the program's About dialog."),
    ("姓名(&U)：", "&User name:"),
    ("单位(&O)：", "&Company:"),
    ("产品序列号(&K)：", "Product &key:"),
    ("序列号由光盘包装或经销商提供，格式为 5 组、每组 4 位字符。\\r\\n",
     "The key is on the disc sleeve or supplied by your dealer: 5 groups of 4 characters.\\r\\n"),
    ("此处可以留空，安装完成后在「工具 - 激活产品」中再输入。",
     "You may leave it blank and enter it later under Tools - Activate Product."),
    ("选择目标位置", "Choose Destination Location"),
    ("安装程序将把 NeuroBench 2003 安装到下列文件夹。\\r\\n",
     "Setup will install NeuroBench 2003 into the following folder.\\r\\n"),
    ("若要安装到其他文件夹，请按「浏览」。", "Click Browse to choose a different folder."),
    ("目标文件夹(&D)：", "Destination &folder:"),
    ("浏览(&B)...", "&Browse..."),
    ("所需空间：16 MB        可用空间：%.0f MB",
     "Space required: 16 MB        Space available: %.0f MB"),
    ("所需空间：16 MB", "Space required: 16 MB"),
    ("目标文件夹必须是一个本地硬盘上的完整路径。",
     "The destination folder must be a full path on a local hard disk."),
    ("选择程序文件夹", "Select Program Folder"),
    ("安装程序将把程序图标添加到下列程序文件夹中。您可以输入新的",
     "Setup will add program icons to the folder below. You may type a new"),
    ("文件夹名称，或从下面的列表中选择一个已有的文件夹。",
     "folder name, or select one from the existing folders list."),
    ("程序文件夹(&P)：", "Program &folder:"),
    ("现有的文件夹(&X)：", "&Existing folders:"),
    ("正在安装", "Installing"),
    ("安装程序正在把文件复制到您的计算机。", "Setup is copying files to your computer."),
    ("正在准备...", "Preparing..."),
    ("请稍候。此过程可能需要几分钟。", "Please wait. This may take a few minutes."),
    ("安装完成", "Installation Complete"),
    ("Axiom NeuroBench 2003 已经安装到您的计算机上。\\r\\n\\r\\n",
     "Axiom NeuroBench 2003 has been installed on your computer.\\r\\n\\r\\n"),
    ("首次使用前，请通过「工具 - 设置」填写云端服务器地址与 API 密钥，",
     "Before first use, open Tools - Options and fill in the cloud server URL and API key,"),
    ("并用「工具 - 激活产品」输入产品序列号完成激活。\\r\\n\\r\\n",
     "then use Tools - Activate Product to enter your product key.\\r\\n\\r\\n"),
    ("单击「完成」结束安装。", "Click Finish to end Setup."),
    ("立即启动 Axiom NeuroBench 2003(&S)", "&Launch Axiom NeuroBench 2003 now"),
    ("在桌面上创建快捷方式(&D)", "Create a shortcut on the &desktop"),
    ("正在复制：%s", "Copying: %s"),
    ("正在检查安装数据...", "Checking installation data..."),
    ("无法创建目标文件夹。请检查路径是否正确，以及您是否有写入该文件夹的权限。",
     "Cannot create the destination folder. Check the path and your write permission."),
    ("复制文件时出错，安装未能完成。请检查磁盘空间是否已满，然后重新运行安装程序。",
     "An error occurred while copying files. Check free disk space and run Setup again."),
    ("正在创建程序图标...", "Creating program icons..."),
    ("快速入门.lnk", "Quick Start.lnk"),
    ("打开帮助文件", "Open the help file"),
    ("自述文件.lnk", "Read Me.lnk"),
    ("自述文件", "Read Me"),
    ("卸载 NeuroBench 2003.lnk", "Uninstall NeuroBench 2003.lnk"),
    ("卸载 NeuroBench 2003", "Uninstall NeuroBench 2003"),
    ("正在登记卸载信息...", "Registering uninstall information..."),
    ("%s\\r\\n\\r\\n安装已经完成，您可以稍后在", "%s\\r\\n\\r\\nSetup is complete. You can enter the key later under"),
    ("「工具 - 激活产品」中重新输入序列号。", "Tools - Activate Product."),
    ("产品序列号", "Product key"),
    ("安装完成。", "Installation finished."),
    ("您尚未接受许可证协议。必须接受协议才能继续安装。\\r\\n\\r\\n",
     "You have not accepted the license agreement. Setup cannot continue.\\r\\n\\r\\n"),
    ("现在接受协议并继续吗？", "Accept the agreement and continue?"),
    ("用户", "User"),
    ("请输入目标文件夹。", "Please enter a destination folder."),
    ("目标文件夹必须是一个完整的路径，例如：\\r\\n",
     "The destination folder must be a full path, for example:\\r\\n"),
    ("请选择 NeuroBench 2003 的安装文件夹", "Select the NeuroBench 2003 installation folder"),
    ("安装正在进行中。确实要退出安装吗？", "Setup is in progress. Really quit?"),
    ("安装尚未完成，确实要退出吗？", "Setup is not complete. Really quit?"),
]


def main():
    dry = "--dry" in sys.argv
    src = open(TARGET, encoding="utf-8").read()
    orig = src
    misses = []
    for zh, en in TRANS:
        needle = '"%s"' % zh
        if needle not in src:
            misses.append(zh)
            continue
        repl = 'TS("%s",\n           "%s")' % (zh, en)
        src = src.replace(needle, repl)
    if not dry:
        open(TARGET, "w", encoding="utf-8").write(src)
    print("替换完成；未命中 %d 条" % len(misses))
    for m in misses:
        print("   MISS:", m.replace("\\r", "\\r").replace("\\n", "\\n"))
    print("文件长度 %d -> %d" % (len(orig), len(src)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
