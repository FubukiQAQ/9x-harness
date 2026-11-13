/* =====================================================================
 * Axiom NeuroBench 2003 - 多供应商设置（表格版）
 *
 * 把原来的单组输入框换成一张表：一行一个供应商/模型。
 * 列：名称 / 接口地址 / 型号 / 思考 / 视觉
 * 按钮：新增 / 编辑 / 删除 / 设为当前 / 测试连接 / 关闭
 * ===================================================================== */
#include "agent.h"
#include <commctrl.h>
#include <commdlg.h>

#define IDC_PV_LIST    5601
#define IDC_PV_NEW     5602
#define IDC_PV_EDIT    5603
#define IDC_PV_DEL     5604
#define IDC_PV_USE     5605
#define IDC_PV_TEST    5606
#define IDC_PV_OK      5607
#define IDC_PV_CANCEL  5608
#define IDC_PV_TAB     5620
#define IDC_PV_PROMPT  5621
#define IDC_PV_TPL     5622
#define IDC_PV_CLR     5623
#define IDC_AG_ROUNDS  5630
#define IDC_AG_PERM0   5631
#define IDC_AG_PERM1   5632
#define IDC_AG_PERM2   5633

/* 编辑子对话框用 */
#define IDC_PE_NAME    5611
#define IDC_PE_URL     5612
#define IDC_PE_MODEL   5613
#define IDC_PE_KEY     5614
#define IDC_PE_VISION  5615
#define IDC_PE_THINK   5616
#define IDC_PE_LEVEL   5617
#define IDC_PE_OK      5618
#define IDC_PE_CANCEL  5619

typedef struct {
    HWND h, list, tab, prompt;
    HWND page0[8]; int n0;   /* 第 1 页：供应商表格 */
    HWND page1[8]; int n1;   /* 第 2 页：系统提示词 */
    HWND page2[8]; int n2;   /* 第 3 页：Agent */
    HWND rounds;             /* 单轮工具上限输入框 */
} ProvDlg;
typedef struct { HWND h; int ok; SmProvider p; } ProvEdit;

static HWND mk(HWND p, const char *cls, const char *txt, DWORD style,
               int x, int y, int w, int h, int id)
{
    HWND c = CreateWindowExA(0, cls, txt, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | style,
                             x, y, w, h, p, (HMENU)(INT_PTR)id, g.hInst, NULL);
    if (c) SendMessageA(c, WM_SETFONT,
                        (WPARAM)(g.hFontDlg ? g.hFontDlg : g.hFontUI),
                        MAKELPARAM(TRUE, 0));
    return c;
}

/* ---------------- 列表填充 ---------------- */
static void prov_fill(HWND list)
{
    int i;
    static const char *lvzh[3] = { "低", "中", "高" };
    static const char *lven[3] = { "low", "medium", "high" };
    LVITEMA it;
    SendMessageA(list, LVM_DELETEALLITEMS, 0, 0);
    for (i = 0; i < g.prov.count; i++) {
        const SmProvider *s = &g.prov.list[i];
        int k = s->thinkLevel < 0 ? 0 : (s->thinkLevel > 2 ? 2 : s->thinkLevel);
        ZeroMemory(&it, sizeof(it));
        it.mask = LVIF_TEXT | LVIF_PARAM;
        it.iItem = i;
        it.lParam = i;
        it.pszText = (char *)s->name;
        SendMessageA(list, LVM_INSERTITEMA, 0, (LPARAM)&it);
        it.mask = LVIF_TEXT;
        it.iSubItem = 1; it.pszText = (char *)s->endpoint;
        SendMessageA(list, LVM_SETITEMA, 0, (LPARAM)&it);
        it.iSubItem = 2; it.pszText = (char *)s->model;
        SendMessageA(list, LVM_SETITEMA, 0, (LPARAM)&it);
        it.iSubItem = 3;
        it.pszText = s->think ? (char *)TS(lvzh[k], lven[k]) : (char *)TS("关", "off");
        SendMessageA(list, LVM_SETITEMA, 0, (LPARAM)&it);
        it.iSubItem = 4;
        it.pszText = s->vision ? (char *)TS("是", "yes") : (char *)TS("否", "no");
        SendMessageA(list, LVM_SETITEMA, 0, (LPARAM)&it);
    }
    if (g.prov.count > 0) {
        LVITEMA sel;
        ZeroMemory(&sel, sizeof(sel));
        sel.state = LVIS_SELECTED | LVIS_FOCUSED;
        sel.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
        SendMessageA(list, LVM_SETITEMSTATE, (WPARAM)g.prov.active, (LPARAM)&sel);
    }
}

static int prov_sel(HWND list)
{
    return (int)SendMessageA(list, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
}

/* ---------------- 编辑子对话框 ---------------- */
static LRESULT CALLBACK ProvEditProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    ProvEdit *d = (ProvEdit *)GetWindowLongPtrA(h, GWLP_USERDATA);
    switch (m) {
    case WM_CREATE:
        return 0;
    case WM_COMMAND:
        if (!d) break;
        switch (LOWORD(w)) {
        case IDC_PE_OK: {
            char tmp[600];
            GetWindowTextA(GetDlgItem(h, IDC_PE_NAME), d->p.name, sizeof(d->p.name));
            GetWindowTextA(GetDlgItem(h, IDC_PE_URL), d->p.endpoint, sizeof(d->p.endpoint));
            GetWindowTextA(GetDlgItem(h, IDC_PE_MODEL), d->p.model, sizeof(d->p.model));
            GetWindowTextA(GetDlgItem(h, IDC_PE_KEY), d->p.apikey, sizeof(d->p.apikey));
            sm_trim(d->p.name); sm_trim(d->p.endpoint);
            sm_trim(d->p.model); sm_trim(d->p.apikey);
            if (!d->p.name[0]) strcpy(d->p.name, TS("未命名", "Untitled"));
            if (!d->p.endpoint[0] || !d->p.model[0]) {
                MessageBoxA(h, TS("接口地址和型号不能为空。", "Endpoint and model are required."),
                            TS("设置", "Settings"), MB_ICONWARNING | MB_OK);
                return 0;
            }
            d->p.vision = (SendMessageA(GetDlgItem(h, IDC_PE_VISION), BM_GETCHECK, 0, 0) == BST_CHECKED);
            d->p.think  = (SendMessageA(GetDlgItem(h, IDC_PE_THINK), BM_GETCHECK, 0, 0) == BST_CHECKED);
            d->p.thinkLevel = (int)SendMessageA(GetDlgItem(h, IDC_PE_LEVEL), CB_GETCURSEL, 0, 0);
            if (d->p.thinkLevel < 0) d->p.thinkLevel = 1;
            GetWindowTextA(GetDlgItem(h, IDC_PE_LEVEL), tmp, sizeof(tmp));
            d->ok = 1;
            DestroyWindow(h);
            return 0;
        }
        case IDC_PE_CANCEL:
        case IDCANCEL:
            DestroyWindow(h);
            return 0;
        default:
            break;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;

    case WM_DESTROY:
        /* 唤醒模态循环里的 GetMessage（它收到 WM_QUIT 会返回 0） */
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

static int prov_edit_dialog(HWND owner, SmProvider *p, const char *title)
{
    static int registered = 0;
    WNDCLASSA wc;
    ProvEdit d;
    HWND h;
    MSG msg;

    if (!registered) {
        ZeroMemory(&wc, sizeof(wc));
        wc.lpfnWndProc = ProvEditProc;
        wc.hInstance = g.hInst;
        wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "NbProvEdit";
        RegisterClassA(&wc);
        registered = 1;
    }
    h = CreateWindowExA(WS_EX_DLGMODALFRAME, "NbProvEdit", title,
        WS_POPUP | WS_CAPTION | WS_SYSMENU, 0, 0, 470, 320, owner, NULL, g.hInst, NULL);
    if (!h) return 0;
    ZeroMemory(&d, sizeof(d));
    d.h = h;
    d.p = *p;
    SetWindowLongPtrA(h, GWLP_USERDATA, (LONG_PTR)&d);

    mk(h, "STATIC", TS("名称：", "Name:"), SS_LEFT, 12, 14, 76, 18, 0);
    mk(h, "EDIT", p->name, ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER, 92, 12, 366, 21, IDC_PE_NAME);
    mk(h, "STATIC", TS("接口地址：", "Endpoint:"), SS_LEFT, 12, 42, 76, 18, 0);
    mk(h, "EDIT", p->endpoint, ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER, 92, 40, 366, 21, IDC_PE_URL);
    mk(h, "STATIC", TS("型号：", "Model:"), SS_LEFT, 12, 70, 76, 18, 0);
    mk(h, "EDIT", p->model, ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER, 92, 68, 366, 21, IDC_PE_MODEL);
    mk(h, "STATIC", TS("API Key：", "API Key:"), SS_LEFT, 12, 98, 76, 18, 0);
    mk(h, "EDIT", p->apikey, ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER, 92, 96, 366, 21, IDC_PE_KEY);
    mk(h, "BUTTON", TS("这个模型支持读图（视觉）", "This model can read pictures (vision)"),
       BS_AUTOCHECKBOX | WS_TABSTOP, 92, 128, 366, 18, IDC_PE_VISION);
    mk(h, "BUTTON", TS("对这个模型启用深度思考", "Enable deep thinking for this model"),
       BS_AUTOCHECKBOX | WS_TABSTOP, 92, 152, 366, 18, IDC_PE_THINK);
    mk(h, "STATIC", TS("思考挡位：", "Thinking level:"), SS_LEFT, 12, 180, 76, 18, 0);
    {
        HWND cb = mk(h, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
                     92, 176, 140, 120, IDC_PE_LEVEL);
        SendMessageA(cb, CB_ADDSTRING, 0, (LPARAM)TS("低（快）", "Low (fast)"));
        SendMessageA(cb, CB_ADDSTRING, 0, (LPARAM)TS("中（默认）", "Medium (default)"));
        SendMessageA(cb, CB_ADDSTRING, 0, (LPARAM)TS("高（慢）", "High (slow)"));
        SendMessageA(cb, CB_SETCURSEL,
                     (WPARAM)(p->thinkLevel < 0 ? 1 : (p->thinkLevel > 2 ? 2 : p->thinkLevel)), 0);
    }
    mk(h, "STATIC", TS("提示：留空 API Key 表示不需要鉴权（本地模型）。", 
       "Tip: leave API Key empty for local endpoints without auth."),
       SS_LEFT, 12, 208, 446, 18, 0);

    mk(h, "BUTTON", TS("确定", "OK"), BS_DEFPUSHBUTTON | WS_TABSTOP, 292, 246, 80, 26, IDC_PE_OK);
    mk(h, "BUTTON", TS("取消", "Cancel"), BS_PUSHBUTTON | WS_TABSTOP, 380, 246, 78, 26, IDC_PE_CANCEL);

    SendMessageA(GetDlgItem(h, IDC_PE_VISION), BM_SETCHECK,
                 p->vision ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(GetDlgItem(h, IDC_PE_THINK), BM_SETCHECK,
                 p->think ? BST_CHECKED : BST_UNCHECKED, 0);

    sm_set_dlg_font(h);
    ShowWindow(h, SW_SHOW);
    sm_center_window(h);
    EnableWindow(owner, FALSE);
    /* 注意：必须等到窗口真的没了才退出循环。
     * 原来只看 GetMessage 的返回值，对话框销毁后消息队列空了，
     * GetMessage 会一直阻塞，整个程序就卡住（Esc/确定之后假死）。 */
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        if (!IsWindow(h)) break;
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) { DestroyWindow(h); continue; }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    if (d.ok) *p = d.p;
    return d.ok;
}

/* ---------------- 主对话框 ---------------- */
/* ---------------- 选项卡显隐 ---------------- */
static void prov_page(ProvDlg *d, int page)
{
    int i;
    for (i = 0; i < d->n0; i++) {
        ShowWindow(d->page0[i], page == 0 ? SW_SHOW : SW_HIDE);
        if (page == 0) InvalidateRect(d->page0[i], NULL, TRUE);
    }
    for (i = 0; i < d->n1; i++) {
        ShowWindow(d->page1[i], page == 1 ? SW_SHOW : SW_HIDE);
        if (page == 1) InvalidateRect(d->page1[i], NULL, TRUE);
    }
    for (i = 0; i < d->n2; i++) {
        ShowWindow(d->page2[i], page == 2 ? SW_SHOW : SW_HIDE);
        if (page == 2) InvalidateRect(d->page2[i], NULL, TRUE);
    }
    if (d->h) UpdateWindow(d->h);
}

static void prov_add0(ProvDlg *d, HWND c)
{
    if (c && d->n0 < (int)(sizeof(d->page0) / sizeof(d->page0[0]))) d->page0[d->n0++] = c;
}

static void prov_add1(ProvDlg *d, HWND c)
{
    if (c && d->n1 < (int)(sizeof(d->page1) / sizeof(d->page1[0]))) d->page1[d->n1++] = c;
}

static void prov_add2(ProvDlg *d, HWND c)
{
    if (c && d->n2 < (int)(sizeof(d->page2) / sizeof(d->page2[0]))) d->page2[d->n2++] = c;
}

/* 只去掉尾部空白和开头的空行，保留正文缩进 */
static void prompt_trim(char *s)
{
    int i, n;
    if (!s) return;
    n = (int)strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' ||
                     s[n - 1] == '\r' || s[n - 1] == '\n')) n--;
    s[n] = 0;
    i = 0;
    while (s[i] == '\r' || s[i] == '\n') i++;
    if (i > 0) memmove(s, s + i, strlen(s + i) + 1);
}

static LRESULT CALLBACK ProvListProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    ProvDlg *d = (ProvDlg *)GetWindowLongPtrA(h, GWLP_USERDATA);
    switch (m) {
    case WM_CREATE:
        return 0;

    case WM_NOTIFY: {
        NMHDR *nh = (NMHDR *)l;
        if (d && nh && nh->hwndFrom == d->tab && nh->code == TCN_SELCHANGE) {
            int sel = (int)SendMessageA(d->tab, TCM_GETCURSEL, 0, 0);
            if (sel < 0 || sel > 2) sel = 0;
            prov_page(d, sel);
        }
        return 0;
    }

    case WM_COMMAND:
        if (!d) break;
        switch (LOWORD(w)) {
        case IDC_PV_NEW:
            if (g.prov.count >= SM_MAX_PROVIDERS) {
                MessageBoxA(h, TS("条目已超过上限（16 条）。", "Reached the limit of 16 entries."),
                            TS("设置", "Settings"), MB_ICONWARNING | MB_OK);
                break;
            }
            {
                SmProvider np;
                memset(&np, 0, sizeof(np));
                np.temperature10 = 7;
                np.maxtokens = 2048;
                np.timeoutSec = 60;
                np.thinkLevel = 1;
                _snprintf(np.name, sizeof(np.name), TS("新建条目 %d", "New %d"), g.prov.count + 1);
                if (prov_edit_dialog(h, &np, TS("添加供应商 / 模型", "Add provider / model"))) {
                    g.prov.list[g.prov.count] = np;
                    g.prov.count++;
                    prov_fill(d->list);
                }
            }
            break;
        case IDC_PV_EDIT: {
            int i = prov_sel(d->list);
            if (i < 0 || i >= g.prov.count) break;
            {
                SmProvider ed = g.prov.list[i];
                if (prov_edit_dialog(h, &ed, TS("编辑供应商 / 模型", "Edit provider / model"))) {
                    g.prov.list[i] = ed;
                    prov_fill(d->list);
                }
            }
            break;
        }
        case IDC_PV_DEL: {
            int i = prov_sel(d->list);
            if (i < 0 || i >= g.prov.count) break;
            if (MessageBoxA(h, TS("确定要删掉这一条？", "Delete this entry?"),
                            TS("设置", "Settings"), MB_ICONQUESTION | MB_YESNO) != IDYES) break;
            {
                int k;
                for (k = i; k < g.prov.count - 1; k++) g.prov.list[k] = g.prov.list[k + 1];
                g.prov.count--;
                if (g.prov.active >= g.prov.count) g.prov.active = g.prov.count ? g.prov.count - 1 : 0;
                prov_fill(d->list);
            }
            break;
        }
        case IDC_PV_USE: {
            int i = prov_sel(d->list);
            if (i < 0 || i >= g.prov.count) break;
            g.prov.active = i;
            prov_fill(d->list);
            break;
        }
        case IDC_PV_TEST: {
            int i = prov_sel(d->list);
            char report[1200];
            if (i < 0 || i >= g.prov.count) break;
            {
                SmProvider *sp = &g.prov.list[i];
                SetCursor(LoadCursorA(NULL, IDC_WAIT));
                EnableWindow(h, FALSE);
                if (sm_tls_test(sp->endpoint, sp->apikey, sp->timeoutSec, g.cfg.ignoreCertDate,
                                report, sizeof(report))) {
                    MessageBoxA(h, report, TS("测试连接", "Test connection"), MB_ICONINFORMATION | MB_OK);
                } else {
                    MessageBoxA(h, report, TS("测试连接", "Test connection"), MB_ICONWARNING | MB_OK);
                }
                EnableWindow(h, TRUE);
                SetCursor(LoadCursorA(NULL, IDC_ARROW));
            }
            break;
        }
        case IDC_PV_TPL:
            if (d->prompt) {
                char *tpl = prompt_default_template();
                SetWindowTextA(d->prompt, tpl);
                sm_free(tpl);
                SetFocus(d->prompt);
            }
            break;
        case IDC_PV_CLR:
            if (d->prompt) SetWindowTextA(d->prompt, "");
            break;
        case IDC_PV_OK: {
            int i = prov_sel(d->list);
            if (i >= 0 && i < g.prov.count) g.prov.active = i;
            sm_providers_save(&g.prov);

            /* Agent 页：单轮工具上限 + 默认权限 */
            if (d->rounds) {
                char rb[32];
                int v;
                GetWindowTextA(d->rounds, rb, sizeof(rb));
                rb[sizeof(rb) - 1] = 0;
                v = 0;
                {
                    const char *q = rb;
                    while (*q == ' ') q++;
                    while (*q >= '0' && *q <= '9') { v = v * 10 + (*q - '0'); q++; }
                }
                if (v < 1) v = 1;
                if (v > 50) v = 50;
                g.cfg.toolRounds = v;
            }
            if (IsDlgButtonChecked(h, IDC_AG_PERM0) == BST_CHECKED)      g.cfg.permMode = 0;
            else if (IsDlgButtonChecked(h, IDC_AG_PERM1) == BST_CHECKED) g.cfg.permMode = 1;
            else                                                        g.cfg.permMode = 2;
            g.cfg.confirmWrite = (g.cfg.permMode == 1) ? 1 : 0;
            sm_config_save(&g.cfg);
            if (g.prov.count > 0) {
                int fs = g.cfg.fontSize;
                int lg = lang_get();
                sm_provider_to_config(&g.prov, g.prov.active, &g.cfg);
                g.cfg.fontSize = fs;
                lang_set(lg);
            }
            /* 保存系统提示词（必须在销毁窗口之前取文本） */
            if (d->prompt) {
                int nlen = GetWindowTextLengthA(d->prompt);
                char *buf;
                if (nlen < 0) nlen = 0;
                buf = (char *)sm_alloc((size_t)nlen + 2);
                if (buf) {
                    char *dflt = prompt_default_template();
                    GetWindowTextA(d->prompt, buf, nlen + 1);
                    prompt_trim(buf);
                    if (dflt && strcmp(buf, dflt) == 0) buf[0] = 0;  /* 没改过就回到内置 */
                    sm_sysprompt_save(buf);
                    sm_logf("settings: sysprompt len=%d (default=%d)",
                            (int)strlen(buf), dflt ? (int)strlen(dflt) : -1);
                    sm_free(dflt);
                    sm_free(buf);
                }
            }
            DestroyWindow(h);
            return 0;
        }
        case IDC_PV_CANCEL:
        case IDCANCEL:
            DestroyWindow(h);
            return 0;
        default:
            break;
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

void dlg_settings(HWND owner)
{
    static int registered = 0;
    WNDCLASSA wc;
    ProvDlg d;
    HWND h, list, tab, prompt, c, gb = NULL;
    MSG msg;
    LVCOLUMNA col;
    TCITEMA ti;

    if (!registered) {
        ZeroMemory(&wc, sizeof(wc));
        wc.lpfnWndProc = ProvListProc;
        wc.hInstance = g.hInst;
        wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "NbProvList";
        RegisterClassA(&wc);
        registered = 1;
    }
    h = CreateWindowExA(WS_EX_DLGMODALFRAME, "NbProvList", L(STR_SET_TITLE),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        0, 0, 620, 424, owner, NULL, g.hInst, NULL);
    if (!h) return;
    ZeroMemory(&d, sizeof(d));
    d.h = h;
    SetWindowLongPtrA(h, GWLP_USERDATA, (LONG_PTR)&d);

    /* ---------- 选项卡 ---------- */
    tab = mk(h, WC_TABCONTROLA, "",
             WS_TABSTOP | TCS_TABS, 6, 4, 600, 332, IDC_PV_TAB);
    d.tab = tab;
    ZeroMemory(&ti, sizeof(ti));
    ti.mask = TCIF_TEXT;
    ti.pszText = (char *)TS("供应商与模型", "Providers && models");
    SendMessageA(tab, TCM_INSERTITEMA, 0, (LPARAM)&ti);
    ti.pszText = (char *)TS("系统提示词", "System prompt");
    SendMessageA(tab, TCM_INSERTITEMA, 1, (LPARAM)&ti);
    ti.pszText = (char *)TS("Agent", "Agent");
    SendMessageA(tab, TCM_INSERTITEMA, 2, (LPARAM)&ti);

    /* ---------- 第 1 页：供应商 ---------- */
    prov_add0(&d, mk(h, "STATIC",
        TS("每行一条：接口地址 + 型号 + 密钥。密钥不在此显示，点「编辑」才能看到。",
           "Each row is one endpoint + model + key. Keys are not shown here; use Edit to view."),
        SS_LEFT, 16, 32, 580, 18, 0));

    list = mk(h, WC_LISTVIEWA, "",
              WS_TABSTOP | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
              16, 52, 580, 200, IDC_PV_LIST);
    SendMessageA(list, LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
                 LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    ZeroMemory(&col, sizeof(col));
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    col.pszText = (char *)TS("名称", "Name");        col.cx = 106; col.iSubItem = 0;
    SendMessageA(list, LVM_INSERTCOLUMNA, 0, (LPARAM)&col);
    col.pszText = (char *)TS("接口地址", "Endpoint"); col.cx = 244; col.iSubItem = 1;
    SendMessageA(list, LVM_INSERTCOLUMNA, 1, (LPARAM)&col);
    col.pszText = (char *)TS("型号", "Model");        col.cx = 106; col.iSubItem = 2;
    SendMessageA(list, LVM_INSERTCOLUMNA, 2, (LPARAM)&col);
    col.pszText = (char *)TS("思考", "Thinking");     col.cx = 56;  col.iSubItem = 3;
    SendMessageA(list, LVM_INSERTCOLUMNA, 3, (LPARAM)&col);
    col.pszText = (char *)TS("视觉", "Vision");       col.cx = 50;  col.iSubItem = 4;
    SendMessageA(list, LVM_INSERTCOLUMNA, 4, (LPARAM)&col);
    d.list = list;
    prov_add0(&d, list);

    prov_add0(&d, mk(h, "BUTTON", TS("新建(&N)", "&New"), BS_PUSHBUTTON | WS_TABSTOP,
                     16, 260, 84, 26, IDC_PV_NEW));
    prov_add0(&d, mk(h, "BUTTON", TS("编辑(&E)", "&Edit"), BS_PUSHBUTTON | WS_TABSTOP,
                     106, 260, 84, 26, IDC_PV_EDIT));
    prov_add0(&d, mk(h, "BUTTON", TS("删除(&D)", "&Delete"), BS_PUSHBUTTON | WS_TABSTOP,
                     196, 260, 84, 26, IDC_PV_DEL));
    prov_add0(&d, mk(h, "BUTTON", TS("设为当前(&U)", "&Use"), BS_PUSHBUTTON | WS_TABSTOP,
                     286, 260, 92, 26, IDC_PV_USE));
    prov_add0(&d, mk(h, "BUTTON", L(STR_SET_TEST), BS_PUSHBUTTON | WS_TABSTOP,
                     384, 260, 96, 26, IDC_PV_TEST));
    prov_add0(&d, mk(h, "STATIC", TS("双击某一行可直接编辑。", "Double-click a row to edit it."),
                     SS_LEFT, 16, 294, 400, 18, 0));

    /* ---------- 第 2 页：系统提示词 ---------- */
    prov_add1(&d, mk(h, "STATIC",
        TS("下面就是本程序原来的系统提示词，可以直接改。清空则使用内置默认。"
           "%OS% %DATE% %LICENSE% %MODEL% %CODEPAGE% %TOOLS% %LANG% 会在发送时替换成真实值。",
           "This is the program's own system prompt; edit it freely. Clear it to fall back "
           "to the built-in. %OS% %DATE% %LICENSE% %MODEL% %CODEPAGE% %TOOLS% %LANG% "
           "are substituted at send time."),
        SS_LEFT, 16, 28, 580, 34, 0));

    prompt = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | WS_VSCROLL |
        ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | ES_NOHIDESEL,
        16, 66, 580, 186, h, (HMENU)(INT_PTR)IDC_PV_PROMPT, g.hInst, NULL);
    if (prompt) {
        SendMessageA(prompt, WM_SETFONT,
                     (WPARAM)(g.hFontDlg ? g.hFontDlg : g.hFontUI), MAKELPARAM(TRUE, 0));
        {
            char *cur = sm_sysprompt_load();
            if (cur && cur[0]) {
                SetWindowTextA(prompt, cur);
            } else {
                /* 没有自定义内容时，把默认模板摆出来给用户当起点 */
                char *dflt = prompt_default_template();
                SetWindowTextA(prompt, dflt);
                sm_free(dflt);
            }
            sm_free(cur);
        }
        sm_fix_scroll_repaint(prompt);
    }
    d.prompt = prompt;
    prov_add1(&d, prompt);

    prov_add1(&d, mk(h, "BUTTON", TS("载入默认模板(&T)", "&Default template"), BS_PUSHBUTTON | WS_TABSTOP,
                     16, 260, 130, 26, IDC_PV_TPL));
    prov_add1(&d, mk(h, "BUTTON", TS("清空(&C)", "&Clear"), BS_PUSHBUTTON | WS_TABSTOP,
                     152, 260, 84, 26, IDC_PV_CLR));
    prov_add1(&d, mk(h, "STATIC",
        TS("说明：填了内容就完全替换系统提示词。改坏了点「载入默认模板」恢复。",
           "Note: any text here replaces the system prompt entirely. Use "
           "\"Insert default template\" to restore."),
        SS_LEFT, 16, 294, 580, 18, 0));

    /* ---------- 第 3 页：Agent ---------- */
    prov_add2(&d, mk(h, "STATIC", TS("单轮工具调用上限：", "Max tool calls per turn:"),
                     SS_LEFT, 16, 30, 150, 18, 0));

    d.rounds = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | ES_NUMBER | ES_RIGHT,
        170, 28, 60, 22, h, (HMENU)(INT_PTR)IDC_AG_ROUNDS, g.hInst, NULL);
    if (d.rounds) {
        char nb[16];
        SendMessageA(d.rounds, WM_SETFONT,
                     (WPARAM)(g.hFontDlg ? g.hFontDlg : g.hFontUI), MAKELPARAM(TRUE, 0));
        _snprintf(nb, sizeof(nb), "%d", g.cfg.toolRounds);
        nb[sizeof(nb) - 1] = 0;
        SetWindowTextA(d.rounds, nb);
        SendMessageA(d.rounds, EM_SETLIMITTEXT, 2, 0);
    }
    prov_add2(&d, d.rounds);

    prov_add2(&d, mk(h, "STATIC", TS("轮（1-50）", "rounds (1-50)"),
                     SS_LEFT, 236, 30, 120, 18, 0));
    prov_add2(&d, mk(h, "STATIC",
        TS("模型在一次回复里可以连续调用工具的次数上限。用完就停下并提示。"
           "机器越慢，这个数建议越小。",
           "How many tool calls the model may chain inside one turn. When it runs out it "
           "stops and reports. Lower is safer on slow machines."),
        SS_LEFT, 16, 54, 580, 32, 0));

    gb = mk(h, "BUTTON", TS("默认权限", "Default permission"),
            BS_GROUPBOX, 16, 96, 580, 104, 0);
    prov_add2(&d, gb);
    prov_add2(&d, mk(h, "BUTTON",
        TS("只读：只能看，不能写文件、执行命令、打开程序",
           "Read-only: can look, cannot write, run or launch"),
        BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP, 30, 116, 550, 18, IDC_AG_PERM0));
    prov_add2(&d, mk(h, "BUTTON",
        TS("总是询问：每次写文件、执行命令前弹窗问我",
           "Always ask: confirm every write or command"),
        BS_AUTORADIOBUTTON | WS_TABSTOP, 30, 138, 550, 18, IDC_AG_PERM1));
    prov_add2(&d, mk(h, "BUTTON",
        TS("完全权限：不再询问，直接执行",
           "Full access: never ask, just do it"),
        BS_AUTORADIOBUTTON | WS_TABSTOP, 30, 162, 550, 18, IDC_AG_PERM2));

    prov_add2(&d, mk(h, "STATIC",
        TS("说明：只读模式下仍可读文件、列目录、截图和取系统信息。"
           "「总是询问」只是在动手前多问一句，最稳妥。",
           "Note: read-only still allows reading files, listing folders, screenshots and "
           "system info. \"Always ask\" only adds one confirmation before acting."),
        SS_LEFT, 16, 208, 580, 34, 0));

    /* 分组框必须压到最底层，否则会盖住里面的单选按钮 */
    if (gb) SetWindowPos(gb, HWND_BOTTOM, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    /* 第 3 页的初始值 */
    CheckRadioButton(h, IDC_AG_PERM0, IDC_AG_PERM2,
                     g.cfg.permMode == 0 ? IDC_AG_PERM0 :
                     (g.cfg.permMode == 1 ? IDC_AG_PERM1 : IDC_AG_PERM2));

    /* ---------- 底部按钮 ---------- */
    c = mk(h, "BUTTON", L(STR_OK), BS_DEFPUSHBUTTON | WS_TABSTOP, 424, 352, 80, 26, IDC_PV_OK);
    mk(h, "BUTTON", L(STR_CANCEL), BS_PUSHBUTTON | WS_TABSTOP, 512, 352, 84, 26, IDC_PV_CANCEL);

    prov_fill(list);
    prov_page(&d, 0);
    /* 选项卡必须压在页控件下面，否则它会把页内容整块盖掉 */
    SetWindowPos(tab, HWND_BOTTOM, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    sm_set_dlg_font(h);
    ShowWindow(h, SW_SHOW);
    sm_center_window(h);
    EnableWindow(owner, FALSE);

    if (c) SetFocus(list);

    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        if (!IsWindow(h)) break;
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) { DestroyWindow(h); continue; }
        if (msg.message == WM_NOTIFY) {
            NMHDR *nh = (NMHDR *)msg.lParam;
            if (nh && nh->hwndFrom == list && nh->code == NM_DBLCLK) {
                int i = prov_sel(list);
                if (i >= 0 && i < g.prov.count) {
                    SmProvider ed = g.prov.list[i];
                    if (prov_edit_dialog(h, &ed, TS("编辑供应商 / 模型", "Edit provider / model"))) {
                        g.prov.list[i] = ed;
                        prov_fill(list);
                    }
                }
                continue;
            }
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
}


/* ------------------------------------------------------------------ */
/* 工具执行确认框                                                      */
/* ------------------------------------------------------------------ */
#define IDC_CF_TEXT 5700
#define IDC_CF_YES  5701
#define IDC_CF_NO   5702

typedef struct { HWND h; int result; } ConfirmDlg;

static LRESULT CALLBACK ConfirmProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    ConfirmDlg *d = (ConfirmDlg *)GetWindowLongPtrA(h, GWLP_USERDATA);
    switch (m) {
    case WM_COMMAND:
        if (!d) break;
        switch (LOWORD(w)) {
        case IDC_CF_YES:
            d->result = 1;
            DestroyWindow(h);
            return 0;
        case IDC_CF_NO:
        case IDCANCEL:
            d->result = 0;
            DestroyWindow(h);
            return 0;
        default:
            break;
        }
        break;
    case WM_CLOSE:
        if (d) d->result = 0;      /* 关掉窗口一律当拒绝 */
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

int dlg_confirm(HWND owner, const char *text)
{
    static int registered = 0;
    WNDCLASSA wc;
    ConfirmDlg d;
    HWND h;
    MSG msg;

    if (!registered) {
        ZeroMemory(&wc, sizeof(wc));
        wc.lpfnWndProc = ConfirmProc;
        wc.hInstance = g.hInst;
        wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "NbConfirm";
        RegisterClassA(&wc);
        registered = 1;
    }
    h = CreateWindowExA(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, "NbConfirm",
        TS(SM_PRODUCT_NAME " - 需要确认", SM_PRODUCT_NAME_EN " - Confirmation"),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        0, 0, 520, 300, owner, NULL, g.hInst, NULL);
    if (!h) return 0;

    ZeroMemory(&d, sizeof(d));
    d.h = h;
    SetWindowLongPtrA(h, GWLP_USERDATA, (LONG_PTR)&d);

    mk(h, "STATIC",
       TS("智能助手想要执行下面的操作。确认无误请按「允许」，否则按「拒绝」。",
          "The assistant wants to perform the operation below. Allow it?"),
       SS_LEFT, 14, 10, 490, 18, 0);

    {
        HWND ed = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_VSCROLL | ES_MULTILINE |
            ES_AUTOVSCROLL | ES_READONLY,
            14, 32, 492, 180, h, (HMENU)(INT_PTR)IDC_CF_TEXT, g.hInst, NULL);
        if (ed) {
            SendMessageA(ed, WM_SETFONT,
                         (WPARAM)(g.hFontDlg ? g.hFontDlg : g.hFontUI), MAKELPARAM(TRUE, 0));
            SetWindowTextA(ed, text ? text : "");
            sm_fix_scroll_repaint(ed);
        }
    }

    mk(h, "STATIC", TS("拿不准就拒绝。", "If unsure, decline."),
       SS_LEFT, 14, 218, 300, 18, 0);
    mk(h, "BUTTON", TS("拒绝(&N)", "&Deny"), BS_PUSHBUTTON | WS_TABSTOP,
       330, 230, 80, 26, IDC_CF_NO);
    mk(h, "BUTTON", TS("允许(&Y)", "&Allow"), BS_DEFPUSHBUTTON | WS_TABSTOP,
       420, 230, 86, 26, IDC_CF_YES);

    sm_set_dlg_font(h);
    ShowWindow(h, SW_SHOW);
    sm_center_window(h);
    SetForegroundWindow(h);
    SetFocus(GetDlgItem(h, IDC_CF_NO));    /* 默认停在「拒绝」上 */
    if (owner) EnableWindow(owner, FALSE);

    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        if (!IsWindow(h)) break;
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            d.result = 0;
            DestroyWindow(h);
            continue;
        }
        if (!IsDialogMessageA(h, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    if (owner) EnableWindow(owner, TRUE);
    sm_logf("dlg_confirm: result=%d", d.result);
    return d.result;
}

