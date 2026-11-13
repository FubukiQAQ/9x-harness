/* =====================================================================
 * NeuroBench 2003 - 产品序列号(CDK)生成器
 * 仅供发行商内部使用（OEM 工具）。它会生成与主程序校验算法一致的序列号。
 * ===================================================================== */
#include "../common/sm.h"
#include "../common/nb_lang.h"
#include <commctrl.h>
#include <commdlg.h>

#define IDC_EDITION   2001
#define IDC_SEATS     2002
#define IDC_SEATS_SP  2003
#define IDC_DAYS      2004
#define IDC_COUNT     2005
#define IDC_SERIAL    2006
#define IDC_OUTPUT    2007
#define IDC_GEN       2008
#define IDC_SAVE      2009
#define IDC_COPY      2010
#define IDC_CLOSE     2011
#define IDC_BATCH     2012

static HINSTANCE g_inst;
static HFONT g_font, g_fontBold, g_fontMono;

static HWND mk(HWND p, const char *cls, const char *txt, DWORD style,
               int x, int y, int w, int h, int id, HFONT f)
{
    HWND c = CreateWindowExA(0, cls, txt, WS_CHILD | WS_VISIBLE | style,
                             x, y, w, h, p, (HMENU)(INT_PTR)id, g_inst, NULL);
    if (c && f) SendMessageA(c, WM_SETFONT, (WPARAM)f, MAKELPARAM(TRUE, 0));
    return c;
}

static void get_text(HWND h, char *buf, int n)
{
    GetWindowTextA(h, buf, n);
    sm_trim(buf);
}

static void do_generate(HWND h)
{
    HWND out = GetDlgItem(h, IDC_OUTPUT);
    HWND cb = GetDlgItem(h, IDC_EDITION);
    char buf[64];
    int edition, seats, days, count, serial, i;
    Sb b;

    edition = (int)SendMessageA(cb, CB_GETCURSEL, 0, 0) + 1;
    if (edition < 1 || edition > 3) edition = 1;

    get_text(GetDlgItem(h, IDC_SEATS), buf, sizeof(buf));
    seats = (int)sm_to_long(buf, 1);
    if (seats < 1) seats = 1;
    if (seats > 32) seats = 32;
    _snprintf(buf, sizeof(buf), "%d", seats);
    SetWindowTextA(GetDlgItem(h, IDC_SEATS), buf);

    get_text(GetDlgItem(h, IDC_DAYS), buf, sizeof(buf));
    days = (int)sm_to_long(buf, 0);
    if (days < 0) days = 0;
    if (days > 511) days = 511;

    get_text(GetDlgItem(h, IDC_COUNT), buf, sizeof(buf));
    count = (int)sm_to_long(buf, 1);
    if (count < 1) count = 1;
    if (count > 200) count = 200;
    _snprintf(buf, sizeof(buf), "%d", count);
    SetWindowTextA(GetDlgItem(h, IDC_COUNT), buf);

    get_text(GetDlgItem(h, IDC_SERIAL), buf, sizeof(buf));
    serial = (int)sm_to_long(buf, 1);

    sb_init(&b);
    sb_puts(&b, "; " SM_PRODUCT_NAME " - 产品序列号清单\r\n");
    {
        char ts[40];
        char edn[32];
        sm_now_str(ts, sizeof(ts));
        sm_license_edition_name(edition, edn, sizeof(edn));
        sb_printf(&b, TS("; 生成时间: %s\r\n",
           "; Generated: %s\r\n"), ts);
        sb_printf(&b, TS("; 版本: %s   用户数: %d   有效天数: %d\r\n",
           "; Edition: %s   Seats: %d   Valid days: %d\r\n"),
                  edn, seats, days);
        sb_printf(&b, TS("; 数量: %d\r\n\r\n",
           "; Quantity: %d\r\n\r\n"), count);
    }
    for (i = 0; i < count; i++) {
        char key[40];
        int ed, se, dy;
        unsigned int sn;
        sm_cdk_generate(edition, seats, days, (unsigned int)(serial + i), key);
        sm_cdk_verify(key, &ed, &se, &dy, &sn);
        sb_printf(&b, "%s\r\n", key);
    }
    SetWindowTextA(out, sb_str(&b));
    sb_free(&b);

    _snprintf(buf, sizeof(buf), "%d", serial + count);
    SetWindowTextA(GetDlgItem(h, IDC_SERIAL), buf);
}

LRESULT CALLBACK KeygenProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_CREATE: {
        HWND cb;
        mk(h, "BUTTON", TS("生成参数(&P)",
           "Generation &parameters"), BS_GROUPBOX, 8, 6, 440, 150, 0, g_font);
        mk(h, "STATIC", TS("产品版本(&V)：",
           "&Edition:"), SS_LEFT, 20, 28, 90, 18, 0, g_font);
        cb = mk(h, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
                116, 26, 140, 200, IDC_EDITION, g_font);
        SendMessageA(cb, CB_ADDSTRING, 0, (LPARAM)TS("标准版 (1 用户)",
           "Standard (1 seat)"));
        SendMessageA(cb, CB_ADDSTRING, 0, (LPARAM)TS("专业版",
           "Professional"));
        SendMessageA(cb, CB_ADDSTRING, 0, (LPARAM)TS("企业版",
           "Enterprise"));
        SendMessageA(cb, CB_SETCURSEL, 0, 0);

        mk(h, "STATIC", TS("用户数(&S)：",
           "&Seats:"), SS_LEFT, 278, 28, 70, 18, 0, g_font);
        mk(h, "EDIT", "1", ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP,
           348, 26, 46, 21, IDC_SEATS, g_font);
        mk(h, "msctls_updown32", "",
           UDS_SETBUDDYINT | UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_NOTHOUSANDS,
           0, 0, 0, 0, IDC_SEATS_SP, g_font);

        mk(h, "STATIC", TS("有效天数(&D)：",
           "Valid &days:"), SS_LEFT, 20, 56, 90, 18, 0, g_font);
        mk(h, "EDIT", "0", ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP,
           116, 54, 60, 21, IDC_DAYS, g_font);
        mk(h, "STATIC", TS("（0 表示永久授权，最大 511）",
           "(0 = perpetual, max 511)"), SS_LEFT, 184, 56, 240, 18, 0, g_font);

        mk(h, "STATIC", TS("生成数量(&N)：",
           "&Quantity:"), SS_LEFT, 20, 84, 90, 18, 0, g_font);
        mk(h, "EDIT", "1", ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP,
           116, 82, 60, 21, IDC_COUNT, g_font);
        mk(h, "STATIC", TS("（一次最多 200 个）",
           "(max 200 at a time)"), SS_LEFT, 184, 84, 200, 18, 0, g_font);

        mk(h, "STATIC", TS("起始序号(&I)：",
           "Starting &serial:"), SS_LEFT, 20, 112, 90, 18, 0, g_font);
        mk(h, "EDIT", "1", ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP,
           116, 110, 90, 21, IDC_SERIAL, g_font);
        mk(h, "STATIC", TS("（仅用于内部记录，不参与校验）",
           "(internal record only, not validated)"), SS_LEFT, 214, 112, 230, 18, 0, g_font);

        mk(h, "STATIC", TS("生成结果(&R)：",
           "&Results:"), SS_LEFT, 8, 162, 100, 18, 0, g_font);
        mk(h, "EDIT", "",
           ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_BORDER | ES_LEFT,
           8, 182, 440, 190, IDC_OUTPUT, g_fontMono);

        mk(h, "BUTTON", TS("生成(&G)",
           "&Generate"), BS_DEFPUSHBUTTON | WS_TABSTOP, 8, 380, 90, 26, IDC_GEN, g_font);
        mk(h, "BUTTON", TS("保存到文件(&F)...",
           "Save to &file..."), BS_PUSHBUTTON | WS_TABSTOP, 104, 380, 120, 26, IDC_SAVE, g_font);
        mk(h, "BUTTON", TS("复制(&C)",
           "&Copy"), BS_PUSHBUTTON | WS_TABSTOP, 230, 380, 80, 26, IDC_COPY, g_font);
        mk(h, "BUTTON", TS("退出(&X)",
           "E&xit"), BS_PUSHBUTTON | WS_TABSTOP, 368, 380, 80, 26, IDC_CLOSE, g_font);

        mk(h, "STATIC", TS("内部工具 · 请勿外传 · " SM_COPYRIGHT,
           "Internal tool - do not distribute - " SM_COPYRIGHT),
           SS_LEFT, 8, 412, 440, 16, 0, g_font);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(w)) {
        case IDC_GEN:
            do_generate(h);
            return 0;
        case IDC_SAVE: {
            OPENFILENAMEA ofn;
            char path[MAX_PATH] = "CDKEY.TXT";
            int n;
            char *txt;
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = h;
            ofn.lpstrFilter = "文本文件 (*.txt)\0*.txt\0所有文件 (*.*)\0*.*\0";
            ofn.lpstrFile = path;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrDefExt = "txt";
            ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
            ofn.lpstrTitle = TS("保存序列号清单",
           "Save key list");
            if (!GetSaveFileNameA(&ofn)) return 0;
            n = GetWindowTextLengthA(GetDlgItem(h, IDC_OUTPUT));
            txt = (char *)sm_alloc((size_t)n + 2);
            GetWindowTextA(GetDlgItem(h, IDC_OUTPUT), txt, n + 1);
            if (!sm_write_file(path, txt, (long)strlen(txt)))
                MessageBoxA(h, TS("写入文件失败。",
           "Could not write the file."), TS("生成器",
           "Key generator"), MB_ICONSTOP | MB_OK);
            sm_free(txt);
            return 0;
        }
        case IDC_COPY: {
            int n = GetWindowTextLengthA(GetDlgItem(h, IDC_OUTPUT));
            char *txt = (char *)sm_alloc((size_t)n + 2);
            GetWindowTextA(GetDlgItem(h, IDC_OUTPUT), txt, n + 1);
            if (OpenClipboard(h)) {
                HGLOBAL hg;
                EmptyClipboard();
                hg = GlobalAlloc(GMEM_MOVEABLE, strlen(txt) + 1);
                if (hg) {
                    char *d = (char *)GlobalLock(hg);
                    strcpy(d, txt);
                    GlobalUnlock(hg);
                    SetClipboardData(CF_TEXT, hg);
                }
                CloseClipboard();
            }
            sm_free(txt);
            return 0;
        }
        case IDC_CLOSE:
            DestroyWindow(h);
            return 0;
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT r;
        GetClientRect(h, &r);
        FillRect(dc, &r, (HBRUSH)(COLOR_BTNFACE + 1));
        EndPaint(h, &ps);
        return 0;
    }
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
    HWND h;
    MSG msg;
    INITCOMMONCONTROLSEX icc;
    LOGFONTA lf;

    (void)prev; (void)cmd;
    g_inst = inst;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_UPDOWN_CLASS | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    ZeroMemory(&lf, sizeof(lf));
    lf.lfHeight = -12;
    strcpy(lf.lfFaceName, "宋体");
    lf.lfCharSet = DEFAULT_CHARSET;
    g_font = CreateFontIndirectA(&lf);
    lf.lfWeight = FW_BOLD;
    g_fontBold = CreateFontIndirectA(&lf);
    lf.lfWeight = FW_NORMAL;
    strcpy(lf.lfFaceName, "新宋体");
    g_fontMono = CreateFontIndirectA(&lf);
    if (!g_font) g_font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    if (!g_fontMono) g_fontMono = g_font;

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = KeygenProc;
    wc.hInstance = inst;
    wc.hIcon = LoadIconA(inst, MAKEINTRESOURCEA(101));
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "NeuroBenchKeygen";
    if (!RegisterClassA(&wc)) return 1;

    h = CreateWindowExA(WS_EX_DLGMODALFRAME, "NeuroBenchKeygen",
        TS(SM_PRODUCT_NAME " 产品序列号生成器 [OEM 内部工具]",
           SM_PRODUCT_NAME_EN " Product Key Generator [OEM internal]"),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 470, 470, NULL, NULL, inst, NULL);
    if (!h) return 1;
    ShowWindow(h, show);
    UpdateWindow(h);

    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageA(h, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    return 0;
}
