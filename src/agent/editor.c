/* Axiom NeuroBench 2003 - 内置文本编辑器
 * 一个独立的顶层窗口，风格照着记事本 / WordPad 来。 */
#include "agent.h"

#define IDC_ED_EDIT    6001
#define IDC_ED_STAT    6002
#define IDC_ED_FIND    6003

#define CMD_ED_NEW     6101
#define CMD_ED_OPEN    6102
#define CMD_ED_SAVE    6103
#define CMD_ED_SAVEAS  6104
#define CMD_ED_EXIT    6105
#define CMD_ED_UNDO    6110
#define CMD_ED_CUT     6111
#define CMD_ED_COPY    6112
#define CMD_ED_PASTE   6113
#define CMD_ED_DEL     6114
#define CMD_ED_SELALL  6115
#define CMD_ED_TIME    6116
#define CMD_ED_FIND    6120
#define CMD_ED_FINDNXT 6121
#define CMD_ED_WRAP    6130
#define CMD_ED_FONT    6131
#define CMD_ED_ABOUT   6140
#define CMD_ED_HELP    6141

typedef struct {
    HWND  hFrame, hEdit, hStat;
    char  path[MAX_PATH];
    int   modified;
    int   wrap;
    LOGFONTA lf;
    UINT  findMsg;
    FINDREPLACEA fr;
    char  findWhat[128];
} EdWin;

static EdWin *g_edwins[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static HWND g_lastEditor = NULL;

/* ------------------------------------------------------------------ */
static EdWin *ed_from(HWND h)
{
    return (EdWin *)GetWindowLongPtrA(h, GWLP_USERDATA);
}

static void ed_update_title(EdWin *e)
{
    char t[420];
    const char *nm = e->path[0] ? e->path : L(STR_EDITOR_UNTITLED);
    const char *base = strrchr(nm, 92);
    if (base) nm = base + 1;
    _snprintf(t, sizeof(t), "%s%s - %s", nm, e->modified ? " *" : "", L(STR_EDITOR_TITLE));
    t[sizeof(t) - 1] = 0;
    SetWindowTextA(e->hFrame, t);
}

static void ed_update_status(EdWin *e)
{
    int line = 1, col = 1, i;
    int n = GetWindowTextLengthA(e->hEdit);
    char *txt;
    char b[160];
    int panes[1];
    RECT rc;
    char fmt[96];

    txt = (char *)sm_alloc((size_t)n + 2);
    GetWindowTextA(e->hEdit, txt, n + 1);
    {
        DWORD s = 0, en = 0;
        SendMessageA(e->hEdit, EM_GETSEL, (WPARAM)&s, (LPARAM)&en);
        for (i = 0; i < (int)s && txt[i]; i++) {
            if (txt[i] == 10) { line++; col = 1; }
            else col++;
        }
    }
    sm_free(txt);
    if (lang_get() == LANG_EN)
        _snprintf(fmt, sizeof(fmt), "  Ln %d, Col %d      %d chars", line, col, n);
    else
        _snprintf(fmt, sizeof(fmt), "  行 %d，列 %d      字数 %d", line, col, n);
    _snprintf(b, sizeof(b), fmt, line, col, n);
    b[sizeof(b) - 1] = 0;
    GetClientRect(e->hFrame, &rc);
    panes[0] = rc.right - 8;
    SendMessageA(e->hStat, SB_SETPARTS, 1, (LPARAM)panes);
    SendMessageA(e->hStat, SB_SETTEXTA, 0, (LPARAM)b);
    ed_update_title(e);
}

static void ed_save(EdWin *e, int saveAs)
{
    OPENFILENAMEA ofn;
    char path[MAX_PATH];
    char *txt;
    int n;

    strcpy(path, e->path);
    if (saveAs || !path[0]) {
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = e->hFrame;
        ofn.lpstrFilter = "Text files (*.txt)\0*.txt\0Batch files (*.bat)\0*.bat\0"
                          "All files (*.*)\0*.*\0";
        ofn.lpstrFile = path;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrDefExt = "txt";
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
        ofn.lpstrTitle = L(STR_SAVE);
        if (!GetSaveFileNameA(&ofn)) return;
    }
    n = GetWindowTextLengthA(e->hEdit);
    txt = (char *)sm_alloc((size_t)n + 2);
    GetWindowTextA(e->hEdit, txt, n + 1);
    if (sm_write_file(path, txt, (long)strlen(txt))) {
        strcpy(e->path, path);
        e->modified = 0;
        ed_update_title(e);
    } else {
        sm_error(e->hFrame, "Cannot save: %s", path);
    }
    sm_free(txt);
}

static void ed_open_file(EdWin *e, const char *path)
{
    long len = 0;
    char *data;
    if (!path || !path[0]) return;
    data = sm_read_file(path, 8 * 1024 * 1024, &len);
    if (!data) { sm_error(e->hFrame, "Cannot open: %s", path); return; }
    SetWindowTextA(e->hEdit, data);
    sm_free(data);
    strcpy(e->path, path);
    e->modified = 0;
    ed_update_status(e);
}

/* ------------------------------------------------------------------ */
static void ed_command(EdWin *e, int id)
{
    switch (id) {
    case CMD_ED_NEW:
        if (e->modified) {
            int r = MessageBoxA(e->hFrame, L(STR_EDITOR_SAVEPROMPT), L(STR_EDITOR_TITLE),
                                MB_ICONQUESTION | MB_YESNOCANCEL);
            if (r == IDCANCEL) return;
            if (r == IDYES) ed_save(e, e->path[0] ? 0 : 1);
        }
        SetWindowTextA(e->hEdit, "");
        e->path[0] = 0;
        e->modified = 0;
        ed_update_status(e);
        break;
    case CMD_ED_OPEN: {
        OPENFILENAMEA ofn;
        char path[MAX_PATH];
        path[0] = 0;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = e->hFrame;
        ofn.lpstrFilter = "Text files (*.txt)\0*.txt\0All files (*.*)\0*.*\0";
        ofn.lpstrFile = path;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (GetOpenFileNameA(&ofn)) ed_open_file(e, path);
        break;
    }
    case CMD_ED_SAVE:   ed_save(e, e->path[0] ? 0 : 1); break;
    case CMD_ED_SAVEAS: ed_save(e, 1); break;
    case CMD_ED_EXIT:   PostMessageA(e->hFrame, WM_CLOSE, 0, 0); break;
    case CMD_ED_UNDO:   SendMessageA(e->hEdit, EM_UNDO, 0, 0); break;
    case CMD_ED_CUT:    SendMessageA(e->hEdit, WM_CUT, 0, 0); break;
    case CMD_ED_COPY:   SendMessageA(e->hEdit, WM_COPY, 0, 0); break;
    case CMD_ED_PASTE:  SendMessageA(e->hEdit, WM_PASTE, 0, 0); break;
    case CMD_ED_DEL:    SendMessageA(e->hEdit, WM_CLEAR, 0, 0); break;
    case CMD_ED_SELALL: SendMessageA(e->hEdit, EM_SETSEL, 0, -1); break;
    case CMD_ED_TIME: {
        char t[64];
        sm_now_str(t, sizeof(t));
        SendMessageA(e->hEdit, EM_REPLACESEL, TRUE, (LPARAM)t);
        break;
    }
    case CMD_ED_FIND:
        ZeroMemory(&e->fr, sizeof(e->fr));
        e->fr.lStructSize = sizeof(e->fr);
        e->fr.hwndOwner = e->hFrame;
        e->fr.lpstrFindWhat = e->findWhat;
        e->fr.wFindWhatLen = sizeof(e->findWhat) - 1;
        e->fr.Flags = FR_DOWN;
        FindTextA(&e->fr);
        break;
    case CMD_ED_WRAP:
        e->wrap = !e->wrap;
        {
            DWORD st = (DWORD)GetWindowLongA(e->hEdit, GWL_STYLE);
            if (e->wrap) SetWindowLongA(e->hEdit, GWL_STYLE, (LONG)(st & ~WS_HSCROLL));
            else SetWindowLongA(e->hEdit, GWL_STYLE, (LONG)(st | WS_HSCROLL));
        }
        break;
    case CMD_ED_FONT: {
        CHOOSEFONTA cf;
        ZeroMemory(&cf, sizeof(cf));
        cf.lStructSize = sizeof(cf);
        cf.hwndOwner = e->hFrame;
        cf.lpLogFont = &e->lf;
        cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT;
        if (ChooseFontA(&cf)) {
            HFONT hf = CreateFontIndirectA(&e->lf);
            if (hf) {
                HFONT old = (HFONT)SendMessageA(e->hEdit, WM_GETFONT, 0, 0);
                SendMessageA(e->hEdit, WM_SETFONT, (WPARAM)hf, MAKELPARAM(TRUE, 0));
                if (old && old != g.hFontUI) DeleteObject(old);
            }
        }
        break;
    }
    case CMD_ED_ABOUT:
        sm_info(e->hFrame, "%s %s\r\n\r\n%s", L(STR_EDITOR_TITLE), SM_VERSION_STR, SM_COPYRIGHT);
        break;
    case CMD_ED_HELP:
        sm_open_help(e->hFrame, lang_get() == LANG_EN ? "en_editor.htm" : "editor.htm");
        break;
    }
}

/* ------------------------------------------------------------------ */
static HMENU ed_build_menu(void)
{
    HMENU mb = CreateMenu();
    HMENU m;
    int en = (lang_get() == LANG_EN);

    m = CreatePopupMenu();
    AppendMenuA(m, MF_STRING, CMD_ED_NEW,    en ? "&New\tCtrl+N" : "新建(&N)\tCtrl+N");
    AppendMenuA(m, MF_STRING, CMD_ED_OPEN,   en ? "&Open...\tCtrl+O" : "打开(&O)...\tCtrl+O");
    AppendMenuA(m, MF_STRING, CMD_ED_SAVE,   en ? "&Save\tCtrl+S" : "保存(&S)\tCtrl+S");
    AppendMenuA(m, MF_STRING, CMD_ED_SAVEAS, en ? "Save &As..." : "另存为(&A)...");
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    AppendMenuA(m, MF_STRING, CMD_ED_EXIT,   en ? "E&xit" : "退出(&X)");
    AppendMenuA(mb, MF_POPUP, (UINT_PTR)m, en ? "&File" : "文件(&F)");

    m = CreatePopupMenu();
    AppendMenuA(m, MF_STRING, CMD_ED_UNDO,   en ? "&Undo\tCtrl+Z" : "撤消(&U)\tCtrl+Z");
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    AppendMenuA(m, MF_STRING, CMD_ED_CUT,    en ? "Cu&t\tCtrl+X" : "剪切(&T)\tCtrl+X");
    AppendMenuA(m, MF_STRING, CMD_ED_COPY,   en ? "&Copy\tCtrl+C" : "复制(&C)\tCtrl+C");
    AppendMenuA(m, MF_STRING, CMD_ED_PASTE,  en ? "&Paste\tCtrl+V" : "粘贴(&P)\tCtrl+V");
    AppendMenuA(m, MF_STRING, CMD_ED_DEL,    en ? "De&lete\tDel" : "删除(&L)\tDel");
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    AppendMenuA(m, MF_STRING, CMD_ED_SELALL, en ? "Select &All\tCtrl+A" : "全选(&A)\tCtrl+A");
    AppendMenuA(m, MF_STRING, CMD_ED_TIME,   en ? "Insert Date/&Time\tF5" : "插入日期时间(&D)\tF5");
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    AppendMenuA(m, MF_STRING, CMD_ED_FIND,   en ? "&Find...\tCtrl+F" : "查找(&F)...\tCtrl+F");
    AppendMenuA(mb, MF_POPUP, (UINT_PTR)m, en ? "&Edit" : "编辑(&E)");

    m = CreatePopupMenu();
    AppendMenuA(m, MF_STRING, CMD_ED_WRAP, en ? "&Word Wrap" : "自动换行(&W)");
    AppendMenuA(m, MF_STRING, CMD_ED_FONT, en ? "&Font..." : "字体(&F)...");
    AppendMenuA(mb, MF_POPUP, (UINT_PTR)m, en ? "F&ormat" : "格式(&O)");

    m = CreatePopupMenu();
    AppendMenuA(m, MF_STRING, CMD_ED_HELP,  en ? "&Help Topics\tF1" : "帮助目录(&H)\tF1");
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    AppendMenuA(m, MF_STRING, CMD_ED_ABOUT, en ? "&About..." : "关于编辑器(&A)...");
    AppendMenuA(mb, MF_POPUP, (UINT_PTR)m, en ? "&Help" : "帮助(&H)");
    return mb;
}

static LRESULT CALLBACK EdProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    EdWin *e = ed_from(h);

    switch (m) {
    case WM_SIZE:
        if (e && e->hEdit) {
            SendMessageA(e->hStat, WM_SIZE, 0, 0);
            MoveWindow(e->hEdit, 0, 0, LOWORD(l), HIWORD(l), TRUE);
            ed_update_status(e);
        }
        return 0;

    case WM_SETFOCUS:
        if (e && e->hEdit) SetFocus(e->hEdit);
        return 0;

    case WM_COMMAND:
        if (e) ed_command(e, LOWORD(w));
        return 0;

    case WM_CLOSE:
        if (e) {
            if (e->modified) {
                int r = MessageBoxA(h, L(STR_EDITOR_SAVEPROMPT), L(STR_EDITOR_TITLE),
                                    MB_ICONQUESTION | MB_YESNOCANCEL);
                if (r == IDCANCEL) return 0;
                if (r == IDYES) ed_save(e, e->path[0] ? 0 : 1);
            }
            {
                int i;
                for (i = 0; i < 8; i++) if (g_edwins[i] == e) g_edwins[i] = NULL;
            }
            if (g_lastEditor == h) g_lastEditor = NULL;
        }
        DestroyWindow(h);
        return 0;

    case WM_DESTROY:
        if (e) sm_free(e);
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

static WNDPROC g_oldEdProc = NULL;

static LRESULT CALLBACK EdEditProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    HWND frame = GetParent(h);
    EdWin *e = frame ? ed_from(frame) : NULL;
    if (e) {
        if (m == WM_KEYDOWN || m == WM_CHAR || m == WM_PASTE || m == WM_CUT ||
            m == WM_UNDO || m == WM_CLEAR)
            e->modified = 1;
        if (m == WM_KEYUP || m == WM_LBUTTONUP)
            ed_update_status(e);
    }
    return CallWindowProcA(g_oldEdProc, h, m, w, l);
}

/* ------------------------------------------------------------------ */
void editor_refresh_strings(void)
{
    int i;
    for (i = 0; i < 8; i++) {
        if (!g_edwins[i]) continue;
        {
            HMENU nm = ed_build_menu();
            HMENU old = GetMenu(g_edwins[i]->hFrame);
            SetMenu(g_edwins[i]->hFrame, nm);
            if (old) DestroyMenu(old);
            DrawMenuBar(g_edwins[i]->hFrame);
            ed_update_status(g_edwins[i]);
        }
    }
}

void editor_open(const char *path)
{
    static int registered = 0;
    WNDCLASSA wc;
    EdWin *e;
    HWND h;
    int slot = -1, i;
    RECT rc;

    for (i = 0; i < 8; i++) if (!g_edwins[i]) { slot = i; break; }
    if (slot < 0) { sm_info(g.hFrame, "Too many editor windows (max 8)."); return; }

    if (!registered) {
        ZeroMemory(&wc, sizeof(wc));
        wc.lpfnWndProc = EdProc;
        wc.hInstance = g.hInst;
        wc.hIcon = LoadIconA(g.hInst, MAKEINTRESOURCEA(101));
        wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "NeuroBenchEditor";
        RegisterClassA(&wc);
        registered = 1;
    }

    e = (EdWin *)sm_calloc(sizeof(EdWin));
    {
        char t[200];
        _snprintf(t, sizeof(t), "%s - %s", L(STR_EDITOR_UNTITLED), L(STR_EDITOR_TITLE));
        t[sizeof(t) - 1] = 0;
        h = CreateWindowExA(0, "NeuroBenchEditor", t,
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 660, 500,
            g.hFrame, ed_build_menu(), g.hInst, NULL);
    }
    if (!h) { sm_free(e); return; }
    SetWindowLongPtrA(h, GWLP_USERDATA, (LONG_PTR)e);
    e->hFrame = h;
    e->modified = 0;
    e->wrap = 1;
    ZeroMemory(&e->lf, sizeof(e->lf));
    {
        LOGFONTA lf;
        if (GetObjectA(g.hFontMono, sizeof(lf), &lf)) e->lf = lf;
        else {
            e->lf.lfHeight = -13;
            strcpy(e->lf.lfFaceName, "宋体");
        }
    }

    e->hEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "RichEdit20A", "",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL |
        ES_WANTRETURN | ES_NOHIDESEL | WS_VSCROLL, 0, 0, 10, 10,
        h, (HMENU)IDC_ED_EDIT, g.hInst, NULL);
    if (!e->hEdit) {
        e->hEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL |
            ES_WANTRETURN | ES_NOHIDESEL | WS_VSCROLL, 0, 0, 10, 10,
            h, (HMENU)IDC_ED_EDIT, g.hInst, NULL);
    }
    {
        HFONT hf = CreateFontIndirectA(&e->lf);
        SendMessageA(e->hEdit, WM_SETFONT, (WPARAM)(hf ? hf : g.hFontMono), MAKELPARAM(TRUE, 0));
    }
    e->hStat = CreateWindowExA(0, STATUSCLASSNAMEA, NULL,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | SBARS_SIZEGRIP, 0, 0, 0, 0,
        h, (HMENU)IDC_ED_STAT, g.hInst, NULL);
    SendMessageA(e->hStat, WM_SETFONT, (WPARAM)g.hFontUI, MAKELPARAM(TRUE, 0));

    g_oldEdProc = (WNDPROC)SetWindowLongPtrA(e->hEdit, GWLP_WNDPROC, (LONG_PTR)EdEditProc);
    e->findMsg = RegisterWindowMessageA(FINDMSGSTRING);

    g_edwins[slot] = e;
    g_lastEditor = h;

    GetClientRect(h, &rc);
    SendMessageA(e->hStat, WM_SIZE, 0, 0);
    MoveWindow(e->hEdit, 0, 0, rc.right, rc.bottom - 22, TRUE);

    ShowWindow(h, SW_SHOW);
    UpdateWindow(h);
    if (path && path[0]) ed_open_file(e, path);
    else ed_update_status(e);
    SetFocus(e->hEdit);
}

HWND editor_last_window(void)
{
    return g_lastEditor;
}
