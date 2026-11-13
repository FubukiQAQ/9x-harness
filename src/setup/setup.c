/* =====================================================================
 * Axiom NeuroBench 2003 - 安装向导 (InstallShield 风格)
 *
 * 界面完全按 90 年代末 InstallShield 5/6 的向导样式手绘：
 * 左侧竖幅位图 + 右侧标题正文 + 底部 < 上一步 / 下一步 > / 取消。
 * ===================================================================== */
#include "../common/sm.h"
#include "../common/sm_arc.h"
#include "../common/nb_lang.h"
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>

#define WIZ_W        500
#define WIZ_H        360
#define CONTENT_X    16
#define CONTENT_W    468
#define BTN_Y        326
#define BTN_H        27

#define IDC_TITLE     3001
#define IDC_BACK      3002
#define IDC_NEXT      3003
#define IDC_CANCEL    3004
#define IDC_PAGEBASE  3100
#define IDC_DIR_BROWSE 3200
#define IDC_LIC_TEXT   3201
#define IDC_LIC_ACCEPT 3202
#define IDC_LIC_DECLINE 3203
#define IDC_USER_NAME  3204
#define IDC_USER_COMP  3205
#define IDC_USER_CDK   3206
#define IDC_DIR_EDIT   3207
#define IDC_FOLDER_EDIT 3208
#define IDC_FOLDER_LIST 3209
#define IDC_AUTORUN_CHK 3210
#define IDC_DESKTOP_CHK 3211
#define IDC_PROG_TEXT  3212
#define IDC_PROG_BAR   3213
#define IDC_LANG_ZH    3214
#define IDC_LANG_EN    3215

enum {
    PG_LANGUAGE = 0,
    PG_WELCOME,
    PG_LICENSE,
    PG_USER,
    PG_DIR,
    PG_FOLDER,
    PG_PROGRESS,
    PG_FINISH,
    PG_COUNT
};

typedef struct {
    HINSTANCE hInst;
    HWND      h;
    HWND      hName, hCompany, hCdk, hDir, hFolder, hLic, hProg, hProgText;
    HWND      hAccept, hDecline, hAutoRun, hDesktop;
    HWND      hLangZh, hLangEn;
    HFONT     fLic;
    HWND      ctl[32];
    int       nctl;
    int       page;
    int       installed;
    int       createDesktop;
    int       autoPage;
    int       autostart;
    int       licenceOk;
    SmArc     memArc;       /* 单文件安装包里内嵌的 DATA1.CAB */
    int       hasMemArc;

    char      appName[128];
    char      appVersion[64];
    char      defaultDir[MAX_PATH];
    char      userName[64];
    char      userCompany[64];
    char      cdk[64];
    char      dir[MAX_PATH];
    char      folder[64];
    char      setupDir[MAX_PATH];
    char      dataFile[MAX_PATH];

    HFONT     fUI, fBold, fTitle, fSmall;
    HBRUSH    brFace;
} Setup;

static Setup S;

/* ------------------------------------------------------------------ */
/* ---------------- 崩溃定位用的检查点 ---------------- */
static int g_ck = 0;

static void ck(int n, const char *what)
{
    g_ck = n;
    sm_logf("ck=%d %s", n, what);
}

static LONG WINAPI setup_fault_filter(EXCEPTION_POINTERS *ep)
{
    if (ep && ep->ExceptionRecord && ep->ContextRecord) {
        sm_logf("FAULT ck=%d code=%08lX eip=%08lX esp=%08lX ebp=%08lX "
                "eax=%08lX ebx=%08lX ecx=%08lX edx=%08lX esi=%08lX edi=%08lX",
                g_ck,
                (unsigned long)ep->ExceptionRecord->ExceptionCode,
                (unsigned long)ep->ContextRecord->Eip,
                (unsigned long)ep->ContextRecord->Esp,
                (unsigned long)ep->ContextRecord->Ebp,
                (unsigned long)ep->ContextRecord->Eax,
                (unsigned long)ep->ContextRecord->Ebx,
                (unsigned long)ep->ContextRecord->Ecx,
                (unsigned long)ep->ContextRecord->Edx,
                (unsigned long)ep->ContextRecord->Esi,
                (unsigned long)ep->ContextRecord->Edi);
    } else {
        sm_logf("FAULT ck=%d (no context)", g_ck);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

static HWND mk(const char *cls, const char *txt, DWORD style, int x, int y,
               int w, int h, int id, HFONT f)
{
    HWND c = CreateWindowExA(0, cls, txt,
                             WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | style,
                             x, y, w, h, S.h, (HMENU)(INT_PTR)id, S.hInst, NULL);
    if (c) {
        if (f) SendMessageA(c, WM_SETFONT, (WPARAM)f, MAKELPARAM(TRUE, 0));
        if (S.nctl < (int)(sizeof(S.ctl) / sizeof(S.ctl[0]))) S.ctl[S.nctl++] = c;
    }
    return c;
}

static void page_destroy(void)
{
    ck(33, "page_destroy enter");
    int i;
    for (i = 0; i < S.nctl; i++)
        if (S.ctl[i]) DestroyWindow(S.ctl[i]);
    S.nctl = 0;
    S.hName = S.hCompany = S.hCdk = NULL;
    S.hDir = S.hFolder = S.hLic = S.hProg = S.hProgText = NULL;
    S.hAccept = S.hDecline = S.hAutoRun = S.hDesktop = NULL;
    S.hLangZh = S.hLangEn = NULL;
}

static void setup_msg(const char *text, UINT flags)
{
    if (S.h && (flags & MB_ICONSTOP)) {
        char m[900];
        _snprintf(m, sizeof(m), "%s\r\n\r\n--\r\n%s%s", text,
                  TS("详细记录已写入：", "Details were written to: "),
                  sm_log_path()[0] ? sm_log_path() : "(none)");
        m[sizeof(m) - 1] = 0;
        MessageBoxA(S.h, m, TS("安装程序", "Setup"), flags);
        return;
    }
    if (S.h) MessageBoxA(S.h, text, TS("安装程序",
           "Setup"), flags);
    else sm_logf("SETUP(静默): %s", text);
}

static void pump(void)
{
    MSG m;
    while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&m);
        DispatchMessageA(&m);
    }
}

/* ------------------------------------------------------------------ */
/* 单文件安装包支持                                                    */
/*                                                                    */
/* 发行时可以把 DATA1.CAB 直接附在 SETUP.EXE 末尾，末尾 20 字节是尾巴： */
/*   "NBPAK1\0\0" + 载荷偏移 + 载荷长度 + CRC32                        */
/* 这样用户把单个 EXE 拷到机器上就能装，不需要光盘、不需要解压。        */
/* ------------------------------------------------------------------ */
#pragma pack(push, 1)
typedef struct {
    char          magic[8];
    unsigned int  off;
    unsigned int  size;
    unsigned int  crc;
} NbPakTail;
#pragma pack(pop)

static int load_embedded_arc(void)
{
    ck(35, "load_embedded_arc enter");
    char exe[MAX_PATH + 2];
    FILE *f;
    long fileLen, payloadLen;
    unsigned char *buf;
    NbPakTail t;
    DWORD crc;

    exe[0] = 0;
    if (!GetModuleFileNameA(NULL, exe, MAX_PATH) || !exe[0]) return 0;
    f = fopen(exe, "rb");
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    fileLen = ftell(f);
    if (fileLen <= (long)sizeof(NbPakTail)) { fclose(f); return 0; }
    if (fseek(f, fileLen - (long)sizeof(NbPakTail), SEEK_SET) != 0) { fclose(f); return 0; }
    if (fread(&t, 1, sizeof(t), f) != sizeof(t)) { fclose(f); return 0; }
    if (memcmp(t.magic, "NBPAK1", 6) != 0) { fclose(f); return 0; }
    if (t.size == 0 ||
        (long)t.off + (long)t.size > fileLen - (long)sizeof(NbPakTail)) {
        fclose(f);
        return 0;
    }
    payloadLen = (long)t.size;
    buf = (unsigned char *)sm_calloc((size_t)payloadLen);
    if (!buf) { fclose(f); return 0; }
    if (fseek(f, (long)t.off, SEEK_SET) != 0 ||
        fread(buf, 1, (size_t)payloadLen, f) != (size_t)payloadLen) {
        sm_free(buf);
        fclose(f);
        return 0;
    }
    fclose(f);
    crc = sm_crc32(buf, (size_t)payloadLen);
    if (crc != t.crc) { sm_free(buf); return 0; }
    if (!sm_arc_open_mem(buf, payloadLen, &S.memArc)) { sm_free(buf); return 0; }
    S.hasMemArc = 1;
    sm_logf("setup: embedded payload found (%ld bytes, %d files)",
            payloadLen, S.memArc.count);
    return 1;
}

/* ------------------------------------------------------------------ */
/* 许可证编辑框的子类化                                                */
/* 只读多行编辑框滚动时只重画新露出来的那一条，字体是后设的，行高缓存 */
/* 与实际绘制高度不一致时就会看到文字叠在一起。这里在滚动之后整块重画。*/
/* ------------------------------------------------------------------ */
static WNDPROC g_licOldProc = NULL;

static LRESULT CALLBACK LicEditProc(HWND hh, UINT m, WPARAM w, LPARAM l)
{
    LRESULT r;

    /* 子类化没装成功时绝不能调 CallWindowProcA(NULL)：9x 上会跳到野地址。
     * 另外顺手记一条日志，便于确认有没有进来。 */
    if (!g_licOldProc) {
        if (m != WM_PAINT && m != WM_NCPAINT && m != WM_ERASEBKGND) {
            sm_logf("lic: no old proc, msg=0x%04X", (unsigned)m);
        }
        return DefWindowProcA(hh, m, w, l);
    }
    r = CallWindowProcA(g_licOldProc, hh, m, w, l);
    switch (m) {
    case WM_VSCROLL:
    case WM_HSCROLL:
    case WM_MOUSEWHEEL:
    case WM_KEYDOWN:
    case WM_LBUTTONDOWN:
        InvalidateRect(hh, NULL, TRUE);
        UpdateWindow(hh);
        break;
    default:
        break;
    }
    return r;
}

/* 从内嵌包里取一个文件（返回静态缓冲区，够用即可） */
static const char *embedded_text(const char *name)
{
    static char buf[65536];
    int idx;
    long len;
    if (!S.hasMemArc) return NULL;
    idx = sm_arc_find(&S.memArc, name);
    if (idx < 0) return NULL;
    len = (long)S.memArc.items[idx].size;
    if (len >= (long)sizeof(buf)) len = (long)sizeof(buf) - 1;
    memcpy(buf, S.memArc.items[idx].data, (size_t)len);
    buf[len] = 0;
    return buf;
}

static char *shell_folder(int csidl)
{
    typedef HRESULT (WINAPI *PFN)(HWND, int, HANDLE, DWORD, LPSTR);
    static PFN pfn = NULL;
    static int tried = 0;
    char buf[MAX_PATH + 2];

    if (!tried) {
        /* shfolder.dll 是 Windows 98/2000/XP 上的正规位置；有的系统只有
         * shell32.dll 里带这个导出，两个都试一遍。 */
        const char *dlls[2];
        int i;
        dlls[0] = "shfolder.dll";
        dlls[1] = "shell32.dll";
        for (i = 0; i < 2 && !pfn; i++) {
            HMODULE h = LoadLibraryA(dlls[i]);
            if (h) pfn = (PFN)GetProcAddress(h, "SHGetFolderPathA");
        }
        tried = 1;
    }

    buf[0] = 0;
    if (pfn) pfn(NULL, csidl, NULL, 0, buf);
    if (buf[0] && GetFileAttributesA(buf) != (DWORD)-1) return sm_strdup(buf);

    /* 拿不到就走环境变量，避免返回空串把快捷方式建到当前目录 */
    buf[0] = 0;
    if (csidl == 0x0017 || csidl == 0x0016) {          /* 程序组 */
        if (!GetEnvironmentVariableA("ALLUSERSPROFILE", buf, MAX_PATH) || !buf[0])
            GetEnvironmentVariableA("APPDATA", buf, MAX_PATH);
    } else if (csidl == 0x0010 || csidl == 0x0011) {   /* 桌面 */
        if (!GetEnvironmentVariableA("USERPROFILE", buf, MAX_PATH) || !buf[0])
            GetEnvironmentVariableA("ALLUSERSPROFILE", buf, MAX_PATH);
    } else if (csidl == 0x001a || csidl == 0x001c) {   /* AppData */
        GetEnvironmentVariableA("APPDATA", buf, MAX_PATH);
    }
    return sm_strdup(buf);
}

static int disk_free_mb(const char *dir, double *freeMb, double *totalMb)
{
    ULARGE_INTEGER f, t;
    char root[MAX_PATH];
    strcpy(root, dir);
    if (strlen(root) < 3) strcpy(root, "C:\\");
    root[3] = 0;
    if (!GetDiskFreeSpaceExA(root, &f, &t, NULL)) return 0;
    if (freeMb) *freeMb = (double)f.QuadPart / (1024.0 * 1024.0);
    if (totalMb) *totalMb = (double)t.QuadPart / (1024.0 * 1024.0);
    return 1;
}

/* ------------------------------------------------------------------ */
static void set_title(const char *t)
{
    SetWindowTextA(GetDlgItem(S.h, IDC_TITLE), t);
}

/* 中英文各用一套字体：宋体没有拉丁字形，英文会掉到等宽字体上去 */
static void make_fonts(void)
{
    ck(30, "make_fonts enter");
    LOGFONTA lf;
    int en = (lang_get() == LANG_EN);
    const char *face = en ? "Tahoma" : "宋体";

    if (S.fUI) DeleteObject(S.fUI);
    if (S.fSmall) DeleteObject(S.fSmall);
    if (S.fLic) DeleteObject(S.fLic);
    if (S.fTitle) DeleteObject(S.fTitle);

    ZeroMemory(&lf, sizeof(lf));
    lf.lfHeight = -12;
    lf.lfCharSet = DEFAULT_CHARSET;
    strcpy(lf.lfFaceName, face);
    S.fUI = CreateFontIndirectA(&lf);
    lf.lfHeight = -11;
    S.fSmall = CreateFontIndirectA(&lf);
    lf.lfHeight = -13;
    S.fLic = CreateFontIndirectA(&lf);
    ZeroMemory(&lf, sizeof(lf));
    lf.lfHeight = -17;
    lf.lfWeight = FW_BOLD;
    lf.lfCharSet = DEFAULT_CHARSET;
    strcpy(lf.lfFaceName, face);
    S.fTitle = CreateFontIndirectA(&lf);
    S.fBold = S.fUI;
}

static void page_create(int page);

static void update_buttons(void)
{
    HWND back = GetDlgItem(S.h, IDC_BACK);
    HWND next = GetDlgItem(S.h, IDC_NEXT);
    EnableWindow(back, S.page > PG_LANGUAGE && S.page < PG_PROGRESS);
    if (S.page == PG_PROGRESS) EnableWindow(next, FALSE);
    else EnableWindow(next, TRUE);
    if (S.page == PG_FINISH) SetWindowTextA(next, TS("完成(&F)", "&Finish"));
    else SetWindowTextA(next, TS("下一步(&N) >", "&Next >"));
    if (S.page >= PG_PROGRESS) EnableWindow(GetDlgItem(S.h, IDC_CANCEL), S.page == PG_PROGRESS);
    else EnableWindow(GetDlgItem(S.h, IDC_CANCEL), TRUE);
}

/* 换语言后：重刷框架上的文字和当前页 */
static void refresh_lang(void)
{
    ck(31, "refresh_lang enter");
    HWND c;
    make_fonts();
    SetWindowTextA(S.h, TS("" SM_SHORT_NAME " 安装向导", "" SM_SHORT_NAME " Setup"));
    c = GetDlgItem(S.h, IDC_TITLE);
    if (c) SendMessageA(c, WM_SETFONT, (WPARAM)S.fTitle, MAKELPARAM(TRUE, 0));
    c = GetDlgItem(S.h, IDC_BACK);
    if (c) {
        SetWindowTextA(c, TS("上一步(&B) <", "< &Back"));
        SendMessageA(c, WM_SETFONT, (WPARAM)S.fUI, MAKELPARAM(TRUE, 0));
    }
    c = GetDlgItem(S.h, IDC_NEXT);
    if (c) {
        SetWindowTextA(c, TS("下一步(&N) >", "&Next >"));
        SendMessageA(c, WM_SETFONT, (WPARAM)S.fUI, MAKELPARAM(TRUE, 0));
    }
    c = GetDlgItem(S.h, IDC_CANCEL);
    if (c) {
        SetWindowTextA(c, TS("取消", "Cancel"));
        SendMessageA(c, WM_SETFONT, (WPARAM)S.fUI, MAKELPARAM(TRUE, 0));
    }
    page_create(S.page);
    update_buttons();
    InvalidateRect(S.h, NULL, TRUE);
}

/* 万一安装盘上找不到许可证文件时的兜底文本 */
static const char *license_fallback(void)
{
    if (lang_get() == LANG_EN) {
        return
            "" SM_PRODUCT_NAME " - End User License Agreement\r\n\r\n"
            "1. You may install and use this product on a single computer, "
            "and make one copy for backup purposes only.\r\n\r\n"
            "2. You may not reverse engineer, decompile, modify, rent, sell or "
            "redistribute the product, or remove any copyright notice.\r\n\r\n"
            "3. The product provides no AI service of its own; it only connects "
            "to a cloud endpoint that you configure. Accounts, keys and fees of "
            "such endpoints are your own responsibility.\r\n\r\n"
            "4. The product is provided AS IS, without warranty of any kind. The "
            "assistant's answers and tool calls may be wrong; you are responsible "
            "for reviewing them.\r\n\r\n"
            "5. " SM_COMPANY " is not liable for any direct or indirect damages "
            "arising from the use of this product.\r\n\r\n"
            "6. This agreement is governed by the laws of the People's Republic "
            "of China.\r\n\r\n"
            "                                    " SM_COMPANY "\r\n"
            "                                    1996 - 2003\r\n";
    }
    return
        "" SM_PRODUCT_NAME " 最终用户许可协议\r\n\r\n"
        "1. 您可以在单台计算机上安装并使用本产品的一份副本，"
        "并出于备份目的复制一份。\r\n\r\n"
        "2. 您不得对本产品进行反向工程、反编译，不得修改、出租、出借、"
        "出售或转让本产品的副本。\r\n\r\n"
        "3. 本产品自身不提供任何智能服务，只连接您自行配置的云端接口；"
        "该接口的账号、密钥及费用由您自行承担。\r\n\r\n"
        "4. 本产品按“现状”提供，不附带任何明示或默示的担保。"
        "助手的回答及工具调用可能存在错误，请您自行复核。\r\n\r\n"
        "5. 因使用本产品造成的任何直接或间接损失，" SM_COMPANY " 不承担责任。\r\n\r\n"
        "6. 本协议受中华人民共和国法律管辖。\r\n\r\n"
        "                                    " SM_COMPANY "\r\n"
        "                                    1996 - 2003\r\n";
}

/* ------------------------------------------------------------------ */
static void page_create(int page)
{
    ck(32, "page_create enter");
    int y = 54;
    page_destroy();
    S.page = page;

    switch (page) {
    case PG_LANGUAGE:
        set_title(TS("选择安装语言", "Choose Setup Language"));
        mk("STATIC",
           "请选择安装程序使用的语言。\r\n"
           "Please choose the language used by this Setup program.",
           SS_LEFT, CONTENT_X, y, CONTENT_W, 36, 0, S.fUI);
        S.hLangZh = mk("BUTTON", "中文（简体）",
                       BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP,
                       CONTENT_X + 12, y + 52, CONTENT_W - 24, 20,
                       IDC_LANG_ZH, S.fUI);
        S.hLangEn = mk("BUTTON", "English",
                       BS_AUTORADIOBUTTON | WS_TABSTOP,
                       CONTENT_X + 12, y + 78, CONTENT_W - 24, 20,
                       IDC_LANG_EN, S.fUI);
        if (lang_get() == LANG_EN)
            SendMessageA(S.hLangEn, BM_SETCHECK, BST_CHECKED, 0);
        else
            SendMessageA(S.hLangZh, BM_SETCHECK, BST_CHECKED, 0);
        mk("STATIC", "提示：这里只改变安装程序的界面语言。",
           SS_LEFT, CONTENT_X, 190, CONTENT_W, 18, 0, S.fSmall);
        mk("STATIC", "This only changes the language of Setup itself.",
           SS_LEFT, CONTENT_X, 210, CONTENT_W, 18, 0, S.fSmall);
        break;

    case PG_WELCOME:
        set_title(TS("欢迎使用 " SM_SHORT_NAME " 安装程序",
                      "Welcome to " SM_SHORT_NAME " Setup"));
        mk("STATIC",
           TS("欢迎使用 " SM_PRODUCT_NAME " 安装向导。\r\n\r\n本向导将指导您完成软件的安装。安装完成后，您可以从「开始」菜单或桌面启动本程序。\r\n\r\n安装前请关闭其他正在运行的程序。\r\n\r\n本程序适用于 Windows 95 / 98 / NT 4.0 / 2000 / XP，需要至少 16 MB 硬盘空间。",
           "Welcome to the " SM_PRODUCT_NAME " Setup Wizard.\r\n\r\nThis wizard will guide you through the installation. When it finishes you can start the program from the Start menu or from the desktop.\r\n\r\nPlease close any other running programs before continuing.\r\n\r\nThis program runs on Windows 95 / 98 / NT 4.0 / 2000 / XP and needs at least 16 MB of free disk space."),
           SS_LEFT, CONTENT_X, y, CONTENT_W, 170, 0, S.fUI);
        mk("STATIC",
           TS("警告：本计算机程序受著作权法和国际公约的保护。未经授权擅自复制或传播本程序或其中任何部分，将承担相应的法律责任。",
           "Warning: this computer program is protected by copyright law and international treaties. Unauthorised reproduction or distribution may lead to legal action."),
           SS_LEFT, CONTENT_X, 240, CONTENT_W, 60, 0, S.fSmall);
        break;

    case PG_LICENSE:
        set_title(TS("许可证协议",
           "License Agreement"));
        mk("STATIC", TS("请仔细阅读下列许可证协议。您必须接受协议的全部条款才能继续安装。",
           "Please read the following license agreement. You must accept all of its terms to continue."),
           SS_LEFT, CONTENT_X, y, CONTENT_W, 32, 0, S.fUI);
        {
            char *txt = NULL;
            char *dir = sm_exe_dir();
            const char *licName = (lang_get() == LANG_EN) ? "LICENSE.EN.TXT"
                                                          : "LICENSE.TXT";
            char *lic = sm_path_join(dir, licName);
            long len = 0;
            const char *emb = embedded_text(licName);
            if (emb && emb[0]) txt = sm_strdup(emb);
            else txt = sm_read_file(lic, 65536, &len);
            if (!txt) txt = sm_strdup(license_fallback());
            S.hLic = mk("EDIT", "",
                        ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL |
                        WS_BORDER | ES_LEFT,
                        CONTENT_X, y + 34, CONTENT_W, 158, IDC_LIC_TEXT, S.fLic);
            if (S.hLic) {
                SendMessageA(S.hLic, WM_SETFONT, (WPARAM)S.fLic, MAKELPARAM(TRUE, 0));
                SetWindowTextA(S.hLic, txt);
                SendMessageA(S.hLic, EM_SETSEL, 0, 0);
                SendMessageA(S.hLic, EM_SCROLLCARET, 0, 0);
                if (!g_licOldProc)
                    g_licOldProc = (WNDPROC)SetWindowLongA(
                        S.hLic, GWL_WNDPROC, (LONG)(INT_PTR)LicEditProc);
                InvalidateRect(S.hLic, NULL, TRUE);
            }
            sm_free(txt);
            sm_free(lic);
            sm_free(dir);
        }
        S.hAccept = mk("BUTTON", TS("我接受上述许可证协议的全部条款(&A)",
           "I &accept the terms of this license agreement"),
                       BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP,
                       CONTENT_X, 252, CONTENT_W, 18, IDC_LIC_ACCEPT, S.fUI);
        S.hDecline = mk("BUTTON", TS("我不接受上述许可证协议(&D)",
           "I do &not accept the agreement"),
                        BS_AUTORADIOBUTTON | WS_TABSTOP,
                        CONTENT_X, 274, CONTENT_W, 18, IDC_LIC_DECLINE, S.fUI);
        if (S.licenceOk) SendMessageA(S.hAccept, BM_SETCHECK, BST_CHECKED, 0);
        else SendMessageA(S.hDecline, BM_SETCHECK, BST_CHECKED, 0);
        break;

    case PG_USER:
        set_title(TS("用户信息",
           "User Information"));
        mk("STATIC", TS("请填写您的姓名和单位。安装程序会把它们记录在软件的“关于”对话框中。",
           "Enter your name and company. Setup stores them in the program's About dialog."),
           SS_LEFT, CONTENT_X, y, CONTENT_W, 34, 0, S.fUI);
        mk("STATIC", TS("姓名(&U)：",
           "&User name:"), SS_LEFT, CONTENT_X, 100, 96, 18, 0, S.fUI);
        S.hName = mk("EDIT", S.userName, ES_AUTOHSCROLL | WS_TABSTOP,
                     CONTENT_X + 100, 98, CONTENT_W - 112, 21, IDC_USER_NAME, S.fUI);
        mk("STATIC", TS("单位(&O)：",
           "&Company:"), SS_LEFT, CONTENT_X, 130, 96, 18, 0, S.fUI);
        S.hCompany = mk("EDIT", S.userCompany, ES_AUTOHSCROLL | WS_TABSTOP,
                        CONTENT_X + 100, 128, CONTENT_W - 112, 21, IDC_USER_COMP, S.fUI);
        mk("STATIC", TS("序列号(&K)：",
           "Product &key:"), SS_LEFT, CONTENT_X, 160, 96, 18, 0, S.fUI);
        S.hCdk = mk("EDIT", S.cdk, ES_AUTOHSCROLL | ES_UPPERCASE | WS_TABSTOP,
                    CONTENT_X + 100, 158, CONTENT_W - 112, 21, IDC_USER_CDK, S.fUI);
        SendMessageA(S.hCdk, EM_SETLIMITTEXT, 40, 0);
        mk("STATIC", TS("序列号由光盘包装或经销商提供，格式为 5 组、每组 4 位字符。\r\n此处可以留空，安装完成后在「工具 - 激活产品」中再输入。",
           "The key is on the disc sleeve or supplied by your dealer: 5 groups of 4 characters.\r\nYou may leave it blank and enter it later under Tools - Activate Product."),
           SS_LEFT, CONTENT_X + 100, 186, CONTENT_W - 112, 40, 0, S.fSmall);
        break;

    case PG_DIR: {
        char req[128];
        double fm = 0, tm = 0;
        set_title(TS("选择目标位置",
           "Choose Destination Location"));
        mk("STATIC", TS("安装程序将把 " SM_SHORT_NAME " 安装到下列文件夹。\r\n若要安装到其他文件夹，请按「浏览」。",
           "Setup will install " SM_SHORT_NAME " into the following folder.\r\nClick Browse to choose a different folder."),
           SS_LEFT, CONTENT_X, y, CONTENT_W, 34, 0, S.fUI);
        mk("STATIC", TS("目标文件夹(&D)：",
           "Destination &folder:"), SS_LEFT, CONTENT_X, 100, 76, 18, 0, S.fUI);
        S.hDir = mk("EDIT", S.dir, ES_AUTOHSCROLL | WS_TABSTOP,
                    CONTENT_X, 118, CONTENT_W - 4, 21, IDC_DIR_EDIT, S.fUI);
        mk("BUTTON", TS("浏览(&B)...",
           "&Browse..."), BS_PUSHBUTTON | WS_TABSTOP,
           CONTENT_X + CONTENT_W - 96, 146, 96, 24, IDC_DIR_BROWSE, S.fUI);
        if (disk_free_mb(S.dir, &fm, &tm))
            _snprintf(req, sizeof(req), TS("所需空间：16 MB        可用空间：%.0f MB",
           "Space required: 16 MB        Space available: %.0f MB"), fm);
        else
            strcpy(req, TS("所需空间：16 MB",
           "Space required: 16 MB"));
        mk("STATIC", req, SS_LEFT, CONTENT_X, 184, CONTENT_W, 18, 0, S.fSmall);
        mk("STATIC", TS("目标文件夹必须是一个本地硬盘上的完整路径。",
           "The destination folder must be a full path on a local hard disk."),
           SS_LEFT, CONTENT_X, 210, CONTENT_W, 18, 0, S.fSmall);
        break;
    }

    case PG_FOLDER: {
        char *sp = shell_folder(0x0017);   /* CSIDL_COMMON_PROGRAMS */
        char *pf = sm_path_join(sp, "*.*");
        WIN32_FIND_DATAA fd;
        HANDLE hf;
        set_title(TS("选择程序文件夹",
           "Select Program Folder"));
        mk("STATIC", TS("安装程序将把程序图标添加到下列程序文件夹中。您可以输入新的文件夹名称，或从下面的列表中选择一个已有的文件夹。",
           "Setup will add program icons to the folder below. You may type a newfolder name, or select one from the existing folders list."),
           SS_LEFT, CONTENT_X, y, CONTENT_W, 34, 0, S.fUI);
        mk("STATIC", TS("程序文件夹(&P)：",
           "Program &folder:"), SS_LEFT, CONTENT_X, 100, 86, 18, 0, S.fUI);
        S.hFolder = mk("EDIT", S.folder, ES_AUTOHSCROLL | WS_TABSTOP,
                       CONTENT_X, 118, CONTENT_W - 4, 21, IDC_FOLDER_EDIT, S.fUI);
        mk("STATIC", TS("现有的文件夹(&X)：",
           "&Existing folders:"), SS_LEFT, CONTENT_X, 146, 100, 18, 0, S.fUI);
        {
            HWND lb = mk("LISTBOX", "", WS_BORDER | WS_VSCROLL | LBS_NOTIFY | WS_TABSTOP,
                         CONTENT_X, 164, CONTENT_W - 4, 96, IDC_FOLDER_LIST, S.fUI);
            hf = FindFirstFileA(pf, &fd);
            if (hf != INVALID_HANDLE_VALUE) {
                do {
                    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
                    if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
                    SendMessageA(lb, LB_ADDSTRING, 0, (LPARAM)fd.cFileName);
                } while (FindNextFileA(hf, &fd));
                FindClose(hf);
            }
            SendMessageA(lb, LB_SETCURSEL, (WPARAM)-1, 0);
            SendMessageA(lb, LB_SETITEMDATA, 0, 0);
        }
        sm_free(pf);
        sm_free(sp);
        break;
    }

    case PG_PROGRESS:
        set_title(TS("正在安装",
           "Installing"));
        mk("STATIC", TS("安装程序正在把文件复制到您的计算机。",
           "Setup is copying files to your computer."),
           SS_LEFT, CONTENT_X, y, CONTENT_W, 20, 0, S.fUI);
        S.hProgText = mk("STATIC", TS("正在准备...",
           "Preparing..."), SS_LEFT,
                         CONTENT_X, 110, CONTENT_W, 18, IDC_PROG_TEXT, S.fUI);
        S.hProg = mk(PROGRESS_CLASSA, NULL, WS_CHILD | WS_VISIBLE,
                     CONTENT_X, 134, CONTENT_W - 4, 18, IDC_PROG_BAR, NULL);
        SendMessageA(S.hProg, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        mk("STATIC", TS("请稍候。此过程可能需要几分钟。",
           "Please wait. This may take a few minutes."),
           SS_LEFT, CONTENT_X, 162, CONTENT_W, 18, 0, S.fSmall);
        break;

    case PG_FINISH:
        set_title(TS("安装完成",
           "Installation Complete"));
        mk("STATIC",
           TS("" SM_PRODUCT_NAME " 已经安装到您的计算机上。\r\n\r\n首次使用前，请通过「工具 - 设置」填写云端服务器地址与 API 密钥，并用「工具 - 激活产品」输入产品序列号完成激活。\r\n\r\n单击「完成」结束安装。",
           "" SM_PRODUCT_NAME " has been installed on your computer.\r\n\r\nBefore first use, open Tools - Options and fill in the cloud server URL and API key,then use Tools - Activate Product to enter your product key.\r\n\r\nClick Finish to end Setup."),
           SS_LEFT, CONTENT_X, y, CONTENT_W, 130, 0, S.fUI);
        S.hAutoRun = mk("BUTTON", TS("立即启动 " SM_PRODUCT_NAME "(&S)",
           "&Launch " SM_PRODUCT_NAME " now"),
                        BS_AUTOCHECKBOX | WS_TABSTOP, CONTENT_X, 196, CONTENT_W, 18, IDC_AUTORUN_CHK, S.fUI);
        S.hDesktop = mk("BUTTON", TS("在桌面上创建快捷方式(&D)",
           "Create a shortcut on the &desktop"),
                        BS_AUTOCHECKBOX | WS_TABSTOP, CONTENT_X, 218, CONTENT_W, 18, IDC_DESKTOP_CHK, S.fUI);
        SendMessageA(S.hAutoRun, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageA(S.hDesktop, BM_SETCHECK, BST_CHECKED, 0);
        break;
    }
    update_buttons();
}

/* ------------------------------------------------------------------ */
/* 快捷方式                                                            */
/* ------------------------------------------------------------------ */
static int make_shortcut(const char *lnk, const char *target, const char *args,
                         const char *workdir, const char *desc, const char *iconFile)
{
    IShellLinkA *psl = NULL;
    IPersistFile *ppf = NULL;
    WCHAR wlnk[MAX_PATH];
    int ok = 0;
    if (FAILED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                &IID_IShellLinkA, (void **)&psl))) {
        sm_logf("shortcut: CoCreateInstance failed for [%s] err=%lu", lnk,
                (unsigned long)GetLastError());
        return 0;
    }
    MultiByteToWideChar(CP_ACP, 0, lnk, -1, wlnk, MAX_PATH);
    psl->lpVtbl->SetPath(psl, target);
    psl->lpVtbl->SetArguments(psl, args ? args : "");
    psl->lpVtbl->SetWorkingDirectory(psl, workdir ? workdir : "");
    psl->lpVtbl->SetDescription(psl, desc ? desc : "");
    psl->lpVtbl->SetIconLocation(psl, iconFile ? iconFile : target, 0);
    if (SUCCEEDED(psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, (void **)&ppf))) {
        if (SUCCEEDED(ppf->lpVtbl->Save(ppf, wlnk, TRUE))) ok = 1;
        ppf->lpVtbl->Release(ppf);
    }
    psl->lpVtbl->Release(psl);
    if (!ok) sm_logf("shortcut: Save failed for [%s] err=%lu", lnk,
                     (unsigned long)GetLastError());
    return ok;
}

/* ------------------------------------------------------------------ */
/* 卸载记录                                                            */
/* ------------------------------------------------------------------ */
extern const char *sm_uninst_root(void);
static void write_uninstall_list(const char *dir, SmArc *arc, const char *startMenu,
                                 const char *desktopLnk)
{
    Sb b;
    char *p;
    int i;
    char rec[MAX_PATH + 40];

    sb_init(&b);
    sb_puts(&b, "; " SM_SHORT_NAME " 卸载记录 - 请勿手工修改\r\n");
    for (i = 0; i < arc->count; i++) {
        if (arc->items[i].attrib & FILE_ATTRIBUTE_DIRECTORY) {
            sb_printf(&b, "D|%s\\%s\r\n", dir, arc->items[i].name);
        } else {
            sb_printf(&b, "F|%s\\%s\r\n", dir, arc->items[i].name);
        }
    }
    if (startMenu) {
        sb_printf(&b, "L|%s\\" SM_SHORT_NAME ".lnk\r\n", startMenu);
        sb_printf(&b, "L|%s\\快速入门.lnk\r\n", startMenu);
        sb_printf(&b, "L|%s\\卸载 " SM_SHORT_NAME ".lnk\r\n", startMenu);
        sb_printf(&b, "L|%s\\自述文件.lnk\r\n", startMenu);
        sb_printf(&b, "D|%s\r\n", startMenu);
    }
    if (desktopLnk) sb_printf(&b, "L|%s\r\n", desktopLnk);
    if (sm_uninst_root()[0])
        sb_printf(&b, "R|%s\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" SM_SHORT_NAME "\r\n", sm_uninst_root());
    sb_printf(&b, "R|HKCU\\%s\r\n", SM_REG_ROOT);
    p = sm_path_join(dir, "UNINST.LST");
    sm_write_file(p, sb_str(&b), (long)b.len);
    sm_free(p);
    sb_free(&b);
    (void)rec;
}

/* 卸载登记：优先 HKLM（XP 的“添加/删除程序”只认这里），
 * 没有管理员权限时退回 HKCU，保证至少能在“程序和功能”里看到。 */
static char g_uninstRoot[8] = "HKLM";
const char *sm_uninst_root(void) { return g_uninstRoot; }


/* 是否具有管理员权限：没有就不要碰 HKLM，否则会弹出 UAC 提示把人卡住 */
static int is_elevated(void)
{
    HANDLE tok = NULL;
    TOKEN_ELEVATION el;
    DWORD cb = sizeof(el);
    int ok = 0;

    /* Windows XP/2000/9x 没有 UAC，GetTokenInformation 也不认
     * TokenElevation（类 20），直接按“有权限写 HKLM”处理。 */
    {
        OSVERSIONINFOA vi;
        memset(&vi, 0, sizeof(vi));
        vi.dwOSVersionInfoSize = sizeof(vi);
        if (GetVersionExA(&vi) && vi.dwMajorVersion < 6) return 1;
    }

    {
        /* OpenProcessToken / GetTokenInformation 只在 NT 系上存在。
         * Windows 98/ME 连导入都不该有，否则加载阶段就失败，所以动态取。 */
        typedef BOOL (WINAPI *PFN_OPT)(HANDLE, DWORD, HANDLE *);
        typedef BOOL (WINAPI *PFN_GTI)(HANDLE, int, void *, DWORD, DWORD *);
        static PFN_OPT popt = NULL;
        static PFN_GTI pgti = NULL;
        static int tried = 0;
        if (!tried) {
            HMODULE a = GetModuleHandleA("advapi32.dll");
            if (!a) a = LoadLibraryA("advapi32.dll");
            if (a) {
                popt = (PFN_OPT)GetProcAddress(a, "OpenProcessToken");
                pgti = (PFN_GTI)GetProcAddress(a, "GetTokenInformation");
            }
            tried = 1;
        }
        if (popt && pgti && popt(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
            if (pgti(tok, 20, &el, sizeof(el), &cb))
                ok = el.TokenIsElevated ? 1 : 0;
            CloseHandle(tok);
        }
    }
    return ok;
    return ok;
}


/* 程序组目录：CSIDL_COMMON_PROGRAMS 在某些系统上取不到，
 * 依次尝试 All Users / 当前用户 的「开始」菜单。 */
static char *startmenu_dir(void)
{
    char buf[MAX_PATH + 2];
    char *p;
    buf[0] = 0;
    p = shell_folder(0x0017);
    if (p && p[0] && GetFileAttributesA(p) != 0xFFFFFFFF) return p;
    sm_free(p);
    if (GetEnvironmentVariableA("ALLUSERSPROFILE", buf, MAX_PATH) && buf[0]) {
        char *r = sm_path_join(buf, "Start Menu\\Programs");
        if (sm_mkdirs(r)) return r;
        sm_free(r);
    }
    if (GetEnvironmentVariableA("APPDATA", buf, MAX_PATH) && buf[0]) {
        char *r = sm_path_join(buf, "Microsoft\\Windows\\Start Menu\\Programs");
        if (sm_mkdirs(r)) return r;
        sm_free(r);
    }
    return sm_strdup(".");
}


/* 当前用户的「开始」菜单程序组（所有用户程序组不可写时使用） */
static char *user_startmenu_dir(void)
{
    char buf[MAX_PATH + 2];
    char *r;
    buf[0] = 0;
    if (GetEnvironmentVariableA("APPDATA", buf, MAX_PATH) && buf[0]) {
        r = sm_path_join(buf, "Microsoft\\Windows\\Start Menu\\Programs");
        if (sm_mkdirs(r)) return r;
        sm_free(r);
    }
    return sm_strdup(".");
}

static void write_uninstall_registry(const char *dir)
{
    char key[256];
    char buf[MAX_PATH + 40];
    HKEY root = HKEY_LOCAL_MACHINE;
    HKEY k;

    strcpy(key, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" SM_SHORT_NAME "");
    if (is_elevated() &&
        RegCreateKeyExA(HKEY_LOCAL_MACHINE, key, 0, NULL, 0, KEY_SET_VALUE,
                        NULL, &k, NULL) == ERROR_SUCCESS) {
        RegCloseKey(k);
        root = HKEY_LOCAL_MACHINE;
        strcpy(g_uninstRoot, "HKLM");
    } else if (RegCreateKeyExA(HKEY_CURRENT_USER, key, 0, NULL, 0, KEY_SET_VALUE,
                               NULL, &k, NULL) == ERROR_SUCCESS) {
        RegCloseKey(k);
        root = HKEY_CURRENT_USER;
        strcpy(g_uninstRoot, "HKCU");
        sm_logf("无法写入 HKLM，卸载信息改记到 HKCU");
    } else {
        strcpy(g_uninstRoot, "");
        return;
    }

    sm_reg_set_sz(root, key, "DisplayName", "" SM_PRODUCT_NAME "");
    _snprintf(buf, sizeof(buf), "%s\\UNINST.EXE", dir);
    buf[sizeof(buf) - 1] = 0;
    sm_reg_set_sz(root, key, "UninstallString", buf);
    sm_reg_set_sz(root, key, "DisplayVersion", S.appVersion);
    sm_reg_set_sz(root, key, "Publisher", SM_COMPANY);
    sm_reg_set_sz(root, key, "InstallLocation", dir);
    _snprintf(buf, sizeof(buf), "%s\\" SM_EXE_NAME, dir);
    buf[sizeof(buf) - 1] = 0;
    sm_reg_set_sz(root, key, "DisplayIcon", buf);
    sm_reg_set_sz(root, key, "InstallSource", S.setupDir);
    sm_reg_set_dw(root, key, "NoModify", 1);
    sm_reg_set_dw(root, key, "NoRepair", 1);
    sm_reg_set_dw(root, key, "Language", 0x0804);
}

/* ------------------------------------------------------------------ */
/* 真正的安装过程                                                      */
/* ------------------------------------------------------------------ */
static int extract_progress(void *ctx, int idx, int total, const char *name)
{
    char buf[MAX_PATH + 80];
    (void)ctx;
    _snprintf(buf, sizeof(buf), TS("正在复制：%s",
           "Copying: %s"), name);
    buf[sizeof(buf) - 1] = 0;
    if (S.hProgText) SetWindowTextA(S.hProgText, buf);
    if (S.hProg) SendMessageA(S.hProg, PBM_SETPOS, (WPARAM)(total ? (idx * 80 / total) : 0), 0);
    pump();
    Sleep(18);
    return 0;
}

static int do_install(void)
{
    SmArc arc;
    char *dataFile;
    char *dir;
    char *startMenu;
    char *exePath;
    char *desktopDir, *desktopLnk;
    char *ini;
    int ok;

    if (S.hProgText) SetWindowTextA(S.hProgText, TS("正在检查安装数据...",
           "Checking installation data..."));
    pump();

    dataFile = sm_path_join(S.setupDir, "DATA1.CAB");
    sm_logf("install: setupDir=[%s] dataFile=[%s] embedded=%d",
            S.setupDir, dataFile, S.hasMemArc);
    if (S.hasMemArc) {
        arc = S.memArc;                 /* 结构体拷贝：只读使用，不释放 */
    } else if (!sm_arc_open(dataFile, &arc)) {
        setup_msg(arc.error, MB_ICONSTOP | MB_OK);
        sm_free(dataFile);
        return 0;
    }

    /* 1. 目标目录 */
    if (S.hDir) GetWindowTextA(S.hDir, S.dir, sizeof(S.dir));
    sm_trim(S.dir);
    if (!S.dir[0]) strcpy(S.dir, S.defaultDir);
    sm_logf("install: target=[%s]", S.dir);
    if (!sm_mkdirs(S.dir)) {
        sm_logf("install: sm_mkdirs failed err=%lu", (unsigned long)GetLastError());
        setup_msg(TS("无法创建目标文件夹。请检查路径是否正确，以及您是否有写入该文件夹的权限。",
           "Cannot create the destination folder. Check the path and your write permission."), MB_ICONSTOP | MB_OK);
        if (!S.hasMemArc) sm_arc_close(&arc);
        sm_free(dataFile);
        return 0;
    }

    /* 2. 解出文件 */
    ok = sm_arc_extract(&arc, S.dir, extract_progress, NULL);
    sm_logf("install: extract=%d items=%d err=%lu", ok, arc.count,
            (unsigned long)GetLastError());
    if (!ok) {
        setup_msg(TS("复制文件时出错，安装未能完成。请检查磁盘空间是否已满，然后重新运行安装程序。",
           "An error occurred while copying files. Check free disk space and run Setup again."), MB_ICONSTOP | MB_OK);
        if (!S.hasMemArc) sm_arc_close(&arc);
        sm_free(dataFile);
        return 0;
    }

    if (S.hProg) SendMessageA(S.hProg, PBM_SETPOS, 85, 0);
    if (S.hProgText) SetWindowTextA(S.hProgText, TS("正在创建程序图标...",
           "Creating program icons..."));
    pump();

    /* 3. 配置文件 */
    dir = S.dir;
    ini = sm_path_join(dir, SM_INI_NAME);
    if (!sm_file_exists(ini)) {
        SmConfig c;
        char *save_dir;
        sm_config_defaults(&c);
        /* 直接以目标目录写 INI */
        sm_ini_set(ini, SM_SHORT_NAME, "Product", SM_SHORT_NAME);
        sm_ini_set(ini, SM_SHORT_NAME, "Version", S.appVersion);
        sm_ini_set(ini, "connection", "endpoint", c.endpoint);
        sm_ini_set(ini, "connection", "model", c.model);
        sm_ini_set(ini, "connection", "temperature", "7");
        sm_ini_set(ini, "connection", "maxtokens", "2048");
        sm_ini_set(ini, "connection", "timeout", "60");
        sm_ini_set(ini, "options", "usetools", "1");
        sm_ini_set(ini, "options", "confirmwrite", "1");
        sm_ini_set(ini, "options", "ignorecertdate", "0");
        sm_ini_set(ini, "display", "fontsize", "12");
    sm_ini_set(ini, "display", "language", (lang_get() == LANG_EN) ? "en" : "zh");
        sm_ini_set(ini, "install", "user", S.userName);
        sm_ini_set(ini, "install", "company", S.userCompany);
        (void)save_dir;
    }
    sm_free(ini);

    /* 4. 快捷方式：先试“所有用户”程序组，不可写就退回当前用户的程序组 */
    startMenu = NULL;
    {
        char *progs = startmenu_dir();
        if (S.hFolder) GetWindowTextA(S.hFolder, S.folder, sizeof(S.folder));
        sm_trim(S.folder);
        if (!S.folder[0]) strcpy(S.folder, "" SM_SHORT_NAME "");
        startMenu = sm_path_join(progs, S.folder);
        if (!sm_mkdirs(startMenu)) {
            char *up = user_startmenu_dir();
            sm_free(startMenu);
            startMenu = sm_path_join(up, S.folder);
            sm_mkdirs(startMenu);
            sm_free(up);
        }
        sm_free(progs);
    }
    exePath = sm_path_join(dir, SM_EXE_NAME);
    {
        char *lnk = sm_path_join(startMenu, "" SM_SHORT_NAME ".lnk");
        make_shortcut(lnk, exePath, "", dir, "" SM_PRODUCT_NAME "", exePath);
        sm_free(lnk);
        lnk = sm_path_join(startMenu, TS("快速入门.lnk",
           "Quick Start.lnk"));
        {
            char *chm = sm_path_join(dir, SM_EXE_BASE ".CHM");
            make_shortcut(lnk, chm, "", dir, TS("打开帮助文件",
           "Open the help file"), chm);
            sm_free(chm);
        }
        sm_free(lnk);
        lnk = sm_path_join(startMenu, TS("自述文件.lnk",
           "Read Me.lnk"));
        {
            char *rd = sm_path_join(dir, "README.TXT");
            make_shortcut(lnk, "notepad.exe", rd, dir, TS("自述文件",
           "Read Me"), rd);
            sm_free(rd);
        }
        sm_free(lnk);
        lnk = sm_path_join(startMenu, TS("卸载 " SM_SHORT_NAME ".lnk",
           "Uninstall " SM_SHORT_NAME ".lnk"));
        {
            char *un = sm_path_join(dir, "UNINST.EXE");
            make_shortcut(lnk, un, "", dir, TS("卸载 " SM_SHORT_NAME "",
           "Uninstall " SM_SHORT_NAME ""), un);
            sm_free(un);
        }
        sm_free(lnk);
    }

    desktopLnk = NULL;
    if (!S.hDesktop || SendMessageA(S.hDesktop, BM_GETCHECK, 0, 0) == BST_CHECKED) {
        char ubuf[MAX_PATH + 2];
        desktopDir = shell_folder(0x0010);   /* CSIDL_DESKTOPDIRECTORY */
        desktopLnk = sm_path_join(desktopDir, "" SM_SHORT_NAME ".lnk");
        make_shortcut(desktopLnk, exePath, "", dir, "" SM_PRODUCT_NAME "", exePath);
        if (!sm_file_exists(desktopLnk)) {
            /* 公共桌面不可写时改用当前用户的桌面 */
            sm_free(desktopLnk);
            ubuf[0] = 0;
            if (GetEnvironmentVariableA("USERPROFILE", ubuf, MAX_PATH) && ubuf[0]) {
                char *ud = sm_path_join(ubuf, "Desktop");
                desktopLnk = sm_path_join(ud, "" SM_SHORT_NAME ".lnk");
                make_shortcut(desktopLnk, exePath, "", dir, "" SM_PRODUCT_NAME "", exePath);
                sm_free(ud);
            } else desktopLnk = NULL;
        }
        sm_free(desktopDir);
    }

    /* 5. 卸载信息 */
    if (S.hProg) SendMessageA(S.hProg, PBM_SETPOS, 92, 0);
    if (S.hProgText) SetWindowTextA(S.hProgText, TS("正在登记卸载信息...",
           "Registering uninstall information..."));
    pump();
    /* 注意顺序：必须先写注册表，再写清单。
     * write_uninstall_list() 里的 "R|" 行要用 sm_uninst_root() 决定根键，
     * 而这个值是在 write_uninstall_registry() 里探测出来的（HKLM 还是 HKCU）。
     * 顺序反过来就会把默认的 HKLM 写进清单，非管理员安装时卸载程序会去删
     * 一个根本不存在的 HKLM 键，"添加/删除程序"里的条目就永远留着。 */
    write_uninstall_registry(dir);
    write_uninstall_list(dir, &arc, startMenu, desktopLnk);
    sm_logf("install: startMenu=[%s] desktop=[%s]",
            startMenu ? startMenu : "(none)",
            desktopLnk ? desktopLnk : "(none)");

    /* 6. 序列号（如果用户填了，或命令行用 /k= 指定了批量授权码） */
    {
        char key[64], err[300];
        SmLicense lic;
        key[0] = 0;
        if (S.hCdk) {
            GetWindowTextA(S.hCdk, key, sizeof(key));
            sm_trim(key);
        }
        if (!key[0] && S.cdk[0]) {
            strncpy(key, S.cdk, sizeof(key) - 1);
            key[sizeof(key) - 1] = 0;
        }
        if (key[0]) {
            if (sm_license_activate(key, &lic, err, sizeof(err))) {
                /* 成功 */
            } else {
                char m[500];
                _snprintf(m, sizeof(m), TS("%s\r\n\r\n安装已经完成，您可以稍后在「工具 - 激活产品」中重新输入序列号。",
           "%s\r\n\r\nSetup is complete. You can enter the key later under Tools - Activate Product."), err);
                m[sizeof(m) - 1] = 0;
                if (S.h) MessageBoxA(S.h, m, TS("产品序列号",
           "Product key"), MB_ICONWARNING | MB_OK);
                else sm_logf("SETUP(静默): 序列号激活失败: %s", err);
            }
        }
    }

    if (S.hProg) SendMessageA(S.hProg, PBM_SETPOS, 100, 0);
    if (S.hProgText) SetWindowTextA(S.hProgText, TS("安装完成。",
           "Installation finished."));
    pump();
    Sleep(320);

    S.installed = 1;
    sm_free(exePath);
    sm_free(startMenu);
    if (desktopLnk) sm_free(desktopLnk);
    if (!S.hasMemArc) sm_arc_close(&arc);
    sm_free(dataFile);
    return 1;
}

/* ------------------------------------------------------------------ */
static void goto_page(int page)
{
    ck(36, "goto_page enter");
    if (page == PG_PROGRESS && !S.installed) {
        page_create(PG_PROGRESS);
        pump();
        if (!do_install()) {
            page_create(PG_WELCOME);
            return;
        }
        page_create(PG_FINISH);
        return;
    }
    page_create(page);
}

static int next_allowed(void)
{
    if (S.page == PG_LICENSE) {
        int accepted = S.hAccept &&
                       SendMessageA(S.hAccept, BM_GETCHECK, 0, 0) == BST_CHECKED;
        if (!accepted) {
            if (MessageBoxA(S.h,
                  TS("您尚未接受许可证协议。必须接受协议才能继续安装。\r\n\r\n现在接受协议并继续吗？",
                     "You have not accepted the license agreement. Setup cannot continue.\r\n\r\nAccept the agreement and continue?"),
                  TS("安装程序", "Setup"),
                  MB_ICONQUESTION | MB_YESNO) != IDYES)
                return 0;
            if (S.hAccept) SendMessageA(S.hAccept, BM_SETCHECK, BST_CHECKED, 0);
            if (S.hDecline) SendMessageA(S.hDecline, BM_SETCHECK, BST_UNCHECKED, 0);
        }
        S.licenceOk = 1;
    }
    if (S.page == PG_USER) {
        if (S.hName) {
            GetWindowTextA(S.hName, S.userName, sizeof(S.userName));
            sm_trim(S.userName);
            if (!S.userName[0]) strcpy(S.userName, TS("用户",
           "User"));
        }
        if (S.hCompany) {
            GetWindowTextA(S.hCompany, S.userCompany, sizeof(S.userCompany));
            sm_trim(S.userCompany);
        }
        if (S.hCdk) {
            GetWindowTextA(S.hCdk, S.cdk, sizeof(S.cdk));
            sm_trim(S.cdk);
        }
    }
    if (S.page == PG_DIR) {
        if (S.hDir) {
            GetWindowTextA(S.hDir, S.dir, sizeof(S.dir));
            sm_trim(S.dir);
        }
        if (!S.dir[0]) { MessageBoxA(S.h, TS("请输入目标文件夹。",
           "Please enter a destination folder."), TS("安装程序",
           "Setup"), MB_ICONSTOP | MB_OK); return 0; }
        if (strlen(S.dir) < 3 || S.dir[1] != ':') {
            MessageBoxA(S.h, "目标文件夹必须是一个完整的路径，例如：\r\n"
   "C:\\Program Files\\" SM_INSTALL_SUBDIR,
                        TS("安装程序",
           "Setup"), MB_ICONSTOP | MB_OK);
            return 0;
        }
    }
    if (S.page == PG_FOLDER) {
        if (S.hFolder) {
            GetWindowTextA(S.hFolder, S.folder, sizeof(S.folder));
            sm_trim(S.folder);
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ */
static void on_browse(void)
{
    BROWSEINFOA bi;
    LPITEMIDLIST pidl;
    char disp[MAX_PATH];
    ZeroMemory(&bi, sizeof(bi));
    bi.hwndOwner = S.h;
    bi.lpszTitle = TS("请选择 " SM_SHORT_NAME " 的安装文件夹",
           "Select the " SM_SHORT_NAME " installation folder");
    bi.ulFlags = BIF_RETURNONLYFSDIRS;
    pidl = SHBrowseForFolderA(&bi);
    if (pidl) {
        if (SHGetPathFromIDListA(pidl, disp)) {
            char *sub = sm_path_join(disp, "" SM_SHORT_NAME "");
            SetWindowTextA(S.hDir, sub);
            sm_free(sub);
        }
        CoTaskMemFree(pidl);
    }
}

/* ------------------------------------------------------------------ */
static LRESULT CALLBACK WizProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_CREATE:
        ck(40, "WM_CREATE enter");
        return 0;

    case WM_PAINT: {
        ck(41, "WM_PAINT enter");
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT r;
        GetClientRect(h, &r);

        /* 标题与内容之间的 3D 折线 */
        {
            HPEN p1 = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
            HPEN p2 = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DHILIGHT));
            HGDIOBJ op;
            op = SelectObject(dc, p1);
            MoveToEx(dc, CONTENT_X, 42, NULL);
            LineTo(dc, r.right - 12, 42);
            SelectObject(dc, p2);
            MoveToEx(dc, CONTENT_X, 43, NULL);
            LineTo(dc, r.right - 12, 43);
            SelectObject(dc, op);
            DeleteObject(p1);
            DeleteObject(p2);
        }
        EndPaint(h, &ps);
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)w;
        HWND c = (HWND)l;
        if (c == GetDlgItem(h, IDC_TITLE)) {
            SetTextColor(dc, RGB(0, 0, 128));
            SetBkMode(dc, TRANSPARENT);
            return (LRESULT)S.brFace;
        }
        SetBkMode(dc, TRANSPARENT);
        return (LRESULT)S.brFace;
    }

    case WM_COMMAND: {
        int id = LOWORD(w);
        if (id == IDC_CANCEL) {
            if (S.page == PG_PROGRESS) {
                if (MessageBoxA(h, TS("安装正在进行中。确实要退出安装吗？",
           "Setup is in progress. Really quit?"),
                                TS("安装程序",
           "Setup"), MB_ICONQUESTION | MB_YESNO) == IDYES)
                    DestroyWindow(h);
            } else {
                if (MessageBoxA(h, TS("安装尚未完成，确实要退出吗？",
           "Setup is not complete. Really quit?"),
                                TS("安装程序",
           "Setup"), MB_ICONQUESTION | MB_YESNO) == IDYES)
                    DestroyWindow(h);
            }
            return 0;
        }
        if (id == IDC_NEXT) {
            if (S.page == PG_FINISH) {
                int run = SendMessageA(S.hAutoRun, BM_GETCHECK, 0, 0) == BST_CHECKED;
                char *exe = sm_path_join(S.dir, SM_EXE_NAME);
                DestroyWindow(h);
                if (run) {
                    ShellExecuteA(NULL, "open", exe, NULL, S.dir, SW_SHOWNORMAL);
                }
                sm_free(exe);
                return 0;
            }
            if (!next_allowed()) return 0;
            goto_page(S.page + 1);
            return 0;
        }
        if (id == IDC_BACK) {
            if (S.page > PG_WELCOME) goto_page(S.page - 1);
            return 0;
        }
        if ((id == IDC_LANG_ZH || id == IDC_LANG_EN) && HIWORD(w) == BN_CLICKED) {
            int want = (id == IDC_LANG_EN) ? LANG_EN : LANG_ZH;
            if (want != lang_get()) {
                lang_set(want);
                refresh_lang();
            }
            return 0;
        }
        if (id == IDC_DIR_BROWSE && HIWORD(w) == BN_CLICKED) {
            /* 只有“浏览”按钮才弹目录选择框。绝不能用 id >= IDC_PAGEBASE 之类的
             * 区间判断：页面上每个控件（单选、编辑框、复选框、列表）都会发
             * WM_COMMAND，区间判断会让点任何控件都跳出浏览对话框。 */
            on_browse();
            return 0;
        }
        if (LOWORD(w) == IDC_FOLDER_LIST && HIWORD(w) == LBN_SELCHANGE && S.hFolder) {
            int sel = (int)SendMessageA(GetDlgItem(h, IDC_FOLDER_LIST), LB_GETCURSEL, 0, 0);
            if (sel >= 0) {
                char it[128];
                SendMessageA(GetDlgItem(h, IDC_FOLDER_LIST), LB_GETTEXT, (WPARAM)sel,
                             (LPARAM)it);
                it[sizeof(it) - 1] = 0;
                SetWindowTextA(S.hFolder, it);
            }
            return 0;
        }
        if (HIWORD(w) == LBN_SELCHANGE) {
            /* 程序文件夹列表 */
        }
        return 0;
    }

    case WM_CLOSE:
        SendMessageA(h, WM_COMMAND, MAKEWPARAM(IDC_CANCEL, 0), 0);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

/* ------------------------------------------------------------------ */
static void load_setup_ins(void)
{
    ck(34, "load_setup_ins enter");
    char *dir = sm_exe_dir();
    char *ins = sm_path_join(dir, "SETUP.INS");
    char *v;

    strcpy(S.setupDir, dir);
    strcpy(S.appName, "" SM_PRODUCT_NAME "");
    strcpy(S.appVersion, SM_VERSION_STR);

    v = sm_ini_get(ins, "Setup", "AppName", S.appName);
    strncpy(S.appName, v, sizeof(S.appName) - 1);
    sm_free(v);
    v = sm_ini_get(ins, "Setup", "Version", S.appVersion);
    strncpy(S.appVersion, v, sizeof(S.appVersion) - 1);
    sm_free(v);
    v = sm_ini_get(ins, "Setup", "DefaultDir", "");
    if (v[0]) strncpy(S.defaultDir, v, MAX_PATH - 1);
    sm_free(v);
    v = sm_ini_get(ins, "Setup", "ProgramFolder", "" SM_SHORT_NAME "");
    strncpy(S.folder, v, sizeof(S.folder) - 1);
    sm_free(v);
    v = sm_ini_get(ins, "Setup", "DataFile", "DATA1.CAB");
    strncpy(S.dataFile, v, sizeof(S.dataFile) - 1);
    sm_free(v);

    if (!S.defaultDir[0]) {
        char *pf = shell_folder(0x0026);   /* CSIDL_PROGRAM_FILES */
        char *sub = sm_path_join(pf, SM_INSTALL_SUBDIR);
        strncpy(S.defaultDir, sub, MAX_PATH - 1);
        sm_free(sub);
        sm_free(pf);
    }
    strcpy(S.dir, S.defaultDir);
    sm_free(ins);
    sm_free(dir);
}

/* ------------------------------------------------------------------ */
/* 命令行开关（InstallShield 风格）：
 *   SETUP.EXE /s              静默安装（不出现界面）
 *   SETUP.EXE /s /d=<目录>    静默安装到指定目录
 *   SETUP.EXE /p=<页号>       直接打开向导的第 n 页（1=欢迎 .. 6=完成）
 * ------------------------------------------------------------------ */
static int parse_cmdline(int *startPage)
{
    char *cl = GetCommandLineA();
    char *p;
    int silent = 0;

    *startPage = 0;

    /* 关键：先跳过 argv[0]。程序自己的文件名里可能就含 "-S"
     * （例如 NEUROBENCH2003-SETUP.EXE），如果在整条命令行里搜 "-s"
     * 就会把正常双击误判成静默安装。 */
    p = cl;
    if (*p == '"') {
        p++;
        while (*p && *p != '"') p++;
        if (*p == '"') p++;
    } else {
        while (*p && *p != ' ' && *p != '\t') p++;
    }

    while (*p) {
        char *tok;
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        tok = p;
        if (*tok == '"') {                      /* 带引号的参数 */
            tok++;
            while (*p && *p != '"') p++;
            if (*p == '"') p++;
        } else {
            while (*p && *p != ' ' && *p != '\t') p++;
        }

        if ((!sm_strnicmp(tok, "/s", 2) || !sm_strnicmp(tok, "-s", 2)) &&
            (tok[2] == 0 || tok[2] == ' ' || tok[2] == '\t' || tok[2] == '"'))
            silent = 1;
        else if (!sm_strnicmp(tok, "/d=", 3) || !sm_strnicmp(tok, "-d=", 3)) {
            int k = 0;
            char *v = tok + 3;
            while (*v && *v != '"' && *v != ' ' && *v != '\t' && k < MAX_PATH - 1)
                S.dir[k++] = *v++;
            S.dir[k] = 0;
            sm_trim(S.dir);
        } else if (!sm_strnicmp(tok, "/k=", 3) || !sm_strnicmp(tok, "-k=", 3)) {
            int k = 0;
            char *v = tok + 3;
            while (*v && *v != '"' && *v != ' ' && *v != '\t' && k < (int)sizeof(S.cdk) - 1)
                S.cdk[k++] = *v++;
            S.cdk[k] = 0;
            sm_trim(S.cdk);
        } else if (!sm_strnicmp(tok, "/p=", 3) || !sm_strnicmp(tok, "-p=", 3)) {
            *startPage = (int)sm_to_long(tok + 3, 0);
            if (*startPage < 1) *startPage = 1;
            if (*startPage > PG_COUNT) *startPage = PG_COUNT;
        }
    }
    return silent;
}

int PASCAL WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    WNDCLASSA wc;
    HWND h;
    MSG msg;
    INITCOMMONCONTROLSEX icc;
    LOGFONTA lf;
    int i;

    (void)prev; (void)cmd;
    memset(&S, 0, sizeof(S));
    S.hInst = inst;
    S.page = PG_WELCOME;
    S.licenceOk = 0;

    SetUnhandledExceptionFilter(setup_fault_filter);
    ck(10, "WinMain entry");
    sm_logf("SETUP image: base=%08lX wizproc=%08lX licproc=%08lX bytes=%lu",
            (unsigned long)(INT_PTR)GetModuleHandleA(NULL),
            (unsigned long)(INT_PTR)&WizProc,
            (unsigned long)(INT_PTR)&LicEditProc,
            (unsigned long)GetModuleFileNameA(NULL, NULL, 0));

    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES;
    sm_logf("SETUP start: build=%s os=%s dir=[%s] cmd=[%s]",
            S.appVersion, sm_get_os_name(), S.setupDir, GetCommandLineA());
    ck(11, "before load_embedded_arc");
    load_embedded_arc();
    ck(12, "before InitCommonControlsEx");
    InitCommonControlsEx(&icc);
    ck(13, "before OleInitialize");
    OleInitialize(NULL);
    ck(14, "before make_fonts");

    S.brFace = (HBRUSH)(COLOR_BTNFACE + 1);
    make_fonts();
    ck(15, "fonts done, before lang_load");

    lang_load();
    ck(16, "lang_load done");
    {
        char *ini2 = sm_ini_path();
        char *lv = sm_ini_get(ini2, "display", "language", "");
        if (lv[0]) lang_set(sm_stricmp(lv, "en") ? LANG_ZH : LANG_EN);
        else lang_set(nb_lang_from_system());
        sm_free(lv);
        sm_free(ini2);
    }
    ck(17, "lang detect done, before load_setup_ins");
    load_setup_ins();
    ck(18, "load_setup_ins done");
    sm_logf("SETUP ready: product=%s version=%s defaultDir=[%s] lang=%s",
            S.appName, S.appVersion, S.defaultDir,
            lang_get() == LANG_EN ? "en" : "zh");
    {
        int startPage = 0;
        int silent = parse_cmdline(&startPage);
        if (silent) {
            int ok;
            if (!S.dir[0]) strcpy(S.dir, S.defaultDir);
            ok = do_install();
            OleUninitialize();
            return ok ? 0 : 2;
        }
        S.autoPage = startPage;
    }
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WizProc;
    wc.hInstance = inst;
    wc.hIcon = LoadIconA(inst, MAKEINTRESOURCEA(102));
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = S.brFace;
    wc.lpszClassName = "NeuroBenchSetup";
    ck(19, "before RegisterClass");
    if (!RegisterClassA(&wc)) { ck(90, "RegisterClass FAILED"); return 1; }
    ck(20, "class registered");

    {
        RECT rc = { 0, 0, WIZ_W, WIZ_H };
        DWORD st = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
        AdjustWindowRectEx(&rc, st, FALSE, WS_EX_DLGMODALFRAME);
        h = CreateWindowExA(WS_EX_DLGMODALFRAME, "NeuroBenchSetup",
                            TS(SM_SHORT_NAME " 安装程序", SM_SHORT_NAME_EN " Setup"), st,
                            CW_USEDEFAULT, CW_USEDEFAULT,
                            rc.right - rc.left, rc.bottom - rc.top,
                            NULL, NULL, inst, NULL);
    }
    ck(21, "CreateWindow returned");
    if (!h) return 1;
    S.h = h;
    ck(22, "main window live");

    /* 标题与底部按钮（常驻） */
    {
        HWND c;
        c = CreateWindowExA(0, "STATIC", TS("欢迎使用 " SM_PRODUCT_NAME " 安装向导", "Welcome to " SM_PRODUCT_NAME_EN " Setup"),
                            WS_CHILD | WS_VISIBLE | SS_LEFT, CONTENT_X, 10, CONTENT_W, 26,
                            h, (HMENU)IDC_TITLE, inst, NULL);
        SendMessageA(c, WM_SETFONT, (WPARAM)S.fTitle, MAKELPARAM(TRUE, 0));

        c = CreateWindowExA(0, "BUTTON", TS("上一步(&B) <", "< &Back"), WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                            224, BTN_Y, 76, BTN_H, h, (HMENU)IDC_BACK, inst, NULL);
        SendMessageA(c, WM_SETFONT, (WPARAM)S.fUI, MAKELPARAM(TRUE, 0));
        c = CreateWindowExA(0, "BUTTON", "下一步(&N) >", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                            308, BTN_Y, 84, BTN_H, h, (HMENU)IDC_NEXT, inst, NULL);
        SendMessageA(c, WM_SETFONT, (WPARAM)S.fUI, MAKELPARAM(TRUE, 0));
        c = CreateWindowExA(0, "BUTTON", TS("取消", "Cancel"), WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                            400, BTN_Y, 84, BTN_H, h, (HMENU)IDC_CANCEL, inst, NULL);
        SendMessageA(c, WM_SETFONT, (WPARAM)S.fUI, MAKELPARAM(TRUE, 0));
    }

    page_create(PG_LANGUAGE);
    if (S.autoPage > 1 && S.autoPage <= PG_COUNT) {
        int p0 = S.autoPage - 1;
        if (p0 == PG_LICENSE) S.licenceOk = 1;
        S.page = p0;
        page_create(p0);
    }
    sm_center_window(h);
    ShowWindow(h, show);
    UpdateWindow(h);

    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageA(h, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    (void)i;
    OleUninitialize();
    return 0;
}
