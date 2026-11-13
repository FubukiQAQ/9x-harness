#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""mkchm.py -- compile a directory tree into a real Microsoft HTML Help (.chm).

The result is a genuine ITSF / MSCompressed container of the kind hhc.exe
(HTML Help Workshop) writes, so hh.exe (the HTML Help viewer) opens it with a
working table of contents, an index, and full-text-free topic navigation on
Windows XP and Windows 11.

No compression library is needed.  By default every file is stored verbatim in
the container's *uncompressed* ITSF storage (directory entries with
"space = 0", which 7-Zip reports as method "Copy" and which the viewer serves
exactly like compressed entries), so the tool never has to compress anything.
An experimental LZX path that wraps the same bytes in LZX "uncompressed"
blocks (block type 3) is available as --storage lzx; see NOTES below.

Container layout written (all field offsets verified byte-for-byte against
CHMs shipped with Windows, such as C:\\Windows\\Help\\mui\\0804\\mmc.CHM):

    0x0000  ITSF header, version 3, 0x60 bytes
    0x0060  ITSF section 1, 0x18 bytes: { 0x1FE, total_file_size, 0 }
    0x0078  ITSF section 2 = the directory, ITSP header (0x54) + chunks
              chunk 0 therefore starts at 0x78 + 0x54 = 0xCC, each chunk 0x1000
              PMGL: "PMGL", u32 free_space, u32 0, i32 prev, u32 next, entries
              PMGI: "PMGI", u32 free_space, entries { u8 len, name, varint chunk }
              entry: u8 name_len, name, varint space, varint offset, varint length
              varint = big-endian base-128, continuation bit 0x80 on all but last
    content section:
              0x00  ::DataSpace/NameList               (60 bytes)
              0x3C  ::DataSpace/.../Transform/List     (38 bytes)
              0x62  ::DataSpace/.../SpanInfo           (8 bytes)
              0x6A  ::DataSpace/.../ControlData        (28 bytes: 6, "LZXC", ...)
              0x86  ::DataSpace/.../ResetTable         (40 + 8 * frames bytes)
              then  the stored file data

NOTES on details that are not in any single public document and that were
established by comparing against hhc.exe output (see the report in
mkchm_selftest.ps1 for how each one is checked):

  * #SYSTEM record 0 must hold the TOC path *without* a leading backslash
    ("toc.hhc"); a leading "\\" or an extra record 6 makes the Contents pane
    report that it cannot find the sitemap.
  * LZX blocks of type 3 (uncompressed) are understood by 7-Zip inside CAB
    files, but 7-Zip's *CHM* handler rejects them ("Unsupported Method"),
    which is why the default storage is the container's plain storage.

Standard library only.  Python 3.8+.
"""

from __future__ import annotations

import argparse
import os
import struct
import sys
import time

PROG = "mkchm"

CHUNK_SIZE = 0x1000        # directory chunk size
FRAME_SIZE = 0x8000        # LZX frame size (uncompressed bytes per frame)

# --- constants exactly as hhc.exe writes them --------------------------------
GUID_ITSF1 = bytes.fromhex("10fd017caa7bd0119e0c00a0c922e6ec")
GUID_ITSF2 = bytes.fromhex("11fd017caa7bd0119e0c00a0c922e6ec")
GUID_ITSP = bytes.fromhex("6a92025d2e21d0119df900a0c922e6ec")
GUID_TRANSFORM = "{7FC28940-9D31-11D0-9B27-00A0C91E9C7C}"

ITSF_HEADER_LEN = 0x60
SECTION1_LEN = 0x18
ITSP_HEADER_LEN = 0x54
PMGL_HEADER_LEN = 20
RESET_TABLE_HEADER_LEN = 0x28
DATA_DIR = 0x78                                   # start of ITSF section 2
DATA_OFF = DATA_DIR + ITSP_HEADER_LEN             # start of directory chunk 0

STORAGE_RAW = "raw"                               # verbatim storage (space 0)
STORAGE_LZX = "lzx"                               # LZX uncompressed blocks (space 1)

# language id -> ANSI code page hhc uses for #SYSTEM strings
LCID_CODEPAGE = {
    0x0409: 1252, 0x0809: 1252, 0x0C09: 1252, 0x0407: 1252, 0x040C: 1252,
    0x0410: 1252, 0x040A: 1252, 0x0413: 1252, 0x0416: 1252, 0x0816: 1252,
    0x041D: 1252, 0x040B: 1252, 0x0406: 1252, 0x0414: 1252, 0x042D: 1252,
    0x0804: 936, 0x1004: 936, 0x0404: 950, 0x0C04: 950, 0x1404: 950,
    0x0411: 932, 0x0412: 949, 0x0424: 1250, 0x0419: 1251, 0x040E: 1250,
    0x0415: 1250, 0x0401: 1256, 0x040D: 1255, 0x0408: 1253, 0x041F: 1254,
    0x0422: 1251, 0x0423: 1251, 0x0425: 1257, 0x0426: 1257, 0x0427: 1257,
}

# #WINDOWS stream for a single plain help window: u32 count = 1, u32 record
# size = 0xC4, then the 0xC4-byte window record (geometry, flags, panes).
_WINDOWS_FIELDS = [
    # (流内偏移, 值) —— 与 hhc.exe 写出的 #WINDOWS 对齐
    (0x00, 0x00000001),   # 窗口记录数
    (0x04, 0x000000C4),   # 每条记录长度
    (0x08, 0x000000C4),   # 记录自身长度
    (0x0C, 0x00000000),
    (0x10, 0x00000001),
    (0x14, 0x00000512),
    (0x18, 0x00023520),
    (0x1C, 0x00000006),
    (0x20, 0x00000000),
    (0x24, 0x00000000),
    (0x28, 0x00000320),   # 左窗格宽
    (0x2C, 0x00000000),
    (0x30, 0x00000640),   # 窗口宽
    (0x34, 0x00000258),   # 窗口高
    (0x68, 0x00000023),   # 导航窗格样式标志
    (0x70, 0x00000030),
    (0x74, 0x00000030),
    (0x78, 0x00003006),
]


def build_windows_stream() -> bytes:
    out = bytearray(204)
    for off, val in _WINDOWS_FIELDS:
        struct.pack_into("<I", out, off, val)
    return bytes(out)


WINDOWS_STREAM = build_windows_stream()


class ChmError(Exception):
    """Fatal, user-facing error."""


# --------------------------------------------------------------------------- #
# primitives
# --------------------------------------------------------------------------- #
def enc_varint(value: int) -> bytes:
    """Big-endian base-128, continuation bit set on every byte but the last."""
    if value < 0:
        raise ChmError("internal error: negative varint %d" % value)
    groups = [value & 0x7F]
    value >>= 7
    while value:
        groups.append(value & 0x7F)
        value >>= 7
    groups.reverse()
    return bytes([g | 0x80 for g in groups[:-1]] + [groups[-1]])


def dec_varint(buf: bytes, pos: int):
    value = 0
    while True:
        if pos >= len(buf):
            raise ChmError("truncated varint")
        byte = buf[pos]
        pos += 1
        value = (value << 7) | (byte & 0x7F)
        if not byte & 0x80:
            return value, pos


def codepage_for_language(lang: int) -> int:
    return LCID_CODEPAGE.get(lang & 0xFFFF, 1252)


def encode_text(text: str, codepage: int) -> bytes:
    try:
        return text.encode("cp%d" % codepage)
    except (LookupError, UnicodeEncodeError) as exc:
        raise ChmError(
            "cannot encode %r with code page %d (%s); pick the matching "
            "--language value" % (text, codepage, exc))


class _WordBitWriter:
    """LZX bit stream: 16-bit little-endian words, bits written MSB first."""

    __slots__ = ("buf", "cur", "nbits")

    def __init__(self) -> None:
        self.buf = bytearray()
        self.cur = 0
        self.nbits = 0

    def write(self, value: int, nbits: int) -> None:
        for shift in range(nbits - 1, -1, -1):
            self.cur = (self.cur << 1) | ((value >> shift) & 1)
            self.nbits += 1
            if self.nbits == 16:
                self.buf += struct.pack("<H", self.cur)
                self.cur = 0
                self.nbits = 0

    def align_word(self) -> None:
        if self.nbits:
            self.cur <<= 16 - self.nbits
            self.buf += struct.pack("<H", self.cur)
            self.cur = 0
            self.nbits = 0

    def getvalue(self) -> bytes:
        if self.nbits:
            raise ChmError("internal error: unaligned LZX bit stream")
        return bytes(self.buf)


# --------------------------------------------------------------------------- #
# LZX "uncompressed block" encoder (used only by --storage lzx)
# --------------------------------------------------------------------------- #
def lzx_uncompressed_block(data: bytes) -> bytes:
    """One LZX block of type 3 (uncompressed) holding `data` (<= FRAME_SIZE)."""
    if len(data) > FRAME_SIZE:
        raise ChmError("internal error: LZX block of %d bytes exceeds one frame"
                       % len(data))
    writer = _WordBitWriter()
    writer.write(3, 3)                # LZX_BLOCKTYPE_UNCOMPRESSED
    writer.align_word()               # uncompressed blocks start word aligned
    header = (writer.getvalue()
              + len(data).to_bytes(3, "little")       # 24-bit block length
              + struct.pack("<III", 1, 1, 1))         # R0, R1, R2
    if len(header) != 17:
        raise ChmError("internal error: LZX block header is %d bytes" % len(header))
    out = bytearray(header)
    out += data
    if len(data) & 1:
        out += b"\x00"                # odd-length blocks are padded to a word
    return bytes(out)


def build_lzx_stream(payload: bytes):
    """Split payload into 32 KiB frames; returns (stream, frame_offsets)."""
    stream = bytearray()
    offsets = []
    if not payload:
        return b"", [0]
    for start in range(0, len(payload), FRAME_SIZE):
        offsets.append(len(stream))
        stream += lzx_uncompressed_block(payload[start:start + FRAME_SIZE])
    return bytes(stream), offsets


# --------------------------------------------------------------------------- #
# ITSF pieces
# --------------------------------------------------------------------------- #
def build_namelist() -> bytes:
    """::DataSpace/NameList: the names of the two container transforms.

    uint16 total size of the count + first record, uint16 count, then for each
    name: uint16 length, UTF-16LE name, uint16 0  (60 bytes in total).
    """
    names = ["Uncompressed", "MSCompressed"]
    first_length = 2 + 2 + len(names[0]) * 2 + 2       # 30 for "Uncompressed"
    out = bytearray(struct.pack("<HH", first_length, len(names)))
    for name in names:
        out += struct.pack("<H", len(name))
        out += name.encode("utf-16-le")
        out += b"\x00\x00"
    if len(out) != 60:
        raise ChmError("internal error: NameList is %d bytes" % len(out))
    return bytes(out)


def build_transform_list() -> bytes:
    """::DataSpace/.../Transform/List: hhc stores 19 UTF-16 chars of the GUID."""
    out = GUID_TRANSFORM[:19].encode("utf-16-le")
    if len(out) != 38:
        raise ChmError("internal error: Transform/List is %d bytes" % len(out))
    return out


def build_control_data() -> bytes:
    """Compression level, LZXC header (v2, reset 2, 128 KiB window, cache 1), 0."""
    return (struct.pack("<I", 6) + b"LZXC" + struct.pack("<IIII", 2, 2, 2, 1)
            + struct.pack("<I", 0))


def build_reset_table(uncompressed_size: int, compressed_size: int, offsets) -> bytes:
    """Frame index for the (possibly empty) compressed stream."""
    out = bytearray(struct.pack("<IIII", 2, len(offsets), 8, RESET_TABLE_HEADER_LEN))
    out += struct.pack("<QQQ", uncompressed_size, compressed_size, FRAME_SIZE)
    for offset in offsets:
        out += struct.pack("<Q", offset)
    return bytes(out)


def build_itsp_header(*, depth: int, index_root: int, index_head: int,
                      index_tail: int, num_chunks: int, language: int) -> bytes:
    out = bytearray(b"ITSP")
    out += struct.pack("<III", 1, ITSP_HEADER_LEN, 0x0A)
    out += struct.pack("<III", CHUNK_SIZE, 2, depth)      # chunk size, density, depth
    out += struct.pack("<i", index_root)                  # PMGI chunk, -1 if none
    out += struct.pack("<II", index_head, index_tail)     # first / last PMGL chunk
    out += struct.pack("<i", -1)
    out += struct.pack("<I", num_chunks)                  # all directory chunks
    out += struct.pack("<I", language & 0xFFFF)
    out += GUID_ITSP
    out += struct.pack("<I", ITSP_HEADER_LEN)
    out += b"\xff" * 12
    if len(out) != ITSP_HEADER_LEN:
        raise ChmError("internal error: ITSP header is %d bytes" % len(out))
    return bytes(out)


def build_itsf_header(*, total_size: int, language: int, dir_off: int,
                      dir_len: int, content_off: int) -> bytes:
    out = bytearray(b"ITSF")
    out += struct.pack("<III", 3, ITSF_HEADER_LEN, 1)     # version, header len, ?
    out += struct.pack("<I", int(time.time()) & 0xFFFFFFFF)
    out += struct.pack("<I", language & 0xFFFF)
    out += GUID_ITSF1
    out += GUID_ITSF2
    out += struct.pack("<Q", ITSF_HEADER_LEN)             # section 1 offset
    out += struct.pack("<Q", SECTION1_LEN)                # section 1 length
    out += struct.pack("<Q", dir_off)                     # section 2: directory
    out += struct.pack("<Q", dir_len)
    out += struct.pack("<Q", content_off)                 # content section
    if len(out) != ITSF_HEADER_LEN:
        raise ChmError("internal error: ITSF header is %d bytes" % len(out))
    return bytes(out)


def build_idxhdr(topic_count: int) -> bytes:
    """真正的 #IDXHDR：4096 字节，"T#SM" 开头的索引头。

    布局按 hhc.exe 的实际产物（逐字节比对过 Windows 自带 CHM）：
        0x00 "T#SM"   0x04 时间戳    0x08 1       0x0C 主题数
        0x10 0        0x14 -1        0x18 0       0x1C 1
        0x20 -1       0x24 -1        0x28 -1      0x2C -1
        0x30-0x3C -1  0x40 0         0x44 1       其余 0
    以前这里写的是「第一个 PMGL 目录块」的副本，Win98 的 HTML Help 解析时直接崩。
    """
    out = bytearray(CHUNK_SIZE)
    out[0:4] = b"T#SM"
    struct.pack_into("<I", out, 0x04, int(time.time()) & 0xFFFFFFFF)
    struct.pack_into("<I", out, 0x08, 1)
    struct.pack_into("<I", out, 0x0C, topic_count & 0xFFFFFFFF)
    struct.pack_into("<I", out, 0x10, 0)
    for off in (0x14, 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C):
        struct.pack_into("<I", out, off, 0xFFFFFFFF)
    struct.pack_into("<I", out, 0x18, 0)
    struct.pack_into("<I", out, 0x1C, 1)
    struct.pack_into("<I", out, 0x40, 0)
    struct.pack_into("<I", out, 0x44, 1)
    return bytes(out)


def build_section1(total_size: int) -> bytes:
    return struct.pack("<QQQ", 0x1FE, total_size, 0)


# --------------------------------------------------------------------------- #
# directory
# --------------------------------------------------------------------------- #
class Entry:
    __slots__ = ("name", "space", "offset", "length")

    def __init__(self, name: str, space: int, offset: int, length: int) -> None:
        self.name = name
        self.space = space
        self.offset = offset
        self.length = length

    def encoded(self) -> bytes:
        name = self.name.encode("utf-8")
        if not 0 < len(name) < 256:
            raise ChmError("internal file name %r cannot be stored (length %d)"
                           % (self.name, len(name)))
        return (bytes([len(name)]) + name + enc_varint(self.space)
                + enc_varint(self.offset) + enc_varint(self.length))

    def sort_key(self):
        # hhc keeps the directory sorted case-insensitively
        return (self.name.lower(), self.name)


def build_pmgl_chunk(entries, prev_chunk: int, next_chunk: int) -> bytes:
    body = bytearray()
    for entry in entries:
        body += entry.encoded()
    used = PMGL_HEADER_LEN + len(body)
    if used > CHUNK_SIZE:
        raise ChmError("internal error: PMGL chunk overflow (%d bytes)" % used)
    out = bytearray(b"PMGL")
    out += struct.pack("<III", CHUNK_SIZE - used, 0, prev_chunk & 0xFFFFFFFF)
    out += struct.pack("<I", next_chunk & 0xFFFFFFFF)
    out += body
    out += b"\x00" * (CHUNK_SIZE - used)
    return bytes(out)


def build_pmgi_chunk(items) -> bytes:
    """items: [(chunk_index, first_name_in_that_chunk), ...]"""
    body = bytearray()
    for index, name in items:
        name_bytes = name.encode("utf-8")
        body += bytes([len(name_bytes)]) + name_bytes + enc_varint(index)
    used = 8 + len(body)
    if used > CHUNK_SIZE:
        raise ChmError(
            "the document set needs %d directory chunks, which does not fit in "
            "one PMGI index chunk; split it into several .chm files" % len(items))
    out = bytearray(b"PMGI")
    out += struct.pack("<I", CHUNK_SIZE - used)
    out += body
    out += b"\x00" * (CHUNK_SIZE - used)
    return bytes(out)


def build_directory(entries):
    """Returns (directory_bytes, nchunks, depth, index_root, first, last)."""
    ordered = sorted(entries, key=Entry.sort_key)
    groups = []
    current = []
    used = PMGL_HEADER_LEN
    for entry in ordered:
        size = len(entry.encoded())
        if PMGL_HEADER_LEN + size > CHUNK_SIZE:
            raise ChmError(
                "directory entry %r needs %d bytes, which does not fit in a "
                "%d-byte chunk" % (entry.name, size, CHUNK_SIZE))
        if used + size > CHUNK_SIZE:
            groups.append(current)
            current = []
            used = PMGL_HEADER_LEN
        current.append(entry)
        used += size
    if current or not groups:
        groups.append(current)

    if len(groups) == 1:
        return build_pmgl_chunk(groups[0], -1, -1), 1, 1, -1, 0, 0

    chunks = []
    for index, group in enumerate(groups):
        chunks.append(build_pmgl_chunk(
            group, index - 1 if index else -1,
            index + 1 if index + 1 < len(groups) else -1))
    pmgi = build_pmgi_chunk([(i, g[0].name) for i, g in enumerate(groups)])
    return b"".join(chunks) + pmgi, len(groups) + 1, 2, len(groups), 0, len(groups) - 1


# --------------------------------------------------------------------------- #
# #SYSTEM
# --------------------------------------------------------------------------- #
def build_system_stream(*, title, default_topic, toc_name, index_name,
                        codepage, language, idxhdr) -> bytes:
    """#SYSTEM: uint32 version 3, then {uint16 code, uint16 length, bytes}.

    Codes as hhc writes them (confirmed against the CHMs shipped with Windows):
        0  contents file (.hhc) inside the CHM      9  compiler version
        1  index file (.hhk) inside the CHM        10  compile timestamp
        2  default topic                           12  zero
        3  title                                   13  copy of #IDXHDR
        4  compiled-file info                      15  zero
        5  window name
    Code 6 ("compiled TOC base name") is deliberately not written: it makes
    hh.exe look for a compiled binary TOC that this tool does not produce.
    """
    def record(code: int, payload: bytes) -> bytes:
        if len(payload) > 0xFFFF:
            raise ChmError("#SYSTEM record %d is %d bytes, the limit is 65535"
                           % (code, len(payload)))
        return struct.pack("<HH", code, len(payload)) + payload

    def text(value: str) -> bytes:
        return encode_text(value, codepage) + b"\x00"

    out = bytearray(struct.pack("<I", 3))
    out += record(10, struct.pack("<I", int(time.time()) & 0xFFFFFFFF))
    out += record(9, b"HHA Version 4.74.8702\x00")
    info = bytearray(struct.pack("<I", language & 0xFFFF))
    info += struct.pack("<IIII", 0, 1, 0, 0)
    info += struct.pack("<Q", 0x01CFAB4C48DF85F1)        # compile time (arbitrary)
    info += struct.pack("<II", 0, 0)
    if len(info) != 36:
        raise ChmError("internal error: #SYSTEM code 4 record is %d bytes" % len(info))
    out += record(4, bytes(info))
    out += record(2, text(default_topic))
    out += record(3, text(title))
    out += record(5, text("Main"))
    out += record(12, struct.pack("<I", 0))
    out += record(13, idxhdr)
    out += record(15, struct.pack("<I", 0))
    if toc_name:
        out += record(0, text(toc_name))
    if index_name:
        out += record(1, text(index_name))
    return bytes(out)


# --------------------------------------------------------------------------- #
# builder
# --------------------------------------------------------------------------- #
def collect_sources(src: str):
    if not os.path.isdir(src):
        raise ChmError("source directory not found: %s" % src)
    files = {}
    for root, dirs, names in os.walk(src):
        dirs.sort()
        for name in sorted(names):
            full = os.path.join(root, name)
            rel = os.path.relpath(full, src).replace(os.sep, "/")
            if rel.startswith("../") or rel == "..":
                raise ChmError("file outside the source tree: %s" % full)
            try:
                with open(full, "rb") as handle:
                    files[rel] = handle.read()
            except OSError as exc:
                raise ChmError("cannot read %s: %s" % (full, exc))
    if not files:
        raise ChmError("no files found below %s" % src)
    return files


class ChmBuilder:
    def __init__(self, *, title, default_topic, toc=None, index=None,
                 language=0x0804, storage=STORAGE_RAW):
        self.title = title
        self.default_topic = default_topic.replace("\\", "/").lstrip("/")
        self.toc = toc
        self.index = index
        self.language = language
        self.storage = storage
        self.codepage = codepage_for_language(language)

    # -- one complete layout for a given #IDXHDR content --------------------
    def _layout(self, files, toc_name, index_name, idxhdr):
        system_stream = build_system_stream(
            title=self.title, default_topic=self.default_topic,
            toc_name=toc_name, index_name=index_name, codepage=self.codepage,
            language=self.language, idxhdr=idxhdr)

        # files carried by the container, in write order
        items = [
            ("/#IDXHDR", idxhdr),
            ("/#IVB", b""),            # 信息类型位图（空）
            ("/#ITBITS", b""),         # 信息类型（空）
            ("/#STRINGS", b"\x00"),
            ("/#TOPICS", b""),
            ("/#URLSTR", b""),
            ("/#URLTBL", b""),
            ("/#WINDOWS", WINDOWS_STREAM),
            ("/#SYSTEM", system_stream),
        ]
        items += [("/" + rel, files[rel]) for rel in sorted(files, key=str.lower)]

        payload = b"".join(data for _, data in items)
        sizes = {name: len(data) for name, data in items}
        stream_offsets = {}
        cursor = 0
        for name, data in items:
            stream_offsets[name] = cursor
            cursor += len(data)
        if cursor != len(payload):
            raise ChmError("internal error: content stream size mismatch")

        # ::DataSpace metadata that precedes the stored data
        namelist = build_namelist()
        transform = build_transform_list()
        spaninfo = struct.pack("<Q", len(payload))
        controldata = build_control_data()
        if self.storage == STORAGE_LZX:
            lzx_stream, frame_offsets = build_lzx_stream(payload)
        else:
            # nothing is compressed, but the frame table must still describe
            # ceil(size / 32 KiB) frames or readers reject the file
            lzx_stream = b""
            frame_offsets = [0] * max(1, (len(payload) + FRAME_SIZE - 1) // FRAME_SIZE)
        reset_table = build_reset_table(len(payload), len(lzx_stream), frame_offsets)

        ds = {}
        ds["namelist"] = 0
        ds["transform"] = len(namelist)
        ds["spaninfo"] = ds["transform"] + len(transform)
        ds["controldata"] = ds["spaninfo"] + len(spaninfo)
        ds["resettable"] = ds["controldata"] + len(controldata)
        ds["content"] = ds["resettable"] + len(reset_table)
        metadata = namelist + transform + spaninfo + controldata + reset_table
        if len(metadata) != ds["content"]:
            raise ChmError("internal error: DataSpace metadata size mismatch")

        if self.storage == STORAGE_LZX:
            space = 1
            offsets = dict(stream_offsets)
            content = metadata + lzx_stream
            content_length = len(lzx_stream)
        else:
            space = 0
            offsets = {name: ds["content"] + off for name, off in stream_offsets.items()}
            content = metadata + payload
            content_length = 0     # nothing is stored in the compressed stream

        entries = [Entry("/", 0, 0, 0)]
        seen = set()
        for name in stream_offsets:
            parts = name.split("/")[1:-1]
            for depth in range(len(parts)):
                folder = "/" + "/".join(parts[:depth + 1]) + "/"
                if folder not in seen:
                    seen.add(folder)
                    entries.append(Entry(folder, 0, 0, 0))
        for name in stream_offsets:
            entries.append(Entry(name, space, offsets[name], sizes[name]))
        entries += [
            Entry("::DataSpace/NameList", 0, ds["namelist"], len(namelist)),
            Entry("::DataSpace/Storage/MSCompressed/Content", 0,
                  ds["content"], content_length),
            Entry("::DataSpace/Storage/MSCompressed/ControlData", 0,
                  ds["controldata"], len(controldata)),
            Entry("::DataSpace/Storage/MSCompressed/SpanInfo", 0,
                  ds["spaninfo"], len(spaninfo)),
            Entry("::DataSpace/Storage/MSCompressed/Transform/List", 0,
                  ds["transform"], len(transform)),
            Entry("::DataSpace/Storage/MSCompressed/Transform/%s/InstanceData/"
                  % GUID_TRANSFORM, 0, 0, 0),
            Entry("::DataSpace/Storage/MSCompressed/Transform/%s/InstanceData/"
                  "ResetTable" % GUID_TRANSFORM, 0, ds["resettable"], len(reset_table)),
        ]

        directory, nchunks, depth, root, first, last = build_directory(entries)
        if len(directory) != nchunks * CHUNK_SIZE:
            raise ChmError("internal error: directory size mismatch")

        return {
            "entries": entries, "directory": directory, "nchunks": nchunks,
            "depth": depth, "root": root, "first": first, "last": last,
            "content": content, "uncompressed": len(payload),
            "lzx_len": len(lzx_stream), "frames": len(frame_offsets),
            "system_size": len(system_stream), "chunk0": directory[:CHUNK_SIZE],
        }

    def build(self, src, out_path):
        files = collect_sources(src)

        toc_name = index_name = None
        if self.toc:
            toc_name = self.toc.replace("\\", "/").lstrip("/")
            if toc_name not in files:
                raise ChmError("toc file %r is not among the files below %s"
                               % (self.toc, src))
        if self.index:
            index_name = self.index.replace("\\", "/").lstrip("/")
            if index_name not in files:
                raise ChmError("index file %r is not among the files below %s"
                               % (self.index, src))
        if self.default_topic not in files:
            raise ChmError(
                "default topic %r is not among the files below %s (found: %s)"
                % (self.default_topic, src, ", ".join(sorted(files)[:8])))

        # #IDXHDR 是独立的 4096 字节索引头（"T#SM"），不是目录块的副本。
        topic_count = sum(1 for f in files
                          if f.lower().endswith((".htm", ".html")))
        idxhdr = build_idxhdr(topic_count)
        layout = self._layout(files, toc_name, index_name, idxhdr)

        dir_off = DATA_DIR
        dir_len = ITSP_HEADER_LEN + len(layout["directory"])
        content_off = dir_off + dir_len
        total_size = content_off + len(layout["content"])

        blob = bytearray()
        blob += build_itsf_header(total_size=total_size, language=self.language,
                                  dir_off=dir_off, dir_len=dir_len,
                                  content_off=content_off)
        blob += build_section1(total_size)
        blob += build_itsp_header(depth=layout["depth"], index_root=layout["root"],
                                  index_head=layout["first"], index_tail=layout["last"],
                                  num_chunks=layout["nchunks"],
                                  language=self.language)
        blob += layout["directory"]
        blob += layout["content"]
        if len(blob) != total_size:
            raise ChmError("internal error: assembled %d bytes, header says %d"
                           % (len(blob), total_size))

        out_dir = os.path.dirname(os.path.abspath(out_path))
        if not os.path.isdir(out_dir):
            raise ChmError("output directory does not exist: %s" % out_dir)
        try:
            with open(out_path, "wb") as handle:
                handle.write(blob)
        except OSError as exc:
            raise ChmError("cannot write %s: %s" % (out_path, exc))

        return {
            "path": os.path.abspath(out_path), "size": len(blob),
            "dir_off": dir_off, "dir_len": dir_len,
            "dir_chunks": layout["nchunks"], "depth": layout["depth"],
            "content_off": content_off, "content_len": len(layout["content"]),
            "lzx_len": layout["lzx_len"], "uncompressed": layout["uncompressed"],
            "frames": layout["frames"],
            "entries": sorted(layout["entries"], key=Entry.sort_key),
            "system_size": layout["system_size"], "files": len(files),
            "language": self.language, "codepage": self.codepage,
            "toc": toc_name, "index": index_name, "default": self.default_topic,
            "title": self.title, "storage": self.storage,
        }


# --------------------------------------------------------------------------- #
# reading / inspection
# --------------------------------------------------------------------------- #
def read_structure(path):
    with open(path, "rb") as handle:
        data = handle.read()
    if data[:4] != b"ITSF":
        raise ChmError("%s: not an ITSF container (bad signature)" % path)
    u32 = lambda offset: struct.unpack_from("<I", data, offset)[0]
    u64 = lambda offset: struct.unpack_from("<Q", data, offset)[0]
    info = {
        "size": len(data), "version": u32(0x04), "header_len": u32(0x08),
        "language": u32(0x14), "dir_off": u64(0x48), "dir_len": u64(0x50),
        "content_off": u64(0x58), "declared_size": u64(0x68),
        "section1": (u64(0x60), u64(0x68), u64(0x70)),
    }
    if info["size"] != info["declared_size"]:
        raise ChmError("%s: file is %d bytes but the header claims %d"
                       % (path, info["size"], info["declared_size"]))
    base = info["dir_off"]
    if data[base:base + 4] != b"ITSP":
        raise ChmError("%s: directory section does not start with 'ITSP'" % path)
    itsp = data[base:base + ITSP_HEADER_LEN]
    info["itsp"] = {
        "version": struct.unpack_from("<I", itsp, 0x04)[0],
        "header_len": struct.unpack_from("<I", itsp, 0x08)[0],
        "chunk_size": struct.unpack_from("<I", itsp, 0x10)[0],
        "density": struct.unpack_from("<I", itsp, 0x14)[0],
        "depth": struct.unpack_from("<I", itsp, 0x18)[0],
        "index_root": struct.unpack_from("<i", itsp, 0x1C)[0],
        "index_head": struct.unpack_from("<I", itsp, 0x20)[0],
        "index_tail": struct.unpack_from("<I", itsp, 0x24)[0],
        "num_chunks": struct.unpack_from("<I", itsp, 0x2C)[0],
        "language": struct.unpack_from("<I", itsp, 0x30)[0],
        "guid": itsp[0x34:0x44].hex(),
    }
    chunk_base = base + ITSP_HEADER_LEN
    nchunks = (info["dir_len"] - ITSP_HEADER_LEN) // CHUNK_SIZE
    info["dir_chunks"] = nchunks
    entries = []
    for index in range(nchunks):
        chunk = data[chunk_base + index * CHUNK_SIZE:
                     chunk_base + (index + 1) * CHUNK_SIZE]
        if chunk[:4] == b"PMGL":
            free = struct.unpack_from("<I", chunk, 4)[0]
            pos, end = PMGL_HEADER_LEN, CHUNK_SIZE - free
            while pos < end:
                nlen = chunk[pos]
                pos += 1
                name = chunk[pos:pos + nlen].decode("utf-8", "replace")
                pos += nlen
                space, pos = dec_varint(chunk, pos)
                offset, pos = dec_varint(chunk, pos)
                length, pos = dec_varint(chunk, pos)
                entries.append((name, space, offset, length))
            if pos != end:
                raise ChmError("%s: PMGL chunk %d overran its used area"
                               % (path, index))
        elif chunk[:4] == b"PMGI":
            continue
        else:
            raise ChmError("%s: bad directory chunk %d (%r)" % (path, index, chunk[:4]))
    info["entries"] = entries

    content = data[info["content_off"]:]
    info["content_len"] = len(content)
    lzxc = content.find(b"LZXC")
    info["lzxc"] = None
    if lzxc >= 0:
        info["lzxc"] = {
            "offset": lzxc,
            "version": struct.unpack_from("<I", content, lzxc + 4)[0],
            "reset_interval": struct.unpack_from("<I", content, lzxc + 8)[0],
            "window": struct.unpack_from("<I", content, lzxc + 12)[0],
            "cache": struct.unpack_from("<I", content, lzxc + 16)[0],
        }
    for name, space, offset, length in entries:
        if name.endswith("/InstanceData/ResetTable"):
            raw = content[offset:offset + length]
            if len(raw) >= RESET_TABLE_HEADER_LEN:
                nframes = struct.unpack_from("<I", raw, 4)[0]
                info["reset_table"] = {
                    "version": struct.unpack_from("<I", raw, 0)[0],
                    "frames": nframes,
                    "entry_size": struct.unpack_from("<I", raw, 8)[0],
                    "header_len": struct.unpack_from("<I", raw, 12)[0],
                    "uncompressed": struct.unpack_from("<Q", raw, 16)[0],
                    "compressed": struct.unpack_from("<Q", raw, 24)[0],
                    "frame_size": struct.unpack_from("<Q", raw, 32)[0],
                    "offsets": [struct.unpack_from("<Q", raw, 40 + 8 * i)[0]
                                for i in range(nframes)
                                if 40 + 8 * i + 8 <= len(raw)],
                }
            break
    return info


def dump_structure(path, out=sys.stdout):
    info = read_structure(path)
    w = out.write
    w("file          : %s\n" % path)
    w("size          : %d bytes (0x%x)\n" % (info["size"], info["size"]))
    w("ITSF          : version %d, header 0x%x, language 0x%04x\n"
      % (info["version"], info["header_len"], info["language"]))
    w("section 1     : {0x%x, %d, %d}\n" % info["section1"])
    w("directory     : offset 0x%x, %d bytes, %d chunk(s)\n"
      % (info["dir_off"], info["dir_len"], info["dir_chunks"]))
    itsp = info["itsp"]
    w("  ITSP        : v%d, header 0x%x, chunk size 0x%x, density %d, depth %d\n"
      % (itsp["version"], itsp["header_len"], itsp["chunk_size"],
         itsp["density"], itsp["depth"]))
    w("  chunks      : root %d, PMGL %d..%d, total %d, language 0x%04x\n"
      % (itsp["index_root"], itsp["index_head"], itsp["index_tail"],
         itsp["num_chunks"], itsp["language"]))
    w("  guid        : %s\n" % itsp["guid"])
    w("content       : offset 0x%x, %d bytes\n"
      % (info["content_off"], info["content_len"]))
    if info["lzxc"]:
        lzxc = info["lzxc"]
        w("  LZXC        : at +0x%x, v%d, reset interval %d, window %d, cache %d\n"
          % (lzxc["offset"], lzxc["version"], lzxc["reset_interval"],
             lzxc["window"], lzxc["cache"]))
    if "reset_table" in info:
        table = info["reset_table"]
        w("  reset table : %d frame(s) of %d bytes, %d bytes stored, "
          "%d bytes compressed\n"
          % (table["frames"], table["frame_size"], table["uncompressed"],
             table["compressed"]))
    w("entries (%d)\n" % len(info["entries"]))
    for name, space, offset, length in info["entries"]:
        w("  %-78s space=%d offset=%-8d length=%d\n" % (name, space, offset, length))


def print_summary(result, out=sys.stdout):
    w = out.write
    w("\nmkchm: %s\n" % result["path"])
    w("  title          : %s\n" % result["title"])
    w("  default topic  : %s\n" % result["default"])
    w("  contents (toc) : %s\n" % (result["toc"] or "-"))
    w("  index          : %s\n" % (result["index"] or "-"))
    w("  language       : 0x%04x (code page %d)\n"
      % (result["language"], result["codepage"]))
    w("  storage        : %s\n"
      % ("verbatim (ITSF space 0)" if result["storage"] == STORAGE_RAW
         else "LZX uncompressed blocks (space 1)"))
    w("  source files   : %d\n" % result["files"])
    w("  total size     : %d bytes (0x%x)\n" % (result["size"], result["size"]))
    w("  directory      : 0x%x (%d bytes, %d chunk(s), depth %d)\n"
      % (result["dir_off"], result["dir_len"], result["dir_chunks"],
         result["depth"]))
    w("  content        : 0x%x (%d bytes)\n"
      % (result["content_off"], result["content_len"]))
    w("  stored data    : %d bytes in %d frame(s), %d bytes of LZX stream\n"
      % (result["uncompressed"], result["frames"], result["lzx_len"]))
    w("  #SYSTEM        : %d bytes\n" % result["system_size"])
    w("\n  %-4s %-9s %-10s %s\n" % ("sp", "offset", "length", "name"))
    w("  %s\n" % ("-" * 84))
    for entry in result["entries"]:
        w("  %-4d %-9d %-10d %s\n"
          % (entry.space, entry.offset, entry.length, entry.name))
    w("")


# --------------------------------------------------------------------------- #
# CLI
# --------------------------------------------------------------------------- #
def main(argv=None):
    parser = argparse.ArgumentParser(
        prog=PROG,
        description="Compile a directory tree into a Microsoft HTML Help (.chm) file.",
        epilog="Files are stored verbatim (ITSF space 0), so nothing is "
               "compressed and internal files keep their exact bytes.")
    parser.add_argument("--src", help="directory whose files become CHM internal files")
    parser.add_argument("--out", help="output .chm path")
    parser.add_argument("--title", help='help window title, e.g. "产品文档"')
    parser.add_argument("--default", help="default topic, relative to --src")
    parser.add_argument("--toc", help="sitemap TOC (.hhc), relative to --src")
    parser.add_argument("--index", help="sitemap index (.hhk), relative to --src")
    parser.add_argument("--language", default="0x0804",
                        help="ITSF language id, decimal or 0x-hex (default 0x0804 "
                             "= Chinese Simplified, code page 936)")
    parser.add_argument("--storage", choices=(STORAGE_RAW, STORAGE_LZX),
                        default=STORAGE_RAW,
                        help="raw: verbatim storage, understood by 7-Zip and "
                             "hh.exe (default); lzx: LZX uncompressed blocks, "
                             "understood by hh.exe but rejected by 7-Zip's CHM "
                             "handler")
    parser.add_argument("--dump", metavar="CHM",
                        help="print the structure of an existing .chm and exit")
    parser.add_argument("--verbose", action="store_true",
                        help="print the full entry table after compiling")
    args = parser.parse_args(argv)

    try:
        if args.dump:
            dump_structure(args.dump)
            return 0
        missing = [name for name in ("src", "out", "title", "default")
                   if not getattr(args, name)]
        if missing:
            parser.error("missing required option(s): %s"
                         % ", ".join("--" + name for name in missing))
        try:
            language = int(args.language, 0)
        except ValueError:
            raise ChmError("invalid --language value: %r" % args.language)
        if not 0 <= language <= 0xFFFF:
            raise ChmError("--language must be a 16-bit value such as 0x0804")

        builder = ChmBuilder(title=args.title, default_topic=args.default,
                             toc=args.toc, index=args.index, language=language,
                             storage=args.storage)
        result = builder.build(args.src, args.out)

        # re-read what was written: catches almost every structural mistake
        info = read_structure(result["path"])
        if len(info["entries"]) != len(result["entries"]):
            raise ChmError("self-check failed: wrote %d directory entries, "
                           "read back %d"
                           % (len(result["entries"]), len(info["entries"])))
        if "reset_table" in info:
            stored = info["reset_table"]["uncompressed"]
            if stored != result["uncompressed"]:
                raise ChmError("self-check failed: reset table says %d bytes, "
                               "wrote %d" % (stored, result["uncompressed"]))

        if args.verbose:
            print_summary(result)
        else:
            print("mkchm: wrote %s (%d bytes, %d files, %d directory chunk(s), "
                  "%d bytes of data)"
                  % (result["path"], result["size"], result["files"],
                     result["dir_chunks"], result["uncompressed"]))
        return 0
    except ChmError as exc:
        sys.stderr.write("%s: error: %s\n" % (PROG, exc))
        return 1


if __name__ == "__main__":
    sys.exit(main())
