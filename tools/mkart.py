#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SmartMate 2000 - 生成全部 16 色复古位图资源（图标/工具栏/启动画面/向导横幅）"""
import os
import sys
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# 标准 16 色 VGA 调色板 —— 1990 年代的味道
PAL = [
    (0, 0, 0), (128, 0, 0), (0, 128, 0), (128, 128, 0),
    (0, 0, 128), (128, 0, 128), (0, 128, 128), (192, 192, 192),
    (128, 128, 128), (255, 0, 0), (0, 255, 0), (255, 255, 0),
    (0, 0, 255), (255, 0, 255), (0, 255, 255), (255, 255, 255),
]
BLACK, MAROON, GREEN, OLIVE, NAVY, PURPLE, TEAL, SILVER = PAL[0:8]
GRAY, RED, LIME, YELLOW, BLUE, FUSCHIA, AQUA, WHITE = PAL[8:16]


def new(size, bg=None):
    im = Image.new("RGB", size, bg or SILVER)
    return im, ImageDraw.Draw(im)


def ic_page(d, ox=0, oy=0, fold=True):
    d.rectangle([1 + ox, 1 + oy, 12 + ox, 14 + oy], fill=WHITE, outline=BLACK)
    d.line([12 + ox, 1 + oy, 12 + ox, 4 + oy], fill=BLACK)
    if fold:
        d.polygon([(9 + ox, 1 + oy), (12 + ox, 4 + oy), (9 + ox, 4 + oy)], fill=SILVER, outline=BLACK)
    for y in (6, 8, 10, 12):
        d.line([3 + ox, y + oy, 10 + ox, y + oy], fill=SILVER)


def icon_new():
    im, d = new((16, 16), SILVER)
    ic_page(d)
    d.line([7, 8, 13, 8], fill=GREEN)
    d.line([10, 5, 10, 11], fill=GREEN)
    return im


def icon_open():
    im, d = new((16, 16), SILVER)
    d.polygon([(1, 4), (7, 4), (8, 6), (14, 6), (14, 13), (1, 13)], fill=YELLOW, outline=BLACK)
    d.rectangle([1, 3, 6, 6], fill=OLIVE, outline=BLACK)
    d.line([2, 8, 13, 8], fill=WHITE)
    d.line([2, 10, 11, 10], fill=WHITE)
    return im


def icon_save():
    im, d = new((16, 16), SILVER)
    d.rectangle([1, 1, 14, 14], fill=NAVY, outline=BLACK)
    d.rectangle([4, 1, 11, 6], fill=SILVER, outline=BLACK)
    d.rectangle([8, 2, 10, 5], fill=BLACK)
    d.rectangle([3, 8, 12, 14], fill=WHITE, outline=BLACK)
    d.line([4, 10, 11, 10], fill=GRAY)
    d.line([4, 12, 11, 12], fill=GRAY)
    return im


def icon_send():
    im, d = new((16, 16), SILVER)
    d.rectangle([1, 4, 10, 12], fill=WHITE, outline=BLACK)
    d.line([1, 4, 5, 8], fill=BLACK)
    d.line([10, 4, 6, 8], fill=BLACK)
    d.polygon([(15, 2), (12, 7), (12, 4)], fill=GREEN, outline=BLACK)
    return im


def icon_stop():
    im, d = new((16, 16), SILVER)
    d.polygon([(4, 1), (11, 1), (14, 4), (14, 11), (11, 14), (4, 14), (1, 11), (1, 4)],
              fill=MAROON, outline=BLACK)
    d.rectangle([4, 6, 11, 9], fill=RED, outline=WHITE)
    return im


def icon_editor():
    im, d = new((16, 16), SILVER)
    ic_page(d, 0, 0, True)
    d.polygon([(7, 12), (12, 7), (13, 8), (8, 13)], fill=YELLOW, outline=BLACK)
    return im


def icon_settings():
    im, d = new((16, 16), SILVER)
    d.ellipse([3, 3, 12, 12], fill=GRAY, outline=BLACK)
    for (x0, y0, x1, y1) in [(7, 0, 8, 3), (7, 12, 8, 15), (0, 7, 3, 8), (12, 7, 15, 8)]:
        d.rectangle([x0, y0, x1, y1], fill=GRAY, outline=BLACK)
    d.ellipse([6, 6, 9, 9], fill=WHITE, outline=BLACK)
    return im


def icon_key():
    im, d = new((16, 16), SILVER)
    d.ellipse([1, 5, 8, 12], fill=YELLOW, outline=BLACK)
    d.ellipse([3, 7, 6, 10], fill=SILVER, outline=BLACK)
    d.rectangle([8, 7, 14, 9], fill=YELLOW, outline=BLACK)
    d.rectangle([11, 9, 12, 11], fill=YELLOW, outline=BLACK)
    d.rectangle([13, 9, 14, 11], fill=YELLOW, outline=BLACK)
    return im


def icon_help():
    im, d = new((16, 16), SILVER)
    d.rectangle([1, 2, 14, 13], fill=NAVY, outline=BLACK)
    d.arc([5, 4, 10, 9], 150, 390, fill=YELLOW)
    d.line([7, 8, 7, 10], fill=YELLOW)
    d.point((7, 11), fill=YELLOW)
    d.point((7, 12), fill=YELLOW)
    return im


ICONS = [icon_new, icon_open, icon_save, icon_send, icon_stop,
         icon_editor, icon_settings, icon_key, icon_help]


def to_pal16(im):
    """把图像映射到标准 16 色 VGA 调色板（最近邻），用于 4bpp 位图。"""
    px = im.convert("RGB").load()
    out = Image.new("P", im.size)
    out.putpalette([c for col in PAL for c in col])
    q = out.load()
    w, h = im.size
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            best, bi = 1 << 30, 0
            for i, (pr, pg, pb) in enumerate(PAL):
                d = (r - pr) ** 2 + (g - pg) ** 2 + (b - pb) ** 2
                if d < best:
                    best, bi = d, i
            q[x, y] = bi
    return out


def _bmp_bytes(q, bpp):
    """把 P 模式图像编码为 bpp(4/8) 的 BMP 字节流。"""
    w, h = q.size
    if bpp == 4:
        stride = ((w * 4 + 31) // 32) * 4
    else:
        stride = ((w * 8 + 31) // 32) * 4
    pix = bytearray(stride * h)
    src = q.load()
    for y in range(h):
        row = h - 1 - y
        for x in range(w):
            v = src[x, y]
            if bpp == 4:
                idx = row * stride + x // 2
                if x % 2 == 0:
                    pix[idx] = (pix[idx] & 0xF0) | (v & 0x0F)
                else:
                    pix[idx] = (pix[idx] & 0x0F) | ((v & 0x0F) << 4)
            else:
                pix[row * stride + x] = v & 0xFF
    npal = 16 if bpp == 4 else 256
    pal = bytearray()
    src_pal = q.getpalette() or []
    for i in range(npal):
        r = src_pal[i * 3] if i * 3 < len(src_pal) else 0
        g = src_pal[i * 3 + 1] if i * 3 + 1 < len(src_pal) else 0
        b = src_pal[i * 3 + 2] if i * 3 + 2 < len(src_pal) else 0
        pal += bytes((b, g, r, 0))
    hdr = bytearray()
    hdr += b"BM"
    hdr += (14 + 40 + len(pal) + len(pix)).to_bytes(4, "little")
    hdr += (0).to_bytes(4, "little")
    hdr += (14 + 40 + len(pal)).to_bytes(4, "little")
    hdr += (40).to_bytes(4, "little")
    hdr += w.to_bytes(4, "little", signed=True)
    hdr += h.to_bytes(4, "little", signed=True)
    hdr += (1).to_bytes(2, "little")
    hdr += bpp.to_bytes(2, "little")
    hdr += (0).to_bytes(4, "little")
    hdr += len(pix).to_bytes(4, "little")
    hdr += (3780).to_bytes(4, "little", signed=True)
    hdr += (3780).to_bytes(4, "little", signed=True)
    hdr += npal.to_bytes(4, "little")
    hdr += npal.to_bytes(4, "little")
    return bytes(hdr + pal + pix)


def save_bmp4(path, im):
    with open(path, "wb") as f:
        f.write(_bmp_bytes(to_pal16(im), 4))
    return im


def save_bmp8(path, im):
    q = im.convert("P", palette=Image.ADAPTIVE, colors=256)
    with open(path, "wb") as f:
        f.write(_bmp_bytes(q, 8))
    return im


def _ico_image_bytes(q):
    w, h = q.size
    stride_xor = ((w * 4 + 31) // 32) * 4
    stride_and = ((w + 31) // 32) * 4
    xor = bytearray(stride_xor * h)
    and_ = bytearray(stride_and * h)
    src = q.load()
    for y in range(h):
        row = h - 1 - y
        for x in range(w):
            v = src[x, y] & 0x0F
            idx = row * stride_xor + x // 2
            if x % 2 == 0:
                xor[idx] = (xor[idx] & 0xF0) | v
            else:
                xor[idx] = (xor[idx] & 0x0F) | (v << 4)
            if v == 7 or v == 0:
                and_[row * stride_and + x // 8] |= (0x80 >> (x % 8))
    hdr = bytearray()
    hdr += (40).to_bytes(4, "little")
    hdr += w.to_bytes(4, "little", signed=True)
    hdr += (h * 2).to_bytes(4, "little", signed=True)
    hdr += (1).to_bytes(2, "little")
    hdr += (4).to_bytes(2, "little")
    hdr += (0).to_bytes(4, "little")
    hdr += (len(xor) + len(and_)).to_bytes(4, "little")
    hdr += (0).to_bytes(4, "little", signed=True)
    hdr += (0).to_bytes(4, "little", signed=True)
    hdr += (16).to_bytes(4, "little")
    hdr += (16).to_bytes(4, "little")
    pal = bytearray()
    for (r, g, b) in PAL:
        pal += bytes((b, g, r, 0))
    return bytes(hdr + pal + xor + and_)


def save_ico(path, im32):
    sizes = (16, 32, 48)
    blobs = [_ico_image_bytes(to_pal16(im32.resize((s, s), Image.NEAREST))) for s in sizes]
    out = bytearray()
    out += (0).to_bytes(2, "little")
    out += (1).to_bytes(2, "little")
    out += len(sizes).to_bytes(2, "little")
    offset = 6 + 16 * len(sizes)
    for s, blob in zip(sizes, blobs):
        out += bytes((s % 256, s % 256, 16, 0))
        out += (1).to_bytes(2, "little")
        out += (4).to_bytes(2, "little")
        out += len(blob).to_bytes(4, "little")
        out += offset.to_bytes(4, "little")
        offset += len(blob)
    for blob in blobs:
        out += blob
    with open(path, "wb") as f:
        f.write(out)


def make_toolbar(path):
    n = len(ICONS)
    im = Image.new("RGB", (16 * n, 16), SILVER)
    for i, f in enumerate(ICONS):
        im.paste(f(), (i * 16, 0))
    save_bmp4(path, im)
    return im


def load_font(size, bold=False):
    cands = [r"C:\Windows\Fonts\msyhbd.ttc" if bold else r"C:\Windows\Fonts\msyh.ttc",
             r"C:\Windows\Fonts\simhei.ttf",
             r"C:\Windows\Fonts\arialbd.ttf" if bold else r"C:\Windows\Fonts\arial.ttf"]
    for c in cands:
        if os.path.exists(c):
            try:
                return ImageFont.truetype(c, size)
            except Exception:
                pass
    return ImageFont.load_default()


def make_splash(path):
    W, H = 400, 240
    im = Image.new("RGB", (W, H), NAVY)
    d = ImageDraw.Draw(im)
    for y in range(H):
        t = y / float(H)
        d.line([0, y, W, y], fill=(int(0 + 0 * t), int(0 + 40 * t), int(96 + 96 * t)))
    for x in range(0, W, 8):
        d.line([x, 0, x + 4, 0], fill=(0, 0, 160))
    im = im.convert('P', palette=Image.ADAPTIVE, colors=256).convert('RGB')
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W - 1, H - 1], outline=BLACK)
    d.rectangle([2, 2, W - 3, H - 3], outline=(200, 200, 255))
    f1 = load_font(38, True)
    f2 = load_font(20)
    f3 = load_font(12)
    d.text((26, 34), "Axiom", font=f1, fill=WHITE)
    d.text((28, 78), "NeuroBench 2003", font=f2, fill=(220, 230, 255))
    d.line([28, 106, 372, 106], fill=(180, 190, 230))
    d.text((28, 116), "Desktop assistant for legacy Windows", font=f3, fill=(210, 220, 240))
    d.text((28, 134), "Built-in SSL/TLS 1.2 - Tool calling - Win 95/98/2000/XP", font=f3, fill=(190, 200, 230))
    d.text((28, 166), "Starting, please wait...", font=f3, fill=(170, 180, 210))
    d.rectangle([28, 188, 372, 200], fill=(10, 10, 60), outline=(120, 130, 170))
    d.rectangle([30, 190, 210, 198], fill=(210, 220, 250))
    d.text((28, 210), "Axiom Software   Version 3.00.2003", font=f3, fill=(160, 170, 210))
    save_bmp8(path, im)
    return im


def make_ico(path):
    im, d = new((32, 32), SILVER)
    d.rectangle([2, 3, 29, 22], fill=NAVY, outline=BLACK)
    d.rectangle([4, 5, 27, 20], fill=(210, 225, 255), outline=NAVY)
    d.polygon([(6, 21), (14, 21), (8, 28)], fill=NAVY, outline=BLACK)
    for i, y in enumerate((9, 13, 17)):
        d.rectangle([7, y, 7 + (14 - i * 4), y + 2], fill=NAVY)
    d.point((24, 6), fill=GREEN)
    d.point((25, 7), fill=GREEN)
    d.point((23, 7), fill=GREEN)
    d.point((24, 8), fill=GREEN)
    save_ico(path, im)
    return im


def make_setup_banner(path, w=164, h=314):
    im = Image.new("RGB", (w, h), NAVY)
    d = ImageDraw.Draw(im)
    for y in range(h):
        t = y / float(h)
        d.line([0, y, w, y], fill=(int(0), int(8 + 52 * t), int(70 + 90 * t)))
    im = im.convert('P', palette=Image.ADAPTIVE, colors=256).convert('RGB')
    d = ImageDraw.Draw(im)
    for i in range(-h, w, 10):
        d.line([i, h, i + h, 0], fill=(24, 40, 110))
    d.line([0, 0, 0, h], fill=BLACK)
    d.line([w - 1, 0, w - 1, h], fill=BLACK)
    f1 = load_font(22, True)
    f2 = load_font(16)
    f3 = load_font(11)
    d.text((14, 20), "Axiom", font=f1, fill=WHITE)
    d.text((14, 48), "NeuroBench", font=f2, fill=(200, 215, 255))
    d.text((14, 70), "2003", font=f2, fill=(200, 215, 255))
    d.line([14, 98, w - 14, 98], fill=(140, 160, 220))
    d.text((14, 112), "Setup Wizard", font=f3, fill=(200, 210, 240))
    d.text((14, 128), "InstallShield style", font=f3, fill=(160, 175, 215))
    d.text((14, h - 66), "Axiom Software", font=f3, fill=(190, 200, 235))
    d.text((14, h - 50), "Version 3.00.2003", font=f3, fill=(160, 175, 215))
    d.text((14, h - 32), "(C) 1993-2003", font=f3, fill=(140, 155, 195))
    save_bmp8(path, im)
    return im


def make_setup_logo(path, w=48, h=48):
    im, d = new((w, h), SILVER)
    d.rectangle([1, 2, w - 2, h - 14], fill=NAVY, outline=BLACK)
    d.rectangle([3, 4, w - 4, h - 16], fill=(210, 225, 255), outline=NAVY)
    d.polygon([(8, h - 15), (20, h - 15), (11, h - 3)], fill=NAVY, outline=BLACK)
    for y in (10, 16, 22):
        d.rectangle([6, y, w - 8, y + 3], fill=NAVY)
    save_bmp4(path, im)
    return im


def main():
    res_agent = os.path.join(ROOT, "src", "agent", "res")
    res_setup = os.path.join(ROOT, "src", "setup", "res")
    res_key = os.path.join(ROOT, "src", "keygen", "res")
    for p in (res_agent, res_setup, res_key):
        os.makedirs(p, exist_ok=True)

    print("toolbar ->", os.path.join(res_agent, "toolbar.bmp"))
    make_toolbar(os.path.join(res_agent, "toolbar.bmp"))
    make_splash(os.path.join(res_agent, "splash.bmp"))
    make_ico(os.path.join(res_agent, "agent.ico"))
    make_setup_banner(os.path.join(res_setup, "banner.bmp"))
    make_setup_logo(os.path.join(res_setup, "logo.bmp"))
    make_ico(os.path.join(res_setup, "setup.ico"))
    make_ico(os.path.join(res_key, "keygen.ico"))
    make_setup_banner(os.path.join(res_key, "banner.bmp"))
    make_setup_banner(os.path.join(res_agent, "banner.bmp"))
    print("done")


if __name__ == "__main__":
    main()
