/* =====================================================================
 * Axiom NeuroBench 2003 - 主程序 / 主窗口
 *
 * 界面布局（照 2003 年那套三栏式工作台）：
 *   +-----------------------------------------------------------+
 *   | 菜单栏                                                     |
 *   +-----------------------------------------------------------+
 *   | 按钮（工具栏）                                              |
 *   +----------+------------+----------------------------------+
 *   | 工作区搜索| 文件搜索    | 对话消息                          |
 *   | 工作区列表| 当前工作区内 |  (对话记录)                      |
 *   |          | 文件树      |  ---------------------------------|
 *   |          |            |  输入                             |
 *   +----------+------------+----------------------------------+
 *   | 其他信息（状态栏）                                          |
 *   +-----------------------------------------------------------+
 * ===================================================================== */
#include "agent.h"

App g;

static const char *SPLASH_CLASS = "NbSplash";
static const char *SPLIT_CLASS  = "NbSplit";
static HWND  g_hSplash = NULL;
static HACCEL g_hAccel = NULL;
static int   g_dpiScale = 1;
static int   g_dragSplit = 0;      /* 1=左分隔条 2=右分隔条 */
static HMENU g_hMenu = NULL;

BOOL CALLBACK SetWindowFontProc(HWND h, LPARAM l);

/* ------------------------------------------------------------------ */
/* 字体                                                                */
/* ------------------------------------------------------------------ */
HFONT sm_ui_font(int px, int bold, int mono)
{
    static const char *faces_ui[] = { "宋体", "SimSun", "MS Sans Serif", "Tahoma", NULL };
    static const char *faces_mono[] = { "新宋体", "NSimSun", "Courier New", "Fixedsys", NULL };
    int i;
    LOGFONTA lf;
    if (px <= 0) px = 12;
    ZeroMemory(&lf, sizeof(lf));
    lf.lfHeight = -px * g_dpiScale;
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = 0;
    lf.lfPitchAndFamily = mono ? (FIXED_PITCH | FF_MODERN) : (VARIABLE_PITCH | FF_SWISS);
    for (i = 0; faces_ui[i]; i++) {
        strcpy(lf.lfFaceName, mono ? faces_mono[i] : faces_ui[i]);
        if (mono && !faces_mono[i]) break;
        {
            HFONT h = CreateFontIndirectA(&lf);
            HDC dc = GetDC(NULL);
            HGDIOBJ old = SelectObject(dc, h);
            char got[LF_FACESIZE];
            GetTextFaceA(dc, LF_FACESIZE, got);
            SelectObject(dc, old);
            ReleaseDC(NULL, dc);
            if (!_stricmp(got, lf.lfFaceName)) return h;
            DeleteObject(h);
        }
    }
    strcpy(lf.lfFaceName, "宋体");
    return CreateFontIndirectA(&lf);
}

void sm_set_dlg_font(HWND hdlg)
{
    EnumChildWindows(hdlg, (WNDENUMPROC)SetWindowFontProc, 0);
}

BOOL CALLBACK SetWindowFontProc(HWND h, LPARAM l)
{
    (void)l;
    SendMessageA(h, WM_SETFONT, (WPARAM)(g.hFontDlg ? g.hFontDlg : g.hFontUI),
                 MAKELPARAM(TRUE, 0));
    return TRUE;
}

void sm_error(HWND owner, const char *fmt, ...)
{
    va_list ap;
    char buf[2048];
    va_start(ap, fmt);
    _vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 1] = 0;
    MessageBoxA(owner, buf, SM_SHORT_NAME, MB_ICONSTOP | MB_OK);
}

void sm_info(HWND owner, const char *fmt, ...)
{
    va_list ap;
    char buf[2048];
    va_start(ap, fmt);
    _vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 1] = 0;
    MessageBoxA(owner, buf, SM_SHORT_NAME, MB_ICONINFORMATION | MB_OK);
}

int sm_ask(HWND owner, const char *fmt, ...)
{
    va_list ap;
    char buf[2048];
    va_start(ap, fmt);
    _vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 1] = 0;
    return MessageBoxA(owner, buf, SM_SHORT_NAME, MB_ICONQUESTION | MB_YESNO) == IDYES;
}

void sm_status_set(int pane, const char *text)
{
    if (g.hStatus) SendMessageA(g.hStatus, SB_SETTEXTA, (WPARAM)pane, (LPARAM)text);
}

void sm_update_title(void)
{
    char t[400];
    char st[96];
    const char *ws = ws_current();
    nb_lic_text(&g.lic, st, sizeof(st));
    _snprintf(t, sizeof(t), "%s - %s [%s]%s",
              L(STR_APP_TITLE), ws && ws[0] ? ws : L(STR_PANE_NOWORKSPACE), st,
              conv_dirty() ? " *" : "");
    t[sizeof(t) - 1] = 0;
    SetWindowTextA(g.hFrame, t);
}

void sm_update_license_ui(void)
{
    char st[96];
    char b[200];
    nb_lic_text(&g.lic, st, sizeof(st));
    sm_status_set(3, st);
    if (g.cfg.think) {
        static const char *lvn[3] = { "低", "中", "高" };
        static const char *lve[3] = { "low", "medium", "high" };
        int k = g.cfg.thinkLevel < 0 ? 0 : (g.cfg.thinkLevel > 2 ? 2 : g.cfg.thinkLevel);
        _snprintf(b, sizeof(b), "%s: %s  [%s:%s]", L(STR_STATUS_MODEL),
                  g.cfg.model[0] ? g.cfg.model : L(STR_NONE),
                  L(STR_THINK_PREFIX), TS(lvn[k], lve[k]));
    } else {
        _snprintf(b, sizeof(b), "%s: %s", L(STR_STATUS_MODEL),
                  g.cfg.model[0] ? g.cfg.model : L(STR_NONE));
    }
    b[sizeof(b) - 1] = 0;
    sm_status_set(2, b);
    sm_status_set(1, ws_current()[0] ? ws_current() : L(STR_PANE_NOWORKSPACE));
    sm_update_title();
}

/* ------------------------------------------------------------------ */
/* 帮助                                                                */
/* ------------------------------------------------------------------ */
/* 用浏览器打开 HELP 目录里的 index.htm（CHM 不好用时的兜底） */
static int open_html_help(HWND owner, const char *topic)
{
    char *dir = sm_app_dir();
    char *helpDir = sm_path_join(dir, "HELP");
    char *page;
    int ok = 0;

    if (topic && topic[0])
        page = sm_path_join(helpDir, topic);
    else
        page = sm_path_join(helpDir, "index.htm");

    if (sm_file_exists(page)) {
        HINSTANCE r = ShellExecuteA(owner, "open", page, NULL, helpDir, SW_SHOWNORMAL);
        ok = ((int)r > 32);
    }
    sm_free(page);
    sm_free(helpDir);
    sm_free(dir);
    return ok;
}

void sm_open_help(HWND owner, const char *topic)
{
    typedef HWND (WINAPI *PFN_HH)(HWND, LPCSTR, UINT, DWORD_PTR);
    HMODULE h = LoadLibraryA("hhctrl.ocx");
    PFN_HH pfn;
    char *dir, *chm, cmd[600];
    if (!h) {
        sm_error(owner, "hhctrl.ocx / Help component missing.\r\n"
                        "Please install Internet Explorer 4.0 or later, or read README.TXT.");
        return;
    }
    pfn = (PFN_HH)GetProcAddress(h, "HtmlHelpA");
    if (!pfn) { sm_error(owner, "Help component does not export HtmlHelpA."); return; }
    dir = sm_app_dir();
    chm = sm_path_join(dir, SM_EXE_BASE ".CHM");
    sm_free(dir);
    if (topic && topic[0])
        _snprintf(cmd, sizeof(cmd), "%s::/%s", chm, topic);
    else
        strncpy(cmd, chm, sizeof(cmd) - 1);
    cmd[sizeof(cmd) - 1] = 0;
    if (!pfn(owner, cmd, HH_DISPLAY_TOPIC, 0)) {
        /* CHM 打不开（老系统上 hh.exe 可能直接崩），改用 HELP 目录里的网页 */
        if (!open_html_help(owner, topic && topic[0] ? NULL : NULL))
            ShellExecuteA(owner, "open", chm, NULL, NULL, SW_SHOWNORMAL);
    }
    sm_free(chm);
}

void sm_open_help_file(HWND owner)
{
    char *dir = sm_app_dir();
    char *chm = sm_path_join(dir, SM_EXE_BASE ".CHM");
    if (!sm_file_exists(chm)) {
        if (!open_html_help(owner, NULL))
            sm_error(owner, SM_EXE_BASE ".CHM not found.\r\n"
                            "Also no HELP folder. Please reinstall.");
    } else {
        ShellExecuteA(owner, "open", chm, NULL, NULL, SW_SHOWNORMAL);
    }
    sm_free(chm);
    sm_free(dir);
}

/* ------------------------------------------------------------------ */
/* 启动画面                                                            */
/* ------------------------------------------------------------------ */
static LRESULT CALLBACK SplashProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        HDC mdc = CreateCompatibleDC(dc);
        HBITMAP bm = LoadBitmapA(g.hInst, MAKEINTRESOURCEA(202));
        if (bm) {
            HGDIOBJ old = SelectObject(mdc, bm);
            BITMAP b;
            GetObjectA(bm, sizeof(b), &b);
            BitBlt(dc, 0, 0, b.bmWidth, b.bmHeight, mdc, 0, 0, SRCCOPY);
            SelectObject(mdc, old);
            DeleteObject(bm);
        }
        DeleteDC(mdc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN:
        sm_splash_hide();
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

void sm_splash_show(void)
{
    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = SplashProc;
    wc.hInstance = g.hInst;
    wc.lpszClassName = SPLASH_CLASS;
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    RegisterClassA(&wc);
    g_hSplash = CreateWindowExA(WS_EX_TOPMOST, SPLASH_CLASS, SM_SHORT_NAME,
                                WS_POPUP, 0, 0, 400, 240, NULL, NULL, g.hInst, NULL);
    if (g_hSplash) {
        RECT rd;
        SystemParametersInfoA(SPI_GETWORKAREA, 0, &rd, 0);
        SetWindowPos(g_hSplash, HWND_TOPMOST,
                     rd.left + ((rd.right - rd.left) - 400) / 2,
                     rd.top + ((rd.bottom - rd.top) - 240) / 2,
                     0, 0, SWP_NOSIZE);
        ShowWindow(g_hSplash, SW_SHOW);
        UpdateWindow(g_hSplash);
        Sleep(1500);
    }
}

void sm_splash_hide(void)
{
    if (g_hSplash) { DestroyWindow(g_hSplash); g_hSplash = NULL; }
}

/* ------------------------------------------------------------------ */
/* 分隔条（竖向：拖动改变窗格宽度）                                    */
/* ------------------------------------------------------------------ */
static LRESULT CALLBACK SplitProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        RECT r;
        HDC dc = BeginPaint(h, &ps);
        GetClientRect(h, &r);
        FillRect(dc, &r, (HBRUSH)(COLOR_BTNFACE + 1));
        {
            HPEN p1 = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DHILIGHT));
            HPEN p2 = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
            HGDIOBJ op = SelectObject(dc, p1);
            MoveToEx(dc, r.left, r.top, NULL);
            LineTo(dc, r.left, r.bottom);
            SelectObject(dc, p2);
            MoveToEx(dc, r.left + 1, r.top, NULL);
            LineTo(dc, r.left + 1, r.bottom);
            SelectObject(dc, op);
            DeleteObject(p1);
            DeleteObject(p2);
        }
        EndPaint(h, &ps);
        return 0;
    }
    case WM_SETCURSOR:
        SetCursor(LoadCursorA(NULL, IDC_SIZEWE));
        return TRUE;
    case WM_LBUTTONDOWN:
        g_dragSplit = (GetDlgCtrlID(h) == IDC_SPLIT1) ? 1 : 2;
        SetCapture(h);
        return 0;
    case WM_MOUSEMOVE:
        if (g_dragSplit) {
            POINT pt;
            RECT rc;
            int x;
            GetCursorPos(&pt);
            ScreenToClient(g.hFrame, &pt);
            GetClientRect(g.hFrame, &rc);
            x = pt.x;
            if (g_dragSplit == 1) {
                if (x < 90) x = 90;
                if (x > rc.right - 300) x = rc.right - 300;
                g.paneL = x;
            } else {
                int mid = g.paneL + 5;
                int mw = x - mid - 5;
                if (mw < 100) mw = 100;
                if (mw > rc.right - mid - 260) mw = rc.right - mid - 260;
                g.paneM = mw;
            }
            SendMessageA(g.hFrame, WM_SIZE, SIZE_RESTORED,
                         MAKELPARAM(rc.right, rc.bottom));
            InvalidateRect(g.hFrame, NULL, TRUE);
        }
        return 0;
    case WM_LBUTTONUP:
        g_dragSplit = 0;
        ReleaseCapture();
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

/* ------------------------------------------------------------------ */
/* 菜单                                                                */
/* ------------------------------------------------------------------ */
static HMENU build_menu(void)
{
    HMENU mb = CreateMenu();
    HMENU m;

    m = CreatePopupMenu();
    AppendMenuA(m, MF_STRING, IDM_FILE_NEW,      L(STR_M_FILE_NEWCONV));
    AppendMenuA(m, MF_STRING, IDM_FILE_OPENCONV, L(STR_M_FILE_OPENCONV));
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    AppendMenuA(m, MF_STRING, IDM_FILE_SAVE,     L(STR_M_FILE_SAVE));
    AppendMenuA(m, MF_STRING, IDM_FILE_SAVEAS,   L(STR_M_FILE_SAVEAS));
    AppendMenuA(m, MF_STRING, IDM_FILE_PRINT,    L(STR_M_FILE_EXPORT));
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    AppendMenuA(m, MF_STRING, IDM_FILE_EDITOR,   L(STR_M_FILE_EDITOR));
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    AppendMenuA(m, MF_STRING, IDM_FILE_EXIT,     L(STR_M_FILE_EXIT));
    AppendMenuA(mb, MF_POPUP, (UINT_PTR)m, L(STR_M_FILE));

    m = CreatePopupMenu();
    AppendMenuA(m, MF_STRING, IDM_EDIT_UNDO,    L(STR_M_EDIT_UNDO));
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    AppendMenuA(m, MF_STRING, IDM_EDIT_CUT,     L(STR_M_EDIT_CUT));
    AppendMenuA(m, MF_STRING, IDM_EDIT_COPY,    L(STR_M_EDIT_COPY));
    AppendMenuA(m, MF_STRING, IDM_EDIT_PASTE,   L(STR_M_EDIT_PASTE));
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    AppendMenuA(m, MF_STRING, IDM_EDIT_SELALL,  L(STR_M_EDIT_SELALL));
    AppendMenuA(m, MF_STRING, IDM_EDIT_FIND,    L(STR_M_EDIT_FIND));
    AppendMenuA(m, MF_STRING, IDM_EDIT_TIME,    L(STR_M_EDIT_TIME));
    AppendMenuA(mb, MF_POPUP, (UINT_PTR)m, L(STR_M_EDIT));

    m = CreatePopupMenu();
    AppendMenuA(m, MF_STRING, IDM_WS_NEW,        L(STR_M_WS_NEW));
    AppendMenuA(m, MF_STRING, IDM_WS_RENAME,     L(STR_M_WS_RENAME));
    AppendMenuA(m, MF_STRING, IDM_WS_DELETE,     L(STR_M_WS_DELETE));
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    AppendMenuA(m, MF_STRING, IDM_WS_NEWFILE,    L(STR_M_WS_NEWFILE));
    AppendMenuA(m, MF_STRING, IDM_WS_ADDFILE,    L(STR_M_WS_IMPORT));
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    AppendMenuA(m, MF_STRING, IDM_WS_OPENFOLDER, L(STR_M_WS_OPENFOLDER));
    AppendMenuA(m, MF_STRING, IDM_WS_REFRESH,    L(STR_M_WS_REFRESH));
    AppendMenuA(mb, MF_POPUP, (UINT_PTR)m, L(STR_M_WS));

    m = CreatePopupMenu();
    AppendMenuA(m, MF_STRING, IDM_CHAT_SEND,    L(STR_M_CONV_SEND));
    AppendMenuA(m, MF_STRING, IDM_CHAT_STOP,    L(STR_M_CONV_STOP));
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    AppendMenuA(m, MF_STRING, IDM_CHAT_CLEAR,   L(STR_M_CONV_CLEAR));
    AppendMenuA(m, MF_STRING, IDM_CHAT_COPYALL, L(STR_M_CONV_COPYALL));
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    AppendMenuA(m, MF_STRING, IDM_CHAT_SYSINFO, L(STR_M_CONV_SYSINFO));
    AppendMenuA(m, MF_STRING, IDM_CHAT_ATTACH,  L(STR_M_CONV_ATTACH));
    AppendMenuA(m, MF_STRING | (g.cfg.reflow ? MF_CHECKED : MF_UNCHECKED),
                IDM_CHAT_REFLOW, TS("自动整理换行(&W)", "&Rewrap long lines"));
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    {
        /* 深度思考开关 + 挡位（低/中/高），挡位当前值打点 */
        HMENU ml = CreatePopupMenu();
        int lv = g.cfg.thinkLevel;
        int cur = (lv <= 0) ? IDM_CHAT_THINK_LOW
                : (lv >= 2) ? IDM_CHAT_THINK_HIGH : IDM_CHAT_THINK_MID;
        AppendMenuA(m, MF_STRING | (g.cfg.think ? MF_CHECKED : MF_UNCHECKED),
                    IDM_CHAT_THINK, L(STR_M_CONV_THINK));
        AppendMenuA(ml, MF_STRING, IDM_CHAT_THINK_LOW,  L(STR_THINK_LOW));
        AppendMenuA(ml, MF_STRING, IDM_CHAT_THINK_MID,  L(STR_THINK_MID));
        AppendMenuA(ml, MF_STRING, IDM_CHAT_THINK_HIGH, L(STR_THINK_HIGH));
        CheckMenuRadioItem(ml, IDM_CHAT_THINK_LOW, IDM_CHAT_THINK_HIGH, cur, MF_BYCOMMAND);
        if (!g.cfg.think) {
            EnableMenuItem(ml, IDM_CHAT_THINK_LOW,  MF_BYCOMMAND | MF_GRAYED);
            EnableMenuItem(ml, IDM_CHAT_THINK_MID,  MF_BYCOMMAND | MF_GRAYED);
            EnableMenuItem(ml, IDM_CHAT_THINK_HIGH, MF_BYCOMMAND | MF_GRAYED);
        }
        AppendMenuA(m, MF_POPUP, (UINT_PTR)ml, L(STR_M_CONV_THINKLEVEL));
    }
    AppendMenuA(mb, MF_POPUP, (UINT_PTR)m, L(STR_M_CONV));

    m = CreatePopupMenu();
    AppendMenuA(m, MF_STRING, IDM_TOOL_SETTINGS, L(STR_M_TOOL_SETTINGS));
    AppendMenuA(m, MF_STRING, IDM_TOOL_ACTIVATE, L(STR_M_TOOL_ACTIVATE));
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    AppendMenuA(m, MF_STRING, IDM_FILE_EDITOR,   L(STR_M_FILE_EDITOR));
    AppendMenuA(m, MF_STRING, IDM_TOOL_CERTUPD,  L(STR_M_TOOL_CERTUPD));
    AppendMenuA(m, MF_STRING, IDM_TOOL_ABOUTSMATE, L(STR_M_TOOL_SYSINFO));
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    {
        HMENU ml = CreatePopupMenu();
        AppendMenuA(ml, MF_STRING, IDM_TOOL_LANG_ZH, L(STR_M_TOOL_LANG_ZH));
        AppendMenuA(ml, MF_STRING, IDM_TOOL_LANG_EN, L(STR_M_TOOL_LANG_EN));
        CheckMenuRadioItem(ml, IDM_TOOL_LANG_ZH, IDM_TOOL_LANG_EN,
                           lang_get() == LANG_EN ? IDM_TOOL_LANG_EN : IDM_TOOL_LANG_ZH,
                           MF_BYCOMMAND);
        AppendMenuA(m, MF_POPUP, (UINT_PTR)ml, L(STR_M_TOOL_LANG));
    }
    AppendMenuA(mb, MF_POPUP, (UINT_PTR)m, L(STR_M_TOOL));

    m = CreatePopupMenu();
    AppendMenuA(m, MF_STRING, IDM_HELP_TOPICS,  L(STR_M_HELP_TOPICS));
    AppendMenuA(m, MF_STRING, IDM_HELP_QUICK,   L(STR_M_HELP_QUICK));
    AppendMenuA(m, MF_STRING, IDM_HELP_README,  L(STR_M_HELP_README));
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    AppendMenuA(m, MF_STRING, IDM_HELP_ABOUT,   L(STR_M_HELP_ABOUT));
    AppendMenuA(mb, MF_POPUP, (UINT_PTR)m, L(STR_M_HELP));

    return mb;
}

static void build_accel(void)
{
    ACCEL a[12];
    int n = 0;
    a[n].fVirt = FVIRTKEY | FCONTROL; a[n].key = 'N'; a[n].cmd = IDM_FILE_NEW; n++;
    a[n].fVirt = FVIRTKEY | FCONTROL; a[n].key = 'O'; a[n].cmd = IDM_FILE_OPENCONV; n++;
    a[n].fVirt = FVIRTKEY | FCONTROL; a[n].key = 'S'; a[n].cmd = IDM_FILE_SAVE; n++;
    a[n].fVirt = FVIRTKEY | FCONTROL; a[n].key = 'F'; a[n].cmd = IDM_EDIT_FIND; n++;
    a[n].fVirt = FVIRTKEY;            a[n].key = VK_F1; a[n].cmd = IDM_HELP_TOPICS; n++;
    a[n].fVirt = FVIRTKEY;            a[n].key = VK_F5; a[n].cmd = IDM_WS_REFRESH; n++;
    a[n].fVirt = FVIRTKEY;            a[n].key = VK_ESCAPE; a[n].cmd = IDM_CHAT_STOP; n++;
    g_hAccel = CreateAcceleratorTableA(a, n);
}

/* ------------------------------------------------------------------ */
/* 工具栏                                                              */
/* ------------------------------------------------------------------ */
static void build_toolbar(HWND h)
{
    TBBUTTON tb[14];
    TBADDBITMAP tbab;
    int n = 0;

    ZeroMemory(tb, sizeof(tb));
    g.hToolbar = CreateWindowExA(0, TOOLBARCLASSNAMEA, NULL,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | CCS_NODIVIDER,
        0, 0, 0, 0, h, (HMENU)IDC_TOOLBAR, g.hInst, NULL);
    SendMessageA(g.hToolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    SendMessageA(g.hToolbar, TB_SETBITMAPSIZE, 0, MAKELPARAM(16, 16));
    tbab.hInst = g.hInst;
    tbab.nID = 201;
    SendMessageA(g.hToolbar, TB_ADDBITMAP, 9, (LPARAM)&tbab);

    tb[n].iBitmap = 0; tb[n].idCommand = IDM_FILE_NEW;      tb[n].iString = -1; tb[n].fsState = TBSTATE_ENABLED; tb[n].fsStyle = TBSTYLE_BUTTON; n++;
    tb[n].iBitmap = 2; tb[n].idCommand = IDM_FILE_SAVE;     tb[n].iString = -1; tb[n].fsState = TBSTATE_ENABLED; tb[n].fsStyle = TBSTYLE_BUTTON; n++;
    tb[n].iBitmap = 5; tb[n].idCommand = IDM_FILE_EDITOR;   tb[n].iString = -1; tb[n].fsState = TBSTATE_ENABLED; tb[n].fsStyle = TBSTYLE_BUTTON; n++;
    tb[n].fsStyle = TBSTYLE_SEP; n++;
    tb[n].iBitmap = 3; tb[n].idCommand = IDM_CHAT_SEND;     tb[n].iString = -1; tb[n].fsState = TBSTATE_ENABLED; tb[n].fsStyle = TBSTYLE_BUTTON; n++;
    tb[n].iBitmap = 4; tb[n].idCommand = IDM_CHAT_STOP;     tb[n].iString = -1; tb[n].fsState = 0;               tb[n].fsStyle = TBSTYLE_BUTTON; n++;
    tb[n].fsStyle = TBSTYLE_SEP; n++;
    tb[n].iBitmap = 1; tb[n].idCommand = IDM_WS_OPENFOLDER; tb[n].iString = -1; tb[n].fsState = TBSTATE_ENABLED; tb[n].fsStyle = TBSTYLE_BUTTON; n++;
    tb[n].iBitmap = 6; tb[n].idCommand = IDM_TOOL_SETTINGS; tb[n].iString = -1; tb[n].fsState = TBSTATE_ENABLED; tb[n].fsStyle = TBSTYLE_BUTTON; n++;
    tb[n].iBitmap = 7; tb[n].idCommand = IDM_TOOL_ACTIVATE; tb[n].iString = -1; tb[n].fsState = TBSTATE_ENABLED; tb[n].fsStyle = TBSTYLE_BUTTON; n++;
    tb[n].fsStyle = TBSTYLE_SEP; n++;
    tb[n].iBitmap = 8; tb[n].idCommand = IDM_HELP_TOPICS;   tb[n].iString = -1; tb[n].fsState = TBSTATE_ENABLED; tb[n].fsStyle = TBSTYLE_BUTTON; n++;

    SendMessageA(g.hToolbar, TB_ADDBUTTONSA, (WPARAM)n, (LPARAM)tb);
    SendMessageA(g.hToolbar, TB_AUTOSIZE, 0, 0);
}

/* ------------------------------------------------------------------ */
/* 布局                                                                */
/* ------------------------------------------------------------------ */
static void layout_children(int cx, int cy)
{
    RECT br, sr;
    int top, bottom, paneTop;
    int xL, wL, xM, wM, xR, wR;
    int lblH = 15, editH = 20, btnH = 24;
    int sh = 22;

    if (!g.hTranscript) return;
    if (g.hToolbar) {
        GetWindowRect(g.hToolbar, &br);
        top = (br.bottom - br.top) + 3;
    } else top = 3;
    if (g.hStatus) {
        GetWindowRect(g.hStatus, &sr);
        sh = sr.bottom - sr.top;
    }
    bottom = cy - sh - 2;
    if (bottom < top + 80) bottom = top + 80;

    if (g.paneL < 90) g.paneL = 150;
    if (g.paneM < 100) g.paneM = 190;
    if (g.paneL + g.paneM + 260 > cx) {
        g.paneM = cx - g.paneL - 260;
        if (g.paneM < 100) { g.paneM = 100; g.paneL = cx - 360; if (g.paneL < 90) g.paneL = 90; }
    }

    xL = 0; wL = g.paneL;
    xM = wL + 5; wM = g.paneM;
    xR = xM + wM + 5; wR = cx - xR;
    paneTop = top;

    /* ---- 左窗格：工作区搜索 + 工作区列表 ---- */
    MoveWindow(g.hLblWsSearch, xL + 4, paneTop, wL - 8, lblH, TRUE);
    MoveWindow(g.hWsSearch, xL + 4, paneTop + lblH + 1, wL - 8, editH, TRUE);
    MoveWindow(g.hLblWs, xL + 4, paneTop + lblH + editH + 4, wL - 8, lblH, TRUE);
    MoveWindow(g.hWsList, xL + 4, paneTop + lblH * 2 + editH + 6, wL - 8,
               bottom - (paneTop + lblH * 2 + editH + 6) - 2, TRUE);

    /* ---- 中窗格：文件搜索 + 文件树 ---- */
    MoveWindow(g.hLblFileSearch, xM + 4, paneTop, wM - 8, lblH, TRUE);
    MoveWindow(g.hFileSearch, xM + 4, paneTop + lblH + 1, wM - 8, editH, TRUE);
    MoveWindow(g.hLblTree, xM + 4, paneTop + lblH + editH + 4, wM - 8, lblH, TRUE);
    MoveWindow(g.hTree, xM + 4, paneTop + lblH * 2 + editH + 6, wM - 8,
               bottom - (paneTop + lblH * 2 + editH + 6) - 2, TRUE);

    /* ---- 右窗格：对话 + 输入 ---- */
    {
        int avail = bottom - paneTop;
        int inH = g.inputH;
        int msgTop = paneTop + lblH + 2;
        int msgH, inTop;
        if (inH < 60) inH = avail / 4;
        if (inH > avail - 120) inH = avail - 120;
        if (inH < 60) inH = 60;
        inTop = bottom - inH - btnH - 4;
        msgH = inTop - msgTop - 4;
        if (msgH < 60) msgH = 60;

        MoveWindow(g.hLblMsg, xR + 4, paneTop, wR - 8, lblH, TRUE);
        MoveWindow(g.hTranscript, xR + 4, msgTop, wR - 8, msgH, TRUE);
        MoveWindow(g.hLblInput, xR + 4, inTop - lblH - 1, wR - 8, lblH, TRUE);
        MoveWindow(g.hInput, xR + 4, inTop, wR - 8, inH, TRUE);
        MoveWindow(g.hBtnSend, xR + wR - 8 - 88, bottom - btnH + 2, 88, btnH - 2, TRUE);
        MoveWindow(g.hBtnStop, xR + wR - 8 - 88 - 70, bottom - btnH + 2, 66, btnH - 2, TRUE);
        MoveWindow(g.hChkTools, xR + 6, bottom - btnH + 4, wR - 180, 18, TRUE);
    }

    /* ---- 分隔条 ---- */
    MoveWindow(g.hSplit1, wL, paneTop, 5, bottom - paneTop, TRUE);
    MoveWindow(g.hSplit2, xM + wM, paneTop, 5, bottom - paneTop, TRUE);
}

/* ------------------------------------------------------------------ */
/* 命令                                                                */
/* ------------------------------------------------------------------ */
static void do_send(void)
{
    char *txt;
    if (g.busy) return;
    if (g.lic.state == SM_LIC_EXPIRED) {
        sm_info(g.hFrame, "%s", L(STR_MSG_EXPIRED));
        dlg_activate(g.hFrame);
        return;
    }
    if (!g.cfg.apikey[0] && strstr(g.cfg.endpoint, "http://") == NULL) {
        sm_info(g.hFrame, "%s", L(STR_MSG_NOKEY));
        dlg_settings(g.hFrame);
        return;
    }
    txt = chat_get_input();
    if (!txt || !txt[0]) { sm_free(txt); return; }
    if (!conv_is_open()) conv_new();
    chat_set_input("");
    chat_append(CT_USER, txt);
    {
        Sb b;
        sb_init(&b);
        sb_printf(&b, "%s\r\n", txt);
        conv_append_text(sb_str(&b));
        sb_free(&b);
    }
    conv_mark_dirty();
    llm_start(txt);
    sm_free(txt);
    if (g.lic.state == SM_LIC_TRIAL) sm_license_consume_trial_message();
    sm_license_read(&g.lic);
    sm_update_license_ui();
}

static void do_new_conv(void)
{
    if (g.busy) { sm_info(g.hFrame, "%s", L(STR_STATUS_CONNECTING)); return; }
    if (conv_dirty()) {
        int r = MessageBoxA(g.hFrame, L(STR_TREE_SAVEFIRST), SM_SHORT_NAME,
                            MB_ICONQUESTION | MB_YESNOCANCEL);
        if (r == IDCANCEL) return;
        if (r == IDYES) conv_save();
    }
    conv_new();
    sm_update_title();
    sm_status_set(0, L(STR_STATUS_READY));
}

static void do_save_conv(void)
{
    if (!conv_is_open()) conv_new();
    if (conv_save()) {
        sm_status_set(0, L(STR_STATUS_SAVED));
        sm_update_title();
    } else {
        sm_error(g.hFrame, "Cannot write: %s", conv_path());
    }
}

static void do_saveas_conv(void)
{
    OPENFILENAMEA ofn;
    char path[MAX_PATH];
    char *dir = ws_current_dir();
    char *ini = sm_path_join(dir, "CONV.nbc");
    strncpy(path, ini, MAX_PATH - 1);
    path[MAX_PATH - 1] = 0;
    sm_free(ini);
    sm_free(dir);
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g.hFrame;
    ofn.lpstrFilter = "NeuroBench conversation (*.nbc)\0*.nbc\0Text files (*.txt)\0*.txt\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "nbc";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L(STR_M_FILE_SAVEAS);
    if (!GetSaveFileNameA(&ofn)) return;
    if (conv_save_as(path)) { sm_status_set(0, L(STR_STATUS_SAVED)); sm_update_title(); }
}

static void do_export_text(void)
{
    OPENFILENAMEA ofn;
    char path[MAX_PATH] = "TRANSCRIPT.TXT";
    char *text;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g.hFrame;
    ofn.lpstrFilter = "Text files (*.txt)\0*.txt\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "txt";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L(STR_M_FILE_EXPORT);
    if (!GetSaveFileNameA(&ofn)) return;
    text = chat_transcript_text();
    if (!sm_write_file(path, text, (long)strlen(text)))
        sm_error(g.hFrame, "Cannot write: %s", path);
    sm_free(text);
}

static void do_open_conv(void)
{
    OPENFILENAMEA ofn;
    char path[MAX_PATH];
    char *dir = ws_current_dir();
    strncpy(path, dir, MAX_PATH - 1);
    path[MAX_PATH - 1] = 0;
    sm_free(dir);
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g.hFrame;
    ofn.lpstrFilter = "NeuroBench conversation (*.nbc)\0*.nbc\0Text files (*.txt)\0*.txt\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L(STR_M_FILE_OPENCONV);
    if (GetOpenFileNameA(&ofn)) ws_open_path(path);
}

static void do_ws_new(void)
{
    char name[128];
    name[0] = 0;
    sm_logf("ws_new: prompt");
    if (!dlg_input(g.hFrame, L(STR_WS_NEW_PROMPT), L(STR_M_WS_NEW), name, sizeof(name), 0)) {
        sm_logf("ws_new: cancelled");
        return;
    }
    sm_trim(name);
    sm_logf("ws_new: name=[%s]", name);
    if (!name[0]) return;
    if (ws_create(name)) {
        ws_populate_list();
        sm_update_title();
        sm_logf("ws_new: ok");
    } else {
        char m[400];
        _snprintf(m, sizeof(m),
            TS("无法创建工作区「%s」。\r\n\r\n"
               "可能是名字含 \ / : * ? \" < > | 之类的字符，或者没有写权限。\r\n"
               "详细信息见安装目录下的 NEUROBENCH.LOG。",
               "Cannot create the workspace \"%s\".\r\n\r\n"
               "The name may contain invalid characters, or the folder is not writable.\r\n"
               "See NEUROBENCH.LOG for details."), name);
        m[sizeof(m) - 1] = 0;
        MessageBoxA(g.hFrame, m, SM_SHORT_NAME, MB_ICONWARNING | MB_OK);
        sm_logf("ws_new: ws_create failed");
    }
}

static void do_ws_rename(void)
{
    char name[128];
    const char *cur = ws_current();
    if (!cur || !cur[0]) return;
    strncpy(name, cur, sizeof(name) - 1);
    name[sizeof(name) - 1] = 0;
    if (dlg_input(g.hFrame, L(STR_WS_RENAME_PROMPT), L(STR_M_WS_RENAME), name, sizeof(name), 0))
        if (ws_rename(cur, name)) sm_update_title();
}

static void do_ws_delete(void)
{
    const char *cur = ws_current();
    char m[400];
    if (!cur || !cur[0]) return;
    _snprintf(m, sizeof(m), L(STR_WS_DELCONFIRM), cur);
    m[sizeof(m) - 1] = 0;
    if (MessageBoxA(g.hFrame, m, SM_SHORT_NAME, MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) == IDYES) {
        ws_delete(cur);
        conv_set_path("");
        sm_update_title();
    }
}

static void do_ws_newfile(void)
{
    char name[128];
    char *dir, *full;
    const char *content = "; new file\r\n";
    name[0] = 0;
    strcpy(name, "notes.txt");
    if (!dlg_input(g.hFrame, L(STR_TREE_NEWFILE_PROMPT), L(STR_M_WS_NEWFILE), name, sizeof(name), 0))
        return;
    if (strpbrk(name, "\\/:*?\"<>|")) { sm_error(g.hFrame, "%s", L(STR_TREE_NAMEBAD)); return; }
    dir = ws_current_dir();
    full = sm_path_join(dir, name);
    if (sm_file_exists(full)) {
        sm_error(g.hFrame, "%s", L(STR_TREE_EXISTS));
    } else {
        sm_write_file(full, content, (long)strlen(content));
        ws_tree_refresh();
        editor_open(full);
    }
    sm_free(full);
    sm_free(dir);
}

static void do_ws_addfile(void)
{
    OPENFILENAMEA ofn;
    char path[MAX_PATH];
    char *dir, *dst, *base;
    path[0] = 0;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g.hFrame;
    ofn.lpstrFilter = "All files (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L(STR_M_WS_IMPORT);
    if (!GetOpenFileNameA(&ofn)) return;
    base = strrchr(path, '\\');
    base = base ? base + 1 : path;
    dir = ws_current_dir();
    dst = sm_path_join(dir, base);
    if (sm_file_exists(dst) && !sm_ask(g.hFrame, "%s\r\n\r\n%s", L(STR_TREE_EXISTS), dst)) {
        sm_free(dst); sm_free(dir); return;
    }
    if (!sm_copy_file(path, dst)) sm_error(g.hFrame, "Copy failed: %s", dst);
    ws_tree_refresh();
    sm_free(dst);
    sm_free(dir);
}

static void command(int id)
{
    switch (id) {
    case IDM_FILE_NEW: do_new_conv(); break;
    case IDM_FILE_OPENCONV: do_open_conv(); break;
    case IDM_FILE_SAVE: do_save_conv(); break;
    case IDM_FILE_SAVEAS: do_saveas_conv(); break;
    case IDM_FILE_PRINT: do_export_text(); break;
    case IDM_FILE_EDITOR: editor_open(NULL); break;
    case IDM_FILE_EXIT: PostMessageA(g.hFrame, WM_CLOSE, 0, 0); break;

    case IDM_EDIT_UNDO:  SendMessageA(g.hInput, EM_UNDO, 0, 0); break;
    case IDM_EDIT_CUT:   SendMessageA(g.hInput, WM_CUT, 0, 0); break;
    case IDM_EDIT_COPY:  SendMessageA(g.hInput, WM_COPY, 0, 0); break;
    case IDM_EDIT_PASTE: SendMessageA(g.hInput, WM_PASTE, 0, 0); break;
    case IDM_EDIT_SELALL:SendMessageA(g.hInput, EM_SETSEL, 0, -1); break;
    case IDM_EDIT_FIND:  chat_find(g.hFrame); break;
    case IDM_EDIT_TIME: {
        char t[64];
        sm_now_str(t, sizeof(t));
        SendMessageA(g.hInput, EM_REPLACESEL, TRUE, (LPARAM)t);
        break;
    }

    case IDM_CHAT_SEND: do_send(); break;
    case IDM_CHAT_STOP:
        if (g.busy) { llm_cancel(); sm_status_set(0, L(STR_CONNECTING_STOP)); }
        break;
    case IDM_CHAT_CLEAR:
        if (g.busy) { sm_info(g.hFrame, "%s", L(STR_STATUS_CONNECTING)); return; }
        chat_clear();
        chat_history_reset();
        g.turnCount = 0;
        conv_mark_dirty();
        break;
    case IDM_CHAT_COPYALL: {
        char *t = chat_transcript_text();
        if (OpenClipboard(g.hFrame)) {
            HGLOBAL hg;
            EmptyClipboard();
            hg = GlobalAlloc(GMEM_MOVEABLE, strlen(t) + 1);
            if (hg) {
                char *p = (char *)GlobalLock(hg);
                strcpy(p, t);
                GlobalUnlock(hg);
                SetClipboardData(CF_TEXT, hg);
            }
            CloseClipboard();
        }
        sm_free(t);
        break;
    }
    case IDM_CHAT_SYSINFO: {
        Sb b;
        char *txt;
        sb_init(&b);
        tools_sysinfo_text(&b);
        txt = sb_take(&b);
        chat_append(CT_INFO, txt);
        chat_set_input(txt);
        sm_free(txt);
        break;
    }

    /* ---- 附加图片 ---- */
    case IDM_CHAT_REFLOW:
        g.cfg.reflow = !g.cfg.reflow;
        sm_config_save(&g.cfg);
        sm_status_set(0, g.cfg.reflow ? TS("已开启自动整理换行", "Line rewrap: on")
                                      : TS("已关闭自动整理换行", "Line rewrap: off"));
        break;

    case IDM_CHAT_ATTACH: {
        char path[MAX_PATH];
        unsigned char *png = NULL;
        long plen = 0;
        int vision = 0;
        if (!dlg_pick_image(path, sizeof(path))) break;
        png = sm_load_image_png(path, &plen);
        if (!png) {
            chat_append(CT_ERR, TS("无法读取这张图片（只支持 PNG 和 24 位 BMP）。",
                               "Cannot read this picture (PNG or 24-bit BMP only)."));
            break;
        }
        if (g_shot_png) sm_free(g_shot_png);
        g_shot_png = png;
        g_shot_len = (int)plen;
        g_shot_w = 0;
        g_shot_h = 0;
        if (g.prov.count > 0 && g.prov.active >= 0 && g.prov.active < g.prov.count)
            vision = g.prov.list[g.prov.active].vision;
        chat_append(CT_SYS, vision ? L(STR_MSG_ATTACHED) : L(STR_MSG_ATTACHED_NOIMG));
        break;
    }

    /* ---- 深度思考开关与挡位 ---- */
    case IDM_CHAT_THINK:
    case IDM_CHAT_THINK_LOW:
    case IDM_CHAT_THINK_MID:
    case IDM_CHAT_THINK_HIGH: {
        char msg[200];
        if (id == IDM_CHAT_THINK) {
            g.cfg.think = g.cfg.think ? 0 : 1;
        } else {
            g.cfg.think = 1;                       /* 选了挡位就顺手打开开关 */
            g.cfg.thinkLevel = (id == IDM_CHAT_THINK_LOW)  ? 0
                             : (id == IDM_CHAT_THINK_HIGH) ? 2 : 1;
        }
        sm_config_save(&g.cfg);
        if (g_hMenu) {
            HMENU m2 = GetSubMenu(g_hMenu, 3);     /* 会话菜单 */
            int cur = g.cfg.thinkLevel <= 0 ? IDM_CHAT_THINK_LOW
                    : g.cfg.thinkLevel >= 2 ? IDM_CHAT_THINK_HIGH : IDM_CHAT_THINK_MID;
            CheckMenuItem(g_hMenu, IDM_CHAT_THINK,
                          MF_BYCOMMAND | (g.cfg.think ? MF_CHECKED : MF_UNCHECKED));
            if (m2) {
                HMENU ml = GetSubMenu(m2, GetMenuItemCount(m2) - 2);
                if (ml) {
                    CheckMenuRadioItem(ml, IDM_CHAT_THINK_LOW, IDM_CHAT_THINK_HIGH,
                                       cur, MF_BYCOMMAND);
                    EnableMenuItem(ml, IDM_CHAT_THINK_LOW,
                        MF_BYCOMMAND | (g.cfg.think ? MF_ENABLED : MF_GRAYED));
                    EnableMenuItem(ml, IDM_CHAT_THINK_MID,
                        MF_BYCOMMAND | (g.cfg.think ? MF_ENABLED : MF_GRAYED));
                    EnableMenuItem(ml, IDM_CHAT_THINK_HIGH,
                        MF_BYCOMMAND | (g.cfg.think ? MF_ENABLED : MF_GRAYED));
                }
            }
        }
        if (g.cfg.think) {
            static const char *lvn[3] = { "低", "中", "高" };
            static const char *lve[3] = { "low", "medium", "high" };
            int k = g.cfg.thinkLevel < 0 ? 0 : (g.cfg.thinkLevel > 2 ? 2 : g.cfg.thinkLevel);
            _snprintf(msg, sizeof(msg), TS("%s（挡位：%s）", "%s (level: %s)"),
                      L(STR_MSG_THINK_ON), TS(lvn[k], lve[k]));
        } else {
            _snprintf(msg, sizeof(msg), "%s", L(STR_MSG_THINK_OFF));
        }
        msg[sizeof(msg) - 1] = 0;
        chat_append(CT_SYS, msg);
        sm_update_license_ui();       /* 顺手刷新状态栏上的思考标记 */
        break;
    }

    case IDM_WS_NEW: do_ws_new(); break;
    case IDM_WS_RENAME: do_ws_rename(); break;
    case IDM_WS_DELETE: do_ws_delete(); break;
    case IDM_WS_NEWFILE: do_ws_newfile(); break;
    case IDM_WS_ADDFILE: do_ws_addfile(); break;
    case IDM_WS_OPENFOLDER: {
        char *d = ws_current_dir();
        ShellExecuteA(g.hFrame, "open", d, NULL, NULL, SW_SHOWNORMAL);
        sm_free(d);
        break;
    }
    case IDM_WS_REFRESH:
        ws_populate_list();
        ws_tree_refresh();
        sm_status_set(0, L(STR_STATUS_READY));
        break;

    case IDM_TOOL_SETTINGS: dlg_settings(g.hFrame); break;
    case IDM_TOOL_ACTIVATE: dlg_activate(g.hFrame); break;
    case IDM_TOOL_ABOUTSMATE: dlg_sysinfo(g.hFrame); break;
    case IDM_TOOL_CERTUPD: {
        char *dir = sm_app_dir();
        char *exe = sm_path_join(dir, "CERTUPD.EXE");
        if (sm_file_exists(exe)) ShellExecuteA(g.hFrame, "open", exe, NULL, dir, SW_SHOWNORMAL);
        else {
            char *c = sm_path_join(dir, "CERTS");
            sm_info(g.hFrame, "CERTUPD.EXE not found.\r\nCertificates folder:\r\n%s", c);
            sm_free(c);
        }
        sm_free(exe);
        sm_free(dir);
        break;
    }
    case IDM_TOOL_LANG_ZH:
    case IDM_TOOL_LANG_EN:
        lang_set(id == IDM_TOOL_LANG_EN ? LANG_EN : LANG_ZH);
        lang_save();
        sm_rebuild_ui();
        break;

    case IDM_HELP_TOPICS: sm_open_help(g.hFrame, NULL); break;
    case IDM_HELP_QUICK:  sm_open_help(g.hFrame, lang_get() == LANG_EN ? "en_quickstart.htm" : "quickstart.htm"); break;
    case IDM_HELP_README: {
        char *dir = sm_app_dir();
        char *f = sm_path_join(dir, "README.TXT");
        if (sm_file_exists(f)) editor_open(f);
        else sm_error(g.hFrame, "README.TXT not found.");
        sm_free(f);
        sm_free(dir);
        break;
    }
    case IDM_HELP_ABOUT: dlg_about(g.hFrame); break;
    }
}

/* ------------------------------------------------------------------ */
/* 事件                                                                */
/* ------------------------------------------------------------------ */
static void handle_event(SmEvent *e)
{
    switch (e->kind) {
    case EVT_USER:      chat_append(CT_USER, e->text); break;
    case EVT_ASSISTANT:
        chat_append(CT_ASSIST, e->text);
        if (conv_is_open()) {
            Sb b;
            sb_init(&b);
            sb_printf(&b, "%s:\r\n%s\r\n\r\n", L(STR_SPEAKER_BOT), e->text);
            conv_append_text(sb_str(&b));
            sb_free(&b);
            conv_mark_dirty();
        }
        break;
    case EVT_REASONING: chat_append(CT_THINK, e->text); break;
    case EVT_NOTICE:    chat_append(CT_SYS, e->text); break;
    case EVT_ERROR:     chat_append(CT_ERR, e->text); break;
    case EVT_STATE:     sm_status_set(0, e->text); break;
    case EVT_DONE:
        InterlockedExchange(&g.busy, 0);
        SendMessageA(g.hToolbar, TB_ENABLEBUTTON, IDM_CHAT_SEND, MAKELONG(1, 0));
        SendMessageA(g.hToolbar, TB_ENABLEBUTTON, IDM_CHAT_STOP, MAKELONG(0, 0));
        EnableWindow(g.hBtnSend, TRUE);
        EnableWindow(g.hBtnStop, FALSE);
        sm_status_set(0, L(STR_STATUS_READY));
        dlg_connecting_close();
        g.turnCount++;
        if (conv_is_open()) conv_save();
        sm_update_license_ui();
        chat_focus_input();
        break;
    }
    if (e->text) sm_free(e->text);
    sm_free(e);
}

/* ------------------------------------------------------------------ */
/* 子类化：输入框（Ctrl+Enter 发送）                                   */
/* ------------------------------------------------------------------ */
static WNDPROC g_oldInputProc = NULL;

static LRESULT CALLBACK InputProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_KEYDOWN) {
        int ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        if (w == VK_RETURN && ctrl) {
            PostMessageA(g.hFrame, WM_COMMAND, MAKEWPARAM(IDM_CHAT_SEND, 0), 0);
            return 0;
        }
        if (w == VK_ESCAPE) {
            PostMessageA(g.hFrame, WM_COMMAND, MAKEWPARAM(IDM_CHAT_STOP, 0), 0);
            return 0;
        }
    }
    return CallWindowProcA(g_oldInputProc, h, m, w, l);
}

/* ------------------------------------------------------------------ */
/* 创建控件                                                            */
/* ------------------------------------------------------------------ */
static HWND mkctl(const char *cls, const char *txt, DWORD style, DWORD ex,
                  int id, HFONT f)
{
    HWND c = CreateWindowExA(ex, cls, txt, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | style,
                             0, 0, 10, 10, g.hFrame, (HMENU)(INT_PTR)id, g.hInst, NULL);
    if (c && f) SendMessageA(c, WM_SETFONT, (WPARAM)f, MAKELPARAM(TRUE, 0));
    return c;
}

static void create_children(HWND h)
{
    RECT rc;
    int panes[4];
    GetClientRect(h, &rc);

    build_toolbar(h);

    g.hStatus = CreateWindowExA(0, STATUSCLASSNAMEA, NULL,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | SBARS_SIZEGRIP, 0, 0, 0, 0,
        h, (HMENU)IDC_STATUSBAR, g.hInst, NULL);
    GetClientRect(h, &rc);
    /* 第三格要放“型号 + 思考挡位”，给它多留点地方 */
    panes[0] = (rc.right * 34) / 100;
    panes[1] = (rc.right * 50) / 100;
    panes[2] = (rc.right * 78) / 100;
    panes[3] = -1;
    SendMessageA(g.hStatus, SB_SETPARTS, 4, (LPARAM)panes);
    SendMessageA(g.hStatus, WM_SETFONT, (WPARAM)g.hFontUI, MAKELPARAM(TRUE, 0));

    /* 左窗格 */
    g.hLblWsSearch = mkctl("STATIC", L(STR_PANE_WS_SEARCH), SS_LEFT, 0, IDC_LBL_WSSEARCH, g.hFontSmall);
    g.hLblWs = mkctl("STATIC", L(STR_PANE_WS_LIST), SS_LEFT, 0, IDC_LBL_WS, g.hFontSmall);
    g.hWsSearch = mkctl("EDIT", "", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE,
                        IDC_WS_SEARCH, g.hFontUI);
    g.hWsList = mkctl("LISTBOX", "", WS_BORDER | WS_VSCROLL | LBS_NOTIFY | WS_TABSTOP,
                      WS_EX_CLIENTEDGE, IDC_WS_LIST, g.hFontUI);

    /* 中窗格 */
    g.hLblFileSearch = mkctl("STATIC", L(STR_PANE_FILE_SEARCH), SS_LEFT, 0, IDC_LBL_FILESEARCH, g.hFontSmall);
    g.hLblTree = mkctl("STATIC", L(STR_PANE_TREE), SS_LEFT, 0, IDC_LBL_TREE, g.hFontSmall);
    g.hFileSearch = mkctl("EDIT", "", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE,
                          IDC_FILE_SEARCH, g.hFontUI);
    g.hTree = mkctl(WC_TREEVIEWA, "",
                    TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS |
                    WS_TABSTOP | TVS_EDITLABELS,
                    WS_EX_CLIENTEDGE, IDC_TREE, g.hFontUI);

    /* 右窗格 */
    g.hLblMsg = mkctl("STATIC", L(STR_PANE_MSG), SS_LEFT, 0, IDC_LBL_MSG, g.hFontSmall);
    chat_init(h);
    g.hLblInput = mkctl("STATIC", L(STR_PANE_INPUT), SS_LEFT, 0, IDC_LBL_INPUT, g.hFontSmall);
    g.hBtnSend = mkctl("BUTTON", L(STR_BTN_SEND), WS_TABSTOP | BS_PUSHBUTTON, 0,
                       IDC_BTN_SEND, g.hFontUI);
    g.hBtnStop = mkctl("BUTTON", L(STR_BTN_STOP), WS_TABSTOP | BS_PUSHBUTTON, 0,
                       IDC_BTN_STOP, g.hFontUI);
    g.hChkTools = mkctl("BUTTON", L(STR_SET_TOOLS), WS_TABSTOP | BS_AUTOCHECKBOX, 0,
                        IDC_CHK_TOOLS, g.hFontUI);
    SendMessageA(g.hChkTools, BM_SETCHECK, g.cfg.useTools ? BST_CHECKED : BST_UNCHECKED, 0);

    /* 分隔条 */
    g.hSplit1 = mkctl(SPLIT_CLASS, NULL, 0, 0, IDC_SPLIT1, NULL);
    g.hSplit2 = mkctl(SPLIT_CLASS, NULL, 0, 0, IDC_SPLIT2, NULL);

    EnableWindow(g.hBtnStop, FALSE);
    layout_children(rc.right, rc.bottom);
}

/* ------------------------------------------------------------------ */
/* 语言切换后刷新界面                                                  */
/* ------------------------------------------------------------------ */
void sm_rebuild_ui(void)
{
    HMENU nm = build_menu();
    if (g_hMenu) DestroyMenu(g_hMenu);
    g_hMenu = nm;
    SetMenu(g.hFrame, nm);
    DrawMenuBar(g.hFrame);

    SetWindowTextA(g.hLblWs, L(STR_PANE_WS_LIST));
    SetWindowTextA(g.hLblTree, L(STR_PANE_TREE));
    SetWindowTextA(g.hLblMsg, L(STR_PANE_MSG));
    SetWindowTextA(g.hLblInput, L(STR_PANE_INPUT));
    SetWindowTextA(g.hBtnSend, L(STR_BTN_SEND));
    SetWindowTextA(g.hBtnStop, L(STR_BTN_STOP));
    SetWindowTextA(g.hLblWsSearch, L(STR_PANE_WS_SEARCH));
    SetWindowTextA(g.hLblFileSearch, L(STR_PANE_FILE_SEARCH));
    SetWindowTextA(g.hChkTools, L(STR_SET_TOOLS));
    editor_refresh_strings();
    sm_status_set(0, L(STR_STATUS_READY));
    sm_update_license_ui();
    InvalidateRect(g.hFrame, NULL, TRUE);
    {
        RECT rc;
        GetClientRect(g.hFrame, &rc);
        layout_children(rc.right, rc.bottom);
    }
}

/* ------------------------------------------------------------------ */
/* 主窗口过程                                                          */
/* ------------------------------------------------------------------ */
static LRESULT CALLBACK FrameProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_CREATE:
        g.hFrame = h;
        create_children(h);
        return 0;

    case WM_SIZE:
        if (g.hStatus && g.hTranscript) {
            SendMessageA(g.hStatus, WM_SIZE, 0, 0);
            SendMessageA(g.hToolbar, TB_AUTOSIZE, 0, 0);
            layout_children(LOWORD(l), HIWORD(l));
            InvalidateRect(h, NULL, TRUE);
        }
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)l;
        mm->ptMinTrackSize.x = 700;
        mm->ptMinTrackSize.y = 460;
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)w;
        SetBkMode(dc, TRANSPARENT);
        return (LRESULT)(HBRUSH)(COLOR_BTNFACE + 1);
    }

    case WM_NOTIFY: {
        NMHDR *nh = (NMHDR *)l;
        if (nh->hwndFrom == g.hToolbar && nh->code == TTN_NEEDTEXT) {
            LPTOOLTIPTEXTA tt = (LPTOOLTIPTEXTA)l;
            switch (tt->hdr.idFrom) {
            case IDM_FILE_NEW:      tt->lpszText = (LPSTR)L(STR_TIP_NEWCONV); break;
            case IDM_FILE_SAVE:     tt->lpszText = (LPSTR)L(STR_TIP_SAVE); break;
            case IDM_FILE_EDITOR:   tt->lpszText = (LPSTR)L(STR_TIP_EDITOR); break;
            case IDM_CHAT_SEND:     tt->lpszText = (LPSTR)L(STR_TIP_SEND); break;
            case IDM_CHAT_STOP:     tt->lpszText = (LPSTR)L(STR_TIP_STOP); break;
            case IDM_WS_OPENFOLDER: tt->lpszText = (LPSTR)L(STR_M_WS_OPENFOLDER); break;
            case IDM_TOOL_SETTINGS: tt->lpszText = (LPSTR)L(STR_TIP_SETTINGS); break;
            case IDM_TOOL_ACTIVATE: tt->lpszText = (LPSTR)L(STR_TIP_ACTIVATE); break;
            case IDM_HELP_TOPICS:   tt->lpszText = (LPSTR)L(STR_TIP_HELP); break;
            }
            return 0;
        }
        if (nh->hwndFrom == g.hTree) {
            if (nh->code == NM_DBLCLK) {
                char *p = tree_selected_path();
                if (p) {
                    DWORD a = GetFileAttributesA(p);
                    if (!(a & FILE_ATTRIBUTE_DIRECTORY)) ws_open_path(p);
                    sm_free(p);
                }
                return 0;
            }
            if (nh->code == NM_RCLICK) {
                POINT pt;
                char *p = tree_selected_path();
                GetCursorPos(&pt);
                if (p) {
                    HMENU pm = CreatePopupMenu();
                    DWORD a = GetFileAttributesA(p);
                    int isDir = (a != 0xFFFFFFFF) && (a & FILE_ATTRIBUTE_DIRECTORY);
                    if (!isDir) {
                        AppendMenuA(pm, MF_STRING, 1, L(STR_OPEN));
                        AppendMenuA(pm, MF_STRING, 2, L(STR_DELETE));
                        AppendMenuA(pm, MF_SEPARATOR, 0, NULL);
                    }
                    AppendMenuA(pm, MF_STRING, 3, L(STR_REFRESH));
                    {
                        int cmd = (int)TrackPopupMenu(pm, TPM_RETURNCMD | TPM_LEFTALIGN,
                                                      pt.x, pt.y, 0, h, NULL);
                        if (cmd == 1) ws_open_path(p);
                        else if (cmd == 2) {
                            char msg[700];
                            _snprintf(msg, sizeof(msg), L(STR_TREE_DELCONFIRM), p);
                            msg[sizeof(msg) - 1] = 0;
                            if (MessageBoxA(h, msg, SM_SHORT_NAME,
                                            MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) == IDYES) {
                                SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL);
                                DeleteFileA(p);
                                if (!sm_stricmp(p, conv_path())) conv_set_path("");
                                ws_tree_refresh();
                            }
                        } else if (cmd == 3) ws_tree_refresh();
                    }
                    DestroyMenu(pm);
                    sm_free(p);
                }
                return 0;
            }
        }
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(w);
        int code = HIWORD(w);
        if (code == EN_CHANGE) {
            if (id == IDC_WS_SEARCH) {
                char buf[64];
                GetWindowTextA(g.hWsSearch, buf, sizeof(buf));
                ws_set_filter(buf);
                return 0;
            }
            if (id == IDC_FILE_SEARCH) {
                char buf[64];
                GetWindowTextA(g.hFileSearch, buf, sizeof(buf));
                ws_set_file_filter(buf);
                return 0;
            }
        }
        if (code == LBN_SELCHANGE && id == IDC_WS_LIST) {
            int sel = (int)SendMessageA(g.hWsList, LB_GETCURSEL, 0, 0);
            if (sel >= 0) {
                char buf[128];
                SendMessageA(g.hWsList, LB_GETTEXT, (WPARAM)sel, (LPARAM)buf);
                buf[sizeof(buf) - 1] = 0;
                if (sm_stricmp(buf, ws_current())) {
                    if (conv_dirty()) conv_save();
                    ws_switch_to(buf);
                    conv_set_path("");
                    sm_update_title();
                }
            }
            return 0;
        }
        if (code == BN_CLICKED) {
            if (id == IDC_BTN_SEND) { do_send(); return 0; }
            if (id == IDC_BTN_STOP) { llm_cancel(); return 0; }
            if (id == IDC_CHK_TOOLS) {
                g.cfg.useTools = (SendMessageA(g.hChkTools, BM_GETCHECK, 0, 0) == BST_CHECKED);
                sm_config_save(&g.cfg);
                return 0;
            }
        }
        if (code == 0 || code == 1) { command(id); return 0; }
        return 0;
    }

    case WM_INITMENUPOPUP:
        EnableMenuItem(GetMenu(h), IDM_CHAT_SEND, g.busy ? MF_GRAYED : MF_ENABLED);
        EnableMenuItem(GetMenu(h), IDM_CHAT_STOP, g.busy ? MF_ENABLED : MF_GRAYED);
        EnableMenuItem(GetMenu(h), IDM_TOOL_SETTINGS, g.busy ? MF_GRAYED : MF_ENABLED);
        EnableMenuItem(GetMenu(h), IDM_FILE_NEW, g.busy ? MF_GRAYED : MF_ENABLED);
        return 0;

    case WM_SM_EVENT:
        handle_event((SmEvent *)l);
        return 0;

    case WM_SETFOCUS:
        chat_focus_input();
        return 0;

    case WM_NB_CONFIRM:
        tools_confirm_show();
        return 0;

    case WM_CLOSE:
        if (g.busy) {
            if (!sm_ask(h, "%s", L(STR_CONNECTING_STOP))) return 0;
            llm_cancel();
            Sleep(120);
        }
        if (conv_dirty()) conv_save();
        DestroyWindow(h);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

/* ------------------------------------------------------------------ */
static void register_classes(void)
{
    WNDCLASSA wc;

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = FrameProc;
    wc.hInstance = g.hInst;
    wc.hIcon = LoadIconA(g.hInst, MAKEINTRESOURCEA(101));
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "NeuroBenchFrame";
    RegisterClassA(&wc);

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = SplitProc;
    wc.hInstance = g.hInst;
    wc.hCursor = LoadCursorA(NULL, IDC_SIZEWE);
    wc.lpszClassName = SPLIT_CLASS;
    RegisterClassA(&wc);
}

int PASCAL WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show)
{
    HWND hFrame;
    MSG msg;
    INITCOMMONCONTROLSEX icc;
    HDC dc;

    (void)hPrev; (void)cmd;
    g.hInst = hInst;

    dc = GetDC(NULL);
    if (dc) {
        int dpi = GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(NULL, dc);
        g_dpiScale = dpi > 108 ? 2 : 1;
    }

    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES | ICC_TAB_CLASSES | ICC_TREEVIEW_CLASSES |
                ICC_LISTVIEW_CLASSES | ICC_UPDOWN_CLASS;
    InitCommonControlsEx(&icc);

    if (!LoadLibraryA("riched20.dll")) LoadLibraryA("riched32.dll");
    {
        INITCOMMONCONTROLSEX icc;
        icc.dwSize = sizeof(icc);
        icc.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES | ICC_TAB_CLASSES |
                    ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES;
        InitCommonControlsEx(&icc);
    }
    OleInitialize(NULL);

    g.hFontUI    = sm_ui_font(12, 0, 0);
    g.hFontDlg   = sm_ui_font(13, 0, 0);
    g.hFontBold  = sm_ui_font(12, 1, 0);
    g.hFontMono  = sm_ui_font(12, 0, 1);
    g.hFontTitle = sm_ui_font(15, 1, 0);
    g.hFontSmall = sm_ui_font(11, 0, 0);
    g.hbrBk      = CreateSolidBrush(GetSysColor(COLOR_WINDOW));

    sm_config_load(&g.cfg);
    /* 多供应商：有表就用表里的当前项覆盖单段配置，没有则自动从老配置迁移 */
    sm_providers_load(&g.prov);
    if (g.prov.count > 0) {
        if (g.prov.active < 0 || g.prov.active >= g.prov.count) g.prov.active = 0;
        sm_provider_to_config(&g.prov, g.prov.active, &g.cfg);
        g.cfg.fontSize = 12;          /* 界面字号不跟着供应商走 */
        { char *ini = sm_ini_path();
          { char *f = sm_ini_get(ini, "display", "fontsize", "12");
            g.cfg.fontSize = (int)sm_to_long(f, 12); sm_free(f); }
          { char *lg = sm_ini_get(ini, "display", "language", "");
            if (lg[0]) lang_set(sm_stricmp(lg, "en") ? LANG_ZH : LANG_EN); sm_free(lg); }
          sm_free(ini); }
    }
    lang_load();
    sm_license_read(&g.lic);
    sb_init(&g.history);
    g.paneL = 150;
    g.paneM = 190;

    register_classes();
    sm_splash_show();

    g_hMenu = build_menu();


    hFrame = CreateWindowExA(0, "NeuroBenchFrame", L(STR_APP_TITLE),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 900, 620,
        NULL, g_hMenu, hInst, NULL);
    if (!hFrame) {
        sm_splash_hide();
        MessageBoxA(NULL, "Cannot create main window.", SM_SHORT_NAME, MB_ICONSTOP);
        return 1;
    }
    build_accel();
    ShowWindow(hFrame, show);
    UpdateWindow(hFrame);
    sm_splash_hide();

    g_oldInputProc = (WNDPROC)SetWindowLongPtrA(g.hInput, GWLP_WNDPROC, (LONG_PTR)InputProc);

    /* 工作区 */
    ws_init();
    sm_status_set(0, L(STR_STATUS_READY));
    {
        char b[160];
        _snprintf(b, sizeof(b), "%s: %s", L(STR_STATUS_MODEL),
                  g.cfg.model[0] ? g.cfg.model : L(STR_NONE));
        b[sizeof(b) - 1] = 0;
        sm_status_set(2, b);
    }
    sm_update_license_ui();
    sm_update_title();

    /* 欢迎信息 */
    chat_append(CT_INFO, L(STR_MSG_WELCOME));
    {
        char line[400];
        char td[80];
        char osbuf[200];
        nb_now_text(td, sizeof(td));
        nb_os_text(osbuf, sizeof(osbuf));
        _snprintf(line, sizeof(line), "%s\r\n%s", td, osbuf);
        line[sizeof(line) - 1] = 0;
        chat_append(CT_SYS, line);
    }
    if (!g.cfg.apikey[0]) chat_append(CT_ERR, L(STR_MSG_NOKEY));
    if (g.lic.state == SM_LIC_TRIAL) chat_append(CT_SYS, L(STR_MSG_TRIAL));
    if (g.lic.state == SM_LIC_EXPIRED) chat_append(CT_ERR, L(STR_MSG_EXPIRED));

    chat_focus_input();

    /* 命令行：/ask:"问题" */
    {
        char *cl = GetCommandLineA();
        /* 命令行：/img=<图片路径>  启动时先附加一张图（脚本化/自测用） */
    {
        char *ip = (char *)sm_stristr(cl, "/img=");
        if (ip) {
            char path[MAX_PATH];
            int k = 0;
            ip += 5;
            if (*ip == '"') ip++;
            while (*ip && *ip != '"' && *ip != ' ' && k < MAX_PATH - 1) path[k++] = *ip++;
            path[k] = 0;
            if (path[0]) {
                long plen = 0;
                unsigned char *png = sm_load_image_png(path, &plen);
                if (png) {
                    if (g_shot_png) sm_free(g_shot_png);
                    g_shot_png = png; g_shot_len = (int)plen;
                    sm_logf("attach: %s (%ld bytes png)", path, plen);
                } else sm_logf("attach: cannot read %s", path);
            }
        }
    }
    char *a = (char *)sm_stristr(cl, "/ask:");
        if (a) {
            char q[2048];
            int n = 0;
            a += 5;
            if (*a == '"') a++;
            while (*a && *a != '"' && n < (int)sizeof(q) - 1) q[n++] = *a++;
            q[n] = 0;
            sm_trim(q);
            if (q[0]) {
                chat_set_input(q);
                PostMessageA(hFrame, WM_COMMAND, MAKEWPARAM(IDM_CHAT_SEND, 0), 0);
            }
        }
    }

    /* 自测开关：/settings 在消息循环开始前投递，此时窗口已就绪 */
    if (sm_stristr(GetCommandLineA(), "/settings"))
        PostMessageA(g.hFrame, WM_COMMAND, MAKEWPARAM(IDM_TOOL_SETTINGS, 0), 0);


    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        if (g_hAccel && TranslateAcceleratorA(hFrame, g_hAccel, &msg)) continue;
        if (chat_is_find_msg(&msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    if (g.history.p) sb_free(&g.history);
    OleUninitialize();
    return 0;
}
