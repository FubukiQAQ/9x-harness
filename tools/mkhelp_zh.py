#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成中文帮助页（src/help/*.htm，GBK 编码）。生成后即可用记事本直接编辑。"""
import io
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "src", "help")

def _brand(text):
    """把帮助文字里的默认品牌名换成 brand.ini 里的值。"""
    import json
    path = os.path.join(ROOT, "src", "common", "brand_summary.json")
    if not os.path.exists(path):
        return text
    with io.open(path, encoding="utf-8") as f:
        b = json.load(f)
    prod = b.get("product", "Axiom NeuroBench 2003")
    short = b.get("short", "NeuroBench 2003")
    exe = (b.get("exe", "NEUROBENCH") + ".EXE").upper()
    ini = b.get("ini_name", "NEBENCH.INI")
    reg = b.get("reg_root", "Software\\Axiom\\NeuroBench2003")
    base = exe[:-4]
    out = text
    out = out.replace("Axiom NeuroBench 2003", prod)
    out = out.replace("NeuroBench 2003", short)
    out = out.replace("Software\\Axiom\\NeuroBench2003", reg)
    out = out.replace("Application Data\\NeuroBench", "Application Data\\" + short)
    out = out.replace("NEBENCH.INI", ini)
    out = out.replace("NEUROBENCH.LIC", base + ".LIC")
    out = out.replace("NEUROBENCH.EXE", exe)
    return out



HEAD = """<HTML><HEAD><META http-equiv="Content-Type" content="text/html; charset=gb2312">
<TITLE>%(t)s</TITLE>
<STYLE type="text/css">
body { font-family: 宋体, SimSun, sans-serif; font-size: 9pt; background: #FFFFFF;
       margin: 12px 16px; color: #000000; }
h1 { font-size: 13pt; color: #000080; border-bottom: 1px solid #808080; padding-bottom: 4px; }
h2 { font-size: 11pt; color: #000080; margin-top: 18px; }
h3 { font-size: 10pt; color: #404040; margin-top: 14px; }
p  { margin: 6px 0; line-height: 150%%; }
li { margin: 3px 0; line-height: 140%%; }
code, pre { font-family: 新宋体, NSimSun, monospace; font-size: 9pt;
       background: #F0F0F0; border: 1px solid #C0C0C0; padding: 6px; display: block; }
table { border-collapse: collapse; font-size: 9pt; }
td, th { border: 1px solid #A0A0A0; padding: 3px 8px; }
th { background: #E0E0E0; }
.note { background: #FFFFE0; border: 1px solid #C0A000; padding: 8px; margin: 10px 0; }
.warn { background: #FFE8E8; border: 1px solid #C00000; padding: 8px; margin: 10px 0; }
.hd { color: #808080; font-size: 8pt; border-bottom: 1px dotted #C0C0C0;
      padding-bottom: 4px; margin-bottom: 12px; }
</STYLE></HEAD><BODY>
<DIV class="hd">Axiom NeuroBench 2003 &nbsp;帮助 &nbsp;版本 3.00.2003</DIV>
"""

FOOT = """
<HR size="1" color="#C0C0C0">
<P style="font-size:8pt;color:#808080">Axiom NeuroBench 2003 &nbsp;版本 3.00.2003 &nbsp;&nbsp;
Axiom Software &nbsp;&nbsp; 1993-2003 &nbsp;&nbsp;
本帮助文件随软件一同安装，可用「帮助 - 帮助目录」再次打开。</P>
</BODY></HTML>
"""

PAGES = []


def page(name, title, body):
    PAGES.append((name, title, body))


page("index.htm", "Axiom NeuroBench 2003 帮助", """
<H1>Axiom NeuroBench 2003</H1>
<P><B>老机器上的桌面智能助手。</B></P>
<P>Axiom NeuroBench 2003 是一个运行在老式个人电脑（Windows 95 / 98 / NT 4.0 /
2000 / XP）上的桌面助手。它把您的提问通过加密的安全连接发送到云端服务，再把回答
显示在这个三栏工作台里；它还可以读写您电脑上的文件、执行命令、查看系统信息——
也就是所谓“工具调用”。</P>

<H2>三个特点</H2>
<UL>
<LI><B>在老系统上也能安全上网。</B>Windows XP 自带的加密组件只支持到 TLS 1.0，
早就不能用了。本程序内置完整的 SSL/TLS 1.2 组件（BearSSL），不依赖系统里的任何
加密服务，因此在 Windows 95 上一样能和现代服务器建立安全连接。</LI>
<LI><B>明白自己住在旧机器里。</B>程序发给云端的环境说明是专门写过的：它知道这台
电脑是 Windows XP、内存只有 128~512 MB、编码是 GBK、没有 PowerShell 也没有
Python。于是回答更贴合实际，不会让您去装那些根本装不上的现代软件。</LI>
<LI><B>一个真正能干活的助手。</B>列目录、读文件、写文件、在命令提示符里执行命令、
打开文件、把文字放进剪贴板。所有有风险的操作都会先弹出确认框。</LI>
</UL>

<H2>开始之前</H2>
<OL>
<LI>在「工具 - 设置」里填写<B>服务器地址</B>、<B>型号名称</B>与<B>API 密钥</B>，
然后按「测试连接」确认网络与加密通信正常；</LI>
<LI>在「工具 - 激活产品」里输入随光盘提供的<B>产品序列号（CDK）</B>。未激活时
可以使用 30 天 / 30 次会话的试用版。</LI>
</OL>

<DIV class="note"><B>提示：</B>本软件本身不包含任何云端账号。如果您不清楚服务器
地址和密钥，请向提供本软件的单位或经销商索取。</DIV>

<H2>本帮助文件的内容</H2>
<UL>
<LI><A HREF="quickstart.htm">快速安装与使用</A></LI>
<LI><A HREF="settings.htm">配置服务器与密钥</A></LI>
<LI><A HREF="tools.htm">工具调用说明</A></LI>
<LI><A HREF="workspace.htm">工作区与文件</A></LI>
<LI><A HREF="activate.htm">激活与试用</A></LI>
<LI><A HREF="editor.htm">内置文本编辑器</A></LI>
<LI><A HREF="faq.htm">常见问题</A></LI>
<LI><A HREF="history.htm">版本历史</A></LI>
</UL>
""")

page("quickstart.htm", "快速安装与使用", """
<H1>快速安装与使用</H1>

<H2>第一步：启动程序</H2>
<P>从「开始 - 程序 - NeuroBench 2003」中单击 <B>NeuroBench 2003</B>，或者双击桌面上
的图标。启动时会显示一个启动画面，稍等一两秒即可。</P>

<H2>第二步：填写服务器信息</H2>
<P>选择菜单「工具 - 设置」，填写：</P>
<TABLE>
<TR><TH>项目</TH><TH>说明</TH></TR>
<TR><TD>服务器地址</TD><TD>完整的接口地址，例如
<CODE>https://api.deepseek.com/chat/completions</CODE></TD></TR>
<TR><TD>型号名称</TD><TD>服务商规定的型号，例如 <CODE>deepseek-chat</CODE></TD></TR>
<TR><TD>API 密钥</TD><TD>服务商给您的密钥，通常以 <CODE>sk-</CODE> 开头</TD></TR>
<TR><TD>超时(秒)</TD><TD>老机器建议填 <B>60</B></TD></TR>
<TR><TD>最长输出</TD><TD>一次回答最多多少令牌，建议 <B>1024</B>~<B>2048</B></TD></TR>
</TABLE>
<P>填好后按<B>「测试连接」</B>。提示“网络、加密与鉴权全部正常”就说明可以用了。</P>

<H2>第三步：开始对话</H2>
<P>在右栏下方的输入框里输入问题，然后按 <B>Ctrl+Enter</B> 或单击<B>「发送」</B>。
等待时状态栏会显示“正在连接服务器...”，按钮暂时变灰。要中途放弃，按 <B>Esc</B>
或单击「停止」。</P>

<H2>第四步：让它动手做事</H2>
<P>先确认窗口下方的<B>「允许智能助手调用本机工具」</B>已勾选，然后可以这样问：</P>
<CODE>帮我看看 D:\ 下面有哪些文件夹
把工作区里的 notes.txt 读出来，把错别字改一下再存回去
写一个自动备份 D:\工作 的批处理，存到 C:\BACKUP.BAT
这台电脑内存多大？还能装点什么？</CODE>
<P>每当助手要写文件或执行命令，都会弹出确认框，您可以选择“否”。</P>

<DIV class="warn"><B>注意：</B>云端回答并不总是正确。涉及删除文件、修改系统设置的
命令，请务必自己看过再同意执行。</DIV>
""")

page("settings.htm", "配置服务器与密钥", """
<H1>配置服务器与密钥</H1>

<H2>配置文件在哪里</H2>
<P>设置保存在程序目录下的 <CODE>NEBENCH.INI</CODE> 中。如果程序目录不可写，会自动
改存到 <CODE>Application Data\\NeuroBench\\NEBENCH.INI</CODE>。它是老式 INI 文件，
可以用记事本直接修改：</P>
<CODE>[connection]
endpoint=https://api.deepseek.com/chat/completions
model=deepseek-chat
apikey=（这里是经过简单混淆的密钥）
temperature=7
maxtokens=2048
timeout=60

[options]
usetools=1
confirmwrite=1
ignorecertdate=0

[display]
fontsize=12
language=zh</CODE>

<H2>各项设置的含义</H2>
<TABLE>
<TR><TH>项目</TH><TH>含义</TH></TR>
<TR><TD>endpoint</TD><TD>接口的完整网址，必须是 OpenAI 兼容的
<CODE>chat/completions</CODE> 接口，以 http:// 或 https:// 开头</TD></TR>
<TR><TD>model</TD><TD>型号名称，由服务商规定</TD></TR>
<TR><TD>apikey</TD><TD>密钥。保存时会做简单混淆（不是加密），请不要把
NEBENCH.INI 发给别人</TD></TR>
<TR><TD>temperature</TD><TD>温度 x10。7 表示 0.7，数值越大回答越活泼</TD></TR>
<TR><TD>maxtokens</TD><TD>单次回答的令牌上限，老机器不建议超过 2048</TD></TR>
<TR><TD>usetools</TD><TD>1 = 允许助手调用本机工具</TD></TR>
<TR><TD>confirmwrite</TD><TD>1 = 写文件、执行命令前弹出确认框（强烈建议保持 1）</TD></TR>
<TR><TD>ignorecertdate</TD><TD>1 = 忽略证书日期错误，仅当本机时钟不准时使用</TD></TR>
<TR><TD>language</TD><TD>zh 或 en，界面语言</TD></TR>
</TABLE>

<H2>关于安全连接</H2>
<P>本程序自带 SSL/TLS 1.2 组件，证书由 <CODE>CERTS</CODE> 文件夹中的根证书校验。
如果服务器换用了新的证书颁发机构，程序会提示“服务器证书不受信任”，此时请运行
工具光盘中的 <B>CERTUPD.EXE</B>，或把新的证书包放进 <CODE>CERTS</CODE> 文件夹。</P>

<DIV class="note"><B>本机时钟不准怎么办？</B>老电脑的 CMOS 电池没电是常事，系统时间
可能停在 2001 年，这会导致证书日期校验失败。请先更正系统时间；实在无法更正时，
可在设置中勾选「忽略证书日期错误」，程序会尝试用网络时间校正。</DIV>

<H2>代理服务器</H2>
<P>本程序不支持 HTTP 代理。如果您的网络必须经过代理才能上网，请与网络管理员联系，
为 443 端口开放直连。</P>
""")

page("tools.htm", "工具调用说明", """
<H1>工具调用说明</H1>
<P>“工具调用”是指：云端服务在回答时，可以请求本程序在您的电脑上做一件具体的事，
本程序执行后再把结果送回给它，最后它用中文向您汇报。整个过程您都能在右栏看到——
灰色的“调用工具”字样就是。</P>

<H2>助手可以使用的工具</H2>
<TABLE>
<TR><TH>工具</TH><TH>作用</TH><TH>是否需确认</TH></TR>
<TR><TD>get_system_info</TD><TD>读取系统版本、CPU、内存、磁盘、屏幕、代码页</TD><TD>否</TD></TR>
<TR><TD>list_dir</TD><TD>列出某个文件夹里的文件与子文件夹</TD><TD>否</TD></TR>
<TR><TD>read_file</TD><TD>读取文本文件内容（按 GBK 解码）</TD><TD>否</TD></TR>
<TR><TD>write_file</TD><TD>把文字写入文件（GBK 编码，会覆盖原文件）</TD><TD><B>是</B></TD></TR>
<TR><TD>run_command</TD><TD>用 cmd.exe /c 执行一条命令并取回输出</TD><TD><B>是</B></TD></TR>
<TR><TD>open_path</TD><TD>用系统默认程序打开文件、文件夹或网址</TD><TD><B>是</B></TD></TR>
<TR><TD>set_clipboard</TD><TD>把文字放进剪贴板</TD><TD>否</TD></TR>
</TABLE>

<H2>确认对话框</H2>
<P>当助手要写文件、执行命令或打开东西时，会弹出这样的框：</P>
<CODE>智能助手想要执行下面的操作，是否允许？

在命令提示符中执行：

cmd.exe /c dir D:\\工作

（如果不放心，请选择“否”。）</CODE>
<P>选择「否」后，助手会收到“用户拒绝”的消息，不会重试，而是向您解释并询问别的
做法。这个对话框永远不会自动通过。</P>

<DIV class="warn"><B>请特别注意：</B>不要因为嫌麻烦就一路按“是”。提问可能被诱导，
使云端服务生成 <CODE>del /s /q</CODE>、<CODE>format</CODE> 这类危险命令。看到不认识
的内容，请选择“否”。</DIV>

<H2>关掉工具调用</H2>
<P>取消勾选主窗口下方的「允许智能助手调用本机工具」即可。此时助手只会给文字建议。</P>

<H2>一次提问最多调用几次工具</H2>
<P>为了防止死循环，每一轮对话最多执行 <B>8 次</B>工具调用，超过后会自动停止并提示
您把需求拆小一些。</P>

<H2>编码问题</H2>
<P>本程序内部使用系统代码页（简体中文系统为 <B>GBK</B>，即代码页 936），而网络接口
使用 <B>UTF-8</B>，程序会自动双向转换。但如果文件本身是 UTF-8 编码的，读出来的中文
可能显示为乱码——那个年代的程序默认都按 GBK 处理文本。</P>
""")

page("workspace.htm", "工作区与文件", """
<H1>工作区与文件</H1>

<H2>三个窗格分别是什么</H2>
<TABLE>
<TR><TH>窗格</TH><TH>用途</TH></TR>
<TR><TD>左：工作区搜索 / 工作区列表</TD><TD>搜索并选择工作区。每个工作区就是磁盘上的
一个文件夹。</TD></TR>
<TR><TD>中：文件搜索 / 当前工作区内的文件</TD><TD>当前工作区文件夹里的文件。双击文本
文件用内置编辑器打开；双击会话文件（*.NBC）则载入该会话。</TD></TR>
<TR><TD>右：对话消息 / 输入</TD><TD>对话记录与输入框。</TD></TR>
</TABLE>

<H2>工作区是什么</H2>
<P>每个工作区是 <CODE>Application Data\\NeuroBench\\Workspaces\\&lt;名称&gt;</CODE>
下的一个文件夹。您在这个工作区里的对话会按文件夹收纳，文件名形如
<CODE>CONV-20031118-1432.NBC</CODE>，与您放在里面的文本文件并列存放。切换工作区会
同时切换对话历史和文件列表。</P>

<H2>常用操作</H2>
<UL>
<LI><B>工作区 - 新建工作区</B>：建立文件夹并切换过去。</LI>
<LI><B>工作区 - 在工作区中新建文本文件</B>：新建并立即用编辑器打开。</LI>
<LI><B>工作区 - 把文件加入工作区</B>：把别处的文件复制进来。</LI>
<LI><B>工作区 - 打开工作区文件夹</B>：在资源管理器中打开。</LI>
<LI><B>F5</B> 或 <B>工作区 - 刷新</B>：重新读取文件夹。</LI>
<LI>在文件树上按鼠标右键，可以“打开 / 删除 / 刷新”。</LI>
</UL>

<H2>会话文件</H2>
<P>会话文件是带几行 <CODE>;</CODE> 表头的纯文本，可以直接用记事本或内置编辑器查看。
<B>文件 - 保存会话</B> 会把当前对话写入该文件；在文件树上删除会话文件即从磁盘删除。</P>

<DIV class="note"><B>建议：</B>一个项目用一个工作区。这样助手为该项目生成的所有东西
都在同一个文件夹里，方便整体复制到软盘或打成压缩包。</DIV>
""")

page("activate.htm", "激活与试用", """
<H1>激活与试用</H1>

<H2>试用版</H2>
<P>没有输入产品序列号时，本软件以<B>试用版</B>方式运行，限制为 <B>30 天</B>或
<B>30 次会话</B>，以先到者为准。试用信息记录在注册表
<CODE>HKEY_CURRENT_USER\\Software\\Axiom\\NeuroBench2003</CODE> 下，并在
<CODE>Application Data\\NeuroBench\\NEBENCH.LIC</CODE> 留有一份副本。</P>
<P>请勿手工修改这些内容：程序会检测到被改动，并把授权状态标记为“已损坏”。</P>

<H2>产品序列号（CDK）</H2>
<P>序列号由 <B>5 组、每组 4 位字符</B>组成，中间用减号分隔，例如：</P>
<CODE>2K7M-4XQ9-BT3P-DE8N-VW6C</CODE>
<P>字符集里没有数字 0、1 和字母 I、O，避免看错。输入时大小写不限，减号可以省略。</P>

<H2>激活步骤</H2>
<OL>
<LI>选择菜单「工具 - 激活产品」；</LI>
<LI>输入序列号；</LI>
<LI>单击「激活」。成功后状态栏会变成“已授权（标准版/专业版/企业版）”。</LI>
</OL>

<H2>版本区别</H2>
<TABLE>
<TR><TH>版本</TH><TH>用户数</TH><TH>说明</TH></TR>
<TR><TD>标准版</TD><TD>1</TD><TD>单机使用</TD></TR>
<TR><TD>专业版</TD><TD>最多 32</TD><TD>小型办公室内多台机器使用</TD></TR>
<TR><TD>企业版</TD><TD>最多 32</TD><TD>随附优先技术支持</TD></TR>
</TABLE>

<H2>序列号无效怎么办</H2>
<UL>
<LI>检查是否漏输或多输了字符（一共 20 位字符，不含减号）；</LI>
<LI>确认您购买的是 <B>NeuroBench 2003</B> 的序列号；</LI>
<LI>如果仍然不行，请把序列号与购买凭证一起发给经销商。</LI>
</UL>

<DIV class="note"><B>关于“限时授权”：</B>部分序列号带有有效天数（例如 365 天），从
<B>首次运行本软件的日期</B>开始计算，到期后程序会提示“试用已到期”，需要更换新的
序列号。</DIV>
""")

page("editor.htm", "内置文本编辑器", """
<H1>内置文本编辑器</H1>
<P>NeuroBench 自带一个小巧的文本编辑器，用来查看和修改助手生成的文件，比如批处理
脚本、说明文字。它不需要另外安装任何东西。</P>

<H2>打开方法</H2>
<UL>
<LI>菜单「文件 - 文本编辑器」，或工具栏第三个按钮；</LI>
<LI>在中栏的文件树里双击文本文件；</LI>
<LI>「工作区 - 在工作区中新建文本文件」会新建并立即打开。</LI>
</UL>

<H2>功能</H2>
<TABLE>
<TR><TH>菜单</TH><TH>功能</TH><TH>快捷键</TH></TR>
<TR><TD>文件 - 新建</TD><TD>清空并开始新文档</TD><TD>Ctrl+N</TD></TR>
<TR><TD>文件 - 打开</TD><TD>打开一个文本文件</TD><TD>Ctrl+O</TD></TR>
<TR><TD>文件 - 保存</TD><TD>保存到原文件</TD><TD>Ctrl+S</TD></TR>
<TR><TD>编辑 - 撤消</TD><TD>撤消上一步</TD><TD>Ctrl+Z</TD></TR>
<TR><TD>编辑 - 查找</TD><TD>系统标准的查找对话框</TD><TD>Ctrl+F</TD></TR>
<TR><TD>编辑 - 插入日期时间</TD><TD>插入当前日期时间</TD><TD>F5</TD></TR>
<TR><TD>格式 - 自动换行</TD><TD>开关自动换行</TD><TD>—</TD></TR>
<TR><TD>格式 - 字体</TD><TD>选择字体与字号</TD><TD>—</TD></TR>
</TABLE>

<H2>编码说明</H2>
<P>编辑器以<B>ANSI（简体中文系统即 GBK）</B>读写文件，与那个年代的程序保持一致，
写出的 .bat、.ini、.txt 可以被 cmd.exe 和记事本正确识别。</P>
<P>如果您打开的文件是 UTF-8 编码，中文可能显示为乱码。这是正常现象——用记事本
“另存为”并选择 ANSI 编码即可转换。</P>

<H2>写批处理的小技巧</H2>
<P>批处理文件里出现中文时，请保存为 ANSI 编码，并在开头加上：</P>
<CODE>@echo off
chcp 936
</CODE>
""")

page("faq.htm", "常见问题", """
<H1>常见问题</H1>

<H3>1. 启动时提示“找不到根证书（CERTS 文件夹为空或缺失）”</H3>
<P>安装目录下的 <CODE>CERTS</CODE> 文件夹被删掉了，或里面的证书文件丢失。请重新
安装，或运行工具光盘中的 <B>CERTUPD.EXE</B> 导入证书包。</P>

<H3>2. 提示“无法解析服务器地址”</H3>
<P>依次检查：网线或调制解调器是否连好；能否用浏览器打开任意网页；服务器地址是否
抄错（注意不要多打空格）；域名服务器（DNS）设置是否正确。</P>

<H3>3. 提示“与服务器建立安全连接失败”</H3>
<UL>
<LI>先看系统时间对不对——时间错了证书校验必然失败；</LI>
<LI>确认服务器地址以 <CODE>https://</CODE> 开头；</LI>
<LI>如果提示“证书不受信任”，运行 CERTUPD.EXE 更新根证书；</LI>
<LI>如果提示“没有双方都支持的加密套件”，说明该服务器只接受 TLS 1.3，
本程序无法连接，请改用支持 TLS 1.2 的接口。</LI>
</UL>

<H3>4. 提示“服务器拒绝了请求（HTTP 401）”</H3>
<P>密钥不对、已失效或余额不足。请到服务商网站检查。</P>

<H3>5. 回答里出现奇怪的方块或问号</H3>
<P>这是字体问题。请确认系统里装有“宋体”，也可以在「工具 - 设置」里把字号调大。</P>

<H3>6. 助手说“我无法访问互联网”</H3>
<P>这是正常的：助手本身不上网，只能通过本程序与云端通信，并且只能通过工具查看您
电脑上的文件。要它看网页内容，请把内容复制粘贴进输入框。</P>

<H3>7. 程序在 Windows 98 上能运行吗？</H3>
<P>可以。本程序是 32 位程序，不依赖 .NET、不依赖现代运行库，Windows 95 OSR2 及以上
装有 IE4 的系统都能运行。不过内存很小时，请把「最长输出」调到 512 以下。</P>

<H3>8. 对话记录存在哪里？</H3>
<P>存在当前工作区的文件夹里，是 <CODE>.NBC</CODE> 文本文件。每轮回答结束后会自动
保存；「文件 - 保存会话」可以立即保存。</P>

<H3>9. 为什么每次发送都要等很久？</H3>
<P>老机器的 CPU 要自己做加密运算（没有硬件加速），加上带宽有限，一次问答 5~20 秒
是正常的。把「最长输出」调小可以快一些。</P>

<H3>10. 怎样完全卸载？</H3>
<P>使用「开始 - 程序 - NeuroBench 2003 - 卸载 NeuroBench 2003」，或「控制面板 -
添加/删除程序」。卸载程序会删除程序文件和快捷方式，但不会动您自己的文档和工作区。</P>
""")

page("history.htm", "版本历史", """
<H1>版本历史</H1>

<H2>3.00.2003（2003 年 11 月）</H2>
<UL>
<LI>NeuroBench 工作台首个发行版本：工作区列表、工作区文件树、对话窗格。</LI>
<LI>工具调用：读写文件、列目录、执行命令、系统信息、剪贴板。</LI>
<LI>内置 SSL/TLS 1.2 组件，Windows 95/98/2000/XP 也能访问现代服务器。</LI>
<LI>内置文本编辑器；在文件树中双击文件即可编辑。</LI>
<LI>产品序列号激活与 30 天试用。</LI>
<LI>中文 / 英文界面，可在运行时切换。</LI>
<LI>CHM 格式帮助文件。</LI>
</UL>

<H2>3.01.2003（服务包 1）</H2>
<UL>
<LI>更新根证书包，兼容更多服务器。</LI>
<LI>更新帮助文件内容。</LI>
<LI>新增系统信息报告工具 SYSINFO.EXE。</LI>
<LI>修正若干界面细节问题。</LI>
</UL>

<H2>1.60.0412（测试版）</H2>
<UL>
<LI>改为向导式安装程序。</LI>
<LI>修正 800x600 分辨率下对话框显示不全的问题。</LI>
</UL>
""")

page("about.htm", "关于本软件", """
<H1>关于 Axiom NeuroBench 2003</H1>
<TABLE>
<TR><TH>项目</TH><TH>内容</TH></TR>
<TR><TD>产品名称</TD><TD>Axiom NeuroBench 2003</TD></TR>
<TR><TD>版本</TD><TD>3.00.2003</TD></TR>
<TR><TD>发行商</TD><TD>Axiom Software</TD></TR>
<TR><TD>著作权</TD><TD>Copyright (C) 1993-2003 Axiom Software</TD></TR>
<TR><TD>安装程序</TD><TD>InstallShield 风格向导（SETUP.EXE）</TD></TR>
<TR><TD>加密组件</TD><TD>BearSSL（内置，支持 SSL/TLS 1.2）</TD></TR>
<TR><TD>帮助格式</TD><TD>Microsoft HTML Help (.CHM)</TD></TR>
</TABLE>

<H2>第三方组件声明</H2>
<P>本程序包含 Thomas Pornin 先生开发的 <B>BearSSL</B> 加密库，按 MIT 许可证使用。
该库提供完整的 TLS 1.2 客户端实现，不依赖操作系统自带的加密服务。</P>

<H2>技术支持</H2>
<P>遇到问题时，请先查阅本帮助文件的<A HREF="faq.htm">常见问题</A>。如果仍然无法
解决，请用「工具 - 系统信息」生成报告，连同问题描述一起发送给经销商。</P>

<DIV class="note">本软件按“现状”提供，不附带任何明示或默示的担保。使用云端接口
产生的费用由用户自行承担。</DIV>
""")


def main():
    for name, title, body in PAGES:
        html = _brand(HEAD % {"t": title} + body + FOOT)
        with open(os.path.join(OUT, name), "wb") as f:
            f.write(html.encode("gbk", "replace"))
    print("生成 %d 个中文帮助页" % len(PAGES))
    return 0


if __name__ == "__main__":
    sys.exit(main())
