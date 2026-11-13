/* Axiom NeuroBench 2003 - 各种对话框（全部用代码创建） */
#include "agent.h"

static HWND mk(HWND p, const char *cls, const char *txt, DWORD style, int x, int y,
               int w, int h, int id)
{
    HWND c = CreateWindowExA(0, cls, txt, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | style,
                             x, y, w, h, p, (HMENU)(INT_PTR)id, g.hInst, NULL);
    if (c) SendMessageA(c, WM_SETFONT,
                        (WPARAM)(g.hFontDlg ? g.hFontDlg : g.hFontUI),
                        MAKELPARAM(TRUE, 0));
    return c;
}

/* 弹出“打开文件”框选一张图片，返回 1 = 选好了 */
int dlg_pick_image(char *outPath, int outLen)
{
    OPENFILENAMEA ofn;
    char file[MAX_PATH];
    static const char filter[] =
        "图片文件 (*.png;*.bmp)\0*.png;*.bmp\0所有文件 (*.*)\0*.*\0\0";
    file[0] = 0;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g.hFrame;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = TS("选择要附加的图片", "Choose a picture to attach");
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameA(&ofn)) return 0;
    strncpy(outPath, file, (size_t)outLen - 1);
    outPath[outLen - 1] = 0;
    return 1;
}

static void modal_loop(HWND hDlg, HWND owner)
{
    MSG msg;
    EnableWindow(owner, FALSE);
    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);
    SetFocus(hDlg);
    while (IsWindow(hDlg) && GetMessageA(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageA(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
}

/* ================================================================== */
/* 通用单行输入对话框                                                  */
/* ================================================================== */
#define ID_INP_EDIT 5501
#define ID_INP_OK   5502
#define ID_INP_CANCEL 5503

typedef struct {
    HWND h, edit;
    int  ok;
    char *buf;          /* 调用方的缓冲区 */
    int  buflen;
} InpDlg;

static LRESULT CALLBACK InpProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    InpDlg *d = (InpDlg *)GetWindowLongPtrA(h, GWLP_USERDATA);
    switch (m) {
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    case WM_COMMAND:
        if (!d) break;
        if (LOWORD(w) == ID_INP_OK) {
            /* 必须在销毁窗口之前把文本取出来：
             * DestroyWindow 之后子控件已经没了，GetWindowText 只能拿到空串，
             * 调用方就会收到一个空名字（新建工作区“点了没反应”就是这个）。 */
            if (d->edit && d->buf && d->buflen > 0)
                GetWindowTextA(d->edit, d->buf, d->buflen);
            d->ok = 1;
            DestroyWindow(h);
            return 0;
        }
        if (LOWORD(w) == ID_INP_CANCEL) { DestroyWindow(h); return 0; }
        break;
    }
    return DefWindowProcA(h, m, w, l);
}

int dlg_input(HWND owner, const char *prompt, const char *title,
              char *buf, int buflen, int selectAll)
{
    static int registered = 0;
    WNDCLASSA wc;
    InpDlg d;
    HWND h;
    int rc;

    if (!registered) {
        ZeroMemory(&wc, sizeof(wc));
        wc.lpfnWndProc = InpProc;
        wc.hInstance = g.hInst;
        wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "NbInput";
        RegisterClassA(&wc);
        registered = 1;
    }
    h = CreateWindowExA(WS_EX_DLGMODALFRAME, "NbInput", title ? title : SM_SHORT_NAME,
        WS_POPUP | WS_CAPTION | WS_SYSMENU, 0, 0, 400, 150, owner, NULL, g.hInst, NULL);
    if (!h) return 0;
    ZeroMemory(&d, sizeof(d));
    d.h = h;
    d.buf = buf;
    d.buflen = buflen;
    SetWindowLongPtrA(h, GWLP_USERDATA, (LONG_PTR)&d);

    mk(h, "STATIC", prompt ? prompt : "", SS_LEFT, 12, 10, 372, 34, 0);
    d.edit = mk(h, "EDIT", buf ? buf : "", ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER,
                12, 48, 372, 22, ID_INP_EDIT);
    SendMessageA(d.edit, EM_SETLIMITTEXT, (WPARAM)(buflen - 1), 0);
    mk(h, "BUTTON", L(STR_OK), BS_DEFPUSHBUTTON | WS_TABSTOP, 218, 82, 80, 26, ID_INP_OK);
    mk(h, "BUTTON", L(STR_CANCEL), BS_PUSHBUTTON | WS_TABSTOP, 304, 82, 80, 26, ID_INP_CANCEL);

    sm_center_window(h);
    SetFocus(d.edit);
    if (selectAll) SendMessageA(d.edit, EM_SETSEL, 0, -1);
    modal_loop(h, owner);
    rc = d.ok;
    if (rc && buf && buflen > 0) sm_trim(buf);
    return rc;
}

/* ================================================================== */
/* 设置对话框                                                          */
/* ================================================================== */
#define ID_SET_URL     5001
#define ID_SET_MODEL   5002
#define ID_SET_KEY     5003
#define ID_SET_TIMEOUT 5004
#define ID_SET_TOOLS   5005
#define ID_SET_CONFIRM 5006
#define ID_SET_CERTDATE 5007
#define ID_SET_TEST    5008
#define ID_SET_OK      5009
#define ID_SET_CANCEL  5010
#define ID_SET_TOKENS  5011

typedef struct {
    HWND h;
    HWND url, model, key, timeout, tokens, chkTools, chkConfirm, chkCert;
    int  result;
} SettingsDlg;

static LRESULT CALLBACK SettingsProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    SettingsDlg *d = (SettingsDlg *)GetWindowLongPtrA(h, GWLP_USERDATA);
    switch (m) {
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    case WM_COMMAND:
        if (!d) break;
        switch (LOWORD(w)) {
        case ID_SET_OK: {
            char buf[512];
            GetWindowTextA(d->url, buf, sizeof(buf));
            sm_trim(buf);
            if (!buf[0]) { sm_error(h, "%s", L(STR_SET_URL_EMPTY)); return 0; }
            if (_strnicmp(buf, "http://", 7) && _strnicmp(buf, "https://", 8)) {
                sm_error(h, "%s", L(STR_SET_URL_BAD));
                return 0;
            }
            strncpy(g.cfg.endpoint, buf, sizeof(g.cfg.endpoint) - 1);
            g.cfg.endpoint[sizeof(g.cfg.endpoint) - 1] = 0;
            GetWindowTextA(d->model, g.cfg.model, sizeof(g.cfg.model));
            sm_trim(g.cfg.model);
            GetWindowTextA(d->key, g.cfg.apikey, sizeof(g.cfg.apikey));
            sm_trim(g.cfg.apikey);
            GetWindowTextA(d->timeout, buf, sizeof(buf));
            g.cfg.timeoutSec = (int)sm_to_long(buf, 60);
            if (g.cfg.timeoutSec < 10) g.cfg.timeoutSec = 10;
            if (g.cfg.timeoutSec > 300) g.cfg.timeoutSec = 300;
            GetWindowTextA(d->tokens, buf, sizeof(buf));
            g.cfg.maxtokens = (int)sm_to_long(buf, 1024);
            if (g.cfg.maxtokens < 64) g.cfg.maxtokens = 64;
            if (g.cfg.maxtokens > 8192) g.cfg.maxtokens = 8192;
            g.cfg.useTools = (SendMessageA(d->chkTools, BM_GETCHECK, 0, 0) == BST_CHECKED);
            g.cfg.confirmWrite = (SendMessageA(d->chkConfirm, BM_GETCHECK, 0, 0) == BST_CHECKED);
            g.cfg.ignoreCertDate = (SendMessageA(d->chkCert, BM_GETCHECK, 0, 0) == BST_CHECKED);
            sm_config_save(&g.cfg);
            {
                char b[200];
                _snprintf(b, sizeof(b), "%s: %s", L(STR_STATUS_MODEL),
                          g.cfg.model[0] ? g.cfg.model : L(STR_NONE));
                b[sizeof(b) - 1] = 0;
                sm_status_set(2, b);
            }
            d->result = 1;
            DestroyWindow(h);
            return 0;
        }
        case ID_SET_CANCEL:
            DestroyWindow(h);
            return 0;
        case ID_SET_TEST: {
            char rep[4096];
            char url[512], key[512];
            HCURSOR old = SetCursor(LoadCursorA(NULL, IDC_WAIT));
            GetWindowTextA(d->url, url, sizeof(url));
            GetWindowTextA(d->key, key, sizeof(key));
            SetWindowTextA(h, L(STR_SET_TESTING));
            sm_tls_test(url, key, 30,
                        SendMessageA(d->chkCert, BM_GETCHECK, 0, 0) == BST_CHECKED,
                        rep, sizeof(rep));
            SetWindowTextA(h, L(STR_SET_TITLE));
            SetCursor(old);
            MessageBoxA(h, rep, L(STR_SET_TEST_TITLE), MB_ICONINFORMATION | MB_OK);
            return 0;
        }
        }
        break;
    }
    return DefWindowProcA(h, m, w, l);
}

void dlg_settings_legacy(HWND owner)
{
    static int registered = 0;
    WNDCLASSA wc;
    SettingsDlg d;
    HWND h;
    char tmp[64];

    if (!registered) {
        ZeroMemory(&wc, sizeof(wc));
        wc.lpfnWndProc = SettingsProc;
        wc.hInstance = g.hInst;
        wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "NbSettings";
        RegisterClassA(&wc);
        registered = 1;
    }
    h = CreateWindowExA(WS_EX_DLGMODALFRAME, "NbSettings", L(STR_SET_TITLE),
        WS_POPUP | WS_CAPTION | WS_SYSMENU, 0, 0, 430, 340, owner, NULL, g.hInst, NULL);
    if (!h) return;
    ZeroMemory(&d, sizeof(d));
    d.h = h;
    SetWindowLongPtrA(h, GWLP_USERDATA, (LONG_PTR)&d);

    mk(h, "BUTTON", L(STR_SET_GROUP_SRV), BS_GROUPBOX, 8, 6, 406, 150, 0);
    mk(h, "STATIC", L(STR_SET_URL), SS_LEFT, 20, 28, 96, 18, 0);
    d.url = mk(h, "EDIT", "", ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER, 118, 26, 284, 21, ID_SET_URL);
    mk(h, "STATIC", L(STR_SET_MODEL), SS_LEFT, 20, 54, 96, 18, 0);
    d.model = mk(h, "EDIT", "", ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER, 118, 52, 284, 21, ID_SET_MODEL);
    mk(h, "STATIC", L(STR_SET_KEY), SS_LEFT, 20, 80, 96, 18, 0);
    d.key = mk(h, "EDIT", "", ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER, 118, 78, 284, 21, ID_SET_KEY);
    mk(h, "STATIC", L(STR_SET_TIMEOUT), SS_LEFT, 20, 106, 64, 18, 0);
    d.timeout = mk(h, "EDIT", "", ES_AUTOHSCROLL | ES_NUMBER | WS_TABSTOP | WS_BORDER, 88, 104, 46, 21, ID_SET_TIMEOUT);
    mk(h, "STATIC", L(STR_SET_TOKENS), SS_LEFT, 150, 106, 64, 18, 0);
    d.tokens = mk(h, "EDIT", "", ES_AUTOHSCROLL | ES_NUMBER | WS_TABSTOP | WS_BORDER, 218, 104, 60, 21, ID_SET_TOKENS);
    mk(h, "STATIC", L(STR_SET_TOKENS_HINT), SS_LEFT, 284, 106, 100, 18, 0);

    mk(h, "BUTTON", L(STR_SET_GROUP_OPT), BS_GROUPBOX, 8, 160, 406, 92, 0);
    d.chkTools   = mk(h, "BUTTON", L(STR_SET_TOOLS), BS_AUTOCHECKBOX | WS_TABSTOP,
                      20, 180, 380, 18, ID_SET_TOOLS);
    d.chkConfirm = mk(h, "BUTTON", L(STR_SET_CONFIRM), BS_AUTOCHECKBOX | WS_TABSTOP,
                      20, 202, 380, 18, ID_SET_CONFIRM);
    d.chkCert    = mk(h, "BUTTON", L(STR_SET_CERTDATE), BS_AUTOCHECKBOX | WS_TABSTOP,
                      20, 224, 380, 18, ID_SET_CERTDATE);

    mk(h, "BUTTON", L(STR_SET_TEST), BS_PUSHBUTTON | WS_TABSTOP, 8, 264, 100, 26, ID_SET_TEST);
    mk(h, "BUTTON", L(STR_OK), BS_DEFPUSHBUTTON | WS_TABSTOP, 246, 264, 80, 26, ID_SET_OK);
    mk(h, "BUTTON", L(STR_CANCEL), BS_PUSHBUTTON | WS_TABSTOP, 334, 264, 80, 26, ID_SET_CANCEL);

    SetWindowTextA(d.url, g.cfg.endpoint);
    SetWindowTextA(d.model, g.cfg.model);
    SetWindowTextA(d.key, g.cfg.apikey);
    _snprintf(tmp, sizeof(tmp), "%d", g.cfg.timeoutSec);
    SetWindowTextA(d.timeout, tmp);
    _snprintf(tmp, sizeof(tmp), "%d", g.cfg.maxtokens);
    SetWindowTextA(d.tokens, tmp);
    SendMessageA(d.chkTools, BM_SETCHECK, g.cfg.useTools ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(d.chkConfirm, BM_SETCHECK, g.cfg.confirmWrite ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(d.chkCert, BM_SETCHECK, g.cfg.ignoreCertDate ? BST_CHECKED : BST_UNCHECKED, 0);

    sm_center_window(h);
    modal_loop(h, owner);
}

/* ================================================================== */
/* 激活对话框                                                          */
/* ================================================================== */
#define ID_ACT_CDK   5101
#define ID_ACT_OK    5102
#define ID_ACT_LATER 5103

typedef struct { HWND h, cdk, info; int ok; } ActDlg;

static LRESULT CALLBACK ActProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    ActDlg *d = (ActDlg *)GetWindowLongPtrA(h, GWLP_USERDATA);
    switch (m) {
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    case WM_COMMAND:
        if (!d) break;
        if (LOWORD(w) == ID_ACT_OK) {
            char key[64], err[512];
            int ok;
            GetWindowTextA(d->cdk, key, sizeof(key));
            ok = sm_license_activate(key, &g.lic, err, sizeof(err));
            if (ok) {
                MessageBoxA(h, L(STR_ACT_OKMSG), SM_SHORT_NAME,
                            MB_ICONINFORMATION | MB_OK);
                sm_update_license_ui();
                d->ok = 1;
                DestroyWindow(h);
            } else {
                MessageBoxA(h, err[0] ? err : L(STR_ACT_BADKEY), SM_SHORT_NAME,
                            MB_ICONSTOP | MB_OK);
            }
            return 0;
        }
        if (LOWORD(w) == ID_ACT_LATER) { DestroyWindow(h); return 0; }
        break;
    }
    return DefWindowProcA(h, m, w, l);
}

void dlg_activate(HWND owner)
{
    static int registered = 0;
    WNDCLASSA wc;
    ActDlg d;
    HWND h;
    Sb info;
    char *txt;
    char ed[48], st[96], line[200];

    if (!registered) {
        ZeroMemory(&wc, sizeof(wc));
        wc.lpfnWndProc = ActProc;
        wc.hInstance = g.hInst;
        wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "NbActivate";
        RegisterClassA(&wc);
        registered = 1;
    }
    sm_license_read(&g.lic);

    h = CreateWindowExA(WS_EX_DLGMODALFRAME, "NbActivate", L(STR_ACT_TITLE),
        WS_POPUP | WS_CAPTION | WS_SYSMENU, 0, 0, 410, 330, owner, NULL, g.hInst, NULL);
    if (!h) return;
    ZeroMemory(&d, sizeof(d));
    d.h = h;
    SetWindowLongPtrA(h, GWLP_USERDATA, (LONG_PTR)&d);

    mk(h, "STATIC", L(STR_ACT_PRODUCT), SS_LEFT, 12, 10, 380, 18, 0);
    mk(h, "STATIC", L(STR_ACT_PROMPT), SS_LEFT, 12, 38, 260, 18, 0);
    d.cdk = mk(h, "EDIT", "", ES_AUTOHSCROLL | ES_UPPERCASE | WS_TABSTOP | WS_BORDER,
               12, 58, 380, 22, ID_ACT_CDK);
    SendMessageA(d.cdk, EM_SETLIMITTEXT, 40, 0);
    mk(h, "STATIC", L(STR_ACT_FORMAT), SS_LEFT, 12, 84, 380, 16, 0);
    mk(h, "STATIC", L(STR_ACT_EXAMPLE), SS_LEFT, 12, 100, 380, 16, 0);

    mk(h, "BUTTON", L(STR_ACT_GROUP), BS_GROUPBOX, 12, 122, 380, 124, 0);
    sb_init(&info);
    strncpy(ed, nb_edition_name(g.lic.edition), sizeof(ed) - 1);
    nb_lic_text(&g.lic, st, sizeof(st));
    _snprintf(line, sizeof(line), L(STR_ACT_EDITION), ed);
    line[sizeof(line) - 1] = 0;
    sb_printf(&info, "%s\r\n", line);
    _snprintf(line, sizeof(line), L(STR_ACT_STATE), st);
    line[sizeof(line) - 1] = 0;
    sb_printf(&info, "%s\r\n", line);
    {
        char td[32];
        time_t t = (time_t)g.lic.install_time;
        struct tm *tmx = localtime(&t);
        if (tmx) {
            _snprintf(td, sizeof(td), "%04d-%02d-%02d", tmx->tm_year + 1900,
                      tmx->tm_mon + 1, tmx->tm_mday);
            td[sizeof(td) - 1] = 0;
            _snprintf(line, sizeof(line), L(STR_ACT_INSTALLDATE), td);
            line[sizeof(line) - 1] = 0;
            sb_printf(&info, "%s\r\n", line);
        }
    }
    if (g.lic.state == SM_LIC_TRIAL) {
        _snprintf(line, sizeof(line), L(STR_ACT_TRIALLEFT),
                  g.lic.trial_days_left, g.lic.trial_msgs_left);
        line[sizeof(line) - 1] = 0;
        sb_printf(&info, "%s\r\n", line);
    }
    if (g.lic.cdk[0]) {
        _snprintf(line, sizeof(line), L(STR_ACT_REGKEY), g.lic.cdk);
        line[sizeof(line) - 1] = 0;
        sb_printf(&info, "%s\r\n", line);
    }
    if (g.lic.tampered) sb_printf(&info, "%s\r\n", L(STR_ACT_WARN));
    txt = sb_take(&info);
    d.info = mk(h, "EDIT", "",
        ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL | ES_LEFT | WS_BORDER,
        20, 142, 364, 94, 0);
    if (d.info) {
        /* 先设字体再灌文本，否则行高按系统字体算过，重绘会叠字 */
        SendMessageA(d.info, WM_SETFONT,
                     (WPARAM)(g.hFontDlg ? g.hFontDlg : g.hFontUI), MAKELPARAM(TRUE, 0));
        SetWindowTextA(d.info, txt);
        SendMessageA(d.info, EM_SETSEL, 0, 0);
        sm_fix_scroll_repaint(d.info);
    }
    sm_free(txt);

    mk(h, "BUTTON", L(STR_ACT_GO), BS_DEFPUSHBUTTON | WS_TABSTOP, 216, 256, 82, 26, ID_ACT_OK);
    mk(h, "BUTTON", L(STR_ACT_LATER), BS_PUSHBUTTON | WS_TABSTOP, 304, 256, 82, 26, ID_ACT_LATER);

    sm_center_window(h);
    SetFocus(d.cdk);
    modal_loop(h, owner);
}

/* ================================================================== */
/* 关于                                                                */
/* ================================================================== */
#define ID_ABOUT_OK   5201
#define ID_ABOUT_SYS  5202

static LRESULT CALLBACK AboutProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    case WM_COMMAND:
        if (LOWORD(w) == ID_ABOUT_OK) { DestroyWindow(h); return 0; }
        if (LOWORD(w) == ID_ABOUT_SYS) { dlg_sysinfo(h); return 0; }
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT r, lr;
        GetClientRect(h, &r);
        FillRect(dc, &r, (HBRUSH)(COLOR_BTNFACE + 1));
        lr.left = 0; lr.top = 0; lr.right = r.right; lr.bottom = 58;
        {
            HBRUSH br = CreateSolidBrush(RGB(0, 0, 128));
            FillRect(dc, &lr, br);
            DeleteObject(br);
        }
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        SelectObject(dc, g.hFontTitle);
        TextOutA(dc, 14, 10, SM_PRODUCT_NAME, (int)strlen(SM_PRODUCT_NAME));
        SelectObject(dc, g.hFontUI);
        TextOutA(dc, 16, 34, L(STR_ABOUT_TAGLINE), (int)strlen(L(STR_ABOUT_TAGLINE)));
        EndPaint(h, &ps);
        return 0;
    }
    }
    return DefWindowProcA(h, m, w, l);
}

void dlg_about(HWND owner)
{
    static int registered = 0;
    WNDCLASSA wc;
    HWND h;
    char b[320];
    char ls[96], lb[220];

    if (!registered) {
        ZeroMemory(&wc, sizeof(wc));
        wc.lpfnWndProc = AboutProc;
        wc.hInstance = g.hInst;
        wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
        wc.lpszClassName = "NbAbout";
        RegisterClassA(&wc);
        registered = 1;
    }
    h = CreateWindowExA(WS_EX_DLGMODALFRAME, "NbAbout", L(STR_ABOUT_TITLE),
        WS_POPUP | WS_CAPTION | WS_SYSMENU, 0, 0, 430, 300, owner, NULL, g.hInst, NULL);
    if (!h) return;

    mk(h, "STATIC", SM_COMPANY, SS_LEFT, 16, 68, 390, 18, 0);
    _snprintf(b, sizeof(b), "Version %s", SM_VERSION_STR);
    b[sizeof(b) - 1] = 0;
    mk(h, "STATIC", b, SS_LEFT, 16, 86, 390, 18, 0);
    mk(h, "STATIC", SM_COPYRIGHT, SS_LEFT, 16, 104, 390, 18, 0);
    mk(h, "STATIC", L(STR_ABOUT_TLS1), SS_LEFT, 16, 132, 390, 16, 0);
    mk(h, "STATIC", L(STR_ABOUT_TLS2), SS_LEFT, 16, 148, 390, 16, 0);
    mk(h, "STATIC", L(STR_ABOUT_TLS3), SS_LEFT, 16, 164, 390, 16, 0);
    nb_lic_text(&g.lic, ls, sizeof(ls));
    _snprintf(lb, sizeof(lb), L(STR_ABOUT_LICSTATE), ls);
    lb[sizeof(lb) - 1] = 0;
    mk(h, "STATIC", lb, SS_LEFT, 16, 190, 390, 16, 0);

    mk(h, "BUTTON", L(STR_ABOUT_SYSINFO), BS_PUSHBUTTON | WS_TABSTOP, 16, 220, 120, 26, ID_ABOUT_SYS);
    mk(h, "BUTTON", L(STR_OK), BS_DEFPUSHBUTTON | WS_TABSTOP, 330, 220, 80, 26, ID_ABOUT_OK);

    sm_center_window(h);
    modal_loop(h, owner);
}

/* ================================================================== */
/* 系统信息                                                            */
/* ================================================================== */
#define ID_SI_COPY 5301
#define ID_SI_OK   5302
#define ID_SI_SAVE 5303

typedef struct { HWND h, edit; } SysDlg;

static LRESULT CALLBACK SysInfoProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    SysDlg *d = (SysDlg *)GetWindowLongPtrA(h, GWLP_USERDATA);
    switch (m) {
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    case WM_COMMAND:
        if (!d) break;
        if (LOWORD(w) == ID_SI_OK) { DestroyWindow(h); return 0; }
        if (LOWORD(w) == ID_SI_COPY) {
            int n = GetWindowTextLengthA(d->edit);
            char *t = (char *)sm_alloc((size_t)n + 2);
            GetWindowTextA(d->edit, t, n + 1);
            if (OpenClipboard(h)) {
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
            return 0;
        }
        if (LOWORD(w) == ID_SI_SAVE) {
            OPENFILENAMEA ofn;
            char path[MAX_PATH] = "SYSINFO.TXT";
            int n;
            char *t;
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = h;
            ofn.lpstrFilter = "Text files (*.txt)\0*.txt\0All files (*.*)\0*.*\0";
            ofn.lpstrFile = path;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrDefExt = "txt";
            ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
            if (!GetSaveFileNameA(&ofn)) return 0;
            n = GetWindowTextLengthA(d->edit);
            t = (char *)sm_alloc((size_t)n + 2);
            GetWindowTextA(d->edit, t, n + 1);
            if (!sm_write_file(path, t, (long)strlen(t)))
                sm_error(h, "Cannot write: %s", path);
            sm_free(t);
            return 0;
        }
        break;
    }
    return DefWindowProcA(h, m, w, l);
}

void dlg_sysinfo(HWND owner)
{
    static int registered = 0;
    WNDCLASSA wc;
    SysDlg d;
    HWND h;
    Sb b;
    char *txt;

    if (!registered) {
        ZeroMemory(&wc, sizeof(wc));
        wc.lpfnWndProc = SysInfoProc;
        wc.hInstance = g.hInst;
        wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "NbSysInfo";
        RegisterClassA(&wc);
        registered = 1;
    }
    h = CreateWindowExA(WS_EX_DLGMODALFRAME, "NbSysInfo", L(STR_SI_TITLE),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME, 0, 0, 500, 440,
        owner, NULL, g.hInst, NULL);
    if (!h) return;
    ZeroMemory(&d, sizeof(d));
    d.h = h;
    SetWindowLongPtrA(h, GWLP_USERDATA, (LONG_PTR)&d);

    sb_init(&b);
    tools_sysinfo_text(&b);
    txt = sb_take(&b);
    d.edit = mk(h, "EDIT", "",
        ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL | ES_LEFT | WS_BORDER,
        8, 8, 480, 360, 0);
    if (d.edit) {
        SendMessageA(d.edit, WM_SETFONT, (WPARAM)g.hFontMono, MAKELPARAM(TRUE, 0));
        SetWindowTextA(d.edit, txt);
        SendMessageA(d.edit, EM_SETSEL, 0, 0);
        sm_fix_scroll_repaint(d.edit);
    }
    sm_free(txt);
    mk(h, "BUTTON", L(STR_SI_COPY), BS_PUSHBUTTON | WS_TABSTOP, 8, 376, 120, 26, ID_SI_COPY);
    mk(h, "BUTTON", L(STR_SI_SAVEAS), BS_PUSHBUTTON | WS_TABSTOP, 134, 376, 110, 26, ID_SI_SAVE);
    mk(h, "BUTTON", L(STR_CLOSE), BS_DEFPUSHBUTTON | WS_TABSTOP, 408, 376, 80, 26, ID_SI_OK);

    sm_center_window(h);
    modal_loop(h, owner);
}

/* ================================================================== */
/* “正在连接”浮动窗口                                                  */
/* ================================================================== */
#define ID_CONN_TEXT 5401
#define ID_CONN_BAR  5402
#define ID_CONN_TMR  5403

static HWND g_connWnd = NULL;
static HWND g_connText = NULL;
static HWND g_connBar = NULL;
static int  g_connPos = 0;

static LRESULT CALLBACK ConnProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_CREATE:
        g_connText = CreateWindowExA(0, "STATIC", L(STR_CONNECTING),
            WS_CHILD | WS_VISIBLE | SS_LEFT, 10, 8, 270, 18, h,
            (HMENU)ID_CONN_TEXT, g.hInst, NULL);
        SendMessageA(g_connText, WM_SETFONT, (WPARAM)g.hFontUI, MAKELPARAM(TRUE, 0));
        g_connBar = CreateWindowExA(0, PROGRESS_CLASSA, NULL, WS_CHILD | WS_VISIBLE,
            10, 34, 270, 16, h, (HMENU)ID_CONN_BAR, g.hInst, NULL);
        SendMessageA(g_connBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SetTimer(h, ID_CONN_TMR, 60, NULL);
        return 0;
    case WM_TIMER:
        g_connPos += 7;
        if (g_connPos > 100) g_connPos = 0;
        SendMessageA(g_connBar, PBM_SETPOS, (WPARAM)g_connPos, 0);
        return 0;
    case WM_CLOSE:
        return 0;
    case WM_DESTROY:
        KillTimer(h, ID_CONN_TMR);
        g_connWnd = NULL;
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

void dlg_connecting(HWND owner)
{
    static int registered = 0;
    WNDCLASSA wc;
    if (g_connWnd) return;
    if (!registered) {
        ZeroMemory(&wc, sizeof(wc));
        wc.lpfnWndProc = ConnProc;
        wc.hInstance = g.hInst;
        wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "NbConnecting";
        RegisterClassA(&wc);
        registered = 1;
    }
    g_connPos = 0;
    g_connWnd = CreateWindowExA(WS_EX_TOOLWINDOW, "NbConnecting", L(STR_PLEASE_WAIT),
        WS_POPUP | WS_BORDER, 0, 0, 290, 62, owner, NULL, g.hInst, NULL);
    if (g_connWnd) {
        sm_center_window(g_connWnd);
        ShowWindow(g_connWnd, SW_SHOW);
        UpdateWindow(g_connWnd);
    }
}

void dlg_connecting_set(const char *text)
{
    if (g_connText && text) SetWindowTextA(g_connText, text);
}

void dlg_connecting_close(void)
{
    if (g_connWnd) {
        DestroyWindow(g_connWnd);
        g_connWnd = NULL;
        g_connText = NULL;
        g_connBar = NULL;
    }
}
