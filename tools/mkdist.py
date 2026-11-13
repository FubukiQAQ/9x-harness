#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""NeuroBench 2003 - 发行盘制作脚本

把 build\\out 中编译好的程序组装成：
  build\\dist\\INSTALL\\   —— 安装光盘 (DISK 1) 目录
  build\\dist\\TOOLS\\     —— 工具 / 服务包光盘目录
  build\\dist\\*.iso       —— 两张光盘的 ISO 镜像

用法:  py mkdist.py [--skip-iso]
"""
import io
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "build", "out")
DIST = os.path.join(ROOT, "build", "dist")
INSTALL = os.path.join(DIST, "INSTALL")
TOOLS = os.path.join(DIST, "TOOLS")
TMP = os.path.join(ROOT, "build", "pack")
# ---- 品牌（由 tools/mkbrand.py 生成 src/common/brand_summary.json）----
_BSUM = os.path.join(ROOT, "src", "common", "brand_summary.json")
if os.path.exists(_BSUM):
    import json as _json
    with io.open(_BSUM, encoding="utf-8") as _f:
        _B = _json.load(_f)
else:
    _B = {}

VER = _B.get("version", "3.00.2003")
PROD = _B.get("product", "Axiom NeuroBench 2003")
SHORT = _B.get("short", "NeuroBench 2003")
COMPANY = _B.get("company", "Axiom Software")
EXE_NAME = (_B.get("exe", "NEUROBENCH") + ".EXE").upper()
ISO_LABEL = _B.get("iso_label", "NEUROBENCH2003")
ISO_LABEL_TOOLS = _B.get("iso_label_tools", "NB2003TOOLS")
CHM_NAME = (_B.get("exe", "NEUROBENCH") + ".CHM").upper()
SUBDIR = _B.get("install_subdir", COMPANY + "\\" + SHORT)
MENU_GROUP = _B.get("menu_group", SUBDIR)
SLUG = _B.get("slug") or "".join(ch for ch in EXE_NAME.replace(".EXE", "") if ch.isalnum())


def brand(text):
    """把生成文本里的默认品牌名换成当前品牌。"""
    if not text:
        return text
    out = text
    if PROD != "Axiom NeuroBench 2003":
        out = out.replace("Axiom NeuroBench 2003", PROD)
    if SHORT != "NeuroBench 2003":
        out = out.replace("NeuroBench 2003", SHORT)
    if COMPANY != "Axiom Software":
        out = out.replace("Axiom Software", COMPANY)
    return out
PY = sys.executable

CA_BUNDLE = os.path.join(ROOT, "dl", "cacert.pem")

# ---------------------------------------------------------------- 文本内容
README_INSTALL = """===========================================================================
                        Axiom NeuroBench 2003
                          版本 %(ver)s
                       Axiom Software  荣誉出品
===========================================================================

一、这是什么
----------------------------------------------------------------------------
Axiom NeuroBench 2003 是一个运行在 Windows 95/98/2000/XP 上的中文桌面
智能助手。它通过加密的安全连接把您的问题发送到云端的智能服务，再把回答显示
在老式的窗口里；它还能读写您电脑上的文件、执行命令、查看系统信息。

二、安装前请确认
----------------------------------------------------------------------------
  * 操作系统: Windows 95 OSR2 / 98 / NT 4.0 / 2000 / XP 或更高
  * 处 理 器: Pentium 166 以上
  * 内    存: 32 MB 以上（建议 256 MB）
  * 硬    盘: 16 MB 以上可用空间
  * 显    示: 800 x 600 或更高，256 色以上
  * 其    他: 需要 Internet Explorer 4.0 以上（用于显示帮助文件）

三、安装步骤
----------------------------------------------------------------------------
  1. 把本光盘放入光驱。如果系统没有自动启动安装程序，请手工运行
     SETUP.EXE。
  2. 按向导提示，依次完成: 许可证协议 -> 用户信息 -> 目标文件夹 ->
     程序文件夹 -> 正在安装 -> 完成。
  3. 安装完成后，从「开始 - 程序 - NeuroBench 2003」启动程序。

  如果您的单位采用无人值守安装，可以使用静默参数：
     SETUP.EXE /s /d="C:\\Program Files\\Feiyang\\NeuroBench 2003"

四、第一次使用
----------------------------------------------------------------------------
  1. 选择菜单「工具 - 设置」，填写服务器地址、型号名称和 API 密钥，
     然后按「测试连接」。若提示一切正常，按「确定」。
  2. 选择菜单「工具 - 激活产品」，输入随光盘提供的产品序列号。未激活时
     可以使用 30 天 / 30 次会话的试用版。
  3. 在输入框中输入问题，按 Ctrl+Enter 发送。

五、光盘内容
----------------------------------------------------------------------------
  SETUP.EXE      安装程序
  SETUP.INS      安装配置文件（供安装程序读取，请勿修改）
  SETUP.PKG      安装文件清单
  DATA1.CAB      安装数据
  LICENSE.TXT    最终用户许可协议
  README.TXT     本文件
  AUTORUN.INF    光盘自动运行配置

六、需要帮助
----------------------------------------------------------------------------
  程序安装完成后，按 F1 或选择「帮助 - 帮助目录」可以打开完整的帮助文件。
  如果仍然无法解决问题，请运行「工具 - 系统信息」生成报告，连同问题描述
  一起发送给经销商。

----------------------------------------------------------------------------
%(copy)s
===========================================================================
"""  % {"ver": VER, "copy": "Copyright (C) 1993-2003 Axiom Software  版权所有"}

LICENSE_TXT = """
              Axiom NeuroBench 2003
              最终用户许可协议 (EULA)

重要提示: 请在使用本软件之前仔细阅读本协议。安装、复制或以其他方式使用
本软件，即表示您同意接受本协议各项条款的约束。如果您不同意本协议的任何
条款，请不要安装或使用本软件。

一、许可的授予
  Axiom Software（以下简称"本工作室"）授予您非独占的、不可转让的使用权，
  允许您：
  1. 在一台计算机上安装、使用本软件的一份副本；
  2. 为备份目的制作本软件的一份副本。

二、限制
  1. 您不得对本软件进行反向工程、反编译或反汇编，但法律明文允许的除外。
  2. 您不得修改、出租、出借、出售、分发本软件或其任何部分。
  3. 您不得删除本软件中的任何版权声明、商标或其他所有权标记。
  4. 一个产品序列号仅授权协议约定数量的计算机使用。

三、云端服务说明
  1. 本软件本身不提供智能服务的算力，需要连接您自行配置的云端接口。
  2. 云端接口的账号、密钥与所产生的费用，由您自行负责。
  3. 本工作室不保证任何第三方接口的可用性、准确性或持续存在。

四、免责条款
  1. 本软件按"现状"提供，不附带任何明示或默示的担保，包括但不限于对
     适销性、特定用途适用性以及不侵权的默示担保。
  2. 本软件提供的智能回答与工具调用结果仅供参考，可能存在错误。您应当
     自行判断并承担使用后果，尤其是删除文件、执行命令等操作。
  3. 因使用或无法使用本软件所造成的任何直接、间接、偶然或必然的损失，
     本工作室及其作者均不承担责任。

五、终止
  如果您违反本协议的任何条款，本许可立即终止，您必须销毁本软件的全部副本。

六、适用法律
  本协议适用中华人民共和国法律。

七、其他
  本协议未尽事宜，按《计算机软件保护条例》及相关法律法规执行。

                                    Axiom Software
                                    1996 - 2003
"""

LICENSE_EN_TXT = """              Axiom NeuroBench 2003
                 END USER LICENSE AGREEMENT

IMPORTANT: READ THIS AGREEMENT CAREFULLY BEFORE USING
THIS PRODUCT. INSTALLING, COPYING OR OTHERWISE USING THE
PRODUCT MEANS THAT YOU ACCEPT ALL OF THE TERMS BELOW.
IF YOU DO NOT AGREE, DO NOT INSTALL OR USE IT.

1. GRANT OF LICENSE
  Axiom Software ("the Author") grants you a non-exclusive,
  non-transferable right to use this product:
  1. installed and used on a single computer;
  2. copied once for backup purposes only.

2. RESTRICTIONS
  1. You may not reverse engineer, decompile or disassemble the
     product, except where such a restriction is prohibited by law.
  2. You may not modify, rent, lease, lend, sell or redistribute any part
     of the product.
  3. You may not remove any copyright, trademark or other proprietary
     notice from the product.
  4. One product key is licensed for use under one license agreement.

3. ABOUT THE CLOUD SERVICES
  1. The product provides no AI service of its own. It only
     connects to a compatible cloud endpoint that you configure.
  2. Accounts, keys and any fees charged by such endpoints are your own
     responsibility.
  3. The Author does not guarantee the availability, accuracy
     or fitness of any third-party endpoint.

4. LIMITED WARRANTY
  1. The product is provided "AS IS" without warranty of any
     kind, either express or implied, including but not limited
     to the implied warranties of merchantability, fitness for a
     particular purpose and non-infringement.
  2. The assistant's answers and its use of tools may be wrong.
     You are responsible for reviewing anything it suggests,
     especially before deleting files or running commands.
  3. In no event shall the Author be liable for any direct,
     indirect, incidental or consequential damages arising out of
     the use of, or inability to use, this product.

5. TERMINATION
  Any breach of these terms terminates your license immediately.
  On termination you must destroy all copies of the product.

6. GOVERNING LAW
  This agreement is governed by the laws of the People's Republic of China.

7. GENERAL
  Matters not covered here shall be handled in accordance with
  applicable law.

                                    Axiom Software
                                    1996 - 2003
"""

README_TOOLS = """===========================================================================
                  NeuroBench 2003  工具与服务包光盘
                       版本 %(ver)s
                    Axiom Software  荣誉出品
===========================================================================

本光盘不能单独安装 NeuroBench 2003。请先使用《安装光盘》完成主程序安装，
再使用本光盘中的附加软件。

一、光盘内容
----------------------------------------------------------------------------
  NB-SP1.EXE   服务包 1。推荐所有用户安装。它会:
                    * 更新根证书包，兼容更多云端服务器
                    * 更新帮助文件
                    * 新增系统信息报告工具 SYSINFO.EXE
                    * 把版本更新为 3.01.2003
  SP1.CAB         服务包 1 的更新数据（请与服务包放在同一目录）
  CERTUPD.EXE     根证书更新程序。当主程序提示"服务器证书不受信任"时运行它。
  SYSINFO.EXE     系统信息报告工具。向经销商求助时，请用它生成报告。
  CDKEY.EXE       产品序列号(CDK)生成器 —— 仅供发行商与授权经销商使用，
                  请勿交给最终用户。
  TOOLS.INI       本光盘说明（配置文件格式）
  AUTORUN.INF     光盘自动运行配置

二、建议的安装顺序
----------------------------------------------------------------------------
  1. 用《安装光盘》安装 NeuroBench 2003 主程序；
  2. 运行本光盘中的 NB-SP1.EXE 安装服务包；
  3. 如果连接服务器时提示证书问题，运行 CERTUPD.EXE 更新根证书；
  4. 需要向经销商求助时，运行 SYSINFO.EXE 生成系统信息报告。

三、注意事项
----------------------------------------------------------------------------
  * 服务包需要写入主程序的安装目录，请确认您有该目录的写权限。
  * CDKEY.EXE 包含发行商的序列号生成算法，请妥善保管本光盘，
    不要外传、不要上传到互联网。
  * 本光盘中的程序同样是 32 位程序，适用于 Windows 95 及以上系统。

----------------------------------------------------------------------------
%(copy)s
===========================================================================
""" % {"ver": VER, "copy": "Copyright (C) 1993-2003 Axiom Software  版权所有"}

README_SP1 = """Axiom NeuroBench 2003  服务包 1
===========================================================================

版本: 3.01.2003
发行: 2003 年 12 月

本次服务包解决的问题
----------------------------------------------------------------------------
1. 部分云端服务器更换了证书颁发机构，导致主程序提示
   "服务器证书不受信任"。本服务包更新了根证书包，包含截至 2003 年 12 月
   的公共根证书。

2. 帮助文件内容有误: "工具调用"一节对 write_file 的描述不准确，已更正。
   另外补充了"编码问题"与"写批处理的小技巧"两节。

3. 新增 SYSINFO.EXE（系统信息报告工具），方便用户向经销商提供故障信息。

安装方法
----------------------------------------------------------------------------
运行 NB-SP1.EXE，确认安装目录后按"安装"。安装完成后重新启动主程序。

卸载方法
----------------------------------------------------------------------------
服务包不单独提供卸载。如需还原，请使用《安装光盘》重新安装主程序。
"""

AUTORUN_INSTALL = """[autorun]
open=SETUP.EXE
icon=SETUP.EXE,0
label=NeuroBench 2003 安装光盘

[Version]
Signature="$Windows NT$"
"""

AUTORUN_TOOLS = """[autorun]
open=NB-SP1.EXE
icon=NB-SP1.EXE,0
label=NeuroBench 2003 工具光盘

[Version]
Signature="$Windows NT$"
"""

SETUP_INS = """; ------------------------------------------------------------------
;  NeuroBench 2003 安装配置文件 (SETUP.INS)
;  安装程序启动时读取本文件。请勿手工修改，否则可能导致安装失败。
; ------------------------------------------------------------------
[Setup]
AppName=Axiom NeuroBench 2003
Version=%(ver)s
DefaultDir=
ProgramFolder=NeuroBench 2003
DataFile=DATA1.CAB
Language=0804
ShowSplash=YES
CreateUninstall=YES
NeedDisk=1

[Components]
Main=1
Help=1
Certs=1
Uninstaller=1
""" % {"ver": VER}

DISK_ID = """NEUROBENCH 2000  DISK 1 OF 2
Product : Axiom NeuroBench 2003
Version : %(ver)s
Vendor  : Axiom Software (Axiom Software)
Date    : 2003-11-18
Media   : CD-ROM (ISO 9660 + Joliet)
Note    : 本盘为安装盘。工具与服务包请使用 DISK 2。
""" % {"ver": VER}

TOOLS_INI = """; ------------------------------------------------------------------
;  NeuroBench 2003 工具光盘说明 (TOOLS.INI)
; ------------------------------------------------------------------
[Disc]
Title=NeuroBench 2003 工具与服务包光盘
Version=%(ver)s
Vendor=Axiom Software
Date=2003-12-05
Disc=2 of 2

[Programs]
SP1=NB-SP1.EXE|服务包 1 (推荐安装)
CERTUPD=CERTUPD.EXE|根证书更新程序
SYSINFO=SYSINFO.EXE|系统信息报告工具
CDKEY=CDKEY.EXE|产品序列号生成器 (OEM 内部工具)

[Notes]
1=本光盘不能单独安装主程序，请先使用安装光盘。
2=服务包需要写入主程序目录，请确认有写权限。
3=CDKEY.EXE 请勿交给最终用户。
""" % {"ver": VER}


def w(path, text, encoding="gbk"):
    text = brand(text)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(text.replace("\n", "\r\n").encode(encoding, "replace"))


def run(args):
    print("  $", " ".join(args[1:]))
    r = subprocess.run(args, capture_output=True)
    if r.returncode != 0:
        sys.stderr.write(r.stdout.decode("gbk", "replace"))
        sys.stderr.write(r.stderr.decode("gbk", "replace"))
        raise SystemExit("命令失败: %s" % args[1])



# ---------------------------------------------------------------------------
# 单文件安装包：SETUP.EXE + DATA1.CAB + 20 字节尾巴
# 尾巴结构（小端）："NBPAK1\0\0" + u32 载荷偏移 + u32 载荷长度 + u32 CRC32
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# 光盘内容的 ZIP 版：宿主机上挂不动盘、或者虚拟机里拷文件时用这个
# ---------------------------------------------------------------------------
def make_zips():
    import zipfile
    out = []
    for src, name in ((INSTALL, "%s-INSTALL-CD.zip" % SLUG),
                      (TOOLS,   "%s-TOOLS-CD.zip" % SLUG)):
        dst = os.path.join(DIST, name)
        if os.path.exists(dst):
            os.remove(dst)
        with zipfile.ZipFile(dst, "w", zipfile.ZIP_DEFLATED) as z:
            for n in sorted(os.listdir(src)):
                p = os.path.join(src, n)
                if os.path.isfile(p):
                    z.write(p, n)
        out.append((name, os.path.getsize(dst)))
    return out


def make_single_exe(cab_path, src_exe="SETUP.EXE", out_name=None):
    if out_name is None:
        out_name = "%s-SETUP.EXE" % SLUG
    import struct, zlib
    setup = os.path.join(OUT, src_exe)
    out = os.path.join(DIST, out_name)
    exe = open(setup, "rb").read()
    cab = open(cab_path, "rb").read()
    off = len(exe)
    tail = b"NBPAK1\0\0" + struct.pack("<III", off, len(cab), zlib.crc32(cab) & 0xFFFFFFFF)
    with open(out, "wb") as f:
        f.write(exe)
        f.write(cab)
        f.write(tail)
    print("      %-34s %10d" % (out_name, os.path.getsize(out)))
    return out



def make_cu_zip():
    import zipfile
    dst = os.path.join(DIST, "NB-CUSE.zip")
    if os.path.exists(dst): os.remove(dst)
    readme = (
        "Axiom NeuroBench 2003 - Computer Use\r\n"
        "=====================================\r\n\r\n"
        "\u7528\u6cd5\uff1a\u628a NBCU.EXE \u653e\u5230\u4efb\u610f\u6587\u4ef6\u5939\uff08\u5efa\u8bae\u4e0e NEUROBENCH.EXE \u540c\u76ee\u5f55\uff09\uff0c\u65e0\u9700\u5b89\u88c5\u3002\r\n\r\n"
        "\u5728\u667a\u80fd\u52a9\u624b\u91cc\u76f4\u63a5\u8bf4\uff1a\u201c\u622a\u5c4f\u770b\u770b\u5c4f\u5e55\u201d\u3001\u201c\u70b9\u51fb\u5f00\u59cb\u83dc\u5355\u201d\uff0c\u5b83\u4f1a\u8c03\u7528 NBCU.EXE\u3002\r\n\r\n"
        "\u5e38\u7528\u547d\u4ee4\uff1a\r\n"
        "  NBCU.EXE size\r\n"
        "  NBCU.EXE shot C:\\TEMP\\SCREEN.BMP\r\n"
        "  NBCU.EXE click 400 300\r\n"
        "  NBCU.EXE drag 100 100 400 300\r\n"
        "  NBCU.EXE type \"hello\"\r\n"
        "  NBCU.EXE key enter\r\n\r\n"
        "\u6025\u505c\uff1a\u4efb\u4f55\u52a8\u4f5c\u524d\u6309 Ctrl+Alt+End \u53ef\u53d6\u6d88\u3002\r\n"
    )
    with zipfile.ZipFile(dst, "w", zipfile.ZIP_DEFLATED) as z:
        z.write(os.path.join(OUT, "NBCU.EXE"), "NBCU.EXE")
        z.writestr("README.TXT", readme.encode("gbk"))
    print("      %-34s %10d" % ("NB-CUSE.zip", os.path.getsize(dst)))


def main():
    skip_iso = "--skip-iso" in sys.argv

    # 清掉上次生成的发行文件，避免不同品牌混在一起
    if os.path.isdir(DIST):
        for n in os.listdir(DIST):
            low = n.lower()
            if low.endswith(".iso") or low.endswith("-setup.exe") or \
               (low.endswith("-cd.zip")) or low.endswith(".zip") and not low.startswith("nb-cuse"):
                try:
                    os.remove(os.path.join(DIST, n))
                except OSError:
                    pass

    for d in (INSTALL, TOOLS, TMP):
        if os.path.isdir(d):
            shutil.rmtree(d)
        os.makedirs(d, exist_ok=True)

    print("[1/5] 收集编译产物")
    need = [EXE_NAME, "SETUP.EXE", "UNINST.EXE", CHM_NAME,
            "CERTUPD.EXE", "SYSINFO.EXE", "CDKEY.EXE", "NB-SP1.EXE"]
    for n in need:
        p = os.path.join(OUT, n)
        if not os.path.exists(p):
            raise SystemExit("缺少编译产物: " + p)
        print("    %-16s %9d 字节" % (n, os.path.getsize(p)))

    print("[2/5] 组装安装盘目录")
    payload = os.path.join(TMP, "payload")
    os.makedirs(os.path.join(payload, "CERTS"), exist_ok=True)
    shutil.copy(os.path.join(OUT, EXE_NAME), payload)
    shutil.copy(os.path.join(OUT, "UNINST.EXE"), payload)
    shutil.copy(os.path.join(OUT, CHM_NAME), payload)
    if os.path.exists(CA_BUNDLE):
        shutil.copy(CA_BUNDLE, os.path.join(payload, "CERTS", "ROOTS.PEM"))
    else:
        raise SystemExit("缺少根证书包 dl\\cacert.pem")
    # 帮助网页：CHM 在老系统上打不开时，程序会退到 HELP\index.htm
    help_src = os.path.join(ROOT, "src", "help")
    help_dst = os.path.join(payload, "HELP")
    os.makedirs(help_dst, exist_ok=True)
    _n_help = 0
    for _n in sorted(os.listdir(help_src)):
        if _n.lower().endswith((".htm", ".html", ".css", ".gif", ".png", ".jpg")):
            shutil.copy(os.path.join(help_src, _n), help_dst)
            _n_help += 1
    print("    HELP\\ 帮助网页 %d 个" % _n_help)

    w(os.path.join(payload, "README.TXT"), README_INSTALL)
    w(os.path.join(payload, "LICENSE.TXT"), LICENSE_TXT)
    w(os.path.join(payload, "LICENSE.EN.TXT"), LICENSE_EN_TXT)

    w(os.path.join(INSTALL, "README.TXT"), README_INSTALL)
    w(os.path.join(INSTALL, "LICENSE.TXT"), LICENSE_TXT)
    w(os.path.join(INSTALL, "LICENSE.EN.TXT"), LICENSE_EN_TXT)
    w(os.path.join(INSTALL, "SETUP.INS"), SETUP_INS)
    w(os.path.join(INSTALL, "AUTORUN.INF"), AUTORUN_INSTALL)
    w(os.path.join(INSTALL, "DISK.ID"), DISK_ID)
    shutil.copy(os.path.join(OUT, "SETUP.EXE"), INSTALL)

    print("[3/5] 打包 DATA1.CAB / SETUP.PKG")
    run([PY, os.path.join(ROOT, "tools", "mkarc.py"),
         "--src", payload,
         "--out", os.path.join(INSTALL, "DATA1.CAB"),
         "--pkg", os.path.join(INSTALL, "SETUP.PKG"),
         "--label", "NEUROBENCH", "--version", VER])

    print("[4/5] 组装工具光盘")
    sp1 = os.path.join(TMP, "sp1")
    os.makedirs(os.path.join(sp1, "CERTS"), exist_ok=True)
    if os.path.exists(CA_BUNDLE):
        shutil.copy(CA_BUNDLE, os.path.join(sp1, "CERTS", "ROOTS.PEM"))
    shutil.copy(os.path.join(OUT, CHM_NAME), sp1)
    shutil.copy(os.path.join(OUT, "SYSINFO.EXE"), sp1)
    w(os.path.join(sp1, "README-SP1.TXT"), README_SP1)
    run([PY, os.path.join(ROOT, "tools", "mkarc.py"),
         "--src", sp1, "--out", os.path.join(TOOLS, "SP1.CAB"),
         "--label", "NEUROBENCH-SP1", "--version", "3.01.2003"])

    for n in ("NB-SP1.EXE", "CERTUPD.EXE", "SYSINFO.EXE", "CDKEY.EXE", "NBCU.EXE"):
        shutil.copy(os.path.join(OUT, n), TOOLS)
    w(os.path.join(TOOLS, "README.TXT"), README_TOOLS)
    w(os.path.join(TOOLS, "TOOLS.INI"), TOOLS_INI)
    w(os.path.join(TOOLS, "AUTORUN.INF"), AUTORUN_TOOLS)

    print("[5/6] 生成单文件安装包与 ZIP")
    make_single_exe(os.path.join(INSTALL, "DATA1.CAB"))

    for n, sz in make_zips():
        print("      %-34s %10d" % (n, sz))

    make_cu_zip()

    print("[6/6] 生成光盘镜像")
    if skip_iso:
        print("    (已跳过)")
    else:
        run([PY, os.path.join(ROOT, "tools", "mkiso.py"),
             "--src", INSTALL,
             "--out", os.path.join(DIST, "%s-DISK1-INSTALL.iso" % SLUG),
             "--label", ISO_LABEL, "--date", "2003-11-18", "--quiet"])
        run([PY, os.path.join(ROOT, "tools", "mkiso.py"),
             "--src", TOOLS,
             "--out", os.path.join(DIST, "%s-DISK2-TOOLS.iso" % SLUG),
             "--label", ISO_LABEL_TOOLS, "--date", "2003-12-05", "--quiet"])

    print("\n完成。发行目录:")
    for d in (INSTALL, TOOLS):
        print("  " + d)
        for n in sorted(os.listdir(d)):
            p = os.path.join(d, n)
            print("      %-34s %10d" % (n, os.path.getsize(p)))
    for n in sorted(os.listdir(DIST)):
        if n.lower().endswith(".iso"):
            print("  %-38s %10d" % (n, os.path.getsize(os.path.join(DIST, n))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
