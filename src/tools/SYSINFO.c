/* =====================================================================
 * NeuroBench 2003 - 系统信息报告工具 SYSINFO.EXE（附加组件）
 * ===================================================================== */
#include "../common/sm.h"
#include "../common/nb_lang.h"
#include <commctrl.h>
#include <commdlg.h>

#define IDC_TEXT 6001
#define IDC_COPY 6002
#define IDC_SAVE 6003
#define IDC_QUIT 6004

static HFONT g_font, g_mono, g_fontB;
static HWND g_win, g_text;

static void build_report(Sb *b)
{
    char *os = sm_get_os_name();
    char user[128] = "", comp[128] = "";
    DWORD n;
    ULARGE_INTEGER fb, tb;
    HDC dc;
    char *v;

    sb_printf(b, SM_PRODUCT_NAME " 系统信息报告\r\n");
    {
        char ts[40];
        sm_now_str(ts, sizeof(ts));
        sb_printf(b, "生成时间：%s\r\n", ts);
    }
    sb_puts(b, "------------------------------------------------------------\r\n");
    sb_printf(b, "操作系统        %s\r\n", os);
    sm_free(os);
    sb_printf(b, "系统代码页      %u\r\n", sm_system_codepage());
    n = sizeof(user);
    if (GetUserNameA(user, &n)) sb_printf(b, "当前用户        %s\r\n", user);
    n = sizeof(comp);
    if (GetComputerNameA(comp, &n)) sb_printf(b, "计算机名        %s\r\n", comp);

    v = sm_reg_get_sz(HKEY_LOCAL_MACHINE,
            "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "ProcessorNameString");
    /* Windows 9x 没有 ProcessorNameString，只有 Identifier（如
     * "GenuineIntel" / "x86 Family 5 Model 2 Stepping 12"）。 */
    if (!v)
        v = sm_reg_get_sz(HKEY_LOCAL_MACHINE,
                "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "Identifier");
    if (v) {
        sm_trim(v);
        sb_printf(b, "处理器          %s\r\n", v);
        sm_free(v);
    }

    {
        unsigned long mt = 0, ma = 0, ml = 0;
        if (sm_memory_info(&mt, &ma, &ml))
            sb_printf(b, "物理内存        %lu MB（可用 %lu MB）\r\n", mt, ma);
    }

    if (GetDiskFreeSpaceExA("C:\\", &fb, &tb, NULL))
        sb_printf(b, "C 盘            共 %.0f MB，可用 %.0f MB\r\n",
                  (double)tb.QuadPart / 1048576.0, (double)fb.QuadPart / 1048576.0);

    dc = GetDC(NULL);
    if (dc) {
        sb_printf(b, "显示            %d x %d，%d 位色\r\n",
                  GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
                  GetDeviceCaps(dc, BITSPIXEL));
        ReleaseDC(NULL, dc);
    }
    v = sm_reg_get_sz(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Internet Explorer", "Version");
    if (v) { sb_printf(b, "Internet Explorer  %s\r\n", v); sm_free(v); }

    sb_puts(b, "------------------------------------------------------------\r\n");
    sb_puts(b, SM_PRODUCT_NAME " 版本  " SM_VERSION_STR "\r\n");
    {
        char *d = sm_app_dir();
        sb_printf(b, "安装目录        %s\r\n", d);
        sm_free(d);
        d = sm_appdata_dir();
        sb_printf(b, "数据目录        %s\r\n", d);
        sm_free(d);
    }
    sb_puts(b, "------------------------------------------------------------\r\n");
    sb_puts(b, "如需技术支持，请把本报告另存为 TXT 文件后发送给经销商。\r\n");
}

static LRESULT CALLBACK SiProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT r;
        GetClientRect(h, &r);
        FillRect(dc, &r, (HBRUSH)(COLOR_BTNFACE + 1));
        EndPaint(h, &ps);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(w) == IDC_QUIT) { DestroyWindow(h); return 0; }
        if (LOWORD(w) == IDC_COPY) {
            int n = GetWindowTextLengthA(g_text);
            char *t = (char *)sm_alloc((size_t)n + 2);
            GetWindowTextA(g_text, t, n + 1);
            if (OpenClipboard(h)) {
                HGLOBAL hg;
                EmptyClipboard();
                hg = GlobalAlloc(GMEM_MOVEABLE, strlen(t) + 1);
                if (hg) {
                    char *d = (char *)GlobalLock(hg);
                    strcpy(d, t);
                    GlobalUnlock(hg);
                    SetClipboardData(CF_TEXT, hg);
                }
                CloseClipboard();
            }
            sm_free(t);
            return 0;
        }
        if (LOWORD(w) == IDC_SAVE) {
            OPENFILENAMEA ofn;
            char path[MAX_PATH] = "SYSINFO.TXT";
            int n;
            char *t;
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = h;
            ofn.lpstrFilter = "文本文件 (*.txt)\0*.txt\0所有文件 (*.*)\0*.*\0";
            ofn.lpstrFile = path;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrDefExt = "txt";
            ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
            if (!GetSaveFileNameA(&ofn)) return 0;
            n = GetWindowTextLengthA(g_text);
            t = (char *)sm_alloc((size_t)n + 2);
            GetWindowTextA(g_text, t, n + 1);
            if (!sm_write_file(path, t, (long)strlen(t)))
                MessageBoxA(h, TS("写入文件失败。",
           "Could not write the file."), "系统信息", MB_ICONSTOP | MB_OK);
            sm_free(t);
            return 0;
        }
        break;
    case WM_CLOSE:
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
    LOGFONTA lf;
    Sb b;
    char *txt;

    (void)prev; (void)cmd;
    ZeroMemory(&lf, sizeof(lf));
    lf.lfHeight = -12;
    lf.lfCharSet = DEFAULT_CHARSET;
    strcpy(lf.lfFaceName, "宋体");
    g_font = CreateFontIndirectA(&lf);
    lf.lfWeight = FW_BOLD;
    g_fontB = CreateFontIndirectA(&lf);
    lf.lfWeight = FW_NORMAL;
    strcpy(lf.lfFaceName, "新宋体");
    g_mono = CreateFontIndirectA(&lf);

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = SiProc;
    wc.hInstance = inst;
    wc.hIcon = LoadIconA(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "NeuroBenchSysInfo";
    RegisterClassA(&wc);

    h = CreateWindowExA(WS_EX_DLGMODALFRAME, "NeuroBenchSysInfo", TS("系统信息报告",
           "System Information Report"),
                        WS_POPUP | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
                        520, 492, NULL, NULL, inst, NULL);
    if (!h) return 1;
    g_win = h;

    sb_init(&b);
    build_report(&b);
    txt = sb_take(&b);
    g_text = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", txt,
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_LEFT,
        10, 10, 496, 414, h, (HMENU)IDC_TEXT, inst, NULL);
    SendMessageA(g_text, WM_SETFONT, (WPARAM)g_mono, MAKELPARAM(TRUE, 0));
    sm_free(txt);

    c = CreateWindowExA(0, "BUTTON", TS("复制(&C)",
           "&Copy"), WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        10, 434, 90, 26, h, (HMENU)IDC_COPY, inst, NULL);
    SendMessageA(c, WM_SETFONT, (WPARAM)g_font, MAKELPARAM(TRUE, 0));
    c = CreateWindowExA(0, "BUTTON", TS("另存为(&S)...",
           "Save &As..."), WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        108, 434, 110, 26, h, (HMENU)IDC_SAVE, inst, NULL);
    SendMessageA(c, WM_SETFONT, (WPARAM)g_font, MAKELPARAM(TRUE, 0));
    c = CreateWindowExA(0, "BUTTON", TS("关闭",
           "Close"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                        426, 434, 80, 26, h, (HMENU)IDC_QUIT, inst, NULL);
    SendMessageA(c, WM_SETFONT, (WPARAM)g_font, MAKELPARAM(TRUE, 0));

    sm_center_window(h);
    ShowWindow(h, show);
    UpdateWindow(h);
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageA(h, &msg)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
    }
    return 0;
}
