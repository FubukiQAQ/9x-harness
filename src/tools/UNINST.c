/* =====================================================================
 * NeuroBench 2003 - 卸载程序 UNINST.EXE
 * 读同目录下的 UNINST.LST（安装时写入），删除文件、快捷方式与注册表项。
 * ===================================================================== */
#include "../common/sm.h"
#include "../common/nb_lang.h"
#include <commctrl.h>
#include <shellapi.h>

#define IDC_INFO    4001
#define IDC_GO      4002
#define IDC_QUIT    4003
#define IDC_PROG    4004
#define IDC_STEP    4005

static HINSTANCE g_inst;
static HWND g_win, g_info, g_prog, g_step;
static HFONT g_font, g_fontB;
static int g_lines = 0;
static int g_done = 0;

static void pump(void)
{
    MSG m;
    while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&m);
        DispatchMessageA(&m);
    }
}

static int delete_reg_tree(HKEY root, const char *path)
{
    HKEY k;
    char sub[256];
    DWORD idx = 0, len;
    while (RegOpenKeyExA(root, path, 0, KEY_ALL_ACCESS, &k) == ERROR_SUCCESS) {
        len = sizeof(sub);
        idx = 0;
        while (RegEnumKeyExA(k, idx, sub, &len, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
            char full[512];
            _snprintf(full, sizeof(full), "%s\\%s", path, sub);
            full[sizeof(full) - 1] = 0;
            RegCloseKey(k);
            delete_reg_tree(root, full);
            if (RegOpenKeyExA(root, path, 0, KEY_ALL_ACCESS, &k) != ERROR_SUCCESS) return 0;
            len = sizeof(sub);
            idx++;
        }
        RegCloseKey(k);
        return RegDeleteKeyA(root, path) == ERROR_SUCCESS;
    }
    return 1;
}

static char g_baseDir[MAX_PATH];

static void do_uninstall(void)
{
    char *dir = g_baseDir[0] ? sm_strdup(g_baseDir) : sm_exe_dir();
    char *lst = sm_path_join(dir, "UNINST.LST");
    char *data = sm_read_file(lst, 4 * 1024 * 1024, NULL);
    int total = 0, i;
    char *p;

    Sleep(400);   /* 等原进程完全退出，否则删不掉 UNINST.EXE 自己 */
    if (!data) {
        MessageBoxA(g_win, "找不到卸载记录 UNINST.LST。\r\n\r\n"
                           "无法自动卸载，请手工删除安装文件夹。",
                    "卸载", MB_ICONSTOP | MB_OK);
        sm_free(lst); sm_free(dir);
        return;
    }
    for (p = data; *p; p++) if (*p == '\n') total++;

    i = 0;
    p = data;
    while (*p) {
        char line[MAX_PATH + 40];
        int n = 0;
        while (*p && *p != '\r' && *p != '\n' && n < (int)sizeof(line) - 1) line[n++] = *p++;
        line[n] = 0;
        while (*p == '\r' || *p == '\n') p++;
        if (n < 3 || line[0] == ';') continue;

        i++;
        if (g_step) {
            char b[MAX_PATH + 60];
            _snprintf(b, sizeof(b), TS("正在删除：%s",
           "Removing: %s"), line + 2);
            b[sizeof(b) - 1] = 0;
            SetWindowTextA(g_step, b);
        }
        if (g_prog) SendMessageA(g_prog, PBM_SETPOS, (WPARAM)(total ? i * 100 / total : 0), 0);
        pump();

        if (line[0] == 'F' && line[1] == '|') {
            SetFileAttributesA(line + 2, FILE_ATTRIBUTE_NORMAL);
            DeleteFileA(line + 2);
        } else if (line[0] == 'L' && line[1] == '|') {
            SetFileAttributesA(line + 2, FILE_ATTRIBUTE_NORMAL);
            DeleteFileA(line + 2);
        } else if (line[0] == 'D' && line[1] == '|') {
            RemoveDirectoryA(line + 2);
        } else if (line[0] == 'R' && line[1] == '|') {
            const char *k = line + 2;
            if (!_strnicmp(k, "HKLM\\", 5)) delete_reg_tree(HKEY_LOCAL_MACHINE, k + 5);
            else if (!_strnicmp(k, "HKCU\\", 5)) delete_reg_tree(HKEY_CURRENT_USER, k + 5);
        }
        Sleep(6);
    }
    sm_free(data);

    DeleteFileA(lst);
    DeleteFileA(sm_path_join(dir, SM_INI_NAME));
    /* 最后清掉自己的目录（先删掉残余文件） */
    {
        char *pat = sm_path_join(dir, "*.*");
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(pat, &fd);
        sm_free(pat);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                char *full;
                if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
                full = sm_path_join(dir, fd.cFileName);
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) sm_delete_tree(full);
                else { SetFileAttributesA(full, FILE_ATTRIBUTE_NORMAL); DeleteFileA(full); }
                sm_free(full);
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        }
    }
    sm_free(lst);
    sm_free(dir);
    g_done = 1;
}

static LRESULT CALLBACK UninstProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT r;
        GetClientRect(h, &r);
        FillRect(dc, &r, (HBRUSH)(COLOR_BTNFACE + 1));
        {
            RECT hr;
            hr.left = 0; hr.top = 0; hr.right = r.right; hr.bottom = 44;
            FillRect(dc, &hr, CreateSolidBrush(RGB(0, 0, 128)));
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(255, 255, 255));
            SelectObject(dc, g_fontB);
            TextOutA(dc, 12, 10, SM_PRODUCT_NAME " 卸载程序",
             lstrlenA(SM_PRODUCT_NAME " 卸载程序"));
            SelectObject(dc, g_font);
            TextOutA(dc, 14, 28, TS("将删除本软件在您计算机上安装的全部文件",
           "Removes every file this program installed on your computer"), 20);
        }
        EndPaint(h, &ps);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(w) == IDC_QUIT) {
            if (!g_done && MessageBoxA(h, TS("卸载尚未完成，确实要退出吗？",
           "Uninstall is not finished. Really quit?"), "卸载",
                                       MB_ICONQUESTION | MB_YESNO) != IDYES) return 0;
            DestroyWindow(h);
            return 0;
        }
        if (LOWORD(w) == IDC_GO) {
            EnableWindow(GetDlgItem(h, IDC_GO), FALSE);
            EnableWindow(GetDlgItem(h, IDC_QUIT), FALSE);
            ShowWindow(g_info, SW_HIDE);
            ShowWindow(g_prog, SW_SHOW);
            ShowWindow(g_step, SW_SHOW);
            do_uninstall();
            MessageBoxA(h, SM_PRODUCT_NAME " 已从您的计算机上删除。\r\n\r\n"
                           "谢谢使用本软件。", "卸载", MB_ICONINFORMATION | MB_OK);
            DestroyWindow(h);
            return 0;
        }
        break;
    case WM_CLOSE:
        if (!g_done && MessageBoxA(h, TS("卸载尚未完成，确实要退出吗？",
           "Uninstall is not finished. Really quit?"), "卸载",
                                   MB_ICONQUESTION | MB_YESNO) != IDYES) return 0;
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

int PASCAL WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    nb_lang_init();
    WNDCLASSA wc;
    HWND h, c;
    MSG msg;
    INITCOMMONCONTROLSEX icc;
    LOGFONTA lf;

    (void)prev;
    g_inst = inst;
    /* 命令行: /s 静默；/dir= 指定安装目录；/fromtemp 表示已从临时目录重启 */
    {
        int silent = 0, fromtemp = 0;
        char *cl = GetCommandLineA();
        char *p;
        g_baseDir[0] = 0;
        if (strstr(cl, "/s") || strstr(cl, "-s")) silent = 1;
        if (strstr(cl, "/fromtemp")) fromtemp = 1;
        p = (char *)sm_stristr(cl, "/dir=");
        if (p) {
            int n = 0;
            p += 5;
            if (*p == 34) p++;                     /* 跳过引号 */
            while (*p && *p != 34 && n < MAX_PATH - 1) g_baseDir[n++] = *p++;
            g_baseDir[n] = 0;
            sm_trim(g_baseDir);
        }
        if (!g_baseDir[0]) {
            char *d = sm_exe_dir();
            strncpy(g_baseDir, d, MAX_PATH - 1);
            g_baseDir[MAX_PATH - 1] = 0;
            sm_free(d);
        }
        /* 卸载程序删不掉自己：先复制到临时目录，再从那里执行 */
        if (!fromtemp) {
            char *td = sm_temp_dir();
            char *cp = sm_path_join(td, "~SMUNINST.EXE");
            char *self = sm_path_join(g_baseDir, "UNINST.EXE");
            if (sm_file_exists(self)) {
                DeleteFileA(cp);
                if (CopyFileA(self, cp, FALSE)) {
                    STARTUPINFOA si;
                    PROCESS_INFORMATION pi;
                    char line[MAX_PATH * 2 + 64];
                    _snprintf(line, sizeof(line), "\"%s\" /fromtemp%s /dir=\"%s\"",
                              cp, silent ? " /s" : "", g_baseDir);
                    line[sizeof(line) - 1] = 0;
                    ZeroMemory(&si, sizeof(si));
                    si.cb = sizeof(si);
                    if (CreateProcessA(NULL, line, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                        CloseHandle(pi.hThread);
                        CloseHandle(pi.hProcess);
                        sm_free(td); sm_free(cp); sm_free(self);
                        return 0;
                    }
                }
            }
            sm_free(td); sm_free(cp); sm_free(self);
        }
        if (silent) {
            do_uninstall();
            return g_done ? 0 : 2;
        }
    }
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    ZeroMemory(&lf, sizeof(lf));
    lf.lfHeight = -12;
    lf.lfCharSet = DEFAULT_CHARSET;
    strcpy(lf.lfFaceName, "宋体");
    g_font = CreateFontIndirectA(&lf);
    lf.lfWeight = FW_BOLD;
    g_fontB = CreateFontIndirectA(&lf);

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = UninstProc;
    wc.hInstance = inst;
    wc.hIcon = LoadIconA(inst, MAKEINTRESOURCEA(101));
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "NeuroBenchUninst";
    RegisterClassA(&wc);

    h = CreateWindowExA(WS_EX_DLGMODALFRAME, "NeuroBenchUninst",
                        TS(SM_SHORT_NAME " 卸载",
           "Uninstall " SM_SHORT_NAME_EN),
                        WS_POPUP | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
                        420, 300, NULL, NULL, inst, NULL);
    if (!h) return 1;
    g_win = h;

    c = CreateWindowExA(0, "EDIT",
        "卸载程序将删除下列内容：\r\n\r\n"
        "  · 安装文件夹中的全部程序文件（含帮助文件与根证书）\r\n"
        "  · 「开始」菜单与桌面上的快捷方式\r\n"
        "  · 本软件在注册表中登记的卸载信息\r\n\r\n"
        "您自己的对话记录与文本文件不会被删除。\r\n\r\n"
        "要继续吗？",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_LEFT,
        12, 54, 380, 150, h, (HMENU)IDC_INFO, inst, NULL);
    SendMessageA(c, WM_SETFONT, (WPARAM)g_font, MAKELPARAM(TRUE, 0));
    g_info = c;

    g_prog = CreateWindowExA(0, PROGRESS_CLASSA, NULL,
        WS_CHILD | PBM_SETPOS, 12, 120, 380, 18, h, (HMENU)IDC_PROG, inst, NULL);
    SendMessageA(g_prog, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    g_step = CreateWindowExA(0, "STATIC", TS("正在准备...",
           "Preparing..."),
        WS_CHILD | SS_LEFT, 12, 100, 380, 18, h, (HMENU)IDC_STEP, inst, NULL);
    SendMessageA(g_step, WM_SETFONT, (WPARAM)g_font, MAKELPARAM(TRUE, 0));

    c = CreateWindowExA(0, "BUTTON", TS("卸载(&U)",
           "&Uninstall"), WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                        BS_DEFPUSHBUTTON, 208, 220, 84, 27, h, (HMENU)IDC_GO, inst, NULL);
    SendMessageA(c, WM_SETFONT, (WPARAM)g_font, MAKELPARAM(TRUE, 0));
    c = CreateWindowExA(0, "BUTTON", TS("退出(&X)",
           "E&xit"), WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        300, 220, 84, 27, h, (HMENU)IDC_QUIT, inst, NULL);
    SendMessageA(c, WM_SETFONT, (WPARAM)g_font, MAKELPARAM(TRUE, 0));

    sm_center_window(h);
    ShowWindow(h, show);
    UpdateWindow(h);
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageA(h, &msg)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
    }
    return 0;
}
