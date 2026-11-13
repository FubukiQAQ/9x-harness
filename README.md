# 9xHarness

一个面向 Windows 95/98/2000/XP 的桌面 AI Agent（智能助手）产品，**外观、行为、
发行方式全部按 2003 年的软件来做**：Win32 三栏工作台、InstallShield 风格安装向导、
可刻录光盘镜像、CDK 序列号激活、CHM 帮助文件。

> 32 位 PE，子系统版本 5.01，静态链接 msvcrt，不依赖 .NET / UCRT / VC++ 运行库。

---

## 1. 交付物

```
build/dist/
  9XHARNESS-DISK1-INSTALL.iso   安装光盘（ISO 9660 + Joliet，含 AUTORUN.INF）
  9XHARNESS-DISK2-TOOLS.iso     工具/服务包光盘
  INSTALL/                           安装盘目录（可直接刻盘或直接运行 SETUP.EXE）
  TOOLS/                             工具盘目录

build/out/
  9XHARNESS.EXE   主程序（工作台 + TLS 1.2 + 工具调用 + 编辑器）
  SETUP.EXE        安装向导（InstallShield 风格）
  UNINST.EXE       卸载程序（支持 /s 静默）
  9XHARNESS.CHM   帮助文件（中文 + English 两个目录分支）
  CDKEY.EXE        产品序列号生成器（OEM 内部工具）
  CERTUPD.EXE      根证书更新程序
  SYSINFO.EXE      系统信息报告工具
  NB-SP1.EXE       服务包 1 安装程序
```

---

## 2. 主程序界面

三栏式工作台（对应需求里的界面草图）：

```
+--------------------------------------------------------------+
| 菜单栏                                                        |
+--------------------------------------------------------------+
| 按钮（工具栏）                                                 |
+-------------+---------------+--------------------------------+
| 工作区搜索   | 文件搜索       | 对话消息                        |
| 工作区列表   | 当前工作区内   | （对话记录）                    |
|             | 的文件树       |--------------------------------|
|             |               | 输入                            |
+-------------+---------------+--------------------------------+
| 其他信息（状态栏）                                             |
+--------------------------------------------------------------+
```

* **工作区 = 文件夹**。每个工作区在
  `Application Data\9xHarness\Workspaces\<名称>` 下，该工作区的对话按文件夹
  收纳成 `CONV-yyyymmdd-hhmmss.NBC`，与用户自己的文本文件并列。
* **双击文件树里的文本文件**即用内置编辑器打开；双击 `.NBC` 载入该会话；
  右键可打开/删除/刷新。
* 左、中两个窗格宽度可用竖向分隔条拖动。
* 菜单「工具 - 语言」可在运行时切换 **简体中文 / English**，选择写入 INI。

## 3. 老旧系统上的 TLS 1.2

Windows XP 的 SChannel 只到 TLS 1.0，现代服务端早已拒绝。本程序内置
[BearSSL](https://www.bearssl.org/)（277 个 C 文件，静态链接），自己做握手、
证书链校验与加解密：

* 支持 TLS 1.2 全套套件，`br_ssl_client_init_full` 默认配置；
* 根证书从 `CERTS\*.PEM` / `*.CRT` 读取（发行包内含 Mozilla CA bundle，
  约 118 个锚点）；
* 证书日期校验用 `br_x509_minimal_set_time`，epoch 从公元 1 年起算
  （`UNIX 纪元 + 719528`）；
* 本机时钟不准时，可用明文 HTTP 的 `Date` 头取回网络时间后重试；
* 主机名校验失败返回 `BR_ERR_X509_BAD_SERVER_NAME(56)`，实测有效。

## 4. 特化提示词

`src/agent/prompt.c` 的中英双份系统提示词明确告诉模型：

* 宿主是 Windows XP/2000/98，32 位，代码页 936（GBK），中文乱码是常态；
* 没有 PowerShell / Python / Node / .NET / Git，只有 cmd 批处理与 WSH；
* 内存 128~512 MB，屏幕 800x600~1024x768，别推荐现代大软件；
* 回答显示在不支持任何标记的纯文本框里，**禁止 Markdown**，行宽 ≤ 60 字符；
* 工具使用规则、危险命令禁止清单、不得编造工具结果。

## 5. 工具调用

| 工具 | 作用 | 需确认 |
|---|---|---|
| `get_system_info` | 系统/CPU/内存/磁盘/屏幕/代码页 | 否 |
| `list_dir` | 列目录（含大小与时间） | 否 |
| `read_file` | 读文本（GBK 解码，上限 64 KB） | 否 |
| `write_file` | 写文本（GBK，覆盖） | **是** |
| `run_command` | `cmd.exe /c` 执行并抓取输出（含超时/取消） | **是** |
| `open_path` | ShellExecute 打开 | **是** |
| `set_clipboard` | 写入剪贴板 | 否 |

每轮最多 8 次工具调用；工具结果以 JSON 回填到对话历史。

## 6. 授权（CDK）

* 序列号 = **5 组 × 4 字符**，字母表 `23456789ABCDEFGHJKLMNPQRSTUVWXYZ`
  （去掉易混的 0/1/I/O），共 20 字符 × 5 bit = 100 bit。
* 载荷：版本(2b) + 用户数(5b) + 有效天数(9b) + 内部序号(24b) + 盐(20b)，
  末 40 bit 是 `HMAC-SHA1(厂商密钥, 载荷)` 截断。
* 试用：30 天 / 30 次会话；状态存 `HKCU\Software\9xHarness\9xHarness`
  并在 `Application Data\9xHarness\9XHARNESS.LIC` 留镜像，带 CRC 防篡改。
* `CDKEY.EXE` 用同一算法批量生成（最多 200 个/次），可存 TXT 或复制。

## 7. 构建

需要：`vendor\mingw32`（i686-msvcrt GCC 15.3）、`vendor\bearssl-0.6`、Python 3。

```powershell
# 全部编译（主程序 + 安装程序 + 工具 + 密钥生成器）
powershell -File build.ps1 -Target all

# 生成帮助文件 CHM
powershell -File build.ps1 -Target docs

# 组装两张光盘目录与 ISO
powershell -File build.ps1 -Target dist

# 一条命令做完整发行版
powershell -File build.ps1 -Target release
```

`build.ps1` 会检查头文件时间戳，头文件改动会触发全部重编（避免结构体布局变化
导致陈旧 `.o` 与新结构错位）。

## 8. 安装程序

InstallShield 风格向导，7 页：欢迎 → 许可证 → 用户信息 → 目标文件夹 →
程序文件夹 → 正在安装 → 完成。左侧竖幅位图 + 右侧标题正文 + 底部
`< 上一步 / 下一步 > / 取消`，配色、分隔线、字体都按当年样式。

命令行（供无人值守部署与自动化测试）：

```
SETUP.EXE /s                     静默安装
SETUP.EXE /s /d="D:\Apps\NB"     静默安装到指定目录
SETUP.EXE /k=XXXX-XXXX-...       安装时同时激活
SETUP.EXE /p=3                   直接打开第 3 页（调试用）
```

安装动作：解 SMARC 容器 → 写 `9XHARNESS.INI` → 建开始菜单（所有用户不可写时
退回当前用户）→ 桌面快捷方式（同上兜底）→ 写 `UNINST.LST` 与卸载注册表项
（无管理员权限时退回 HKCU）→ 可选激活。

## 9. 帮助文件

`9XHARNESS.CHM` 由 `tools/mkchm.py`（自研 CHM/ITSF 写入器，纯标准库）编译，
无需 Microsoft HTML Help Workshop。目录树分「中文帮助 / English help」两支，
索引含中英关键字。

## 10. 源码结构

```
src/common/     公共层：工具函数、SHA1/HMAC/Base64、JSON、INI、注册表、
                CDK 与授权、BearSSL HTTPS 客户端、SMARC 容器、双语字符串表
src/agent/      主程序：main(框架/菜单/布局) chat(对话区) llm(请求与工具循环)
                tools(工具实现) prompt(系统提示词) workspace(工作区/文件树/会话)
                editor(文本编辑器) dlgs(设置/激活/关于/系统信息) res(位图资源)
src/setup/      安装向导
src/keygen/     CDK 生成器
src/tools/      卸载、证书更新、系统信息、服务包
src/help/       帮助页源文件（中文 GBK / 英文 CP1252）
tools/          构建脚本：mkart(位图) mkhelp_zh/mkhelp_en(帮助) mkchm(CHM)
                mkarc(SMARC 打包) mkiso(ISO 母盘) mkdist(发行盘) mockserver(联调)
```

## 11. 已验证

| 项目 | 结果 |
|---|---|
| 32 位 PE，subsystem 5.01 | 全部 8 个 EXE |
| 导入表审计 | 无 Vista+ 专有 API、无 UCRT/api-ms-win-* 依赖 |
| 真实 HTTPS 握手 | `example.com` TLS 1.2 成功；错误主机名正确拒绝(56) |
| 工具调用闭环 | 假服务器两轮：tool_calls → 执行 → 文本总结 |
| 双击文件树开编辑器 | 通过（真实鼠标事件） |
| 语言切换 | 中↔英即时生效，INI 持久化 |
| 静默安装 + 激活 | 退出码 0，8 个文件，注册表写入企业版/10 用户 |
| 从 ISO 安装 | 解出的 SETUP.EXE 静默安装成功并显示「已授权」 |
| 卸载 | `/s` 静默，文件/快捷方式/注册表全部清除 |
| CHM | 7-Zip 可列出并解出全部文件；hh.exe 正常显示中文目录与正文 |
| ISO | 7-Zip 识别为 ISO，8 文件，Joliet 长名正常 |
