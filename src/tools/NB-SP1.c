/* =====================================================================
 * NeuroBench 2003 服务包 1 (NB-SP1.EXE)
 *
 * 这是一个“补丁 / 附加软件”，随工具光盘单独发行。
 * 它从同目录的 SP1.CAB 中取出更新文件，写入已经安装的 NeuroBench 目录，
 * 并更新证书包与帮助文件，最后在 NEBENCH.INI 中记录服务包版本。
 * ===================================================================== */
#include "../common/sm.h"
#include "../common/nb_lang.h"
#include "../common/sm_arc.h"
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>

#define IDC_TEXT  7001
#define IDC_DIR   7002
#define IDC_BROWSE 7003
#define IDC_GO    7004
#define IDC_QUIT  7005
#define IDC_PROG  7006
#define IDC_STEP  7007

static HINSTANCE g_inst;
static HWND g_win, g_text, g_dir, g_prog, g_step, g_go;
static HFONT g_font, g_fontB, g_mono;
static int g_done = 0;
static char g_target[MAX_PATH];

static void pump(void)
{
    MSG m;
    while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&m);
        DispatchMessage(&m);
    }
}

static int find_installed_dir(char *out, int n)
{
    char *v = sm_reg_get_sz(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" SM_SHORT_NAME "",
        "InstallLocation");
    if (v) {
        char *probe = sm_path_join(v, SM_EXE_NAME);
        int ok = sm_file_exists(probe);
        if (ok) {
            strncpy(out, v, (size_t)n - 1);
            out[n - 1] = 0;
        }
        sm_free(probe);
        sm_free(v);
        return ok;
    }
    return 0;
}

static int prog_cb(void *ctx, int idx, int total, const char *name)
{
    char b[MAX_PATH + 80];
    (void)ctx;
    _snprintf(b, sizeof(b), TS("正在更新：%s",
           "Updating: %s"), name);
    b[sizeof(b) - 1] = 0;
    if (g_step) SetWindowTextA(g_step, b);
    if (g_prog) SendMessageA(g_prog, PBM_SETPOS, (WPARAM)(total ? idx * 100 / total : 0), 0);
    pump();
    Sleep(120);
    return 0;
}

static void do_patch(void)
{
    SmArc arc;
    char *exeDir = sm_exe_dir();
    char *cab = sm_path_join(exeDir, "SP1.CAB");
    char *ini;
    int ok;

    GetWindowTextA(g_dir, g_target, sizeof(g_target));
    sm_trim(g_target);
    if (!g_target[0]) {
        MessageBoxA(g_win, TS("请先指定 " SM_SHORT_NAME " 的安装目录。",
           "Please specify the " SM_SHORT_NAME " installation folder first."),
                    TS("服务包",
           "Service Pack"), MB_ICONSTOP | MB_OK);
        return;
    }
    {
        char *probe = sm_path_join(g_target, SM_EXE_NAME);
        if (!sm_file_exists(probe)) {
            char m[MAX_PATH + 200];
            _snprintf(m, sizeof(m),
                      "在下面的文件夹中没有找到 NEUROBENCH.EXE：\r\n\r\n%s\r\n\r\n"
                      "请确认这是 " SM_SHORT_NAME " 的安装目录。", g_target);
            m[sizeof(m) - 1] = 0;
            MessageBoxA(g_win, m, TS("服务包",
           "Service Pack"), MB_ICONSTOP | MB_OK);
            sm_free(probe);
            return;
        }
        sm_free(probe);
    }
    if (!sm_arc_open(cab, &arc)) {
        MessageBoxA(g_win, arc.error, TS("服务包",
           "Service Pack"), MB_ICONSTOP | MB_OK);
        sm_free(cab); sm_free(exeDir);
        return;
    }
    if (g_step) SetWindowTextA(g_step, "正在更新文件...");
    pump();
    ok = sm_arc_extract(&arc, g_target, prog_cb, NULL);
    sm_arc_close(&arc);
    sm_free(cab);

    if (ok) {
        ini = sm_path_join(g_target, SM_INI_NAME);
        sm_ini_set(ini, "NeuroBench", "ServicePack", "1");
        sm_ini_set(ini, "NeuroBench", "Version", "3.01.2003");
        sm_free(ini);
        if (g_prog) SendMessageA(g_prog, PBM_SETPOS, 100, 0);
        if (g_step) SetWindowTextA(g_step, "服务包 1 安装完成。");
        pump();
        g_done = 1;
        MessageBoxA(g_win,
            "" SM_SHORT_NAME " 服务包 1 已成功安装。\r\n\r\n"
            "本次更新包括：\r\n"
            "  · 更新根证书包（CERTS\\ROOTS.PEM），支持更多服务器；\r\n"
            "  · 更新帮助文件 " SM_EXE_BASE ".CHM；\r\n"
            "  · 新增系统信息报告工具 SYSINFO.EXE；\r\n"
            "  · 版本号更新为 3.01.2003。\r\n\r\n"
            "请重新启动 " SM_SHORT_NAME " 使更新生效。",
            "服务包 1", MB_ICONINFORMATION | MB_OK);
        DestroyWindow(g_win);
    } else {
        MessageBoxA(g_win, "更新文件时出错，服务包未能完整安装。\r\n\r\n"
                           "请关闭所有正在运行的程序后重试。",
                    TS("服务包",
           "Service Pack"), MB_ICONSTOP | MB_OK);
    }
    sm_free(exeDir);
}

static LRESULT CALLBACK SpProc(HWND h, UINT m, WPARAM w, LPARAM l)
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
            hr.left = 0; hr.top = 0; hr.right = r.right; hr.bottom = 52;
            FillRect(dc, &hr, CreateSolidBrush(RGB(0, 0, 128)));
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(255, 255, 255));
            SelectObject(dc, g_fontB);
            {
            const char *ht = TS("" SM_SHORT_NAME " 服务包 1",
                                "" SM_PRODUCT_NAME " Service Pack 1");
            TextOutA(dc, 12, 8, ht, (int)strlen(ht));
        }
            SelectObject(dc, g_mono);
            SetTextColor(dc, RGB(200, 205, 240));
            TextOutA(dc, 14, 30, "SERVICE PACK 1  /  Build 3.01.2003",
                 (int)strlen("SERVICE PACK 1  /  Build 3.01.2003"));
        }
        EndPaint(h, &ps);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(w) == IDC_QUIT) {
            if (!g_done && MessageBoxA(h, TS("服务包尚未安装，确实要退出吗？",
           "Service Pack 1 is not installed. Really quit?"), "服务包 1",
                                       MB_ICONQUESTION | MB_YESNO) != IDYES) return 0;
            DestroyWindow(h);
            return 0;
        }
        if (LOWORD(w) == IDC_BROWSE) {
            BROWSEINFOA bi;
            LPITEMIDLIST pidl;
            char disp[MAX_PATH];
            ZeroMemory(&bi, sizeof(bi));
            bi.hwndOwner = h;
            bi.lpszTitle = TS("请选择 " SM_SHORT_NAME " 的安装文件夹",
           "Select the " SM_SHORT_NAME " installation folder");
            bi.ulFlags = BIF_RETURNONLYFSDIRS;
            pidl = SHBrowseForFolderA(&bi);
            if (pidl) {
                if (SHGetPathFromIDListA(pidl, disp)) SetWindowTextA(g_dir, disp);
                CoTaskMemFree(pidl);
            }
            return 0;
        }
        if (LOWORD(w) == IDC_GO) {
            EnableWindow(g_go, FALSE);
            EnableWindow(GetDlgItem(h, IDC_QUIT), FALSE);
            EnableWindow(g_dir, FALSE);
            ShowWindow(g_prog, SW_SHOW);
            ShowWindow(g_step, SW_SHOW);
            do_patch();
            return 0;
        }
        break;
    case WM_CLOSE:
        if (!g_done && MessageBoxA(h, TS("服务包尚未安装，确实要退出吗？",
           "Service Pack 1 is not installed. Really quit?"), "服务包 1",
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

    (void)prev; (void)cmd;
    g_inst = inst;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);
    OleInitialize(NULL);

    ZeroMemory(&lf, sizeof(lf));
    lf.lfHeight = -12;
    lf.lfCharSet = DEFAULT_CHARSET;
    strcpy(lf.lfFaceName, "宋体");
    g_font = CreateFontIndirectA(&lf);
    lf.lfWeight = FW_BOLD;
    lf.lfHeight = -16;
    g_fontB = CreateFontIndirectA(&lf);
    ZeroMemory(&lf, sizeof(lf));
    lf.lfHeight = -11;
    lf.lfCharSet = DEFAULT_CHARSET;
    strcpy(lf.lfFaceName, "新宋体");
    g_mono = CreateFontIndirectA(&lf);

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = SpProc;
    wc.hInstance = inst;
    wc.hIcon = LoadIconA(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "NeuroBenchSP1";
    RegisterClassA(&wc);

    h = CreateWindowExA(WS_EX_DLGMODALFRAME, "NeuroBenchSP1",
                        TS("" SM_SHORT_NAME " 服务包 1 安装程序",
           "" SM_PRODUCT_NAME " Service Pack 1 Setup"),
                        WS_POPUP | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
                        500, 330, NULL, NULL, inst, NULL);
    if (!h) return 1;
    g_win = h;

    g_text = CreateWindowExA(0, "EDIT",
        "本服务包用于更新已经安装的 " SM_PRODUCT_NAME "。\r\n\r\n"
        "更新内容：\r\n"
        "  1. 更新根证书包，兼容更多云端服务器；\r\n"
        "  2. 更新帮助文件（CHM）；\r\n"
        "  3. 新增系统信息报告工具 SYSINFO.EXE；\r\n"
        "  4. 修正若干界面细节问题。\r\n\r\n"
        "请确认下面的安装目录，然后按「安装」。",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_LEFT,
        10, 62, 480, 120, h, (HMENU)IDC_TEXT, inst, NULL);
    SendMessageA(g_text, WM_SETFONT, (WPARAM)g_font, MAKELPARAM(TRUE, 0));

    c = CreateWindowExA(0, "STATIC", TS("安装目录(&D)：",
           "Install &folder:"), WS_CHILD | WS_VISIBLE | SS_LEFT,
                        10, 190, 80, 18, h, NULL, inst, NULL);
    SendMessageA(c, WM_SETFONT, (WPARAM)g_font, MAKELPARAM(TRUE, 0));
    g_dir = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
        94, 188, 300, 21, h, (HMENU)IDC_DIR, inst, NULL);
    SendMessageA(g_dir, WM_SETFONT, (WPARAM)g_font, MAKELPARAM(TRUE, 0));
    c = CreateWindowExA(0, "BUTTON", TS("浏览(&B)...",
           "&Browse..."), WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        400, 187, 90, 24, h, (HMENU)IDC_BROWSE, inst, NULL);
    SendMessageA(c, WM_SETFONT, (WPARAM)g_font, MAKELPARAM(TRUE, 0));

    g_step = CreateWindowExA(0, "STATIC", TS("等待开始...",
           "Ready to install..."), WS_CHILD | SS_LEFT,
                             10, 222, 480, 18, h, (HMENU)IDC_STEP, inst, NULL);
    SendMessageA(g_step, WM_SETFONT, (WPARAM)g_font, MAKELPARAM(TRUE, 0));
    g_prog = CreateWindowExA(0, PROGRESS_CLASSA, NULL, WS_CHILD,
                             10, 244, 480, 16, h, (HMENU)IDC_PROG, inst, NULL);
    SendMessageA(g_prog, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

    g_go = CreateWindowExA(0, "BUTTON", TS("安装(&I)",
           "&Install"), WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                           BS_DEFPUSHBUTTON, 306, 272, 90, 27, h, (HMENU)IDC_GO, inst, NULL);
    SendMessageA(g_go, WM_SETFONT, (WPARAM)g_font, MAKELPARAM(TRUE, 0));
    c = CreateWindowExA(0, "BUTTON", TS("退出(&X)",
           "E&xit"), WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        404, 272, 86, 27, h, (HMENU)IDC_QUIT, inst, NULL);
    SendMessageA(c, WM_SETFONT, (WPARAM)g_font, MAKELPARAM(TRUE, 0));

    if (!find_installed_dir(g_target, sizeof(g_target))) {
        char *d = sm_app_dir();
        strncpy(g_target, d, MAX_PATH - 1);
        sm_free(d);
    }
    SetWindowTextA(g_dir, g_target);

    sm_center_window(h);
    ShowWindow(h, show);
    UpdateWindow(h);
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageA(h, &msg)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
    }
    OleUninitialize();
    return 0;
}
