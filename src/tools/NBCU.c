/* =====================================================================
 * Axiom NeuroBench 2003 - Computer Use 附加工具 (NBCU.EXE)
 *
 * 纯用户态实现：GDI 截屏 + SendInput 注入键鼠。
 * 由智能助手通过 run_command 调用，例如：
 *     NBCU.EXE size
 *     NBCU.EXE shot C:\TEMP\SCREEN.BMP
 *     NBCU.EXE click 400 300
 *     NBCU.EXE type "hello"
 *     NBCU.EXE key enter
 *
 * 不安驱动、不改系统设置，用完即走。
 * 急停：执行任何动作前检查 Ctrl+Alt+End，按下则拒绝动作并退出码 9。
 * ===================================================================== */
#include <windows.h>
#include "../common/nb_brand.h"   /* 品牌宏 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define EXIT_PANIC   9
#define MAX_TEXT     4096

static int panic_pressed(void)
{
    return (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
           (GetAsyncKeyState(VK_MENU) & 0x8000) &&
           (GetAsyncKeyState(VK_END) & 0x8000);
}

static int guard(void)
{
    if (panic_pressed()) {
        printf("PANIC: Ctrl+Alt+End 已按下，取消操作。\n");
        return 0;
    }
    return 1;
}

/* ---------------- 截屏 ---------------- */
static int save_bmp(const char *path)
{
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    HDC screen, mem;
    HBITMAP bmp, old;
    BITMAPFILEHEADER fh;
    BITMAPINFOHEADER ih;
    DWORD linesz, imgsz;
    unsigned char *bits = NULL;
    HDC dc;
    FILE *f;

    if (w <= 0 || h <= 0) return 0;
    screen = GetDC(NULL);
    if (!screen) return 0;
    mem = CreateCompatibleDC(screen);
    bmp = CreateCompatibleBitmap(screen, w, h);
    if (!mem || !bmp) {
        if (bmp) DeleteObject(bmp);
        if (mem) DeleteDC(mem);
        ReleaseDC(NULL, screen);
        return 0;
    }
    old = (HBITMAP)SelectObject(mem, bmp);
    BitBlt(mem, 0, 0, w, h, screen, 0, 0, SRCCOPY);
    SelectObject(mem, old);

    linesz = ((w * 3 + 3) & ~3u);
    imgsz = linesz * (DWORD)h;
    bits = (unsigned char *)malloc(imgsz);
    if (!bits) { DeleteObject(bmp); DeleteDC(mem); ReleaseDC(NULL, screen); return 0; }

    ZeroMemory(&ih, sizeof(ih));
    ih.biSize = sizeof(ih);
    ih.biWidth = w;
    ih.biHeight = h;                 /* 正数 = 自下而上 */
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    ih.biCompression = BI_RGB;
    ih.biSizeImage = imgsz;

    dc = CreateCompatibleDC(NULL);
    SelectObject(dc, bmp);
    if (!GetDIBits(dc, bmp, 0, (UINT)h, bits, (BITMAPINFO *)&ih, DIB_RGB_COLORS)) {
        DeleteDC(dc); free(bits); DeleteObject(bmp); DeleteDC(mem); ReleaseDC(NULL, screen);
        return 0;
    }
    DeleteDC(dc);

    ZeroMemory(&fh, sizeof(fh));
    fh.bfType = 0x4D42;              /* "BM" */
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + imgsz;

    f = fopen(path, "wb");
    if (!f) { free(bits); DeleteObject(bmp); DeleteDC(mem); ReleaseDC(NULL, screen); return 0; }
    fwrite(&fh, sizeof(fh), 1, f);
    fwrite(&ih, sizeof(ih), 1, f);
    fwrite(bits, 1, imgsz, f);
    fclose(f);

    free(bits);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(NULL, screen);
    return 1;
}

/* ------------------------------------------------------------------ */
/* SendInput 在 Windows 98/ME 上不一定存在，运行时探测，               */
/* 取不到就退回 mouse_event / keybd_event。                            */
/* ------------------------------------------------------------------ */
typedef UINT (WINAPI *PFN_SENDINPUT)(UINT, INPUT *, int);
static PFN_SENDINPUT g_sendinput = NULL;
static int g_si_probed = 0;

static int has_sendinput(void)
{
    if (!g_si_probed) {
        HMODULE u = GetModuleHandleA("user32.dll");
        if (!u) u = LoadLibraryA("user32.dll");
        if (u) g_sendinput = (PFN_SENDINPUT)GetProcAddress(u, "SendInput");
        g_si_probed = 1;
    }
    return g_sendinput != NULL;
}

static void do_input(int n, INPUT *in)
{
    int i;
    if (has_sendinput()) {
        g_sendinput((UINT)n, in, (int)sizeof(INPUT));
        return;
    }
    for (i = 0; i < n; i++) {
        if (in[i].type == INPUT_MOUSE) {
            mouse_event(in[i].mi.dwFlags, in[i].mi.dx, in[i].mi.dy,
                        in[i].mi.mouseData, 0);
        } else if (in[i].type == INPUT_KEYBOARD) {
            if (in[i].ki.dwFlags & KEYEVENTF_UNICODE) continue;
            keybd_event((BYTE)in[i].ki.wVk, (BYTE)in[i].ki.wScan,
                        in[i].ki.dwFlags, 0);
        }
    }
}

/* ---------------- 鼠标 ---------------- */
static void put_mouse(DWORD flags, LONG dx, LONG dy, DWORD data)
{
    INPUT in;
    ZeroMemory(&in, sizeof(in));
    in.type = INPUT_MOUSE;
    in.mi.dx = dx;
    in.mi.dy = dy;
    in.mi.dwFlags = flags;
    in.mi.mouseData = data;
    do_input(1, &in);
}

static int mouse_abs(int x, int y)
{
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    if (w < 2) w = 2;
    if (h < 2) h = 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= w) x = w - 1;
    if (y >= h) y = h - 1;
    put_mouse(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE,
              (LONG)((double)x * 65535.0 / (w - 1) + 0.5),
              (LONG)((double)y * 65535.0 / (h - 1) + 0.5), 0);
    return 1;
}

static DWORD btn_down_flag(const char *b)
{
    if (b && (b[0] == 'r' || b[0] == 'R')) return MOUSEEVENTF_RIGHTDOWN;
    if (b && (b[0] == 'm' || b[0] == 'M')) return MOUSEEVENTF_MIDDLEDOWN;
    return MOUSEEVENTF_LEFTDOWN;
}
static DWORD btn_up_flag(const char *b)
{
    if (b && (b[0] == 'r' || b[0] == 'R')) return MOUSEEVENTF_RIGHTUP;
    if (b && (b[0] == 'm' || b[0] == 'M')) return MOUSEEVENTF_MIDDLEUP;
    return MOUSEEVENTF_LEFTUP;
}

static int mouse_click(const char *b, int count)
{
    int i;
    for (i = 0; i < count; i++) {
        put_mouse(btn_down_flag(b), 0, 0, 0);
        Sleep(40);
        put_mouse(btn_up_flag(b), 0, 0, 0);
        if (count > 1) Sleep(120);
    }
    return 1;
}

static int mouse_drag(int x1, int y1, int x2, int y2)
{
    int steps = 12, i;
    mouse_abs(x1, y1);
    Sleep(80);
    put_mouse(MOUSEEVENTF_LEFTDOWN, 0, 0, 0);
    Sleep(80);
    for (i = 1; i <= steps; i++) {
        int x = x1 + (x2 - x1) * i / steps;
        int y = y1 + (y2 - y1) * i / steps;
        mouse_abs(x, y);
        Sleep(25);
    }
    Sleep(80);
    put_mouse(MOUSEEVENTF_LEFTUP, 0, 0, 0);
    return 1;
}

/* ---------------- 键盘 ---------------- */
struct keyname { const char *n; int vk; };
static const struct keyname KEYS[] = {
    { "enter", VK_RETURN }, { "return", VK_RETURN }, { "esc", VK_ESCAPE },
    { "escape", VK_ESCAPE }, { "tab", VK_TAB }, { "space", VK_SPACE },
    { "back", VK_BACK }, { "backspace", VK_BACK }, { "delete", VK_DELETE },
    { "del", VK_DELETE }, { "insert", VK_INSERT }, { "home", VK_HOME },
    { "end", VK_END }, { "pageup", VK_PRIOR }, { "pagedown", VK_NEXT },
    { "up", VK_UP }, { "down", VK_DOWN }, { "left", VK_LEFT }, { "right", VK_RIGHT },
    { "win", VK_LWIN }, { "apps", VK_APPS }, { "printscreen", VK_SNAPSHOT },
    { "f1", VK_F1 }, { "f2", VK_F2 }, { "f3", VK_F3 }, { "f4", VK_F4 },
    { "f5", VK_F5 }, { "f6", VK_F6 }, { "f7", VK_F7 }, { "f8", VK_F8 },
    { "f9", VK_F9 }, { "f10", VK_F10 }, { "f11", VK_F11 }, { "f12", VK_F12 },
    { "ctrl", VK_CONTROL }, { "alt", VK_MENU }, { "shift", VK_SHIFT },
    { NULL, 0 }
};

static int key_vk(const char *name)
{
    int i;
    if (!name || !name[0]) return 0;
    if (name[0] == '0' && (name[1] == 'x' || name[1] == 'X'))
        return (int)strtol(name + 2, NULL, 16);
    if (strlen(name) == 1) {
        SHORT v = VkKeyScanA(name[0]);
        if (v != -1) return (int)(v & 0xFF);
    }
    for (i = 0; KEYS[i].n; i++)
        if (_stricmp(KEYS[i].n, name) == 0) return KEYS[i].vk;
    return 0;
}

static void key_event(int vk, int down)
{
    INPUT in;
    ZeroMemory(&in, sizeof(in));
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = (WORD)vk;
    in.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    do_input(1, &in);
}

/* ANSI(GBK) 文本 -> 逐个字符用 Unicode 方式注入，中文也能打进去 */
/* 9x 没有 SendInput，也就不支持 KEYEVENTF_UNICODE，改用剪贴板 + Ctrl+V */
static int paste_text(const char *ansi)
{
    size_t len = strlen(ansi) + 1;
    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, (SIZE_T)len);
    void *p;
    if (!hg) return 0;
    p = GlobalLock(hg);
    if (!p) { GlobalFree(hg); return 0; }
    memcpy(p, ansi, len);
    GlobalUnlock(hg);
    if (!OpenClipboard(NULL)) { GlobalFree(hg); return 0; }
    EmptyClipboard();
    if (!SetClipboardData(CF_TEXT, hg)) { CloseClipboard(); GlobalFree(hg); return 0; }
    CloseClipboard();
    key_event(VK_CONTROL, 1);
    Sleep(40);
    key_event('V', 1); key_event('V', 0);
    Sleep(40);
    key_event(VK_CONTROL, 0);
    return 1;
}

static int type_text(const char *ansi)
{
    int n, i;
    WCHAR *wide;
    if (!ansi || !ansi[0]) return 0;
    if (!has_sendinput()) return paste_text(ansi);
    n = MultiByteToWideChar(CP_ACP, 0, ansi, -1, NULL, 0);
    if (n <= 1) return 0;
    wide = (WCHAR *)malloc(sizeof(WCHAR) * n);
    if (!wide) return 0;
    MultiByteToWideChar(CP_ACP, 0, ansi, -1, wide, n);
    for (i = 0; i < n - 1; i++) {
        INPUT in[2];
        ZeroMemory(in, sizeof(in));
        in[0].type = INPUT_KEYBOARD;
        in[0].ki.wScan = wide[i];
        in[0].ki.dwFlags = KEYEVENTF_UNICODE;
        in[1] = in[0];
        in[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        do_input(2, in);
        if (wide[i] == L'\n') { /* 换行另发回车，某些控件只认 VK_RETURN */
            key_event(VK_RETURN, 1);
            key_event(VK_RETURN, 0);
        }
        Sleep(8);
    }
    free(wide);
    return 1;
}

/* ---------------- 命令分发 ---------------- */
static void usage(void)
{
    printf(SM_PRODUCT_NAME " - Computer Use 工具\n"
           "用法: NBCU.EXE <命令> [参数]\n"
           "  size                        打印屏幕分辨率\n"
           "  shot <文件.bmp>             截屏保存\n"
           "  move <x> <y>                移动鼠标\n"
           "  click <x> <y> [l|r|m] [n]   单击（可指定键与次数）\n"
           "  drag <x1> <y1> <x2> <y2>    拖动\n"
           "  down <l|r|m> / up <l|r|m>   按下 / 松开\n"
           "  scroll <格数>               滚轮（正=上）\n"
           "  key <键名|0xVK>             按一次键\n"
           "  keydown <键名> / keyup <键名>\n"
           "  type <文本>                 输入文本（支持中文）\n"
           "  sleep <毫秒>                等待\n"
           "急停: 任何动作前按 Ctrl+Alt+End 可取消。\n");
}

int main(int argc, char **argv)
{
    const char *cmd;
    if (argc < 2) { usage(); return 2; }
    cmd = argv[1];

    if (_stricmp(cmd, "size") == 0) {
        printf("%dx%d\n", GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
        return 0;
    }
    if (_stricmp(cmd, "shot") == 0) {
        if (argc < 3) { printf("缺少输出文件。\n"); return 2; }
        if (!guard()) return EXIT_PANIC;
        if (!save_bmp(argv[2])) { printf("截屏失败。\n"); return 3; }
        printf("已保存: %s\n", argv[2]);
        return 0;
    }
    if (_stricmp(cmd, "sleep") == 0) {
        int ms = (argc > 2) ? atoi(argv[2]) : 500;
        if (ms < 0) ms = 0;
        if (ms > 10000) ms = 10000;
        Sleep(ms);
        return 0;
    }
    if (_stricmp(cmd, "move") == 0) {
        if (argc < 4) { printf("缺少坐标。\n"); return 2; }
        if (!guard()) return EXIT_PANIC;
        mouse_abs(atoi(argv[2]), atoi(argv[3]));
        return 0;
    }
    if (_stricmp(cmd, "click") == 0 || _stricmp(cmd, "dclick") == 0) {
        const char *b = "l";
        int cnt = (_stricmp(cmd, "dclick") == 0) ? 2 : 1;
        if (argc < 4) { printf("缺少坐标。\n"); return 2; }
        if (argc > 4) b = argv[4];
        if (argc > 5) { cnt = atoi(argv[5]); if (cnt < 1) cnt = 1; if (cnt > 5) cnt = 5; }
        if (!guard()) return EXIT_PANIC;
        mouse_abs(atoi(argv[2]), atoi(argv[3]));
        Sleep(80);
        mouse_click(b, cnt);
        return 0;
    }
    if (_stricmp(cmd, "drag") == 0) {
        if (argc < 6) { printf("需要四个坐标。\n"); return 2; }
        if (!guard()) return EXIT_PANIC;
        mouse_drag(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
        return 0;
    }
    if (_stricmp(cmd, "down") == 0 || _stricmp(cmd, "up") == 0) {
        const char *b = (argc > 2) ? argv[2] : "l";
        if (!guard()) return EXIT_PANIC;
        put_mouse(_stricmp(cmd, "down") == 0 ? btn_down_flag(b) : btn_up_flag(b), 0, 0, 0);
        return 0;
    }
    if (_stricmp(cmd, "scroll") == 0) {
        int n = (argc > 2) ? atoi(argv[2]) : 1;
        if (!guard()) return EXIT_PANIC;
        put_mouse(MOUSEEVENTF_WHEEL, 0, 0, (DWORD)(n * WHEEL_DELTA));
        return 0;
    }
    if (_stricmp(cmd, "key") == 0 || _stricmp(cmd, "keydown") == 0 ||
        _stricmp(cmd, "keyup") == 0) {
        int vk;
        if (argc < 3) { printf("缺少键名。\n"); return 2; }
        vk = key_vk(argv[2]);
        if (!vk) { printf("无法识别的键: %s\n", argv[2]); return 2; }
        if (!guard()) return EXIT_PANIC;
        if (_stricmp(cmd, "key") == 0) {
            key_event(vk, 1);
            Sleep(40);
            key_event(vk, 0);
        } else {
            key_event(vk, _stricmp(cmd, "keydown") == 0);
        }
        return 0;
    }
    if (_stricmp(cmd, "type") == 0) {
        if (argc < 3) { printf("缺少文本。\n"); return 2; }
        if (!guard()) return EXIT_PANIC;
        if (!type_text(argv[2])) { printf("输入失败。\n"); return 3; }
        return 0;
    }

    printf("未知命令: %s\n\n", cmd);
    usage();
    return 2;
}
