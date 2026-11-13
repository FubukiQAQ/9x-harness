/* Axiom NeuroBench 2003 - 基础工具函数 */
#include "sm.h"
#include <ctype.h>

static char g_app_dir[MAX_PATH + 2];
static char g_log_path[MAX_PATH + 2];

/* ------------------------------------------------------------------ */
void *sm_alloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p) { MessageBoxA(NULL, "内存不足，程序无法继续运行。", SM_SHORT_NAME, MB_ICONSTOP | MB_OK); ExitProcess(1); }
    return p;
}

void *sm_calloc(size_t n)
{
    void *p = calloc(1, n ? n : 1);
    if (!p) { MessageBoxA(NULL, "内存不足，程序无法继续运行。", SM_SHORT_NAME, MB_ICONSTOP | MB_OK); ExitProcess(1); }
    return p;
}

void *sm_realloc(void *p, size_t n)
{
    void *q = realloc(p, n ? n : 1);
    if (!q) { MessageBoxA(NULL, "内存不足，程序无法继续运行。", SM_SHORT_NAME, MB_ICONSTOP | MB_OK); ExitProcess(1); }
    return q;
}

void sm_free(void *p) { if (p) free(p); }

char *sm_strdup(const char *s)
{
    size_t n;
    char *d;
    if (!s) return NULL;
    n = strlen(s);
    d = (char *)sm_alloc(n + 1);
    memcpy(d, s, n + 1);
    return d;
}

char *sm_strndup(const char *s, size_t n)
{
    char *d = (char *)sm_alloc(n + 1);
    memcpy(d, s, n);
    d[n] = 0;
    return d;
}

/* ------------------------------------------------------------------ */
void sb_init(Sb *b) { memset(b, 0, sizeof(*b)); }

void sb_free(Sb *b) { if (b->p) free(b->p); memset(b, 0, sizeof(*b)); }

void sb_clear(Sb *b) { b->len = 0; if (b->p) b->p[0] = 0; }

static void sb_grow(Sb *b, size_t need)
{
    size_t nc;
    if (b->oom) return;
    if (b->len + need + 1 <= b->cap) return;
    nc = b->cap ? b->cap : 256;
    while (nc < b->len + need + 1) {
        if (nc > 0x40000000u) { b->oom = 1; return; }
        nc *= 2;
    }
    b->p = (char *)realloc(b->p, nc);
    if (!b->p) { b->oom = 1; b->cap = 0; b->len = 0; return; }
    b->cap = nc;
    if (b->len == 0) b->p[0] = 0;
}

void sb_putn(Sb *b, const char *s, size_t n)
{
    if (!s || !n) return;
    sb_grow(b, n);
    if (b->oom) return;
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = 0;
}

void sb_puts(Sb *b, const char *s) { if (s) sb_putn(b, s, strlen(s)); }

void sb_putc(Sb *b, int c)
{
    char ch = (char)c;
    sb_putn(b, &ch, 1);
}

void sb_vprintf(Sb *b, const char *fmt, va_list ap)
{
    char tmp[2048];
    int n;
    n = _vsnprintf(tmp, sizeof(tmp) - 1, fmt, ap);
    if (n < 0) n = (int)sizeof(tmp) - 1;
    tmp[n] = 0;
    sb_putn(b, tmp, (size_t)n);
}

void sb_printf(Sb *b, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    sb_vprintf(b, fmt, ap);
    va_end(ap);
}

void sb_put_utf8(Sb *b, const char *utf8)
{
    char *a;
    if (!utf8) return;
    a = sm_utf8_to_ansi(utf8);
    if (a) { sb_puts(b, a); sm_free(a); }
}

const char *sb_str(Sb *b) { return b->p ? b->p : ""; }

char *sb_take(Sb *b)
{
    char *p;
    if (!b->p) { p = sm_strdup(""); }
    else p = b->p;
    b->p = NULL; b->len = b->cap = 0;
    return p;
}

/* ------------------------------------------------------------------ */
char *sm_conv(UINT from, UINT to, const char *s, int len)
{
    int wlen;
    WCHAR *w;
    int alen;
    char *out;

    if (!s) return sm_strdup("");
    if (len < 0) len = (int)strlen(s);
    if (len == 0) return sm_strdup("");

    wlen = MultiByteToWideChar(from, 0, s, len, NULL, 0);
    if (wlen <= 0) return sm_strdup("");
    w = (WCHAR *)sm_alloc(sizeof(WCHAR) * (wlen + 1));
    MultiByteToWideChar(from, 0, s, len, w, wlen);
    w[wlen] = 0;

    alen = WideCharToMultiByte(to, 0, w, wlen, NULL, 0, NULL, NULL);
    if (alen <= 0) { sm_free(w); return sm_strdup(""); }
    out = (char *)sm_alloc(alen + 1);
    WideCharToMultiByte(to, 0, w, wlen, out, alen, NULL, NULL);
    out[alen] = 0;
    sm_free(w);
    return out;
}

char *sm_utf8_to_ansi(const char *u) { return sm_conv(CP_UTF8, CP_ACP, u, -1); }
char *sm_ansi_to_utf8(const char *a) { return sm_conv(CP_ACP, CP_UTF8, a, -1); }

UINT sm_system_codepage(void) { return GetACP(); }

/* ------------------------------------------------------------------ */
char *sm_exe_dir(void)
{
    char buf[MAX_PATH + 2];
    char *p;
    DWORD n = GetModuleFileNameA(NULL, buf, MAX_PATH);
    buf[n] = 0;
    p = strrchr(buf, '\\');
    if (p) p[1] = 0; else strcpy(buf, ".\\");
    return sm_strdup(buf);
}

char *sm_app_dir(void)
{
    if (g_app_dir[0]) return sm_strdup(g_app_dir);
    {
        char *d = sm_exe_dir();
        strncpy(g_app_dir, d, MAX_PATH);
        g_app_dir[MAX_PATH] = 0;
        sm_free(d);
    }
    return sm_strdup(g_app_dir);
}

char *sm_path_join(const char *dir, const char *name)
{
    Sb b;
    sb_init(&b);
    sb_puts(&b, dir ? dir : "");
    if (b.len && b.p[b.len - 1] != '\\' && b.p[b.len - 1] != '/') sb_putc(&b, '\\');
    sb_puts(&b, name ? name : "");
    return sb_take(&b);
}

char *sm_appdata_dir(void)
{
    char buf[MAX_PATH + 2];
    char *p;
    typedef HRESULT (WINAPI *PFN_SGFP)(HWND, int, HANDLE, DWORD, LPSTR);
    ZeroMemory(buf, sizeof(buf));
    /* 先取环境变量（Win9x/NT 通用），再退回 shell32 动态调用 */
    if (!GetEnvironmentVariableA("APPDATA", buf, MAX_PATH) || !buf[0]) {
        HMODULE h = LoadLibraryA("shell32.dll");
        PFN_SGFP pfn = h ? (PFN_SGFP)GetProcAddress(h, "SHGetFolderPathA") : NULL;
        if (pfn && SUCCEEDED(pfn(NULL, 0x001a /*CSIDL_APPDATA*/, NULL, 0, buf)) && buf[0]) {
            p = sm_path_join(buf, SM_SHORT_NAME);
            sm_mkdirs(p);
            sm_free(p);
            return sm_path_join(buf, SM_SHORT_NAME);
        }
        buf[0] = 0;
    }
    if (buf[0]) {
        p = sm_path_join(buf, SM_SHORT_NAME);
        sm_mkdirs(p);
        sm_free(p);
        return sm_path_join(buf, SM_SHORT_NAME);
    }
    return sm_app_dir();
}

char *sm_ini_path(void)
{
    char *dir = sm_app_dir();
    char *p = sm_path_join(dir, SM_INI_NAME);
    char *ad;
    sm_free(dir);
    /* 程序目录可写则用程序目录（Win9x 风格），否则退到 AppData */
    if (sm_file_exists(p)) return p;
    dir = sm_app_dir();
    {
        char *probe = sm_path_join(dir, "_wtest.tmp");
        FILE *f = fopen(probe, "wb");
        sm_free(dir);
        if (f) { fclose(f); DeleteFileA(probe); sm_free(probe); return p; }
        sm_free(probe);
    }
    ad = sm_appdata_dir();
    sm_free(p);
    p = sm_path_join(ad, SM_INI_NAME);
    sm_free(ad);
    return p;
}

char *sm_temp_dir(void)
{
    char buf[MAX_PATH + 2];
    DWORD n = GetTempPathA(MAX_PATH, buf);
    if (n == 0) { GetWindowsDirectoryA(buf, MAX_PATH); strcat(buf, "\\TEMP"); }
    return sm_strdup(buf);
}

/* ------------------------------------------------------------------ */
int sm_file_exists(const char *path)
{
    DWORD a = GetFileAttributesA(path);
    return (a != 0xFFFFFFFF) && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

long sm_file_size(const char *path)
{
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &d)) return -1;
    if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) return -1;
    return (long)d.nFileSizeLow;
}

char *sm_read_file(const char *path, long maxlen, long *outlen)
{
    FILE *f;
    long sz, got;
    char *buf;
    if (outlen) *outlen = 0;
    f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    if (maxlen > 0 && sz > maxlen) sz = maxlen;
    buf = (char *)sm_alloc((size_t)sz + 1);
    got = (long)fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (got < 0) got = 0;
    buf[got] = 0;
    if (outlen) *outlen = got;
    return buf;
}

int sm_write_file(const char *path, const void *data, long len)
{
    FILE *f;
    size_t w;
    f = fopen(path, "wb");
    if (!f) return 0;
    w = len > 0 ? fwrite(data, 1, (size_t)len, f) : 0;
    fclose(f);
    return (long)w == len;
}

int sm_mkdirs(const char *path)
{
    char tmp[MAX_PATH + 2];
    char *p;
    size_t i, n;
    if (!path) return 0;
    n = strlen(path);
    if (n >= MAX_PATH) return 0;
    strcpy(tmp, path);
    for (i = 0; i < n; i++) if (tmp[i] == '/') tmp[i] = '\\';
    for (p = tmp + 1; *p; p++) {
        if (*p == '\\') {
            *p = 0;
            CreateDirectoryA(tmp, NULL);
            *p = '\\';
        }
    }
    CreateDirectoryA(tmp, NULL);
    {
        DWORD a = GetFileAttributesA(tmp);
        return (a != 0xFFFFFFFF) && (a & FILE_ATTRIBUTE_DIRECTORY);
    }
}

int sm_copy_file(const char *src, const char *dst)
{
    return CopyFileA(src, dst, FALSE) ? 1 : 0;
}

int sm_delete_tree(const char *path)
{
    /* 只删一层内的文件 + 空目录，够用 */
    char buf[MAX_PATH + 2];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    char *pat = sm_path_join(path, "*.*");
    h = FindFirstFileA(pat, &fd);
    sm_free(pat);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            char *full;
            if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
            full = sm_path_join(path, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                sm_delete_tree(full);
                RemoveDirectoryA(full);
            } else {
                SetFileAttributesA(full, FILE_ATTRIBUTE_NORMAL);
                DeleteFileA(full);
            }
            sm_free(full);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    strcpy(buf, path);
    return RemoveDirectoryA(buf) ? 1 : 0;
}

void sm_touch_file(const char *path, const char *text)
{
    sm_write_file(path, text, (long)strlen(text ? text : ""));
}

int sm_list_dir(const char *path, SmDirEnt **ents, int *count)
{
    char *pat;
    WIN32_FIND_DATAA fd;
    HANDLE h;
    int n = 0, cap = 32;
    SmDirEnt *arr;

    *ents = NULL; *count = 0;
    if (!path || !path[0]) return 0;
    if (path[strlen(path) - 1] == '\\' || path[strlen(path) - 1] == '/')
        pat = sm_path_join(path, "*.*");
    else
        pat = sm_path_join(path, "*.*");
    h = FindFirstFileA(pat, &fd);
    sm_free(pat);
    if (h == INVALID_HANDLE_VALUE) return 0;

    arr = (SmDirEnt *)sm_alloc(sizeof(SmDirEnt) * cap);
    do {
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
        if (n == cap) { cap *= 2; arr = (SmDirEnt *)sm_realloc(arr, sizeof(SmDirEnt) * cap); }
        strncpy(arr[n].name, fd.cFileName, MAX_PATH - 1);
        arr[n].name[MAX_PATH - 1] = 0;
        arr[n].attrib = fd.dwFileAttributes;
        arr[n].size = fd.nFileSizeLow;
        arr[n].mtime = fd.ftLastWriteTime;
        n++;
    } while (FindNextFileA(h, &fd));
    FindClose(h);

    /* 目录在前，再按名字排序（插入排序，条目不多） */
    {
        int i, j;
        for (i = 1; i < n; i++) {
            SmDirEnt key = arr[i];
            int kd = (key.attrib & FILE_ATTRIBUTE_DIRECTORY) ? 0 : 1;
            j = i - 1;
            while (j >= 0) {
                int jd = (arr[j].attrib & FILE_ATTRIBUTE_DIRECTORY) ? 0 : 1;
                if (jd < kd || (jd == kd && sm_stricmp(arr[j].name, key.name) <= 0)) break;
                arr[j + 1] = arr[j];
                j--;
            }
            arr[j + 1] = key;
        }
    }
    *ents = arr;
    *count = n;
    return 1;
}

/* ------------------------------------------------------------------ */
void sm_filetime_str(const FILETIME *ft, char *buf, size_t len)
{
    FILETIME lt;
    SYSTEMTIME st;
    if (!FileTimeToLocalFileTime(ft, &lt) || !FileTimeToSystemTime(&lt, &st)) {
        strncpy(buf, "????-??-?? ??:??", len);
        return;
    }
    _snprintf(buf, len, "%04d-%02d-%02d %02d:%02d",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    buf[len - 1] = 0;
}

void sm_now_str(char *buf, size_t len)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    _snprintf(buf, len, "%04d-%02d-%02d %02d:%02d:%02d",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    buf[len - 1] = 0;
}

void sm_now_str_era(char *buf, size_t len)
{
    static const char *wd[] = { "日", "一", "二", "三", "四", "五", "六" };
    SYSTEMTIME st;
    GetLocalTime(&st);
    _snprintf(buf, len, "%d年%d月%d日 星期%s %02d:%02d",
              st.wYear, st.wMonth, st.wDay, wd[st.wDayOfWeek % 7], st.wHour, st.wMinute);
    buf[len - 1] = 0;
}

DWORD sm_now(void) { return (DWORD)time(NULL); }

/* 把窗口居中到屏幕或父窗口 */
void sm_center_window(HWND hwnd)
{
    RECT r, rd;
    HWND d = GetParent(hwnd);
    int w, h, x, y;
    if (!hwnd) return;
    GetWindowRect(hwnd, &r);
    w = r.right - r.left;
    h = r.bottom - r.top;
    if (!d || !IsWindowVisible(d)) {
        SystemParametersInfoA(SPI_GETWORKAREA, 0, &rd, 0);
    } else GetWindowRect(d, &rd);
    x = rd.left + ((rd.right - rd.left) - w) / 2;
    y = rd.top + ((rd.bottom - rd.top) - h) / 2;
    if (x < rd.left) x = rd.left;
    if (y < rd.top) y = rd.top;
    SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

/* ------------------------------------------------------------------ */
DWORD sm_crc32(const void *data, size_t len)
{
    static DWORD tab[256];
    static int init = 0;
    const unsigned char *p = (const unsigned char *)data;
    DWORD c;
    size_t i;
    if (!init) {
        DWORD k; int n, j;
        for (n = 0; n < 256; n++) {
            k = (DWORD)n;
            for (j = 0; j < 8; j++) k = (k & 1) ? (0xEDB88320u ^ (k >> 1)) : (k >> 1);
            tab[n] = k;
        }
        init = 1;
    }
    c = 0xFFFFFFFFu;
    for (i = 0; i < len; i++) c = tab[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

void sm_random_bytes(void *out, size_t n)
{
    typedef BOOLEAN (WINAPI *PFNRGR)(PVOID, ULONG);
    static PFNRGR pfn = NULL;
    static int tried = 0;
    if (!tried) {
        HMODULE h = LoadLibraryA("advapi32.dll");
        if (h) pfn = (PFNRGR)GetProcAddress(h, "SystemFunction036");
        tried = 1;
    }
    if (pfn && pfn(out, (ULONG)n)) return;
    {
        unsigned char *p = (unsigned char *)out;
        DWORD t = GetTickCount();
        DWORD pid = GetCurrentProcessId();
        size_t i;
        for (i = 0; i < n; i++) {
            t = t * 1103515245u + 12345u + pid;
            p[i] = (unsigned char)(t >> 16);
        }
    }
}

int sm_stricmp(const char *a, const char *b)
{
    if (!a) a = ""; if (!b) b = "";
    return lstrcmpiA(a, b);
}

int sm_strnicmp(const char *a, const char *b, size_t n)
{
    if (!a) a = ""; if (!b) b = "";
    return _strnicmp(a, b, n);
}

const char *sm_stristr(const char *hay, const char *needle)
{
    size_t nl;
    if (!hay || !needle) return NULL;
    nl = strlen(needle);
    if (!nl) return hay;
    for (; *hay; hay++)
        if (!_strnicmp(hay, needle, nl)) return hay;
    return NULL;
}

void sm_logf(const char *fmt, ...)
{
    va_list ap;
    FILE *f = NULL;
    char ts[40];

    if (!g_log_path[0]) {
        char *d = sm_app_dir();
        if (d && d[0]) {
            _snprintf(g_log_path, sizeof(g_log_path) - 1, "%sNEUROBENCH.LOG", d);
            g_log_path[sizeof(g_log_path) - 1] = 0;
            f = fopen(g_log_path, "a");
            if (!f) g_log_path[0] = 0;
        }
        sm_free(d);
        if (!f) {
            /* 程序目录不可写（例如直接在光盘上运行）：改用 %TEMP%，再不行用 C:\ */
            char tmp[MAX_PATH + 2];
            tmp[0] = 0;
            if (GetTempPathA(MAX_PATH, tmp) && tmp[0]) {
                _snprintf(g_log_path, sizeof(g_log_path) - 1, "%sNEUROBENCH.LOG", tmp);
                g_log_path[sizeof(g_log_path) - 1] = 0;
                f = fopen(g_log_path, "a");
            }
            if (!f) {
                strcpy(g_log_path, "C:\\NEUROBENCH.LOG");
                f = fopen(g_log_path, "a");
            }
            if (!f) { g_log_path[0] = 0; return; }
        }
    } else {
        f = fopen(g_log_path, "a");
        if (!f) return;
    }

    sm_now_str(ts, sizeof(ts));
    fprintf(f, "[%s] ", ts);
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fprintf(f, "\r\n");
    fclose(f);
}

const char *sm_log_path(void)
{
    return g_log_path;
}

/* ------------------------------------------------------------------ */
/* 只读多行编辑框滚动时的重影                                          */
/*                                                                    */
/* 这类控件滚动时只重画新露出来的一条，行高缓存和实际字体高度稍有出入  */
/* 就会留下上一屏的像素，看起来像字体重影（中文尤其明显）。这里给控件  */
/* 挂一层子类：每次滚动之后立刻整块重画并同步刷出去。                  */
/* ------------------------------------------------------------------ */
static LRESULT CALLBACK sm_scrollfix_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    WNDPROC oldp = (WNDPROC)GetPropA(h, "nboldproc");
    LRESULT r = CallWindowProcA(oldp, h, m, w, l);
    switch (m) {
    case WM_VSCROLL:
    case WM_HSCROLL:
    case WM_MOUSEWHEEL:
    case WM_KEYDOWN:
    case WM_LBUTTONDOWN:
        InvalidateRect(h, NULL, TRUE);
        UpdateWindow(h);          /* 立刻重画，别等消息循环空闲 */
        break;
    default:
        break;
    }
    return r;
}

void sm_fix_scroll_repaint(HWND h)
{
    WNDPROC oldp;
    if (!h) return;
    if (GetPropA(h, "nboldproc")) return;      /* 已经装过 */
    oldp = (WNDPROC)SetWindowLongA(h, GWL_WNDPROC, (LONG)(INT_PTR)sm_scrollfix_proc);
    if (oldp) SetPropA(h, "nboldproc", (HANDLE)oldp);
}

/* ------------------------------------------------------------------ */
/* 物理内存：GlobalMemoryStatusEx 是 NT 专有的导出，Windows 9x 上加载时  */
/* 就会因为导入表解析失败而拒绝运行，所以按需取，取不到退回老接口。      */
/* ------------------------------------------------------------------ */
int sm_memory_info(unsigned long *totalMB, unsigned long *availMB, unsigned long *loadPct)
{
    typedef BOOL (WINAPI *PFN_GMSE)(LPVOID);
    static PFN_GMSE pfn = NULL;
    static int tried = 0;

    if (!tried) {
        HMODULE k = GetModuleHandleA("kernel32.dll");
        if (!k) k = LoadLibraryA("kernel32.dll");
        if (k) pfn = (PFN_GMSE)GetProcAddress(k, "GlobalMemoryStatusEx");
        tried = 1;
    }
    if (pfn) {
        MEMORYSTATUSEX ms;
        ZeroMemory(&ms, sizeof(ms));
        ms.dwLength = sizeof(ms);
        if (pfn(&ms)) {
            if (totalMB) *totalMB = (unsigned long)(ms.ullTotalPhys >> 20);
            if (availMB) *availMB = (unsigned long)(ms.ullAvailPhys >> 20);
            if (loadPct) *loadPct = (unsigned long)ms.dwMemoryLoad;
            return 1;
        }
    }
    {
        MEMORYSTATUS m;                 /* Windows 95/98/ME 只有这个 */
        ZeroMemory(&m, sizeof(m));
        m.dwLength = sizeof(m);
        GlobalMemoryStatus(&m);
        if (totalMB) *totalMB = (unsigned long)(m.dwTotalPhys >> 20);
        if (availMB) *availMB = (unsigned long)(m.dwAvailPhys >> 20);
        if (loadPct) *loadPct = (unsigned long)m.dwMemoryLoad;
        return 1;
    }
}


/* ------------------------------------------------------------------ */
/* 合并模型自己折的硬换行。                                            */
/* 模型常在 60~80 列处硬换行，而对话框会按窗口宽度自动折行，于是看起来 */
/* 就像“没写满就换行”。这里把这类软折行接回长行；段落之间、列表项、    */
/* 缩进块（代码）保持原样。                                            */
/* ------------------------------------------------------------------ */
static int rf_is_block_start(const char *l)
{
    const char *q = l;

    if (!l || !*l) return 1;
    if (*l == ' ' || *l == '\t') return 1;               /* 缩进 / 代码 */
    if (*l == '-' || *l == '*' || *l == '+' || *l == '>' ||
        *l == '#' || *l == '|' || *l == '=') return 1;   /* 列表 / 引用 / 表格 */
    if ((unsigned char)l[0] == 0xA1 &&
        ((unsigned char)l[1] == 0xA4 || (unsigned char)l[1] == 0xA2)) return 1;

    {   /* 数字编号 1.  1)  1:  1、 */
        int any = 0;
        while (*q >= '0' && *q <= '9') { q++; any = 1; }
        if (any) {
            if (*q == '.' || *q == ')' || *q == ':') return 1;
            if ((unsigned char)q[0] == 0xA1 && (unsigned char)q[1] == 0xA2) return 1;
        }
    }
    return 0;
}

static int rf_ends_heading(const char *l, int len)
{
    if (len <= 0) return 0;
    if (l[len - 1] == ':') return 1;
    if (len >= 2 && (unsigned char)l[len - 2] == 0xA3 &&
        (unsigned char)l[len - 1] == 0xBA) return 1;    /* GBK 全角冒号 */
    return 0;
}

char *sm_reflow_text(const char *in)
{
    Sb b;
    const char *p;
    const char *lineStart;
    int prevEmpty = 1;
    int prevHeading = 0;

    if (!in) return sm_strdup("");

    sb_init(&b);
    p = in;
    lineStart = p;

    for (;;) {
        int atEnd = (*p == 0);

        if (atEnd || *p == '\n' || *p == '\r') {
            int n = (int)(p - lineStart);

            if (n == 0) {
                if (!atEnd) sb_puts(&b, "\r\n");   /* 空行：段落分隔，保留 */
                prevEmpty = 1;
                prevHeading = 0;
            } else {
                int isBlock = rf_is_block_start(lineStart);

                if (prevEmpty || isBlock || prevHeading) {
                    if (b.len > 0) sb_puts(&b, "\r\n");
                } else {
                    /* 接回上一行；两端都是中文时不加空格 */
                    unsigned char last = (unsigned char)b.p[b.len - 1];
                    unsigned char first = (unsigned char)lineStart[0];
                    if (!((last >= 0x81) && (first >= 0x81))) sb_putc(&b, ' ');
                }
                sb_putn(&b, lineStart, (size_t)n);
                prevEmpty = 0;
                prevHeading = rf_ends_heading(lineStart, n);
            }

            if (atEnd) break;
            if (*p == '\r' && p[1] == '\n') p += 2;
            else p++;
            lineStart = p;
            continue;
        }
        p++;
    }
    return sb_take(&b);
}

void sm_trim(char *s)
{
    char *p;
    size_t n;
    if (!s) return;
    p = s;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) s[--n] = 0;
}

long sm_to_long(const char *s, long def)
{
    if (!s || !*s) return def;
    return strtol(s, NULL, 10);
}

/* RtlGetVersion - 在 Win8.1+ 上仍返回真实版本 */
typedef struct {
    ULONG dwOSVersionInfoSize;
    ULONG dwMajorVersion;
    ULONG dwMinorVersion;
    ULONG dwBuildNumber;
    ULONG dwPlatformId;
    WCHAR szCSDVersion[128];
} SM_OSVI;

char *sm_get_os_name(void)
{
    typedef LONG (WINAPI *PFN_RTLGETVERSION)(SM_OSVI *);
    static PFN_RTLGETVERSION pfn = NULL;
    static int tried = 0;
    SM_OSVI vi;
    DWORD major = 0, minor = 0, build = 0, plat = 0;
    char csd[128];
    char buf[256];

    csd[0] = 0;

    if (!tried) {
        HMODULE h = GetModuleHandleA("ntdll.dll");
        if (!h) h = LoadLibraryA("ntdll.dll");
        if (h) pfn = (PFN_RTLGETVERSION)GetProcAddress(h, "RtlGetVersion");
        tried = 1;
    }

    /* 1) NT 系：RtlGetVersion 能绕开兼容性垫片，拿到真实版本 */
    if (pfn) {
        ZeroMemory(&vi, sizeof(vi));
        vi.dwOSVersionInfoSize = sizeof(vi);
        if (pfn(&vi) == 0 &&
            vi.dwMajorVersion >= 3 && vi.dwMajorVersion <= 15 &&
            vi.dwMinorVersion <= 99 && vi.dwBuildNumber < 100000) {
            major = vi.dwMajorVersion;
            minor = vi.dwMinorVersion;
            build = vi.dwBuildNumber;
            plat  = vi.dwPlatformId;
            WideCharToMultiByte(CP_ACP, 0, vi.szCSDVersion, -1, csd, sizeof(csd), NULL, NULL);
        }
    }

    /* 2) Windows 9x / 上面失败：GetVersionExA */
    if (major == 0) {
        OSVERSIONINFOA o;
        ZeroMemory(&o, sizeof(o));
        o.dwOSVersionInfoSize = sizeof(o);
        if (GetVersionExA(&o) && o.dwMajorVersion >= 3) {
            major = o.dwMajorVersion;
            minor = o.dwMinorVersion;
            build = o.dwBuildNumber;
            plat  = o.dwPlatformId;      /* ← 之前漏了这一行，98 被当成 NT 4.0 */
            strncpy(csd, o.szCSDVersion, sizeof(csd) - 1);
            csd[sizeof(csd) - 1] = 0;
        }
    }

    /* 3) 还不行就用最老的 GetVersion()，哪里有都有 */
    if (major == 0) {
        DWORD v = GetVersion();
        major = LOBYTE(LOWORD(v));
        minor = HIBYTE(LOWORD(v));
        build = HIWORD(v) & 0x7FFF;
        plat  = (HIWORD(v) & 0x8000) ? 1 : 2;
    }

    /* Windows 9x 的 build 号有时不可靠，用 GetVersion() 校正一次 */
    if (plat == 1) {
        DWORD v = GetVersion();
        DWORD b2 = HIWORD(v) & 0x7FFF;
        if (build == 0 || build > 100000) build = b2;
    }
    if (build > 100000) build = 0;

    /* ---- 取名字 ---- */
    if (plat == 1) {
        if (major == 4 && minor == 0)       strcpy(buf, "Windows 95");
        else if (major == 4 && minor >= 90) strcpy(buf, "Windows Me");
        else if (major == 4)                strcpy(buf, "Windows 98");
        else                                strcpy(buf, "Windows 9x");
    } else if (major == 5 && minor == 0)    strcpy(buf, "Windows 2000");
    else if (major == 5 && minor == 1)      strcpy(buf, "Windows XP");
    else if (major == 5 && minor == 2)      strcpy(buf, "Windows Server 2003");
    else if (major == 4)                    strcpy(buf, "Windows NT 4.0");
    else if (major == 6 && minor == 0)      strcpy(buf, "Windows Vista");
    else if (major == 6 && minor == 1)      strcpy(buf, "Windows 7");
    else if (major == 6)                    strcpy(buf, "Windows 8/8.1");
    else if (major == 10)                   strcpy(buf, "Windows 10/11");
    else _snprintf(buf, sizeof(buf), "Windows %lu.%lu", major, minor);

    if (csd[0]) {
        sm_trim(csd);
        if (csd[0]) {
            strcat(buf, " ");
            strcat(buf, csd);
        }
    }
    if (build) {
        _snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf) - 1,
                  " (版本 %lu.%lu.%lu, %d 位)",
                  major, minor, build, (int)(sizeof(void *) * 8));
    } else {
        _snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf) - 1,
                  " (版本 %lu.%lu, %d 位)",
                  major, minor, (int)(sizeof(void *) * 8));
    }
    buf[sizeof(buf) - 1] = 0;
    return sm_strdup(buf);
}