/* =====================================================================
 * NeuroBench 2003 - 根证书更新程序 CERTUPD.EXE
 * 用来更新 CERTS 目录中的根证书包（服务器证书不受信任时运行它）。
 * ===================================================================== */
#include "../common/sm.h"
#include "../common/nb_lang.h"
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>

#define IDC_TEXT   5001
#define IDC_IMPORT 5002
#define IDC_TEST   5003
#define IDC_OPEN   5004
#define IDC_QUIT   5005

static HINSTANCE g_inst;
static HWND g_win, g_text;
static HFONT g_font, g_mono, g_fontB;
static int g_caCount, g_caState;

/* 与 sm_tls.c 中一致的加载逻辑做一次统计（通过临时连接触发太慢，这里直接数文件） */
static void scan_certs(int *files, long *bytes)
{
    char *dir = sm_app_dir();
    char *cd = sm_path_join(dir, "CERTS");
    char *pat = sm_path_join(cd, "*.*");
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pat, &fd);
    int n = 0;
    long b = 0;
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            n++;
            b += (long)fd.nFileSizeLow;
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    sm_free(pat);
    sm_free(cd);
    sm_free(dir);
    *files = n;
    *bytes = b;
}

static void refresh(void)
{
    Sb b;
    int files;
    long bytes;
    char ts[40];

    scan_certs(&files, &bytes);
    sb_init(&b);
    sm_now_str(ts, sizeof(ts));
    sb_printf(&b, "根证书更新程序    %s\r\n\r\n", ts);
    sb_printf(&b, "证书目录：%s\r\n", "/");
    {
        char *d = sm_app_dir();
        char *cd = sm_path_join(d, "CERTS");
        sb_printf(&b, "           %s\r\n", cd);
        sm_free(cd);
        sm_free(d);
    }
    sb_printf(&b, "证书文件：%d 个，共 %ld 字节\r\n\r\n", files, bytes);

    sb_puts(&b,
        "什么时候需要运行本程序？\r\n"
        "  · 主程序提示“服务器证书不受信任（缺少根证书）”；\r\n"
        "  · 服务器更换了新的证书颁发机构；\r\n"
        "  · 刚重装过系统、或从别处拷贝了程序目录。\r\n\r\n"
        "使用方法：\r\n"
        "  1. 把随光盘或经销商提供的证书包（*.pem / *.crt）放到本目录的\r\n"
        "     CERTS 文件夹里，文件名任意；\r\n"
        "  2. 或者按下面的“从文件导入”，选择一个证书包，程序会自动复制到\r\n"
        "     CERTS 文件夹并命名为 ROOTS.PEM；\r\n"
        "  3. 按“测试连接”确认能否与服务器建立安全连接。\r\n\r\n"
        "提示：本程序只负责管理证书文件。证书是公开信息，不包含任何私钥。\r\n");
    SetWindowTextA(g_text, sb_str(&b));
    sb_free(&b);
    (void)g_caCount; (void)g_caState;
}

static void do_import(void)
{
    OPENFILENAMEA ofn;
    char path[MAX_PATH];
    char *src = NULL;
    long len = 0;
    path[0] = 0;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_win;
    ofn.lpstrFilter = "证书文件 (*.pem;*.crt;*.cer)\0*.pem;*.crt;*.cer\0所有文件 (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = TS("选择根证书包",
           "Select a root certificate bundle");
    if (!GetOpenFileNameA(&ofn)) return;
    src = sm_read_file(path, 8 * 1024 * 1024, &len);
    if (!src) {
        MessageBoxA(g_win, TS("无法读取所选文件。",
           "Could not read the selected file."), "证书更新", MB_ICONSTOP | MB_OK);
        return;
    }
    if (strstr(src, "-----BEGIN")) {
        char *d = sm_app_dir();
        char *cd = sm_path_join(d, "CERTS");
        char *dst;
        sm_mkdirs(cd);
        dst = sm_path_join(cd, "ROOTS.PEM");
        if (sm_write_file(dst, src, len))
            MessageBoxA(g_win, TS("根证书包已更新。\r\n\r\n请重新启动主程序后再试。",
           "The root certificate bundle has been updated.\r\n\r\nRestart the main program and try again."),
                        "证书更新", MB_ICONINFORMATION | MB_OK);
        else
            MessageBoxA(g_win, TS("写入证书文件失败，请检查磁盘空间或权限。",
           "Could not write the certificate file. Check disk space and permissions."),
                        "证书更新", MB_ICONSTOP | MB_OK);
        sm_free(dst);
        sm_free(cd);
        sm_free(d);
    } else {
        MessageBoxA(g_win,
            "所选文件不是 PEM 格式的证书包（应当包含 -----BEGIN CERTIFICATE-----）。\r\n\r\n"
            "如果您手上是 .crt/.cer 单证书文件（DER 二进制格式），"
            "请直接把它复制到 CERTS 文件夹中即可。",
            "证书更新", MB_ICONEXCLAMATION | MB_OK);
    }
    sm_free(src);
    refresh();
}

static void do_test(void)
{
    SmConfig c;
    char rep[4096];
    HCURSOR old = SetCursor(LoadCursorA(NULL, IDC_WAIT));
    SetWindowTextA(g_text, TS("正在测试与服务器的安全连接，请稍候...\r\n",
           "Testing the secure connection to the server, please wait...\r\n"));
    UpdateWindow(g_text);
    sm_config_load(&c);
    if (sm_tls_test(c.endpoint, c.apikey, 30, 1, rep, sizeof(rep)))
        MessageBoxA(g_win, rep, TS("连接测试结果",
           "Connection test"), MB_ICONINFORMATION | MB_OK);
    else
        MessageBoxA(g_win, rep, TS("连接测试结果",
           "Connection test"), MB_ICONWARNING | MB_OK);
    SetCursor(old);
    refresh();
}

static LRESULT CALLBACK CertProc(HWND h, UINT m, WPARAM w, LPARAM l)
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
            hr.left = 0; hr.top = 0; hr.right = r.right; hr.bottom = 40;
            FillRect(dc, &hr, CreateSolidBrush(RGB(0, 0, 128)));
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(255, 255, 255));
            SelectObject(dc, g_fontB);
            TextOutA(dc, 10, 8, TS("根证书更新程序",
           "Root Certificate Update"), 14);
            SelectObject(dc, g_font);
            TextOutA(dc, 12, 24, TS(SM_PRODUCT_NAME " 附加组件",
        SM_PRODUCT_NAME_EN " add-on"), 22);
        }
        EndPaint(h, &ps);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(w) == IDC_IMPORT) { do_import(); return 0; }
        if (LOWORD(w) == IDC_TEST) { do_test(); return 0; }
        if (LOWORD(w) == IDC_OPEN) {
            char *d = sm_app_dir();
            char *cd = sm_path_join(d, "CERTS");
            sm_mkdirs(cd);
            ShellExecuteA(h, "open", cd, NULL, NULL, SW_SHOWNORMAL);
            sm_free(cd);
            sm_free(d);
            return 0;
        }
        if (LOWORD(w) == IDC_QUIT) { DestroyWindow(h); return 0; }
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

    (void)prev; (void)cmd;
    g_inst = inst;
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
    wc.lpfnWndProc = CertProc;
    wc.hInstance = inst;
    wc.hIcon = LoadIconA(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "NeuroBenchCertUpd";
    RegisterClassA(&wc);

    h = CreateWindowExA(WS_EX_DLGMODALFRAME, "NeuroBenchCertUpd", TS("根证书更新程序",
           "Root Certificate Update"),
                        WS_POPUP | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
                        520, 400, NULL, NULL, inst, NULL);
    if (!h) return 1;
    g_win = h;

    g_text = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_LEFT,
        10, 48, 496, 300, h, (HMENU)IDC_TEXT, inst, NULL);
    SendMessageA(g_text, WM_SETFONT, (WPARAM)g_mono, MAKELPARAM(TRUE, 0));

    c = CreateWindowExA(0, "BUTTON", TS("从文件导入(&I)...",
           "&Import from file..."), WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        10, 356, 120, 26, h, (HMENU)IDC_IMPORT, inst, NULL);
    SendMessageA(c, WM_SETFONT, (WPARAM)g_font, MAKELPARAM(TRUE, 0));
    c = CreateWindowExA(0, "BUTTON", TS("测试连接(&T)",
           "&Test connection"), WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        138, 356, 100, 26, h, (HMENU)IDC_TEST, inst, NULL);
    SendMessageA(c, WM_SETFONT, (WPARAM)g_font, MAKELPARAM(TRUE, 0));
    c = CreateWindowExA(0, "BUTTON", TS("打开证书文件夹(&O)",
           "&Open certificates folder"), WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        246, 356, 140, 26, h, (HMENU)IDC_OPEN, inst, NULL);
    SendMessageA(c, WM_SETFONT, (WPARAM)g_font, MAKELPARAM(TRUE, 0));
    c = CreateWindowExA(0, "BUTTON", TS("关闭",
           "Close"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                        426, 356, 80, 26, h, (HMENU)IDC_QUIT, inst, NULL);
    SendMessageA(c, WM_SETFONT, (WPARAM)g_font, MAKELPARAM(TRUE, 0));

    refresh();
    sm_center_window(h);
    ShowWindow(h, show);
    UpdateWindow(h);
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageA(h, &msg)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
    }
    return 0;
}
