/* =====================================================================
 * Axiom NeuroBench 2003 - 工作区与文件树
 *
 * 每个工作区 = 一个文件夹，里面收纳该工作区的全部会话文件（*.NBC）
 * 以及用户放在里面的文本文件。左窗格列出工作区，中窗格是当前工作区的
 * 文件树，双击文本类文件即可用内置编辑器打开。
 * ===================================================================== */
#include "agent.h"

#define CONV_EXT ".nbc"

/* ------------------------------------------------------------------ */
/* 路径                                                                */
/* ------------------------------------------------------------------ */
static char g_wsRoot[MAX_PATH];
static char g_wsCur[128];
static char g_wsFilter[64];
static char g_fileFilter[64];
static char g_convFile[MAX_PATH];
static int  g_convDirty = 0;
static int  g_convSeq = 0;

char *ws_root(void)
{
    if (!g_wsRoot[0]) {
        char *ad = sm_appdata_dir();
        char *r = sm_path_join(ad, "Workspaces");
        strncpy(g_wsRoot, r, MAX_PATH - 1);
        g_wsRoot[MAX_PATH - 1] = 0;
        sm_free(r);
        sm_free(ad);
        sm_mkdirs(g_wsRoot);
    }
    return g_wsRoot;
}

char *ws_current_dir(void)
{
    char *r = ws_root();
    char *p = sm_path_join(r, g_wsCur[0] ? g_wsCur : "Default");
    sm_mkdirs(p);
    return p;
}

const char *ws_current(void) { return g_wsCur; }

int conv_is_open(void) { return g_convFile[0] != 0; }
const char *conv_path(void) { return g_convFile; }
int  conv_dirty(void) { return g_convDirty; }
void conv_mark_dirty(void) { g_convDirty = 1; }
void conv_clear_dirty(void) { g_convDirty = 0; }

void conv_set_path(const char *p)
{
    strncpy(g_convFile, p ? p : "", MAX_PATH - 1);
    g_convFile[MAX_PATH - 1] = 0;
    g_convDirty = 0;
}

/* ------------------------------------------------------------------ */
/* 图标（借用系统外壳图标，那个年代都这么干）                          */
/* ------------------------------------------------------------------ */
static HIMAGELIST g_imgList = NULL;

/* 树节点的路径表：lParam 存 1-based 下标，刷新时统一释放，
 * 这样不会出现 lParam 指向已释放内存的问题。 */
#define TREE_MAX_PATHS 1024
static char *g_treePaths[TREE_MAX_PATHS];
static int   g_treeN = 0;

static int tree_add_path(const char *path)
{
    if (g_treeN >= TREE_MAX_PATHS) return 0;
    g_treePaths[g_treeN] = sm_strdup(path);
    g_treeN++;
    return g_treeN;          /* 1-based */
}

static const char *tree_get_path(LPARAM lp)
{
    int idx = (int)lp;
    if (idx <= 0 || idx > g_treeN) return NULL;
    return g_treePaths[idx - 1];
}

static void tree_free_paths(void)
{
    int i;
    for (i = 0; i < g_treeN; i++) {
        if (g_treePaths[i]) sm_free(g_treePaths[i]);
        g_treePaths[i] = NULL;
    }
    g_treeN = 0;
}

static int img_index(const char *path, int isDir)
{
    SHFILEINFOA fi;
    DWORD flags = SHGFI_SMALLICON | SHGFI_ICON | SHGFI_USEFILEATTRIBUTES;
    DWORD attr = isDir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    int n;
    if (!g_imgList) return -1;
    ZeroMemory(&fi, sizeof(fi));
    if (!SHGetFileInfoA(path && path[0] ? path : "x.txt", attr, &fi,
                        sizeof(fi), flags))
        return -1;
    n = ImageList_AddIcon(g_imgList, fi.hIcon);
    DestroyIcon(fi.hIcon);
    return n;
}

static void tree_setup(void)
{
    if (g_imgList) return;
    g_imgList = ImageList_Create(16, 16, ILC_COLOR8 | ILC_MASK, 4, 8);
    if (!g_imgList) return;
    {
        SHFILEINFOA fi;
        ZeroMemory(&fi, sizeof(fi));
        if (SHGetFileInfoA("C:\\", FILE_ATTRIBUTE_DIRECTORY, &fi, sizeof(fi),
                           SHGFI_SMALLICON | SHGFI_ICON | SHGFI_USEFILEATTRIBUTES)) {
            ImageList_AddIcon(g_imgList, fi.hIcon);
            DestroyIcon(fi.hIcon);
        }
        ZeroMemory(&fi, sizeof(fi));
        if (SHGetFileInfoA("x.txt", FILE_ATTRIBUTE_NORMAL, &fi, sizeof(fi),
                           SHGFI_SMALLICON | SHGFI_ICON | SHGFI_USEFILEATTRIBUTES)) {
            ImageList_AddIcon(g_imgList, fi.hIcon);
            DestroyIcon(fi.hIcon);
        }
    }
    SendMessageA(g.hTree, TVM_SETIMAGELIST, TVSIL_NORMAL, (LPARAM)g_imgList);
}

/* ------------------------------------------------------------------ */
/* 工具函数                                                            */
/* ------------------------------------------------------------------ */
int path_ext_supported(const char *path)
{
    static const char *exts[] = {
        ".txt", ".bat", ".cmd", ".ini", ".log", ".csv", ".md", ".nfo",
        ".inf", ".reg", ".vbs", ".js", ".c", ".h", ".cpp", ".bas", ".pas",
        ".html", ".htm", ".xml", ".cfg", ".dat", ".lst", ".nbc", ".me",
        ".1st", ".doc", NULL
    };
    const char *dot;
    int i;
    if (!path) return 0;
    dot = strrchr(path, '.');
    if (!dot) return 0;
    for (i = 0; exts[i]; i++)
        if (!sm_stricmp(dot, exts[i])) return 1;
    return 0;
}

int path_is_conv(const char *path)
{
    const char *dot;
    if (!path) return 0;
    dot = strrchr(path, '.');
    return dot && !sm_stricmp(dot, CONV_EXT);
}

static int name_ok(const char *n)
{
    static const char *bad = "\\/:*?\"<>|";
    if (!n || !n[0]) return 0;
    if (strpbrk(n, bad)) return 0;
    if (!sm_stricmp(n, ".") || !sm_stricmp(n, "..")) return 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/* 工作区列表                                                          */
/* ------------------------------------------------------------------ */
static char g_wsNames[64][128];
static int  g_wsN = 0;

static int match_filter(const char *name, const char *filter)
{
    if (!filter || !filter[0]) return 1;
    return sm_stristr(name, filter) != NULL;
}

static void ws_enum(void)
{
    SmDirEnt *ents = NULL;
    int n = 0, i;
    char *root = ws_root();
    g_wsN = 0;
    if (sm_list_dir(root, &ents, &n)) {
        for (i = 0; i < n && g_wsN < 64; i++) {
            if (!(ents[i].attrib & FILE_ATTRIBUTE_DIRECTORY)) continue;
            if (!match_filter(ents[i].name, g_wsFilter)) continue;
            strncpy(g_wsNames[g_wsN], ents[i].name, 127);
            g_wsNames[g_wsN][127] = 0;
            g_wsN++;
        }
        sm_free(ents);
    }
    if (g_wsN == 0 && !g_wsFilter[0]) {
        strcpy(g_wsNames[0], "Default");
        g_wsN = 1;
    }
}

void ws_populate_list(void)
{
    int i, sel = -1;
    if (!g.hWsList) return;
    SendMessageA(g.hWsList, LB_RESETCONTENT, 0, 0);
    ws_enum();
    for (i = 0; i < g_wsN; i++) {
        SendMessageA(g.hWsList, LB_ADDSTRING, 0, (LPARAM)g_wsNames[i]);
        if (!sm_stricmp(g_wsNames[i], g_wsCur)) sel = i;
    }
    if (sel < 0 && g_wsN > 0) sel = 0;
    if (sel >= 0) {
        SendMessageA(g.hWsList, LB_SETCURSEL, (WPARAM)sel, 0);
        if (sm_stricmp(g_wsNames[sel], g_wsCur))
            ws_switch_to(g_wsNames[sel]);
    }
}

void ws_set_filter(const char *text)
{
    strncpy(g_wsFilter, text ? text : "", sizeof(g_wsFilter) - 1);
    g_wsFilter[sizeof(g_wsFilter) - 1] = 0;
    sm_trim(g_wsFilter);
    ws_populate_list();
}

void ws_switch_to(const char *name)
{
    if (!name || !name[0]) return;
    strncpy(g_wsCur, name, sizeof(g_wsCur) - 1);
    g_wsCur[sizeof(g_wsCur) - 1] = 0;
    ws_tree_refresh();
    sm_status_set(1, g_wsCur);
}

int ws_create(const char *name)
{
    char *d;
    if (!name_ok(name)) return 0;
    d = ws_root();
    {
        char *p = sm_path_join(d, name);
        int ok = sm_mkdirs(p);
        sm_logf("ws_create: dir=[%s] ok=%d err=%lu", p, ok, (unsigned long)GetLastError());
        sm_free(p);
        if (ok) {
            ws_switch_to(name);
            ws_populate_list();
        }
        return ok;
    }
}

int ws_rename(const char *oldn, const char *newn)
{
    char *root = ws_root();
    char *a = sm_path_join(root, oldn);
    char *b = sm_path_join(root, newn);
    int ok = 0;
    if (!name_ok(newn)) { sm_free(a); sm_free(b); return 0; }
    if (!sm_file_exists(a) && GetFileAttributesA(a) == 0xFFFFFFFF) {
        /* 源不存在 */
    }
    ok = MoveFileA(a, b) ? 1 : 0;
    if (ok) {
        ws_switch_to(newn);
        ws_populate_list();
    }
    sm_free(a);
    sm_free(b);
    return ok;
}

int ws_delete(const char *name)
{
    char *root = ws_root();
    char *p = sm_path_join(root, name);
    int ok = sm_delete_tree(p);
    sm_free(p);
    if (ok) {
        if (!sm_stricmp(g_wsCur, name)) {
            g_wsCur[0] = 0;
            conv_set_path("");
        }
        ws_populate_list();
    }
    return ok;
}

/* ------------------------------------------------------------------ */
/* 文件树                                                              */
/* ------------------------------------------------------------------ */
static void tree_add_dir(HTREEITEM parent, const char *dir, int depth)
{
    SmDirEnt *ents = NULL;
    int n = 0, i;
    if (depth > 4) return;
    if (!sm_list_dir(dir, &ents, &n)) return;
    for (i = 0; i < n; i++) {
        char *full = sm_path_join(dir, ents[i].name);
        int isDir = (ents[i].attrib & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
        int show = isDir || match_filter(ents[i].name, g_fileFilter);
        if (show) {
            TVINSERTSTRUCTA is;
            HTREEITEM it;
            ZeroMemory(&is, sizeof(is));
            is.hParent = parent;
            is.hInsertAfter = TVI_SORT;
            is.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
            is.item.pszText = (char *)ents[i].name;
            is.item.lParam = (LPARAM)tree_add_path(full);
            is.item.iImage = isDir ? 0 : 1;
            is.item.iSelectedImage = isDir ? 0 : 1;
            it = (HTREEITEM)SendMessageA(g.hTree, TVM_INSERTITEM, 0, (LPARAM)&is);
            if (isDir) tree_add_dir(it, full, depth + 1);
            sm_free(full);
        } else {
            sm_free(full);
        }
    }
    sm_free(ents);
}

static void tree_clear(void)
{
    SendMessageA(g.hTree, TVM_DELETEITEM, 0, (LPARAM)TVI_ROOT);
    tree_free_paths();
}

void ws_tree_refresh(void)
{
    char *dir;
    HTREEITEM root;
    TVINSERTSTRUCTA is;
    char label[160];

    if (!g.hTree) return;
    tree_setup();
    tree_clear();

    dir = ws_current_dir();
    _snprintf(label, sizeof(label), "%s", g_wsCur[0] ? g_wsCur : "Default");
    label[sizeof(label) - 1] = 0;

    ZeroMemory(&is, sizeof(is));
    is.hParent = TVI_ROOT;
    is.hInsertAfter = TVI_LAST;
    is.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    is.item.pszText = label;
    is.item.lParam = (LPARAM)tree_add_path(dir);
    is.item.iImage = 0;
    is.item.iSelectedImage = 0;
    root = (HTREEITEM)SendMessageA(g.hTree, TVM_INSERTITEM, 0, (LPARAM)&is);
    tree_add_dir(root, dir, 0);
    SendMessageA(g.hTree, TVM_EXPAND, TVE_EXPAND, (LPARAM)root);
    SendMessageA(g.hTree, TVM_EXPAND, TVE_EXPAND, (LPARAM)root);
    sm_free(dir);
}

void ws_set_file_filter(const char *text)
{
    strncpy(g_fileFilter, text ? text : "", sizeof(g_fileFilter) - 1);
    g_fileFilter[sizeof(g_fileFilter) - 1] = 0;
    sm_trim(g_fileFilter);
    ws_tree_refresh();
}

char *tree_selected_path(void)
{
    HTREEITEM it = (HTREEITEM)SendMessageA(g.hTree, TVM_GETNEXTITEM, TVGN_CARET, 0);
    TVITEMA item;
    if (!it) return NULL;
    ZeroMemory(&item, sizeof(item));
    item.mask = TVIF_PARAM;
    item.hItem = it;
    if (!SendMessageA(g.hTree, TVM_GETITEM, 0, (LPARAM)&item)) return NULL;
    {
        const char *p = tree_get_path(item.lParam);
        return p ? sm_strdup(p) : NULL;
    }
}

/* ------------------------------------------------------------------ */
/* 会话文件                                                            */
/* ------------------------------------------------------------------ */
static void conv_make_name(char *out, int len)
{
    SYSTEMTIME st;
    int n = 0;
    GetLocalTime(&st);
    do {
        _snprintf(out, (size_t)len, "CONV-%04d%02d%02d-%02d%02d%02d%s",
                  st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
                  CONV_EXT);
        out[len - 1] = 0;
        if (!sm_file_exists(out)) break;
        st.wSecond = (WORD)((st.wSecond + 1) % 60);
        n++;
    } while (n < 60);
}

/* 新建会话：在工作区里生成一个新的会话文件（先写一个只有表头的文件） */
void conv_new(void)
{
    char *dir = ws_current_dir();
    char name[64];
    char *full;
    Sb b;
    char ts[64];

    conv_make_name(name, sizeof(name));
    full = sm_path_join(dir, name);

    sm_now_str(ts, sizeof(ts));
    sb_init(&b);
    sb_puts(&b, "; " SM_PRODUCT_NAME " conversation file\r\n");
    sb_printf(&b, "; Workspace=%s\r\n", g_wsCur);
    sb_printf(&b, "; Created=%s\r\n", ts);
    sb_printf(&b, "; Model=%s\r\n\r\n", g.cfg.model);
    sm_write_file(full, sb_str(&b), (long)b.len);
    sb_free(&b);

    sm_logf("conv_new: [%s] workspace=[%s]", full, g_wsCur);
    conv_set_path(full);
    chat_clear();
    chat_history_reset();
    g.turnCount = 0;
    sm_free(full);
    sm_free(dir);
    ws_tree_refresh();
}

/* 追加一段文字到当前会话文件 */
void conv_append_text(const char *text)
{
    FILE *f;
    if (!g_convFile[0] || !text) return;
    f = fopen(g_convFile, "ab");
    if (!f) return;
    fwrite(text, 1, strlen(text), f);
    fclose(f);
}

/* 把整份对话保存进会话文件（保留表头） */
int conv_save(void)
{
    char *dir;
    char *txt;
    Sb b;
    char ts[64];
    FILE *f;

    if (!g_convFile[0]) {
        dir = ws_current_dir();
        {
            char name[64];
            conv_make_name(name, sizeof(name));
            conv_set_path(sm_path_join(dir, name));
        }
        sm_free(dir);
    }
    txt = chat_transcript_text();
    sm_now_str(ts, sizeof(ts));
    sb_init(&b);
    sb_puts(&b, "; " SM_PRODUCT_NAME " conversation file\r\n");
    sb_printf(&b, "; Workspace=%s\r\n", g_wsCur);
    sb_printf(&b, "; Saved=%s\r\n", ts);
    sb_printf(&b, "; Model=%s\r\n\r\n", g.cfg.model);
    sb_puts(&b, txt);
    sm_free(txt);

    f = fopen(g_convFile, "wb");
    if (!f) { sb_free(&b); return 0; }
    fwrite(sb_str(&b), 1, b.len, f);
    fclose(f);
    sb_free(&b);
    g_convDirty = 0;
    ws_tree_refresh();
    return 1;
}

int conv_save_as(const char *path)
{
    char *txt = chat_transcript_text();
    Sb b;
    char ts[64];
    int ok;
    sm_now_str(ts, sizeof(ts));
    sb_init(&b);
    sb_puts(&b, "; " SM_PRODUCT_NAME " conversation file\r\n");
    sb_printf(&b, "; Workspace=%s\r\n", g_wsCur);
    sb_printf(&b, "; Saved=%s\r\n", ts);
    sb_printf(&b, "; Model=%s\r\n\r\n", g.cfg.model);
    sb_puts(&b, txt);
    sm_free(txt);
    ok = sm_write_file(path, sb_str(&b), (long)b.len);
    sb_free(&b);
    if (ok) {
        conv_set_path(path);
        ws_tree_refresh();
    }
    return ok;
}

/* 读取会话文件正文（跳过以 ';' 开头的表头） */
void conv_load(const char *path)
{
    long len = 0;
    char *data = sm_read_file(path, 8 * 1024 * 1024, &len);
    char *body;
    char msg[600];
    if (!data) return;
    body = data;
    /* 跳过表头 */
    while (*body == ';') {
        char *nl = strchr(body, '\n');
        if (!nl) { body += strlen(body); break; }
        body = nl + 1;
    }
    chat_clear();
    chat_set_raw(body);
    chat_history_reset();
    g.turnCount = 0;
    conv_set_path(path);
    _snprintf(msg, sizeof(msg), L(STR_MSG_LOADED_CONV), path);
    msg[sizeof(msg) - 1] = 0;
    chat_append(CT_SYS, msg);
    sm_free(data);
    (void)g_convSeq;
}

/* ------------------------------------------------------------------ */
/* 打开文件（双击文件树）                                              */
/* ------------------------------------------------------------------ */
void ws_open_path(const char *path)
{
    if (!path) return;
    if (path_is_conv(path)) {
        if (g_convDirty) {
            int r = MessageBoxA(g.hFrame, L(STR_TREE_SAVEFIRST), SM_SHORT_NAME,
                                MB_ICONQUESTION | MB_YESNOCANCEL);
            if (r == IDCANCEL) return;
            if (r == IDYES) conv_save();
        }
        conv_load(path);
        return;
    }
    if (path_ext_supported(path)) {
        editor_open(path);
        return;
    }
    {
        char m[700];
        _snprintf(m, sizeof(m), L(STR_TREE_NOTSUPPORTED), path);
        m[sizeof(m) - 1] = 0;
        if (MessageBoxA(g.hFrame, m, L(STR_TREE_OPENWITH),
                        MB_ICONQUESTION | MB_YESNO) == IDYES)
            ShellExecuteA(g.hFrame, "open", path, NULL, NULL, SW_SHOWNORMAL);
    }
}

/* ------------------------------------------------------------------ */
void ws_init(void)
{
    char *root = ws_root();
    char *dflt = sm_path_join(root, "Default");
    sm_mkdirs(dflt);
    sm_free(dflt);
    if (!g_wsCur[0]) strcpy(g_wsCur, "Default");
    ws_populate_list();
    ws_tree_refresh();
}
