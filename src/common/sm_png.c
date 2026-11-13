/* =====================================================================
 * Axiom NeuroBench 2003 - 屏幕抓取与 PNG 编码
 *
 * 只为了“让模型看图”这一件事：
 *   sm_grab_screen()  抓全屏到内存（BGR，自下而上，和 BMP 一致）
 *   sm_png_encode()   编成 PNG（zlib 存储块，不压缩、零外部依赖）
 *
 * 不压缩是有意为之：老机器 CPU 慢，压缩省的带宽不值那点 CPU；
 * 而且省掉了整个 zlib 依赖。
 * ===================================================================== */
#include "sm.h"

/* ---------------- 抓屏 ---------------- */
int sm_grab_screen(unsigned char **outBgr, int *outW, int *outH)
{
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    HDC screen, mem, dc;
    HBITMAP bmp, old;
    BITMAPINFOHEADER ih;
    DWORD linesz, imgsz;
    unsigned char *bits;

    *outBgr = NULL;
    *outW = 0;
    *outH = 0;
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
    bits = (unsigned char *)sm_calloc(imgsz);
    if (!bits) { DeleteObject(bmp); DeleteDC(mem); ReleaseDC(NULL, screen); return 0; }

    ZeroMemory(&ih, sizeof(ih));
    ih.biSize = sizeof(ih);
    ih.biWidth = w;
    ih.biHeight = h;
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    ih.biCompression = BI_RGB;
    ih.biSizeImage = imgsz;

    dc = CreateCompatibleDC(NULL);
    SelectObject(dc, bmp);
    if (!GetDIBits(dc, bmp, 0, (UINT)h, bits, (BITMAPINFO *)&ih, DIB_RGB_COLORS)) {
        DeleteDC(dc);
        sm_free(bits);
        DeleteObject(bmp);
        DeleteDC(mem);
        ReleaseDC(NULL, screen);
        return 0;
    }
    DeleteDC(dc);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(NULL, screen);

    *outBgr = bits;
    *outW = w;
    *outH = h;
    return 1;
}

/* 抓屏并等比缩小到上限内：模型看 1024x768 足够，全屏原图 base64 太大 */
int sm_grab_screen_max(unsigned char **outBgr, int *outW, int *outH,
                       int maxW, int maxH)
{
    unsigned char *src = NULL;
    int w = 0, h = 0, dw, dh;
    HDC screen, mem, dc;
    HBITMAP bmp, old;
    BITMAPINFOHEADER ih;
    DWORD linesz, imgsz;
    unsigned char *bits;

    if (!sm_grab_screen(&src, &w, &h)) return 0;
    if (maxW <= 0 || maxH <= 0 || (w <= maxW && h <= maxH)) {
        *outBgr = src; *outW = w; *outH = h;
        return 1;
    }
    if ((long)w * maxH <= (long)h * maxW) { dw = maxW; dh = (int)((long)h * maxW / w); }
    else                                 { dh = maxH; dw = (int)((long)w * maxH / h); }
    if (dw < 8) dw = 8;
    if (dh < 8) dh = 8;

    screen = GetDC(NULL);
    mem = CreateCompatibleDC(screen);
    bmp = CreateCompatibleBitmap(screen, dw, dh);
    if (!mem || !bmp) {
        if (bmp) DeleteObject(bmp);
        if (mem) DeleteDC(mem);
        ReleaseDC(NULL, screen);
        sm_free(src);
        return 0;
    }
    old = (HBITMAP)SelectObject(mem, bmp);
    SetStretchBltMode(mem, HALFTONE);
    StretchBlt(mem, 0, 0, dw, dh, screen, 0, 0, w, h, SRCCOPY);
    SelectObject(mem, old);
    ReleaseDC(NULL, screen);

    linesz = ((dw * 3 + 3) & ~3u);
    imgsz = linesz * (DWORD)dh;
    bits = (unsigned char *)sm_calloc(imgsz);
    if (!bits) { DeleteObject(bmp); DeleteDC(mem); sm_free(src); return 0; }

    ZeroMemory(&ih, sizeof(ih));
    ih.biSize = sizeof(ih);
    ih.biWidth = dw; ih.biHeight = dh; ih.biPlanes = 1;
    ih.biBitCount = 24; ih.biCompression = BI_RGB; ih.biSizeImage = imgsz;

    dc = CreateCompatibleDC(NULL);
    SelectObject(dc, bmp);
    if (!GetDIBits(dc, bmp, 0, (UINT)dh, bits, (BITMAPINFO *)&ih, DIB_RGB_COLORS)) {
        DeleteDC(dc); sm_free(bits); DeleteObject(bmp); DeleteDC(mem); sm_free(src);
        return 0;
    }
    DeleteDC(dc);
    DeleteObject(bmp);
    DeleteDC(mem);
    sm_free(src);

    *outBgr = bits; *outW = dw; *outH = dh;
    return 1;
}

/* 读一张本地图片变成 PNG 字节：.png 直通，.bmp 解码后重编码 */
unsigned char *sm_load_image_png(const char *path, long *outLen)
{
    FILE *f;
    unsigned char *buf;
    long len;

    *outLen = 0;
    if (!path || !path[0]) return NULL;
    f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 8 || len > 32L * 1024 * 1024) { fclose(f); return NULL; }
    buf = (unsigned char *)sm_alloc((size_t)len);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)len, f) != (size_t)len) { fclose(f); sm_free(buf); return NULL; }
    fclose(f);

    if (len > 8 && buf[0] == 0x89 && buf[1] == 'P' && buf[2] == 'N' && buf[3] == 'G') {
        *outLen = len;
        return buf;                       /* 已经是 PNG */
    }
    if (buf[0] == 'B' && buf[1] == 'M' && len > 54) {
        /* BMP：取像素区，转成我们的 BGR 自下而上布局再编 PNG */
        unsigned long off = *(unsigned long *)(buf + 10);
        long hdr = *(long *)(buf + 14);
        long w = *(long *)(buf + 18);
        long h = *(long *)(buf + 22);
        int bpp = *(unsigned short *)(buf + 28);
        long stride = ((w * 3 + 3) & ~3L);
        unsigned char *bgr;
        long rows, y;
        if (w <= 0 || h == 0 || bpp != 24 || hdr < 40) { sm_free(buf); return NULL; }
        rows = (h < 0) ? -h : h;
        bgr = (unsigned char *)sm_alloc((size_t)(stride * rows));
        if (!bgr) { sm_free(buf); return NULL; }
        for (y = 0; y < rows; y++) {
            long srcRow = (h > 0) ? (rows - 1 - y) : y;   /* 正高 = 自下而上 */
            long so = (long)off + srcRow * ((w * 3 + 3) & ~3L);
            if (so + w * 3 > len) break;
            memcpy(bgr + y * stride, buf + so, (size_t)(w * 3));
        }
        {
            long plen = 0;
            unsigned char *png = sm_png_encode(bgr, (int)w, (int)rows, &plen);
            sm_free(bgr);
            sm_free(buf);
            if (!png) return NULL;
            *outLen = plen;
            return png;
        }
    }
    sm_free(buf);
    return NULL;
}

/* ---------------- PNG 编码 ---------------- */
typedef struct { unsigned char *p; long len, cap; } Pbuf;

static void pb_need(Pbuf *b, long n)
{
    if (b->len + n <= b->cap) return;
    while (b->cap < b->len + n) b->cap = b->cap ? b->cap * 2 : 4096;
    b->p = (unsigned char *)sm_realloc(b->p, (size_t)b->cap);
}
static void pb_byte(Pbuf *b, unsigned char c) { pb_need(b, 1); b->p[b->len++] = c; }
static void pb_raw(Pbuf *b, const void *d, long n)
{
    pb_need(b, n);
    memcpy(b->p + b->len, d, (size_t)n);
    b->len += n;
}
static void pb_be32(Pbuf *b, unsigned long v)
{
    pb_byte(b, (unsigned char)(v >> 24));
    pb_byte(b, (unsigned char)(v >> 16));
    pb_byte(b, (unsigned char)(v >> 8));
    pb_byte(b, (unsigned char)v);
}

static void png_chunk(Pbuf *b, const char *type, const unsigned char *data, long n)
{
    pb_be32(b, (unsigned long)n);
    pb_raw(b, type, 4);
    if (n) pb_raw(b, data, n);
    {
        /* CRC 覆盖 type + data */
        unsigned char *crcsrc;
        long crclen = n + 4;
        unsigned short dummy = 0;
        (void)dummy;
        crcsrc = (unsigned char *)sm_alloc((size_t)crclen);
        if (!crcsrc) return;
        memcpy(crcsrc, type, 4);
        if (n) memcpy(crcsrc + 4, data, (size_t)n);
        {
            unsigned long crc = sm_crc32(crcsrc, (size_t)crclen);
            pb_be32(b, crc);
        }
        sm_free(crcsrc);
    }
}

static unsigned long adler32_calc(const unsigned char *d, long n)
{
    unsigned long a = 1, b = 0;
    long i;
    for (i = 0; i < n; i++) {
        a = (a + d[i]) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

/* BGR 自下而上 -> PNG（RGB 自上而下），返回堆内存，长度写入 *outLen */
unsigned char *sm_png_encode(const unsigned char *bgr, int w, int h, long *outLen)
{
    Pbuf out, raw;
    long y, x, stride;
    long rawLen;
    unsigned char ihdr[13];

    *outLen = 0;
    if (!bgr || w <= 0 || h <= 0) return NULL;
    stride = (long)w * 3;

    /* 1) 原始扫描线（每行前加 filter=0） */
    raw.p = NULL; raw.len = 0; raw.cap = 0;
    rawLen = (stride + 1) * h;
    pb_need(&raw, rawLen);
    for (y = 0; y < h; y++) {
        const unsigned char *src = bgr + (long)(h - 1 - y) * (((long)w * 3 + 3) & ~3L);
        pb_byte(&raw, 0);                     /* filter: None */
        for (x = 0; x < w; x++) {
            pb_byte(&raw, src[x * 3 + 2]);    /* R */
            pb_byte(&raw, src[x * 3 + 1]);    /* G */
            pb_byte(&raw, src[x * 3 + 0]);    /* B */
        }
    }

    /* 2) 外壳 */
    out.p = NULL; out.len = 0; out.cap = 0;
    pb_raw(&out, "\x89PNG\r\n\x1a\n", 8);

    ihdr[0] = (unsigned char)(w >> 24); ihdr[1] = (unsigned char)(w >> 16);
    ihdr[2] = (unsigned char)(w >> 8);  ihdr[3] = (unsigned char)w;
    ihdr[4] = (unsigned char)(h >> 24); ihdr[5] = (unsigned char)(h >> 16);
    ihdr[6] = (unsigned char)(h >> 8);  ihdr[7] = (unsigned char)h;
    ihdr[8] = 8;    /* bit depth */
    ihdr[9] = 2;    /* colour type: truecolour */
    ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    png_chunk(&out, "IHDR", ihdr, 13);

    /* 3) IDAT：zlib 头 + 若干存储块 + adler32 */
    {
        Pbuf z;
        long pos = 0;
        z.p = NULL; z.len = 0; z.cap = 0;
        pb_byte(&z, 0x78);      /* CMF: deflate, 32K window */
        pb_byte(&z, 0x01);      /* FLG: 无字典，最快 */
        while (pos < raw.len) {
            long n = raw.len - pos;
            int final = 0;
            if (n > 65535) n = 65535;
            if (pos + n >= raw.len) final = 1;
            pb_byte(&z, (unsigned char)(final ? 1 : 0));   /* BFINAL, BTYPE=00 */
            pb_byte(&z, (unsigned char)(n & 0xFF));
            pb_byte(&z, (unsigned char)((n >> 8) & 0xFF));
            pb_byte(&z, (unsigned char)(~n & 0xFF));
            pb_byte(&z, (unsigned char)((~n >> 8) & 0xFF));
            pb_raw(&z, raw.p + pos, n);
            pos += n;
        }
        pb_be32(&z, adler32_calc(raw.p, raw.len));
        png_chunk(&out, "IDAT", z.p, z.len);
        sm_free(z.p);
    }
    png_chunk(&out, "IEND", NULL, 0);
    sm_free(raw.p);

    *outLen = out.len;
    return out.p;      /* 调用方负责 sm_free */
}
