/* Axiom NeuroBench 2003 - 对话显示区 / 输入区 / 对话历史 */
#include "agent.h"

static const char *g_editClass = "RichEdit20A";
static UINT  g_findMsg = 0;
static FINDREPLACEA g_fr;
static char  g_findWhat[128];
static UINT  g_findFlags = FR_DOWN;

/* ------------------------------------------------------------------ */
static void re_setfmt(HWND h, COLORREF col, int bold, int italic, HFONT font, int sz)
{
    CHARFORMAT2A cf;
    LOGFONTA lf;
    if (_stricmp(g_editClass, "EDIT") == 0) return;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR | CFM_BOLD | CFM_ITALIC | CFM_FACE | CFM_SIZE | CFM_CHARSET;
    cf.dwEffects = (bold ? CFE_BOLD : 0) | (italic ? CFE_ITALIC : 0);
    cf.crTextColor = col;
    cf.yHeight = (LONG)(sz * 15);
    cf.bCharSet = DEFAULT_CHARSET;
    ZeroMemory(&lf, sizeof(lf));
    if (font && GetObjectA(font, sizeof(lf), &lf))
        strncpy(cf.szFaceName, lf.lfFaceName, LF_FACESIZE - 1);
    SendMessageA(h, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
}

static void re_append(HWND h, const char *text, COLORREF col, int bold, int italic,
                      HFONT font, int sz)
{
    int len;

    if (!text) return;

    /* Win98 的 riched20 v2 / EDIT：只读控件会拒绝 EM_REPLACESEL； */
    /* 而且它把 EM_SETSEL(-1,-1) 理解成“全选”而不是“移到末尾”，       */
    /* 会导致每次追加都覆盖全文。所以：临时解除只读 + 全部用显式下标。   */
    SendMessageA(h, EM_SETREADONLY, FALSE, 0);

    len = GetWindowTextLengthA(h);
    if (len < 0) len = 0;

    if (_stricmp(g_editClass, "EDIT") == 0) {
        SendMessageA(h, EM_SETSEL, (WPARAM)len, (LPARAM)len);
        SendMessageA(h, EM_REPLACESEL, FALSE, (LPARAM)text);
    } else {
        CHARRANGE a, b;
        SendMessageA(h, EM_SETSEL, (WPARAM)len, (LPARAM)len);
        SendMessageA(h, EM_EXGETSEL, 0, (LPARAM)&a);
        SendMessageA(h, EM_REPLACESEL, FALSE, (LPARAM)text);
        SendMessageA(h, EM_EXGETSEL, 0, (LPARAM)&b);
        if (a.cpMin < 0) a.cpMin = len;
        if (b.cpMin < a.cpMin) b.cpMin = len + (int)strlen(text);
        SendMessageA(h, EM_SETSEL, (WPARAM)a.cpMin, (LPARAM)b.cpMin);
        re_setfmt(h, col, bold, italic, font, sz);
        {
            int nl = GetWindowTextLengthA(h);
            if (nl < 0) nl = 0;
            SendMessageA(h, EM_SETSEL, (WPARAM)nl, (LPARAM)nl);
        }
    }

    SendMessageA(h, EM_SETREADONLY, TRUE, 0);   /* 恢复只读 */
    SendMessageA(h, EM_SCROLLCARET, 0, 0);
}

/* Ctrl+滚轮缩放：WM_MOUSEWHEEL 发给有焦点的窗口，所以两个输入区都挂上 */
static WNDPROC g_txOld = NULL;
static WNDPROC g_inOld = NULL;

static LRESULT CALLBACK chat_wheel_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    WNDPROC old = (h == g.hTranscript) ? g_txOld : g_inOld;
    if (m == WM_MOUSEWHEEL && (GetKeyState(VK_CONTROL) & 0x8000)) {
        chat_zoom(((short)HIWORD(w) > 0) ? 1 : -1);
        return 0;
    }
    return CallWindowProcA(old, h, m, w, l);
}

/* ------------------------------------------------------------------ */
void chat_init(HWND hwndFrame)
{
    g.hTranscript = CreateWindowExA(WS_EX_CLIENTEDGE, g_editClass, "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_READONLY |
        WS_VSCROLL | ES_AUTOVSCROLL, 0, 0, 10, 10,
        hwndFrame, (HMENU)IDC_TRANSCRIPT, g.hInst, NULL);
    if (!g.hTranscript) {
        g_editClass = "EDIT";
        g.hTranscript = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_READONLY |
            WS_VSCROLL | ES_AUTOVSCROLL, 0, 0, 10, 10,
            hwndFrame, (HMENU)IDC_TRANSCRIPT, g.hInst, NULL);
    }

    g.hInput = CreateWindowExA(WS_EX_CLIENTEDGE, g_editClass, "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL |
        ES_WANTRETURN | WS_VSCROLL | ES_NOHIDESEL, 0, 0, 10, 10,
        hwndFrame, (HMENU)IDC_INPUT, g.hInst, NULL);

    sm_logf("chat_init: class=%s transcript=%p input=%p", g_editClass,
            (void *)g.hTranscript, (void *)g.hInput);
    SendMessageA(g.hTranscript, WM_SETFONT, (WPARAM)g.hFontUI, MAKELPARAM(FALSE, 0));
    SendMessageA(g.hInput, WM_SETFONT, (WPARAM)g.hFontUI, MAKELPARAM(TRUE, 0));

    if (_stricmp(g_editClass, "EDIT") != 0) {
        SendMessageA(g.hTranscript, EM_SETBKGNDCOLOR, 0, (LPARAM)GetSysColor(COLOR_WINDOW));
        SendMessageA(g.hInput, EM_SETBKGNDCOLOR, 0, (LPARAM)GetSysColor(COLOR_WINDOW));
        SendMessageA(g.hTranscript, EM_SETUNDOLIMIT, 0, 0);
        /* 关掉 RichEdit 自带的缩放：它只作用于当前控件实例，
         * 切换/新建会话时字号就和上一段对不上，看着像“缩放丢了”。 */
        SendMessageA(g.hTranscript, EM_SETZOOM, 0, 0);
    }
    SendMessageA(g.hInput, EM_SETLIMITTEXT, 0x7FFFFFF0, 0);
    if (g.hTranscript)
        g_txOld = (WNDPROC)SetWindowLongA(g.hTranscript, GWL_WNDPROC,
                                          (LONG)(INT_PTR)chat_wheel_proc);
    if (g.hInput)
        g_inOld = (WNDPROC)SetWindowLongA(g.hInput, GWL_WNDPROC,
                                          (LONG)(INT_PTR)chat_wheel_proc);
    g_findMsg = RegisterWindowMessageA(FINDMSGSTRING);
}

/* ------------------------------------------------------------------ */
void chat_append(int kind, const char *text)
{
    char head[160];
    char ts[32];
    COLORREF col;
    int bold = 0, italic = 0, sz = chat_font_px();

    if (!g.hTranscript || !text) return;
    {
        SYSTEMTIME st;
        GetLocalTime(&st);
        _snprintf(ts, sizeof(ts), "[%02d:%02d]", st.wHour, st.wMinute);
        ts[sizeof(ts) - 1] = 0;
    }

    switch (kind) {
    case CT_USER:
        _snprintf(head, sizeof(head), "%s %s:\r\n", ts, L(STR_SPEAKER_YOU));
        col = RGB(0, 0, 128); bold = 1;
        break;
    case CT_ASSIST:
        _snprintf(head, sizeof(head), "%s %s:\r\n", ts, L(STR_SPEAKER_BOT));
        col = RGB(0, 96, 0); bold = 1;
        break;
    case CT_ERR:
        _snprintf(head, sizeof(head), "%s %s:\r\n", ts, L(STR_WARNING));
        col = RGB(160, 0, 0); bold = 1;
        break;
    case CT_SYS:
        head[0] = 0;
        col = RGB(110, 110, 110); italic = 1;
        break;
    case CT_THINK:
        _snprintf(head, sizeof(head), "%s %s:\r\n", ts, L(STR_THINK_PREFIX));
        col = RGB(128, 128, 128); italic = 1;
        break;
    default:
        head[0] = 0;
        col = RGB(0, 0, 0);
        break;
    }
    head[sizeof(head) - 1] = 0;

    if (head[0]) re_append(g.hTranscript, head, col, bold, 0, g.hFontUI, sz);
    if (kind == CT_ASSIST && g.cfg.reflow) {
        /* 模型自己折的硬换行合并回长行，交给控件按宽度折行 */
        char *flow = sm_reflow_text(text);
        re_append(g.hTranscript, flow, col, 0, italic, g.hFontUI, sz);
        sm_free(flow);
    } else {
        re_append(g.hTranscript, text, col, 0, italic, g.hFontUI, sz);
    }
    re_append(g.hTranscript, "\r\n\r\n", col, 0, 0, g.hFontUI, sz);
    sm_logf("chat_append: kind=%d len=%d (chunk=%d)", kind,
            GetWindowTextLengthA(g.hTranscript), (int)strlen(text));
}

void chat_clear(void)
{
    if (g.hTranscript) SetWindowTextA(g.hTranscript, "");
}

static void chat_apply_size(void)
{
    CHARFORMAT2A cf;
    if (!g.hTranscript) return;
    if (g_editClass && _stricmp(g_editClass, "EDIT") == 0) return;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_SIZE;
    cf.yHeight = (LONG)(chat_font_px() * 15);
    SendMessageA(g.hTranscript, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
}

int chat_font_px(void)
{
    int sz = g.cfg.fontSize;
    if (sz < 9) sz = 9;
    if (sz > 28) sz = 28;
    return sz;
}

/* Ctrl+滚轮：全局调字号，写回 INI，并把整篇对话重排一次 */
void chat_zoom(int delta)
{
    int sz = chat_font_px() + delta;
    if (sz < 9) sz = 9;
    if (sz > 28) sz = 28;
    if (sz == g.cfg.fontSize) return;
    g.cfg.fontSize = sz;
    sm_config_save(&g.cfg);
    chat_apply_size();
    {
        char b[64];
        _snprintf(b, sizeof(b), TS("字号 %d", "Font size %d"), sz);
        b[sizeof(b) - 1] = 0;
        sm_status_set(0, b);
    }
}

void chat_set_raw(const char *text)
{
    if (!g.hTranscript) return;
    SendMessageA(g.hTranscript, EM_SETREADONLY, FALSE, 0);
    SetWindowTextA(g.hTranscript, text ? text : "");
    SendMessageA(g.hTranscript, EM_SETREADONLY, TRUE, 0);
    chat_apply_size();
    {
        int len = GetWindowTextLengthA(g.hTranscript);
        if (len < 0) len = 0;
        SendMessageA(g.hTranscript, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    }
    SendMessageA(g.hTranscript, EM_SCROLLCARET, 0, 0);
}

char *chat_get_input(void)
{
    int n = GetWindowTextLengthA(g.hInput);
    char *b = (char *)sm_alloc((size_t)n + 2);
    GetWindowTextA(g.hInput, b, n + 1);
    sm_trim(b);
    return b;
}

void chat_set_input(const char *s)
{
    SetWindowTextA(g.hInput, s ? s : "");
    {
        int len = GetWindowTextLengthA(g.hInput);
        SendMessageA(g.hInput, EM_SETSEL, (WPARAM)len, (LPARAM)len);
        SendMessageA(g.hInput, EM_SCROLLCARET, 0, 0);
    }
}

void chat_focus_input(void)
{
    if (g.hInput) SetFocus(g.hInput);
}

char *chat_transcript_text(void)
{
    int n = GetWindowTextLengthA(g.hTranscript);
    char *b = (char *)sm_alloc((size_t)n + 2);
    GetWindowTextA(g.hTranscript, b, n + 1);
    return b;
}

/* ------------------------------------------------------------------ */
/* 对话历史（JSON 片段，UTF-8）                                        */
/* ------------------------------------------------------------------ */
void chat_history_reset(void)
{
    if (g.history.p) sb_clear(&g.history);
    else sb_init(&g.history);
}

void chat_history_append_user(const char *ansi)
{
    if (g.history.len) sb_putc(&g.history, ',');
    sb_puts(&g.history, "{\"role\":\"user\",\"content\":");
    sb_put_json_str(&g.history, ansi);
    sb_putc(&g.history, '}');
}

void chat_history_append_json(const char *json)
{
    if (g.history.len) sb_putc(&g.history, ',');
    sb_puts(&g.history, json);
}

void chat_history_trim(void)
{
    size_t i, n = g.history.len, depth = 0, start = 0, keepFrom = 0;
    int inStr = 0, esc = 0, objs = 0;
    int *starts;

    if (n < 100 * 1024) return;
    starts = (int *)sm_alloc(sizeof(int) * 256);
    for (i = 0; i < n; i++) {
        char ch = g.history.p[i];
        if (inStr) {
            if (esc) esc = 0;
            else if (ch == '\\') esc = 1;
            else if (ch == '"') inStr = 0;
            continue;
        }
        if (ch == '"') { inStr = 1; continue; }
        if (ch == '{') {
            if (depth == 0) {
                start = i;
                if (objs < 256) starts[objs] = (int)start;
                objs++;
            }
            depth++;
        } else if (ch == '}') {
            if (depth) depth--;
        }
    }
    if (objs > 8) {
        keepFrom = (size_t)starts[objs - 6];
        memmove(g.history.p, g.history.p + keepFrom, g.history.len - keepFrom + 1);
        g.history.len -= keepFrom;
    }
    sm_free(starts);
}

/* ------------------------------------------------------------------ */
/* 查找                                                                */
/* ------------------------------------------------------------------ */
void chat_find(HWND owner)
{
    ZeroMemory(&g_fr, sizeof(g_fr));
    g_fr.lStructSize = sizeof(g_fr);
    g_fr.hwndOwner = owner;
    g_fr.lpstrFindWhat = g_findWhat;
    g_fr.wFindWhatLen = sizeof(g_findWhat) - 1;
    g_fr.Flags = g_findFlags;
    FindTextA(&g_fr);
}

static void do_find_next(void)
{
    char *txt;
    int n, st, sel0, sel1;
    const char *p;
    size_t wl;
    if (!g_findWhat[0]) return;
    txt = chat_transcript_text();
    n = (int)strlen(txt);
    wl = strlen(g_findWhat);
    SendMessageA(g.hTranscript, EM_GETSEL, (WPARAM)&sel0, (LPARAM)&sel1);
    st = (g_findFlags & FR_DOWN) ? sel1 : sel0 - 1;
    if (st < 0) st = 0;
    p = NULL;
    if (g_findFlags & FR_DOWN) {
        int i;
        for (i = st; i + (int)wl <= n; i++)
            if (!_strnicmp(txt + i, g_findWhat, wl)) { p = txt + i; break; }
    } else {
        int i;
        for (i = st; i >= 0; i--)
            if (!_strnicmp(txt + i, g_findWhat, wl)) { p = txt + i; break; }
    }
    if (p) {
        int off = (int)(p - txt);
        SendMessageA(g.hTranscript, EM_SETSEL, (WPARAM)off, (LPARAM)(off + (int)wl));
        SendMessageA(g.hTranscript, EM_SCROLLCARET, 0, 0);
        SetFocus(g.hTranscript);
    } else {
        MessageBoxA(g.hFrame, "Not found.", SM_SHORT_NAME, MB_ICONINFORMATION | MB_OK);
    }
    sm_free(txt);
}

int chat_is_find_msg(MSG *msg)
{
    if (!g_findMsg || msg->message != g_findMsg) return 0;
    if (g_fr.Flags & FR_DIALOGTERM) return 1;
    if (g_fr.Flags & FR_FINDNEXT) { do_find_next(); return 1; }
    return 1;
}
