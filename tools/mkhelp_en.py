#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成英文帮助页（src/help/en_*.htm）。只需运行一次；生成后即可用记事本直接编辑。"""
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



HEAD = """<HTML><HEAD><META http-equiv="Content-Type" content="text/html; charset=windows-1252">
<TITLE>%(t)s</TITLE>
<STYLE type="text/css">
body { font-family: Verdana, Arial, sans-serif; font-size: 9pt; background: #FFFFFF;
       margin: 12px 16px; color: #000000; }
h1 { font-size: 13pt; color: #000080; border-bottom: 1px solid #808080; padding-bottom: 4px; }
h2 { font-size: 11pt; color: #000080; margin-top: 18px; }
h3 { font-size: 10pt; color: #404040; margin-top: 14px; }
p  { margin: 6px 0; line-height: 150%%; }
li { margin: 3px 0; line-height: 140%%; }
code, pre { font-family: "Courier New", monospace; font-size: 9pt;
       background: #F0F0F0; border: 1px solid #C0C0C0; padding: 6px; display: block; }
table { border-collapse: collapse; font-size: 9pt; }
td, th { border: 1px solid #A0A0A0; padding: 3px 8px; }
th { background: #E0E0E0; }
.note { background: #FFFFE0; border: 1px solid #C0A000; padding: 8px; margin: 10px 0; }
.warn { background: #FFE8E8; border: 1px solid #C00000; padding: 8px; margin: 10px 0; }
.hd { color: #808080; font-size: 8pt; border-bottom: 1px dotted #C0C0C0;
      padding-bottom: 4px; margin-bottom: 12px; }
</STYLE></HEAD><BODY>
<DIV class="hd">Axiom NeuroBench 2003 &nbsp;Help &nbsp;Version 3.00.2003</DIV>
"""

FOOT = """
<HR size="1" color="#C0C0C0">
<P style="font-size:8pt;color:#808080">Axiom NeuroBench 2003 &nbsp;Version 3.00.2003 &nbsp;&nbsp;
Axiom Software &nbsp;&nbsp; 1993-2003 &nbsp;&nbsp;
This help file is installed with the program; reopen it with Help - Help Topics.</P>
</BODY></HTML>
"""

PAGES = []


def page(name, title, body):
    PAGES.append((name, title, body))


page("en_index.htm", "Axiom NeuroBench 2003 Help", """
<H1>Axiom NeuroBench 2003</H1>
<P><B>A desktop assistant for old machines.</B></P>
<P>Axiom NeuroBench 2003 is a desktop assistant for legacy PCs (Windows 95 / 98 /
NT 4.0 / 2000 / XP). It sends your question over an encrypted connection to a cloud
service and shows the answer in this plain three-pane workbench. It can also read and
write files on this computer, run commands and report system information - that is
what "tool calling" means here.</P>

<H2>Three things that make it unusual</H2>
<UL>
<LI><B>Secure connections on old systems.</B> Windows XP only speaks TLS 1.0, which
modern servers refuse. NeuroBench carries its own SSL/TLS 1.2 stack (BearSSL), so it
does not depend on any operating-system crypto service and works even on Windows 95.</LI>
<LI><B>It knows it lives on an old machine.</B> The instructions sent to the cloud
describe this host: Windows XP, 128-512 MB RAM, code page 936, no PowerShell, no
Python, IE6 only. Answers therefore fit the machine instead of suggesting software
you cannot install.</LI>
<LI><B>It can actually get work done.</B> List folders, read and write files, run
commands, open documents and put text on the clipboard. Anything risky asks for
confirmation first.</LI>
</UL>

<H2>Before you start</H2>
<OL>
<LI>Open <B>Tools - Options</B> and fill in the server URL, model name and API key,
then press <B>Test connection</B>.</LI>
<LI>Open <B>Tools - Activate Product</B> and enter the product key from your disc
sleeve. Without a key the program runs as a 30-day / 30-conversation trial.</LI>
</OL>

<DIV class="note"><B>Note:</B> this program ships no cloud account of its own. Ask
your supplier for the server URL and key.</DIV>

<H2>In this help file</H2>
<UL>
<LI><A HREF="en_quickstart.htm">Quick start</A> - talking to it in five minutes</LI>
<LI><A HREF="en_settings.htm">Server and key settings</A></LI>
<LI><A HREF="en_tools.htm">Tool calling</A> - what it may do to this computer</LI>
<LI><A HREF="en_workspace.htm">Workspaces and files</A></LI>
<LI><A HREF="en_activate.htm">Activation and trial</A></LI>
<LI><A HREF="en_editor.htm">The built-in text editor</A></LI>
<LI><A HREF="en_faq.htm">Troubleshooting</A></LI>
<LI><A HREF="en_history.htm">Version history</A></LI>
</UL>
""")

page("en_quickstart.htm", "Quick start", """
<H1>Quick start</H1>

<H2>1. Start the program</H2>
<P>Choose <B>Start - Programs - NeuroBench 2003 - NeuroBench 2003</B>, or double-click
the desktop icon. A splash screen appears for a second or two.</P>

<H2>2. Fill in the server details</H2>
<P>Open <B>Tools - Options</B>:</P>
<TABLE>
<TR><TH>Field</TH><TH>Meaning</TH></TR>
<TR><TD>Server URL</TD><TD>Full endpoint, e.g.
<CODE>https://api.deepseek.com/chat/completions</CODE></TD></TR>
<TR><TD>Model name</TD><TD>Whatever your provider calls it, e.g. <CODE>deepseek-chat</CODE></TD></TR>
<TR><TD>API key</TD><TD>The key from your provider, usually starting with <CODE>sk-</CODE></TD></TR>
<TR><TD>Timeout (s)</TD><TD>Use <B>60</B> on an old machine</TD></TR>
<TR><TD>Max output</TD><TD>Tokens per answer; <B>1024</B>-<B>2048</B> is sensible</TD></TR>
</TABLE>
<P>Press <B>Test connection</B>. "Network, encryption and authentication all OK"
means you are ready.</P>

<H2>3. Talk to it</H2>
<P>Type in the box under the conversation pane, then press <B>Ctrl+Enter</B> or click
<B>Send</B>. While it works the status bar shows "Connecting to server..." and the
buttons grey out. Press <B>Esc</B> or <B>Stop</B> to abandon the request.</P>

<H2>4. Let it do things</H2>
<P>Tick <B>Allow the assistant to use local tools</B> at the bottom of the window, then
ask for example:</P>
<CODE>What folders are on drive D?
Read notes.txt from my workspace and fix the typos
Write a batch file that backs up D:\\work to C:\\BACKUP.BAT
How much memory does this machine have?</CODE>
<P>Whenever it wants to write a file or run a command, a confirmation box appears and
you can say No.</P>

<DIV class="warn"><B>Careful:</B> cloud answers are not always right. Read every
command before you approve it, especially anything that deletes files.</DIV>
""")

page("en_settings.htm", "Server and key settings", """
<H1>Server and key settings</H1>

<H2>Where settings live</H2>
<P>Settings are stored in <CODE>NEBENCH.INI</CODE> in the program folder. If that
folder is read-only the file goes to
<CODE>Application Data\\NeuroBench\\NEBENCH.INI</CODE> instead. It is a plain INI file
and can be edited in Notepad:</P>
<CODE>[connection]
endpoint=https://api.deepseek.com/chat/completions
model=deepseek-chat
apikey=(stored lightly obfuscated)
temperature=7
maxtokens=2048
timeout=60

[options]
usetools=1
confirmwrite=1
ignorecertdate=0

[display]
fontsize=12
language=en</CODE>

<H2>Meaning of each value</H2>
<TABLE>
<TR><TH>Key</TH><TH>Meaning</TH></TR>
<TR><TD>endpoint</TD><TD>Full URL of an OpenAI-compatible
<CODE>chat/completions</CODE> endpoint; must start with http:// or https://</TD></TR>
<TR><TD>model</TD><TD>Model name as required by your provider</TD></TR>
<TR><TD>apikey</TD><TD>Your key. It is obfuscated, not encrypted - do not share
NEBENCH.INI</TD></TR>
<TR><TD>temperature</TD><TD>Temperature x10 (7 = 0.7)</TD></TR>
<TR><TD>maxtokens</TD><TD>Answer length limit; keep it modest on old machines</TD></TR>
<TR><TD>usetools</TD><TD>1 = the assistant may use local tools</TD></TR>
<TR><TD>confirmwrite</TD><TD>1 = always ask before writing files or running commands</TD></TR>
<TR><TD>ignorecertdate</TD><TD>1 = ignore certificate date errors (only if the clock
is wrong)</TD></TR>
<TR><TD>language</TD><TD>zh or en - the user interface language</TD></TR>
</TABLE>

<H2>About the secure connection</H2>
<P>NeuroBench verifies server certificates against the root certificates in the
<CODE>CERTS</CODE> folder. If a server switches to a new certificate authority the
program reports "server certificate not trusted"; run <B>CERTUPD.EXE</B> from the tools
disc, or drop a new PEM bundle into <CODE>CERTS</CODE>.</P>

<DIV class="note"><B>Wrong clock?</B> A dead CMOS battery is common on these machines
and makes certificate dates fail. Fix the system clock first; if you cannot, tick
"Ignore certificate date errors" and the program will try to correct the time from the
network.</DIV>

<H2>Proxy servers</H2>
<P>HTTP proxies are not supported. If your network needs one, ask your administrator to
allow direct outbound access to port 443.</P>
""")

page("en_tools.htm", "Tool calling", """
<H1>Tool calling</H1>
<P>Tool calling means the cloud service may ask this program to do something on your
computer, the program does it, and the result is sent back so the service can summarise
it in plain language. You see every step in the conversation pane.</P>

<H2>Available tools</H2>
<TABLE>
<TR><TH>Tool</TH><TH>What it does</TH><TH>Confirmation</TH></TR>
<TR><TD>get_system_info</TD><TD>OS, CPU, memory, disks, screen, code page</TD><TD>No</TD></TR>
<TR><TD>list_dir</TD><TD>List files and subfolders of a folder</TD><TD>No</TD></TR>
<TR><TD>read_file</TD><TD>Read a text file (decoded as GBK/ANSI)</TD><TD>No</TD></TR>
<TR><TD>write_file</TD><TD>Write text to a file (GBK, overwrites)</TD><TD><B>Yes</B></TD></TR>
<TR><TD>run_command</TD><TD>Run a command through cmd.exe /c and capture output</TD><TD><B>Yes</B></TD></TR>
<TR><TD>open_path</TD><TD>Open a file, folder or URL with the default program</TD><TD><B>Yes</B></TD></TR>
<TR><TD>set_clipboard</TD><TD>Put text on the clipboard</TD><TD>No</TD></TR>
</TABLE>

<H2>The confirmation box</H2>
<P>Writing files, running commands and opening things always asks first:</P>
<CODE>The assistant wants to perform this operation. Allow it?

Run in the command prompt:

cmd.exe /c dir D:\\work

(If unsure, choose No.)</CODE>
<P>Answering No tells the assistant the request was refused; it will not retry and
instead explain and ask what to do differently. The box is never auto-approved.</P>

<DIV class="warn"><B>Please be careful:</B> do not click Yes out of habit. A prompt can
be crafted so that the service emits <CODE>del /s /q</CODE> or
<CODE>format</CODE>. If you do not recognise the command, say No.</DIV>

<H2>Turning it off</H2>
<P>Clear <B>Allow the assistant to use local tools</B> at the bottom of the main window
and it will only give written advice.</P>

<H2>How many tool calls per question</H2>
<P>Each turn may run at most <B>8</B> tool calls; after that the program stops and asks
you to split the task.</P>

<H2>Encoding</H2>
<P>Internally the program uses the system code page (936, GBK, on Simplified Chinese
Windows) while the network API uses UTF-8; conversion is automatic. Files that are
themselves UTF-8 will look garbled, because programs of this era assume GBK.</P>
""")

page("en_workspace.htm", "Workspaces and files", """
<H1>Workspaces and files</H1>

<H2>The three panes</H2>
<TABLE>
<TR><TH>Pane</TH><TH>Purpose</TH></TR>
<TR><TD>Workspace search / Workspaces (left)</TD><TD>Search and choose a workspace.
Each workspace is a folder on disk.</TD></TR>
<TR><TD>File search / Files in workspace (middle)</TD><TD>The files inside the current
workspace folder. Double-click a text file to open it in the built-in editor;
double-click a conversation (*.NBC) to load it.</TD></TR>
<TR><TD>Conversation / Input (right)</TD><TD>The transcript and the box you type in.</TD></TR>
</TABLE>

<H2>What a workspace is</H2>
<P>Every workspace is a folder under
<CODE>Application Data\\NeuroBench\\Workspaces\\&lt;name&gt;</CODE>. Conversations you
have in that workspace are saved there as <CODE>CONV-yyyymmdd-hhmmss.NBC</CODE>, next to
any text files you put there. Switching workspace switches the conversation history and
the file list.</P>

<H2>Everyday operations</H2>
<UL>
<LI><B>Workspace - New Workspace</B> creates a folder and selects it.</LI>
<LI><B>Workspace - New Text File in Workspace</B> creates a file and opens it.</LI>
<LI><B>Workspace - Add File to Workspace</B> copies a file into the current folder.</LI>
<LI><B>Workspace - Open Workspace Folder</B> opens the folder in Explorer.</LI>
<LI><B>F5</B> or <B>Workspace - Refresh</B> re-reads the folder from disk.</LI>
<LI>Right-click a file in the tree for Open / Delete / Refresh.</LI>
</UL>

<H2>Conversation files</H2>
<P>Conversations are plain text with a short <CODE>;</CODE> header. They can be opened
in Notepad or the built-in editor, and they are what <B>File - Save Conversation</B>
writes. Deleting a conversation file from the tree deletes it from disk.</P>

<DIV class="note"><B>Tip:</B> keep one workspace per project. Everything the assistant
produces for that project then lands in one folder you can copy to a floppy or a ZIP
archive.</DIV>
""")

page("en_activate.htm", "Activation and trial", """
<H1>Activation and trial</H1>

<H2>Trial edition</H2>
<P>Without a product key the program runs as a <B>trial</B> limited to <B>30 days</B> or
<B>30 conversations</B>, whichever comes first. Trial state is kept in
<CODE>HKEY_CURRENT_USER\\Software\\Axiom\\NeuroBench2003</CODE> and mirrored to
<CODE>Application Data\\NeuroBench\\NEBENCH.LIC</CODE>.</P>
<P>Do not edit those values by hand: the program notices and marks the licence as
corrupted.</P>

<H2>Product keys (CDK)</H2>
<P>A key is <B>5 groups of 4 characters</B> separated by hyphens:</P>
<CODE>2K7M-4XQ9-BT3P-DE8N-VW6C</CODE>
<P>The alphabet omits 0, 1, I and O to avoid misreading. Case does not matter and the
hyphens are optional.</P>

<H2>Activating</H2>
<OL>
<LI>Choose <B>Tools - Activate Product</B>.</LI>
<LI>Type the key.</LI>
<LI>Press <B>Activate</B>. The status bar then shows "Licensed (Standard /
Professional / Enterprise)".</LI>
</OL>

<H2>Editions</H2>
<TABLE>
<TR><TH>Edition</TH><TH>Seats</TH><TH>Notes</TH></TR>
<TR><TD>Standard</TD><TD>1</TD><TD>Single machine</TD></TR>
<TR><TD>Professional</TD><TD>up to 32</TD><TD>Small office</TD></TR>
<TR><TD>Enterprise</TD><TD>up to 32</TD><TD>Priority support</TD></TR>
</TABLE>

<H2>If the key is rejected</H2>
<UL>
<LI>Check for missing or extra characters (20 characters, hyphens excluded);</LI>
<LI>Make sure it is a <B>NeuroBench 2003</B> key and not one for another product;</LI>
<LI>Otherwise send the key and your invoice to your dealer.</LI>
</UL>

<DIV class="note"><B>Time-limited keys:</B> some keys carry a validity period (for
example 365 days). It counts from the day the program first ran; when it expires the
program reports "trial expired" and needs a new key.</DIV>
""")

page("en_editor.htm", "The built-in text editor", """
<H1>The built-in text editor</H1>
<P>NeuroBench includes a small text editor for the files the assistant produces -
batch files, notes, configuration files - without needing anything beyond Notepad.</P>

<H2>Opening it</H2>
<UL>
<LI><B>File - Text Editor</B>, or the third toolbar button;</LI>
<LI>double-click any text file in the workspace file tree;</LI>
<LI><B>Workspace - New Text File in Workspace</B> creates and opens one.</LI>
</UL>

<H2>Features</H2>
<TABLE>
<TR><TH>Menu</TH><TH>Action</TH><TH>Shortcut</TH></TR>
<TR><TD>File - New</TD><TD>Empty document</TD><TD>Ctrl+N</TD></TR>
<TR><TD>File - Open</TD><TD>Open a text file</TD><TD>Ctrl+O</TD></TR>
<TR><TD>File - Save</TD><TD>Save to the same file</TD><TD>Ctrl+S</TD></TR>
<TR><TD>Edit - Undo</TD><TD>Undo</TD><TD>Ctrl+Z</TD></TR>
<TR><TD>Edit - Find</TD><TD>Standard Find dialog</TD><TD>Ctrl+F</TD></TR>
<TR><TD>Edit - Insert Date/Time</TD><TD>Insert current date and time</TD><TD>F5</TD></TR>
<TR><TD>Format - Word Wrap</TD><TD>Toggle wrapping</TD><TD>-</TD></TR>
<TR><TD>Format - Font</TD><TD>Choose font and size</TD><TD>-</TD></TR>
</TABLE>

<H2>Encoding</H2>
<P>The editor reads and writes <B>ANSI (GBK on Chinese systems)</B>, like every program
of this era, so the files it writes are understood by cmd.exe and Notepad.</P>
<P>UTF-8 files will show garbled Chinese. That is expected: open them in Notepad,
choose Save As and pick ANSI.</P>

<H2>Writing batch files</H2>
<P>Save as ANSI and begin with:</P>
<CODE>@echo off
chcp 936
</CODE>
""")

page("en_faq.htm", "Troubleshooting", """
<H1>Troubleshooting</H1>

<H3>1. "Root certificates not found (CERTS folder missing or empty)"</H3>
<P>The <CODE>CERTS</CODE> folder or its PEM bundle is gone. Reinstall, or run
<B>CERTUPD.EXE</B> from the tools disc to import a bundle.</P>

<H3>2. "Cannot resolve server address"</H3>
<P>Check the cable or modem, confirm you can open any page in a browser, check the URL
for stray spaces, and check your DNS settings.</P>

<H3>3. "Failed to establish a secure connection"</H3>
<UL>
<LI>Check the system date and time first - wrong time breaks certificate validation;</LI>
<LI>Make sure the URL starts with <CODE>https://</CODE>;</LI>
<LI>"certificate not trusted" means the root bundle is out of date: run CERTUPD.EXE;</LI>
<LI>"no cipher suite in common" means the server only accepts TLS 1.3, which this
client cannot do; use a TLS 1.2 endpoint.</LI>
</UL>

<H3>4. "The server rejected the request (HTTP 401)"</H3>
<P>The key is wrong, expired or out of credit. Check it on your provider's site.</P>

<H3>5. Strange boxes or question marks in the transcript</H3>
<P>Font problem. Make sure SimSun is installed, and try a larger font size in
Tools - Options.</P>

<H3>6. The assistant says it cannot browse the web</H3>
<P>Correct: it has no browser. It can only use the tools listed in this help file and
the text you paste into the input box.</P>

<H3>7. Does it run on Windows 98?</H3>
<P>Yes. It is a 32-bit program with no .NET or modern runtime dependencies; Windows 95
OSR2 or later with IE4+ is enough. On such machines keep Max output below 512 tokens.</P>

<H3>8. Where are conversations stored?</H3>
<P>In the current workspace folder, as <CODE>.NBC</CODE> text files. They are saved
automatically after each answer; <B>File - Save Conversation</B> forces a save.</P>

<H3>9. Why is every answer slow?</H3>
<P>The CPU does the TLS maths in software and the link is slow. 5-20 seconds per answer
is normal; lower Max output to speed it up.</P>

<H3>10. How do I remove the program?</H3>
<P>Use <B>Start - Programs - NeuroBench 2003 - Uninstall NeuroBench 2003</B> or
Add/Remove Programs. Your own documents and workspaces are left alone.</P>
""")

page("en_history.htm", "Version history", """
<H1>Version history</H1>

<H2>3.00.2003 (November 2003)</H2>
<UL>
<LI>First release of the NeuroBench workbench: workspace list, workspace file tree and
conversation pane.</LI>
<LI>Tool calling: read/write files, list folders, run commands, system information,
clipboard.</LI>
<LI>Built-in SSL/TLS 1.2 stack so Windows 95/98/2000/XP can reach modern servers.</LI>
<LI>Built-in text editor; double-click a file in the tree to edit it.</LI>
<LI>Product key activation and a 30-day trial.</LI>
<LI>Chinese and English user interface, switchable at run time.</LI>
<LI>HTML Help (.CHM) documentation.</LI>
</UL>

<H2>3.01.2003 (Service Pack 1)</H2>
<UL>
<LI>Updated root certificate bundle.</LI>
<LI>Updated help file.</LI>
<LI>New system information reporting tool (SYSINFO.EXE).</LI>
<LI>Minor interface fixes.</LI>
</UL>

<H2>1.60.0412 (beta)</H2>
<UL>
<LI>Wizard-based installer.</LI>
<LI>Fixed clipped dialogs at 800x600.</LI>
</UL>
""")

page("en_about.htm", "About", """
<H1>About Axiom NeuroBench 2003</H1>
<TABLE>
<TR><TH>Item</TH><TH>Value</TH></TR>
<TR><TD>Product</TD><TD>Axiom NeuroBench 2003</TD></TR>
<TR><TD>Version</TD><TD>3.00.2003</TD></TR>
<TR><TD>Vendor</TD><TD>Axiom Software</TD></TR>
<TR><TD>Copyright</TD><TD>Copyright (C) 1993-2003 Axiom Software</TD></TR>
<TR><TD>Installer</TD><TD>InstallShield-style wizard (SETUP.EXE)</TD></TR>
<TR><TD>Crypto</TD><TD>BearSSL (built in, SSL/TLS 1.2)</TD></TR>
<TR><TD>Help format</TD><TD>Microsoft HTML Help (.CHM)</TD></TR>
</TABLE>

<H2>Third-party components</H2>
<P>This program includes <B>BearSSL</B> by Thomas Pornin, used under the MIT licence. It
provides a complete TLS 1.2 client that does not rely on operating-system crypto.</P>

<H2>Support</H2>
<P>Read the troubleshooting section first. If that does not help, run
<B>Tools - System Information</B>, save the report and send it to your dealer together
with a description of the problem.</P>

<DIV class="note">This software is provided "as is", without warranty of any kind. Any
charges incurred by the cloud service are the user's responsibility.</DIV>
""")


def main():
    for name, title, body in PAGES:
        html = _brand(HEAD % {"t": title} + body + FOOT)
        with open(os.path.join(OUT, name), "wb") as f:
            f.write(html.encode("cp1252", "replace"))
    print("生成 %d 个英文帮助页" % len(PAGES))
    return 0


if __name__ == "__main__":
    sys.exit(main())
