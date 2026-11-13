/* NeuroBench 2003 - 工具调用（函数调用）实现 */
#include "agent.h"

/* 下一次请求要附带的截图（PNG）。由 screenshot 工具填充，llm.c 发完清空。 */
unsigned char *g_shot_png = NULL;
int            g_shot_len = 0;
int            g_shot_w = 0, g_shot_h = 0;

#define TOOL_OUT_LIMIT (24 * 1024)

static const char *T2(const char *zh, const char *en)
{
    return (lang_get() == LANG_EN) ? en : zh;
}

/* ------------------------------------------------------------------ */
/* 工具定义（发给云端的 JSON，必须是 UTF-8）                            */
/* ------------------------------------------------------------------ */
static void tool_begin(Sb *b, const char *name, const char *desc)
{
    sb_puts(b, "{\"type\":\"function\",\"function\":{\"name\":\"");
    sb_puts(b, name);
    sb_puts(b, "\",\"description\":");
    sb_put_json_str(b, desc);
    sb_puts(b, ",\"parameters\":{\"type\":\"object\",\"properties\":{");
}

static void tool_prop(Sb *b, int first, const char *name, const char *type, const char *desc)
{
    if (!first) sb_putc(b, ',');
    sb_putc(b, '"');
    sb_puts(b, name);
    sb_puts(b, "\":{\"type\":\"");
    sb_puts(b, type);
    sb_puts(b, "\",\"description\":");
    sb_put_json_str(b, desc);
    sb_putc(b, '}');
}

static void tool_end(Sb *b, const char *required /* 逗号分隔，可为 NULL */)
{
    sb_puts(b, "},\"required\":[");
    if (required && required[0]) {
        const char *p = required;
        int first = 1;
        while (*p) {
            const char *e = strchr(p, ',');
            int n = e ? (int)(e - p) : (int)strlen(p);
            if (!first) sb_putc(b, ',');
            sb_putc(b, '"');
            sb_putn(b, p, (size_t)n);
            sb_putc(b, '"');
            first = 0;
            if (!e) break;
            p = e + 1;
        }
    }
    sb_puts(b, "]}}}");
}

char *tools_spec_json(void)
{
    Sb b;
    sb_init(&b);
    sb_putc(&b, '[');

    tool_begin(&b, "get_system_info",
        "读取这台电脑的基本情况：操作系统版本、CPU、内存、磁盘剩余空间、屏幕、"
        "系统代码页、当前用户与常用目录。不了解这台机器时先调用它。");
    tool_end(&b, NULL);
    sb_putc(&b, ',');

    tool_begin(&b, "screenshot",
        "抓取这台电脑当前屏幕，并把它随下一次请求发给模型（只有支持读图的模型才看得到）。"
        "用户说“看看屏幕/界面上有什么/按钮在哪/帮我确认一下”时调用一次。"
        "调用后不需要再读文件，直接根据你看到的画面回答。");
    tool_end(&b, NULL);
    sb_putc(&b, ',');

    tool_begin(&b, "list_dir",
        "列出某个文件夹中的文件和子文件夹（含大小与修改时间）。path 必须是完整"
        "Windows 路径，例如 C:\\WINDOWS 或 D:\\。");
    tool_prop(&b, 1, "path", "string", "完整的 Windows 文件夹路径");
    tool_end(&b, "path");
    sb_putc(&b, ',');

    tool_begin(&b, "read_file",
        "读取一个文本文件的内容（按 GBK/ANSI 解码，若失败则按 UTF-8 解码）。"
        "只能读取文本，不要用来读 .exe/.dll 等二进制文件。");
    tool_prop(&b, 1, "path", "string", "完整的 Windows 文件路径");
    tool_prop(&b, 0, "max_bytes", "integer", "最多读取多少字节，默认 16384，最大 65536");
    tool_end(&b, "path");
    sb_putc(&b, ',');

    tool_begin(&b, "write_file",
        "把文本内容写入文件（会以 GBK/ANSI 编码保存，覆盖原文件）。"
        "写入前会用对话框征求用户同意。适合保存笔记、批处理脚本、生成的文本。");
    tool_prop(&b, 1, "path", "string", "完整的 Windows 文件路径");
    tool_prop(&b, 0, "content", "string", "要写入的完整文本内容");
    tool_end(&b, "path,content");
    sb_putc(&b, ',');

    tool_begin(&b, "run_command",
        "在本机用 cmd.exe /c 执行一条命令，并返回它的输出（含错误输出）。"
        "只能用于安全的只读命令或用户明确要求的操作，会先弹出确认对话框。");
    tool_prop(&b, 1, "command", "string", "要执行的命令行，不含 cmd.exe 本身");
    tool_prop(&b, 0, "timeout_sec", "integer", "超时秒数，默认 20，最大 120");
    tool_end(&b, "command");
    sb_putc(&b, ',');

    tool_begin(&b, "open_path",
        "用系统默认程序打开一个文件、文件夹或网址（相当于在资源管理器里双击）。");
    tool_prop(&b, 1, "path", "string", "文件、文件夹的完整路径或 http 网址");
    tool_end(&b, "path");
    sb_putc(&b, ',');

    tool_begin(&b, "set_clipboard",
        "把一段文字放进系统剪贴板，方便用户直接粘贴到别处。");
    tool_prop(&b, 1, "text", "string", "要放进剪贴板的文字");
    tool_end(&b, "text");

    sb_putc(&b, ']');
    return sb_take(&b);
}

/* ------------------------------------------------------------------ */
/* 系统信息                                                            */
/* ------------------------------------------------------------------ */
#ifndef SM_REMAP
#define SM_REMAP
#endif

void tools_sysinfo_text(Sb *out)
{
    char *os = sm_get_os_name();
    char user[128] = "", comp[128] = "";
    DWORD n;
    ULONGLONG total = 0, avail = 0;
    char root[MAX_PATH];
    ULARGE_INTEGER freeb, totalb;
    char *cpuname = NULL;
    char ie[64] = "";
    int depth;
    HDC dc;

    sb_printf(out, "%s\r\n", L(STR_SI_HEADER));
    sb_printf(out, "%s %s\r\n", T2("操作系统：", "OS:"), os);
    sm_free(os);
    sb_printf(out, "%s %u (%s)\r\n", T2("系统代码页：", "Code page:"), sm_system_codepage(),
              sm_system_codepage() == 936 ? "简体中文 GBK" : "非简体中文");

    n = sizeof(user);
    if (GetUserNameA(user, &n)) sb_printf(out, "%s %s\r\n", T2("当前用户：", "User:"), user);
    n = sizeof(comp);
    if (GetComputerNameA(comp, &n)) sb_printf(out, "%s %s\r\n", T2("计算机名：", "Computer:"), comp);

    {
        char *v = sm_reg_get_sz(HKEY_LOCAL_MACHINE,
                  "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "ProcessorNameString");
    /* Windows 9x 没有 ProcessorNameString，只有 Identifier（如
     * "GenuineIntel" / "x86 Family 5 Model 2 Stepping 12"）。 */
    if (!v)
        v = sm_reg_get_sz(HKEY_LOCAL_MACHINE,
                "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "Identifier");
        if (v) {
            cpuname = v;
            sm_trim(cpuname);
            sb_printf(out, "%s %s\r\n", T2("处理器：", "CPU:"), cpuname);
            sm_free(v);
        }
    }

    {
        unsigned long mt = 0, ma = 0, ml = 0;
        if (sm_memory_info(&mt, &ma, &ml)) {
            sb_printf(out, T2("物理内存：%lu MB（可用约 %lu MB）\r\n", "RAM: %lu MB (free %lu MB)\r\n"), mt, ma);
            sb_printf(out, T2("内存占用：%lu%%\r\n", "Memory load: %lu%%\r\n"), ml);
        }
    }

    (void)total; (void)avail;

    {
        char win[MAX_PATH];
        char sys[MAX_PATH];
        GetWindowsDirectoryA(win, MAX_PATH);
        GetSystemDirectoryA(sys, MAX_PATH);
        sb_printf(out, "%s %s\r\n", T2("系统目录：", "Windows dir:"), win);
        sb_printf(out, "%s %s\r\n", T2("系统32目录：", "System32 dir:"), sys);
    }

    strcpy(root, "C:\\");
    if (GetDiskFreeSpaceExA(root, &freeb, &totalb, NULL)) {
        sb_printf(out, T2("C 盘：共 %.0f MB，可用 %.0f MB\r\n", "Drive C: %.0f MB total, %.0f MB free\r\n"),
                  (double)totalb.QuadPart / (1024.0 * 1024.0),
                  (double)freeb.QuadPart / (1024.0 * 1024.0));
    }
    {
        DWORD drives = GetLogicalDrives();
        char dl[64] = "";
        int i;
        for (i = 0; i < 26; i++) {
            if (drives & (1u << i)) {
                char d[8];
                d[0] = (char)('A' + i); d[1] = ':'; d[2] = '\\'; d[3] = 0;
                if (dl[0]) strcat(dl, " ");
                strcat(dl, d);
            }
        }
        sb_printf(out, "%s %s\r\n", T2("逻辑驱动器：", "Drives:"), dl);
    }

    dc = GetDC(NULL);
    if (dc) {
        depth = GetDeviceCaps(dc, BITSPIXEL);
        sb_printf(out, T2("屏幕：%d x %d，%d 位色\r\n", "Screen: %d x %d, %d bpp\r\n"),
                  GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), depth);
        ReleaseDC(NULL, dc);
    }

    if (sm_reg_get_sz(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Internet Explorer", "Version")) {
        char *v = sm_reg_get_sz(HKEY_LOCAL_MACHINE,
                    "SOFTWARE\\Microsoft\\Internet Explorer", "Version");
        if (v) {
            strncpy(ie, v, sizeof(ie) - 1);
            sm_free(v);
        }
        if (ie[0]) sb_printf(out, "Internet Explorer: %s\r\n", ie);
    }

    {
        char *up = NULL;
        up = sm_appdata_dir();
        sb_printf(out, "%s %s\r\n", T2("本软件数据目录：", "Data dir:"), up);
        sm_free(up);
    }
    {
        char *d = sm_app_dir();
        sb_printf(out, "%s %s\r\n", T2("本软件安装目录：", "Install dir:"), d);
        sm_free(d);
    }
    {
        /* 当前工作区：模型要往这里放文件，必须让它知道 */
        char *wd = ws_current_dir();
        sb_printf(out, "%s %s\r\n", T2("当前工作区：", "Workspace:"),
                  ws_current()[0] ? ws_current() : T2("(无)", "(none)"));
        sb_printf(out, "%s %s\r\n", T2("工作区目录：", "Workspace folder:"), wd);
        sm_free(wd);
    }
    {
        char td[64];
        sm_now_str_era(td, sizeof(td));
        sb_printf(out, "%s %s\r\n", T2("当前时间：", "Local time:"), td);
    }
}

/* ------------------------------------------------------------------ */
/* 执行命令                                                            */
/* ------------------------------------------------------------------ */
static char *run_capture(const char *cmd, int timeoutSec, int *exitCode,
                         volatile LONG *cancel)
{
    SECURITY_ATTRIBUTES sa;
    HANDLE hR = NULL, hW = NULL;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    char cmdline[2048];
    Sb out;
    DWORD waitStart;
    int done = 0;

    *exitCode = -1;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;
    if (!CreatePipe(&hR, &hW, &sa, 0)) return NULL;
    SetHandleInformation(hR, HANDLE_FLAG_INHERIT, 0);

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = NULL;
    si.hStdOutput = hW;
    si.hStdError = hW;

    _snprintf(cmdline, sizeof(cmdline), "cmd.exe /c %s", cmd);
    cmdline[sizeof(cmdline) - 1] = 0;

    ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, CREATE_NO_WINDOW,
                        NULL, NULL, &si, &pi)) {
        CloseHandle(hR);
        CloseHandle(hW);
        return NULL;
    }
    CloseHandle(hW);

    sb_init(&out);
    waitStart = GetTickCount();
    for (;;) {
        DWORD avail = 0;
        char buf[2048];
        DWORD got = 0;
        if (cancel && *cancel) break;
        if (!PeekNamedPipe(hR, NULL, 0, NULL, &avail, NULL)) break;
        if (avail > 0) {
            DWORD take = avail > sizeof(buf) ? (DWORD)sizeof(buf) : avail;
            if (!ReadFile(hR, buf, take, &got, NULL) || got == 0) break;
            if (out.len < TOOL_OUT_LIMIT) sb_putn(&out, buf, got);
        } else {
            DWORD wr = WaitForSingleObject(pi.hProcess, 40);
            if (wr == WAIT_OBJECT_0) {
                /* 进程结束，把管道里剩下的读完 */
                for (;;) {
                    if (!PeekNamedPipe(hR, NULL, 0, NULL, &avail, NULL)) break;
                    if (avail == 0) break;
                    {
                        DWORD take = avail > sizeof(buf) ? (DWORD)sizeof(buf) : avail;
                        if (!ReadFile(hR, buf, take, &got, NULL) || got == 0) break;
                        if (out.len < TOOL_OUT_LIMIT) sb_putn(&out, buf, got);
                    }
                }
                done = 1;
                break;
            }
        }
        if ((GetTickCount() - waitStart) > (DWORD)timeoutSec * 1000) {
            sb_puts(&out, "\r\n[命令执行超时，已被强制结束]");
            TerminateProcess(pi.hProcess, 1);
            break;
        }
    }
    if (done) {
        DWORD ec = 0;
        GetExitCodeProcess(pi.hProcess, &ec);
        *exitCode = (int)ec;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(hR);
    return sb_take(&out);
}

/* ------------------------------------------------------------------ */
static char        g_confText[1200];
static HANDLE      g_confEvent = NULL;
static volatile LONG g_confResult = -1;   /* -1 未决 0 拒绝 1 允许 */

/* worker 线程调用：请主线程弹确认框，然后等结果 */
int tools_confirm(const char *what)
{
    DWORD w;

    if (!g.hFrame) return 0;
    _snprintf(g_confText, sizeof(g_confText), "%s", what ? what : "");
    g_confText[sizeof(g_confText) - 1] = 0;
    if (!g_confEvent) g_confEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (!g_confEvent) return 0;

    InterlockedExchange(&g_confResult, -1);
    if (!PostMessageA(g.hFrame, WM_NB_CONFIRM, 0, 0)) return 0;

    for (;;) {
        w = WaitForSingleObject(g_confEvent, 200);
        if (w == WAIT_OBJECT_0) break;
        if (g.cancel) {                 /* 用户按了停止：一律当拒绝 */
            InterlockedExchange(&g_confResult, 0);
            break;
        }
    }
    return InterlockedCompareExchange(&g_confResult, 0, 0) == 1;
}

/* 主线程：真正弹框 */
void tools_confirm_show(void)
{
    int r = dlg_confirm(g.hFrame, g_confText);
    InterlockedExchange(&g_confResult, r ? 1 : 0);
    if (g_confEvent) SetEvent(g_confEvent);
}

/* ------------------------------------------------------------------ */
static char *arg_str(Js *obj, const char *key)
{
    Js *v = js_obj_get(obj, key);
    const char *s = js_str(v, NULL);
    if (!s) return NULL;
    return sm_strdup(s);
}

/* ------------------------------------------------------------------ */
/* 工具权限：0=只读 1=总是询问 2=完全权限                              */
/* 只读模式在 tools_execute 入口统一拦截写操作；这里管「要不要问一句」。 */
/* ------------------------------------------------------------------ */
static char g_why[400];

static int tool_perm(const char *desc, char *why, int whylen)
{
    if (g.cfg.permMode >= 2) return 1;          /* 完全权限：不问 */
    if (tools_confirm(desc)) return 1;
    _snprintf(why, whylen, T2(
        "用户拒绝了这个操作。不要重复尝试，也不要换个说法再试一次；等用户主动要求后再做。",
        "The user declined this operation. Do not retry it or rephrase it; "
        "wait until the user asks again."));
    why[whylen - 1] = 0;
    return 0;
}

static int tool_is_write(const char *name)
{
    return !strcmp(name, "write_file") || !strcmp(name, "run_command") ||
           !strcmp(name, "open_path") || !strcmp(name, "set_clipboard");
}

char *tools_execute(const char *name, const char *argsJson, int *isError)
{
    Sb out;
    Js *args = NULL;
    const char *err = NULL;

    *isError = 0;
    sb_init(&out);

    if (!name) { *isError = 1; return sm_strdup("错误：未指定工具名。"); }

    /* 只读模式：写操作类工具一律拒绝，不打扰用户 */
    if (g.cfg.permMode <= 0 && name && tool_is_write(name)) {
        *isError = 1;
        sb_puts(&out, T2(
            "当前权限是「只读」：不能写文件、执行命令、打开程序或改剪贴板。"
            "如果用户确实需要，请告诉用户到 工具 → 设置 → Agent 里把权限改成"
            "「总是询问」或「完全权限」，然后再让他重试。",
            "Permission is currently READ-ONLY: writing files, running commands, "
            "opening programs and changing the clipboard are blocked. Tell the user "
            "they can change this in Tools > Settings > Agent."));
        js_free(args);
        return sb_take(&out);
    }
    args = js_parse(argsJson && argsJson[0] ? argsJson : "{}", &err);
    if (!args) {
        *isError = 1;
        sb_printf(&out, "错误：工具参数不是合法的 JSON（%s）。", err ? err : "未知");
        return sb_take(&out);
    }

    if (!strcmp(name, "screenshot")) {
        unsigned char *bgr = NULL;
        int sw = 0, sh = 0;
        if (!sm_grab_screen_max(&bgr, &sw, &sh, 1024, 768)) {
            *isError = 1;
            sb_puts(&out, "截屏失败：无法抓取屏幕。");
        } else {
            long pngLen = 0;
            unsigned char *png = sm_png_encode(bgr, sw, sh, &pngLen);
            sm_free(bgr);
            if (!png) {
                *isError = 1;
                sb_puts(&out, "截屏失败：PNG 编码出错。");
            } else {
                /* 交给 llm.c 在下一次请求里带上 */
                sm_free(g_shot_png);
                g_shot_png = png;
                g_shot_len = (int)pngLen;
                g_shot_w = sw;
                g_shot_h = sh;
                sb_printf(&out, "已抓取屏幕 %dx%d，画面会随下一次请求发给你。", sw, sh);
            }
        }
    } else if (!strcmp(name, "get_system_info")) {
        tools_sysinfo_text(&out);
    } else if (!strcmp(name, "list_dir")) {
        char *p = arg_str(args, "path");
        if (!p || !p[0]) {
            *isError = 1;
            sb_puts(&out, "错误：缺少 path 参数。");
        } else {
            SmDirEnt *ents = NULL;
            int cnt = 0;
            char *path = sm_utf8_to_ansi(p);   /* JSON 里是 UTF-8 */
            sm_free(p);
            if (!sm_list_dir(path, &ents, &cnt)) {
                *isError = 1;
                sb_printf(&out, "错误：无法打开目录 %s（可能不存在或没有权限）。", path);
            } else {
                char ts[32];
                int i, shown = 0;
                sb_printf(&out, "目录 %s 共 %d 项：\r\n", path, cnt);
                for (i = 0; i < cnt && shown < 300; i++, shown++) {
                    sm_filetime_str(&ents[i].mtime, ts, sizeof(ts));
                    if (ents[i].attrib & FILE_ATTRIBUTE_DIRECTORY)
                        sb_printf(&out, "[目录] %-40s %s\r\n", ents[i].name, ts);
                    else
                        sb_printf(&out, "       %-40s %8lu 字节  %s\r\n",
                                  ents[i].name, (unsigned long)ents[i].size, ts);
                }
                if (cnt > shown) sb_printf(&out, "...（还有 %d 项未显示）\r\n", cnt - shown);
            }
            sm_free(path);
            sm_free(ents);
        }
    } else if (!strcmp(name, "read_file")) {
        char *p = arg_str(args, "path");
        Js *mb = js_obj_get(args, "max_bytes");
        int maxb = (int)js_num(mb, 16384);
        if (maxb <= 0) maxb = 16384;
        if (maxb > 65536) maxb = 65536;
        if (!p || !p[0]) {
            *isError = 1;
            sb_puts(&out, "错误：缺少 path 参数。");
        } else {
            char *path = sm_utf8_to_ansi(p);
            long got = 0;
            char *data;
            sm_free(p);
            data = sm_read_file(path, maxb, &got);
            if (!data) {
                *isError = 1;
                sb_printf(&out, "错误：无法读取文件 %s。", path);
            } else {
                sb_printf(&out, "文件 %s 的内容（%ld 字节）：\r\n", path, got);
                sb_putn(&out, data, (size_t)got);
                if (got >= maxb) sb_puts(&out, "\r\n[已达读取上限，内容可能不完整]");
                sm_free(data);
            }
            sm_free(path);
        }
    } else if (!strcmp(name, "write_file")) {
        char *p = arg_str(args, "path");
        Js *cv = js_obj_get(args, "content");
        const char *contentU = js_str(cv, "");
        if (!p || !p[0]) {
            *isError = 1;
            sb_puts(&out, "错误：缺少 path 参数。");
        } else {
            char *path = sm_utf8_to_ansi(p);
            char *content = sm_utf8_to_ansi(contentU);
            char what[600];
            int ok = 1;
            sm_free(p);
            if (1) {
                _snprintf(what, sizeof(what), "写入文件：\r\n%s\r\n\r\n内容长度：%d 字节",
                          path, (int)strlen(content));
                what[sizeof(what) - 1] = 0;
                ok = tool_perm(what, g_why, sizeof(g_why));
            }
            if (!ok) {
                *isError = 1;
                sb_puts(&out, g_why);
            } else if (sm_write_file(path, content, (long)strlen(content))) {
                sb_printf(&out, "已写入 %s（%d 字节，GBK 编码）。", path, (int)strlen(content));
            } else {
                *isError = 1;
                sb_printf(&out, "错误：写入失败 %s（磁盘满或只读？）。", path);
            }
            sm_free(content);
            sm_free(path);
        }
    } else if (!strcmp(name, "run_command")) {
        char *c = arg_str(args, "command");
        int tsec = (int)js_num(js_obj_get(args, "timeout_sec"), 20);
        if (tsec < 5) tsec = 5;
        if (tsec > 120) tsec = 120;
        if (!c || !c[0]) {
            *isError = 1;
            sb_puts(&out, "错误：缺少 command 参数。");
        } else {
            char *cmd = sm_utf8_to_ansi(c);
            int ec = -1;
            char *res = NULL;
            int ok = 1;
            sm_free(c);
            if (1) {
                char what[1200];
                _snprintf(what, sizeof(what),
                          "在命令提示符中执行：\r\n\r\ncmd.exe /c %s", cmd);
                what[sizeof(what) - 1] = 0;
                ok = tool_perm(what, g_why, sizeof(g_why));
            }
            if (!ok) {
                *isError = 1;
                sb_puts(&out, g_why);
            } else {
                res = run_capture(cmd, tsec, &ec, &g.cancel);
                if (!res) {
                    *isError = 1;
                    sb_puts(&out, "错误：命令无法启动。");
                } else {
                    sb_printf(&out, "命令：%s\r\n退出码：%d\r\n输出：\r\n", cmd, ec);
                    sb_puts(&out, res);
                    if (!res[0]) sb_puts(&out, "（没有任何输出）");
                    if (ec != 0) *isError = 1;
                    sm_free(res);
                }
            }
            sm_free(cmd);
        }
    } else if (!strcmp(name, "open_path")) {
        char *p = arg_str(args, "path");
        if (!p || !p[0]) {
            *isError = 1;
            sb_puts(&out, "错误：缺少 path 参数。");
        } else {
            char *path = sm_utf8_to_ansi(p);
            char what[600];
            int ok = 1;
            sm_free(p);
            if (1) {
                _snprintf(what, sizeof(what), "用系统默认程序打开：\r\n%s", path);
                what[sizeof(what) - 1] = 0;
                ok = tool_perm(what, g_why, sizeof(g_why));
            }
            if (ok) {
                HINSTANCE r = ShellExecuteA(g.hFrame, "open", path, NULL, NULL, SW_SHOWNORMAL);
                if ((int)r <= 32) {
                    *isError = 1;
                    sb_printf(&out, "错误：无法打开 %s。", path);
                } else {
                    sb_printf(&out, "已经用系统默认程序打开 %s。", path);
                }
            } else {
                *isError = 1;
                sb_puts(&out, "用户拒绝了这个操作。");
            }
            sm_free(path);
        }
    } else if (!strcmp(name, "set_clipboard")) {
        Js *tv = js_obj_get(args, "text");
        const char *t = js_str(tv, "");
        char *a = sm_utf8_to_ansi(t);
        if (OpenClipboard(g.hFrame)) {
            HGLOBAL hg;
            EmptyClipboard();
            hg = GlobalAlloc(GMEM_MOVEABLE, strlen(a) + 1);
            if (hg) {
                char *dst = (char *)GlobalLock(hg);
                strcpy(dst, a);
                GlobalUnlock(hg);
                SetClipboardData(CF_TEXT, hg);
            }
            CloseClipboard();
            sb_puts(&out, "文字已经放进剪贴板，用户可以直接按 Ctrl+V 粘贴。");
        } else {
            *isError = 1;
            sb_puts(&out, "错误：剪贴板被其他程序占用，请稍后再试。");
        }
        sm_free(a);
    } else {
        *isError = 1;
        sb_printf(&out, "错误：没有名为 %s 的工具。", name);
    }

    js_free(args);
    return sb_take(&out);
}
