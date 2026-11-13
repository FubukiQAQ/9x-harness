#!/usr/bin/env python3
"""
mkiso.py - build an ISO 9660 CD-ROM image (with an optional Joliet tree).

Target: masters for a "Windows XP era" software CD.  Pure Python standard
library only.  Deterministic: identical input trees always produce
byte-identical images.

What is produced
----------------
  * 2048 byte logical sectors, 16 empty sectors of system area (32 KiB).
  * Primary Volume Descriptor (type 1) at LBA 16.
  * Supplementary Volume Descriptor (type 2, Joliet) at LBA 17.  Names are
    UCS-2 big endian, escape sequences %/E @/E ^/E (UCS-2 level 3).
  * Volume Descriptor Set Terminator (type 255).
  * L and M path tables (little endian / big endian) for both trees.
  * One directory extent per directory in each tree, consisting of
    "." and ".." records plus the sorted children.  Records never straddle a
    sector boundary; odd-length records get their mandatory padding byte.
  * Primary tree is strict ISO 9660 level 1: uppercase 8.3 identifiers,
    ';1' file version suffix, directory entries flagged 0x02.
  * Joliet tree: long names, no version suffix, sorted by code point.
  * Every 16/32 bit volume field is stored both-endian (LSB then MSB).

CLI
---
  py mkiso.py --src <dir> --out <x.iso> --label SMARTMATE2000
              [--no-joliet] [--date 2003-06-01] [--boot-image none]

Exit status: 0 on success, 2 on a user error (reported on stderr).
"""

from __future__ import annotations

import argparse
import os
import struct
import sys

# --------------------------------------------------------------------------- #
# Constants
# --------------------------------------------------------------------------- #

SECTOR_SIZE = 2048
SYSTEM_AREA_SECTORS = 16          # LBA 0..15 are reserved for a boot loader
PVD_LBA = 16

VD_PRIMARY = 1
VD_SUPPLEMENTARY = 2
VD_TERMINATOR = 255

FLAG_DIRECTORY = 0x02

#: Largest file we are willing to put into a single extent.  The primary
#: volume descriptor caps a file at 4 GiB - 1 and ISO 9660 without
#: multi-extent files cannot describe anything larger anyway; the task also
#: asks for a clean failure above 2 GiB, which is what we enforce here.
MAX_FILE_SIZE = 2 * 1024 ** 3

#: Joliet: a name is limited to 64 UCS-2 characters == 128 bytes.
MAX_JOLIET_NAME_CHARS = 64

#: ISO 9660 level 1 directory identifier budget.
MAX_ISO_DIR_NAME = 8
MAX_ISO_FILE_STEM = 8
MAX_ISO_FILE_EXT = 3

#: ISO 9660 allows at most 8 hierarchy levels (root = level 1).
MAX_HIERARCHY_LEVELS = 8

#: Characters allowed in a level 1 "d-character" file/directory identifier.
D_CHARACTERS = frozenset("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")

#: Characters that must never appear in a Joliet name (illegal on Windows).
JOLIET_FORBIDDEN = frozenset('/\\:*?"<>|')

UNSPECIFIED_DATE_17 = b"0" * 16 + b"\x00"


class IsoError(Exception):
    """A problem the user can fix (bad input tree, bad argument, ...)."""


# --------------------------------------------------------------------------- #
# Small encoding helpers
# --------------------------------------------------------------------------- #

def sectors_for(nbytes: int) -> int:
    """Number of 2048 byte sectors needed to hold *nbytes*."""
    return (nbytes + SECTOR_SIZE - 1) // SECTOR_SIZE


def pad_to_sector(data: bytes) -> bytes:
    """Zero-pad *data* up to the next sector boundary."""
    rest = len(data) % SECTOR_SIZE
    if rest == 0:
        return data
    return data + b"\x00" * (SECTOR_SIZE - rest)


def both16(value: int) -> bytes:
    """16 bit field stored little endian then big endian (ISO 9660 7.2.3)."""
    return struct.pack("<H", value) + struct.pack(">H", value)


def both32(value: int) -> bytes:
    """32 bit field stored little endian then big endian (ISO 9660 7.3.3)."""
    return struct.pack("<I", value) + struct.pack(">I", value)


class Stamp:
    """A fixed point in time used for every timestamp in the image.

    Using one caller supplied stamp (instead of the timestamps of the source
    files) is what makes the output reproducible.
    """

    __slots__ = ("year", "month", "day", "hour", "minute", "second", "tz")

    def __init__(self, year, month, day, hour=0, minute=0, second=0, tz=0):
        self.year = year
        self.month = month
        self.day = day
        self.hour = hour
        self.minute = minute
        self.second = second
        self.tz = tz          # offset from GMT in 15 minute units

    def record_7(self) -> bytes:
        """The 7 byte 'recording date and time' of a directory record."""
        if not 1900 <= self.year <= 2155:
            raise IsoError("year %d cannot be encoded in a 7 byte timestamp"
                           % self.year)
        return bytes((self.year - 1900, self.month, self.day,
                      self.hour, self.minute, self.second, self.tz & 0xFF))

    def description_17(self) -> bytes:
        """The 17 byte 'date and time' used in volume descriptors."""
        text = "%04d%02d%02d%02d%02d%02d00" % (
            self.year, self.month, self.day,
            self.hour, self.minute, self.second)
        return text.encode("ascii") + bytes((self.tz & 0xFF,))

    @classmethod
    def parse(cls, text: str) -> "Stamp":
        """Accept 2003-06-01, 2003-06-01T12:34:56 or 2003-06-01 12:34:56."""
        raw = text.strip().replace("T", " ").replace("t", " ")
        date_part, _, time_part = raw.partition(" ")
        try:
            y, m, d = (int(x) for x in date_part.split("-"))
        except ValueError:
            raise IsoError("--date must look like YYYY-MM-DD, got %r" % text)
        hour = minute = second = 0
        if time_part:
            try:
                bits = time_part.split(":")
                hour = int(bits[0])
                minute = int(bits[1]) if len(bits) > 1 else 0
                second = int(bits[2]) if len(bits) > 2 else 0
            except (ValueError, IndexError):
                raise IsoError("--date time must look like HH:MM:SS, got %r"
                               % text)
        if not (1 <= m <= 12 and 1 <= d <= 31):
            raise IsoError("--date is not a valid calendar date: %r" % text)
        if not (0 <= hour < 24 and 0 <= minute < 60 and 0 <= second < 61):
            raise IsoError("--date has an out of range time: %r" % text)
        return cls(y, m, d, hour, minute, second, 0)


# --------------------------------------------------------------------------- #
# The source tree
# --------------------------------------------------------------------------- #

class Node:
    """One file or directory of the source tree, plus its ISO bookkeeping."""

    __slots__ = (
        "name", "is_dir", "children", "size", "src_path", "parent",
        "iso_id", "joliet_id",
        "iso_children", "joliet_children",
        "iso_index", "joliet_index",
        "iso_extent", "iso_dir_size", "joliet_extent", "joliet_dir_size",
        "extent",
    )

    def __init__(self, name, is_dir, src_path):
        self.name = name
        self.is_dir = is_dir
        self.children = []
        self.size = 0
        self.src_path = src_path
        self.parent = None
        # Identifier bytes as stored in a directory record (files carry ";1"
        # in the primary tree).  Directories use the bare name.
        self.iso_id = b""
        self.joliet_id = b""
        self.iso_children = []
        self.joliet_children = []
        self.iso_index = 0
        self.joliet_index = 0
        self.iso_extent = 0
        self.iso_dir_size = 0
        self.joliet_extent = 0
        self.joliet_dir_size = 0
        self.extent = 0


def scan_tree(src_root: str) -> Node:
    """Read *src_root* into memory.  Order is already deterministic."""
    if not os.path.isdir(src_root):
        raise IsoError("source directory not found: %s" % src_root)

    def walk(abs_path: str, name: str) -> Node:
        node = Node(name, True, abs_path)
        try:
            entries = list(os.scandir(abs_path))
        except OSError as exc:
            raise IsoError("cannot list %s: %s" % (abs_path, exc))
        # Deterministic: sort by uppercase name, then by exact name.
        entries.sort(key=lambda e: (e.name.upper(), e.name))
        for entry in entries:
            try:
                if entry.is_symlink():
                    raise IsoError(
                        "symbolic links are not supported: %s" % entry.path)
                if entry.is_dir(follow_symlinks=False):
                    child = walk(entry.path, entry.name)
                elif entry.is_file(follow_symlinks=False):
                    child = Node(entry.name, False, entry.path)
                    child.size = entry.stat(follow_symlinks=False).st_size
                    if child.size > MAX_FILE_SIZE:
                        raise IsoError(
                            "%s is %d bytes; this builder supports at most "
                            "2 GiB per file (no multi-extent support)"
                            % (entry.path, child.size))
                else:
                    raise IsoError("unsupported entry type: %s" % entry.path)
            except OSError as exc:
                raise IsoError("cannot stat %s: %s" % (entry.path, exc))
            child.parent = node
            node.children.append(child)
        return node

    return walk(os.path.abspath(src_root), "")


# --------------------------------------------------------------------------- #
# Identifier generation
# --------------------------------------------------------------------------- #

def _iso_sanitize(text: str) -> str:
    """Map every character onto a level 1 d-character."""
    out = []
    for ch in text.upper():
        out.append(ch if ch in D_CHARACTERS else "_")
    return "".join(out)


def iso_identifier(name: str, is_dir: bool) -> str:
    """Build the level 1 identifier (without the ';1' version suffix)."""
    upper = name.upper()
    if is_dir:
        # Directory identifiers carry no version and no extension in level 1.
        return _iso_sanitize(upper)[:MAX_ISO_DIR_NAME] or "_"

    if "." in upper.lstrip("."):
        stem, _, ext = upper.rpartition(".")
    else:
        stem, ext = upper, ""
    stem = _iso_sanitize(stem)[:MAX_ISO_FILE_STEM] or "_"
    ext = _iso_sanitize(ext)[:MAX_ISO_FILE_EXT]
    return stem + "." + ext if ext else stem


def joliet_identifier(name: str) -> str:
    """Build a Joliet name: keep case and spaces, drop illegal characters."""
    chars = []
    for ch in name:
        code = ord(ch)
        # UCS-2 only: no surrogates, no control characters.
        if 0xD800 <= code <= 0xDFFF or code < 0x20 or code in (0x7F,):
            chars.append("_")
        elif ch in JOLIET_FORBIDDEN:
            chars.append("_")
        else:
            chars.append(ch)
    text = "".join(chars).rstrip(" .")      # Windows dislikes trailing dots
    if not text:
        text = "_"
    return text[:MAX_JOLIET_NAME_CHARS]


def _assign_identifiers(node: Node, path: str = "") -> None:
    """Fill in iso_id / joliet_id for the whole tree, checking collisions."""
    iso_seen = {}
    joliet_seen = {}
    for child in node.children:
        here = (path + "/" if path else "") + child.name
        iso_text = iso_identifier(child.name, child.is_dir)
        iso_bytes = iso_text.encode("ascii")
        if not child.is_dir:
            iso_bytes += b";1"
        joliet_text = joliet_identifier(child.name)
        joliet_bytes = joliet_text.encode("utf-16-be")

        checks = [(iso_seen, iso_bytes, iso_text, "ISO 9660 level 1"),
                  (joliet_seen, joliet_bytes, joliet_text, "Joliet")]
        for seen, ident, readable, kind in checks:
            other = seen.get(ident)
            if other is not None and other != child.name:
                raise IsoError(
                    "%s name collision in %s: %r and %r both become %r; "
                    "rename one of them"
                    % (kind, path or "<root>", other, child.name, readable))
            seen[ident] = child.name

        child.iso_id = iso_bytes
        child.joliet_id = joliet_bytes
        for ident in (child.iso_id, child.joliet_id):
            if len(ident) > 255:
                raise IsoError("identifier too long for %s" % here)
        if child.is_dir:
            _assign_identifiers(child, here)


def _sort_children(node: Node) -> None:
    """Directory records must be sorted, and differently per tree."""
    for child in node.children:
        if child.is_dir:
            _sort_children(child)
    node.iso_children = sorted(node.children, key=lambda n: n.iso_id)
    node.joliet_children = sorted(node.children, key=lambda n: n.joliet_id)


def _breadth_first(root: Node, children_attr: str, index_attr: str):
    """Directories in path table order: by level, then parent, then name."""
    order = [root]
    index = 0
    while index < len(order):
        current = order[index]
        index += 1
        for child in getattr(current, children_attr):
            if child.is_dir:
                order.append(child)
    for position, directory in enumerate(order, 1):
        setattr(directory, index_attr, position)
    return order


def _require_shallow(root: Node) -> None:
    """ISO 9660 allows at most 8 levels of directories (root is level 1)."""
    def depth(node: Node, level: int) -> int:
        deepest = level
        for child in node.children:
            if child.is_dir:
                deepest = max(deepest, depth(child, level + 1))
        return deepest

    deepest = depth(root, 1)
    if deepest > MAX_HIERARCHY_LEVELS:
        raise IsoError(
            "directory nesting is %d levels deep; ISO 9660 allows %d"
            % (deepest, MAX_HIERARCHY_LEVELS))


# --------------------------------------------------------------------------- #
# Directory records and path tables
# --------------------------------------------------------------------------- #

def make_directory_record(ident: bytes, extent: int, data_length: int,
                          flags: int, stamp: Stamp) -> bytes:
    """One ISO 9660 directory record (9.1), padded to an even length."""
    length = 33 + len(ident)
    if length % 2:                 # a record must have an even length
        length += 1
    record = bytearray(length)
    record[0] = length
    record[1] = 0                              # extended attribute length
    record[2:10] = both32(extent)
    record[10:18] = both32(data_length)
    record[18:25] = stamp.record_7()
    record[25] = flags
    record[26] = 0                             # file unit size
    record[27] = 0                             # interleave gap size
    record[28:32] = both16(1)                  # volume sequence number
    record[32] = len(ident)
    record[33:33 + len(ident)] = ident
    return bytes(record)


def pack_directory(records) -> bytes:
    """Lay records out in sectors; a record never crosses a sector boundary."""
    out = bytearray()
    for record in records:
        offset = len(out) % SECTOR_SIZE
        if offset + len(record) > SECTOR_SIZE:
            out.extend(b"\x00" * (SECTOR_SIZE - offset))
        out.extend(record)
    return pad_to_sector(bytes(out))


def measure_directory(node: Node, children_attr: str, stamp: Stamp) -> int:
    """Size in bytes of a directory extent, before extents are known.

    Only the record lengths matter here, so the extent values can be dummies.
    """
    records = [make_directory_record(b"\x00", 0, 0, FLAG_DIRECTORY, stamp),
               make_directory_record(b"\x01", 0, 0, FLAG_DIRECTORY, stamp)]
    ident_attr = "iso_id" if children_attr == "iso_children" else "joliet_id"
    for child in getattr(node, children_attr):
        records.append(make_directory_record(
            getattr(child, ident_attr), 0, 0,
            FLAG_DIRECTORY if child.is_dir else 0, stamp))
    return len(pack_directory(records))


def build_directory(node: Node, tree: str, stamp: Stamp) -> bytes:
    """Build the real bytes of a directory extent for *tree*."""
    if tree == "iso":
        children_attr = "iso_children"
        ident_attr = "iso_id"
        extent_attr = "iso_extent"
        size_attr = "iso_dir_size"
    else:
        children_attr = "joliet_children"
        ident_attr = "joliet_id"
        extent_attr = "joliet_extent"
        size_attr = "joliet_dir_size"

    parent = node.parent if node.parent is not None else node
    records = [
        make_directory_record(b"\x00", getattr(node, extent_attr),
                              getattr(node, size_attr), FLAG_DIRECTORY, stamp),
        make_directory_record(b"\x01", getattr(parent, extent_attr),
                              getattr(parent, size_attr), FLAG_DIRECTORY, stamp),
    ]
    for child in getattr(node, children_attr):
        ident = getattr(child, ident_attr)
        if child.is_dir:
            records.append(make_directory_record(
                ident, getattr(child, extent_attr),
                getattr(child, size_attr), FLAG_DIRECTORY, stamp))
        else:
            records.append(make_directory_record(
                ident, child.extent, child.size, 0, stamp))
    return pack_directory(records)


def path_table_records(directories, ident_attr: str, index_attr: str,
                       extent_attr: str, big_endian: bool) -> bytes:
    """Serialise a path table (ISO 9660 6.9).

    *directories* must already be in path table order (by level, then parent
    directory number, then identifier) which _breadth_first guarantees.
    """
    pack_int = (lambda v: struct.pack(">I", v)) if big_endian else \
               (lambda v: struct.pack("<I", v))
    pack_short = (lambda v: struct.pack(">H", v)) if big_endian else \
                 (lambda v: struct.pack("<H", v))

    out = bytearray()
    for directory in directories:
        if directory.parent is None:
            name = b"\x00"                    # the root directory: one 0x00
            parent_number = 1
        else:
            name = getattr(directory, ident_attr)
            parent_number = getattr(directory.parent, index_attr)
        record = bytearray()
        record.append(len(name))              # length of the identifier
        record.append(0)                      # extended attribute length
        record += pack_int(getattr(directory, extent_attr))
        record += pack_short(parent_number)
        record += name
        if len(record) % 2:
            record.append(0)
        out += record
    return bytes(out)


# --------------------------------------------------------------------------- #
# Volume descriptors
# --------------------------------------------------------------------------- #

def encode_label_primary(label: str) -> bytes:
    """Volume identifier for the PVD: 32 d-characters, space padded."""
    text = "".join(ch if ch in D_CHARACTERS else "_" for ch in label.upper())
    text = text[:32]
    if not text:
        raise IsoError("the volume label needs at least one valid character")
    return text.ljust(32).encode("ascii")


def encode_label_joliet(label: str) -> bytes:
    """Volume identifier for the SVD: 16 UCS-2BE characters, space padded."""
    text = label.upper()[:16]
    encoded = text.encode("utf-16-be")
    padding = (32 - len(encoded)) // 2
    return encoded + b"\x00\x20" * padding


def build_volume_descriptor(vd_type: int, label: bytes, volume_space_size: int,
                            path_table_size: int, l_path_table: int,
                            m_path_table: int, root_record: bytes,
                            stamp: Stamp, joliet: bool) -> bytes:
    """A 2048 byte primary or supplementary volume descriptor (8.4 / 8.5)."""
    vd = bytearray(SECTOR_SIZE)
    vd[0] = vd_type
    vd[1:6] = b"CD001"
    vd[6] = 1
    # 7 is unused.
    vd[8:40] = b" " * 32                      # system identifier
    vd[40:72] = label                         # volume identifier (32 bytes)
    vd[72:80] = b"\x00" * 8                   # unused
    vd[80:88] = both32(volume_space_size)
    if joliet:
        # Escape sequences selecting the UCS-2 level 3 character set.
        vd[88:91] = b"%/E"
        vd[91:94] = b"@/E"
        vd[94:97] = b"^/E"
    else:
        vd[88:120] = b"\x00" * 32
    vd[120:124] = both16(1)                   # volume set size
    vd[124:128] = both16(1)                   # volume sequence number
    vd[128:132] = both16(SECTOR_SIZE)         # logical block size
    vd[132:140] = both32(path_table_size)
    vd[140:144] = struct.pack("<I", l_path_table)   # type L path table
    vd[144:148] = struct.pack("<I", 0)              # optional type L
    vd[148:152] = struct.pack(">I", m_path_table)   # type M path table
    vd[152:156] = struct.pack(">I", 0)              # optional type M
    if len(root_record) != 34:
        raise IsoError("internal error: root directory record is %d bytes"
                       % len(root_record))
    vd[156:190] = root_record
    vd[190:318] = b" " * 128                  # volume set identifier
    vd[318:446] = b" " * 128                  # publisher identifier
    vd[446:574] = b" " * 128                  # data preparer identifier
    vd[574:702] = b"MKISO.PY - ISO 9660/JOLIET MASTER".ljust(128, b" ")
    vd[702:739] = b" " * 37                   # copyright file identifier
    vd[739:776] = b" " * 37                   # abstract file identifier
    vd[776:813] = b" " * 37                   # bibliographic file identifier
    vd[813:830] = stamp.description_17()      # creation
    vd[830:847] = stamp.description_17()      # modification
    vd[847:864] = UNSPECIFIED_DATE_17         # expiration (not specified)
    vd[864:881] = UNSPECIFIED_DATE_17         # effective (not specified)
    vd[881] = 1                               # file structure version
    vd[882] = 0                               # unused
    return bytes(vd)


def build_terminator() -> bytes:
    """Volume descriptor set terminator (8.3)."""
    vd = bytearray(SECTOR_SIZE)
    vd[0] = VD_TERMINATOR
    vd[1:6] = b"CD001"
    vd[6] = 1
    return bytes(vd)


# --------------------------------------------------------------------------- #
# The image builder
# --------------------------------------------------------------------------- #

class _Writer:
    """Sequential writer that keeps track of (and can assert) its LBA."""

    def __init__(self, handle):
        self.handle = handle
        self.position = 0

    def write(self, data: bytes) -> None:
        self.handle.write(data)
        self.position += len(data)

    def zeros(self, count: int) -> None:
        while count > 0:
            chunk = min(count, 1 << 20)
            self.write(b"\x00" * chunk)
            count -= chunk

    def pad_sector(self) -> None:
        rest = self.position % SECTOR_SIZE
        if rest:
            self.zeros(SECTOR_SIZE - rest)

    def expect_sector(self, lba: int) -> None:
        if self.position != lba * SECTOR_SIZE:
            raise IsoError("internal layout error: at byte %d, expected sector "
                           "%d" % (self.position, lba))

    @property
    def lba(self) -> int:
        if self.position % SECTOR_SIZE:
            raise IsoError("internal layout error: not sector aligned")
        return self.position // SECTOR_SIZE


class IsoImage:
    """Composes the whole image: descriptors, path tables, dirs, file data."""

    def __init__(self, root: Node, label: str, joliet: bool, stamp: Stamp):
        self.root = root
        self.label = label
        self.joliet = joliet
        self.stamp = stamp
        self.total_sectors = 0
        self._layout_done = False

    # -- layout ---------------------------------------------------------- #

    def _collect_files(self) -> list:
        files = []

        def walk(node: Node) -> None:
            for child in node.iso_children:
                if child.is_dir:
                    walk(child)
                else:
                    files.append(child)

        walk(self.root)
        return files

    def layout(self) -> None:
        """Decide every LBA.  Names and sizes are already fixed at this point."""
        root = self.root
        self.iso_dirs = _breadth_first(root, "iso_children", "iso_index")
        self.joliet_dirs = _breadth_first(root, "joliet_children",
                                          "joliet_index") if self.joliet else []

        for directory in self.iso_dirs:
            directory.iso_dir_size = measure_directory(
                directory, "iso_children", self.stamp)
        if self.joliet:
            for directory in self.joliet_dirs:
                directory.joliet_dir_size = measure_directory(
                    directory, "joliet_children", self.stamp)

        # Path table sizes only depend on the identifiers.
        self.iso_pt_size = self._path_table_size(self.iso_dirs, "iso_id")
        self.joliet_pt_size = (self._path_table_size(self.joliet_dirs,
                                                     "joliet_id")
                               if self.joliet else 0)

        cursor = PVD_LBA
        self.pvd_lba = cursor
        cursor += 1
        self.svd_lba = None
        if self.joliet:
            self.svd_lba = cursor
            cursor += 1
        self.terminator_lba = cursor
        cursor += 1

        self.iso_pt_l_lba = cursor
        cursor += sectors_for(self.iso_pt_size)
        self.iso_pt_m_lba = cursor
        cursor += sectors_for(self.iso_pt_size)
        if self.joliet:
            self.joliet_pt_l_lba = cursor
            cursor += sectors_for(self.joliet_pt_size)
            self.joliet_pt_m_lba = cursor
            cursor += sectors_for(self.joliet_pt_size)

        for directory in self.iso_dirs:
            directory.iso_extent = cursor
            cursor += sectors_for(directory.iso_dir_size)
        if self.joliet:
            for directory in self.joliet_dirs:
                directory.joliet_extent = cursor
                cursor += sectors_for(directory.joliet_dir_size)

        files = self._collect_files()
        self.files = files
        file_sectors = sum(sectors_for(f.size) for f in files)
        self.total_sectors = cursor + file_sectors

        data_cursor = cursor
        for node in files:
            if node.size:
                node.extent = data_cursor
                data_cursor += sectors_for(node.size)
            else:
                # Zero length files still need an in-range extent.
                node.extent = min(data_cursor, self.total_sectors - 1)
        if data_cursor != self.total_sectors:
            raise IsoError("internal layout error: sector accounting mismatch")
        if self.total_sectors >= 2 ** 32:
            raise IsoError("image would need %d sectors, too many for ISO 9660"
                           % self.total_sectors)
        self._layout_done = True

    def _path_table_size(self, directories, ident_attr: str) -> int:
        size = 0
        for directory in directories:
            if directory.parent is None:
                name_length = 1
            else:
                name_length = len(getattr(directory, ident_attr))
            record = 8 + name_length
            if record % 2:
                record += 1
            size += record
        return size

    # -- writing --------------------------------------------------------- #

    def write(self, path: str) -> None:
        if not self._layout_done:
            raise IsoError("internal error: layout() must run before write()")

        root = self.root
        root_record_iso = make_directory_record(
            b"\x00", root.iso_extent, root.iso_dir_size, FLAG_DIRECTORY,
            self.stamp)
        root_record_joliet = None
        if self.joliet:
            root_record_joliet = make_directory_record(
                b"\x00", root.joliet_extent, root.joliet_dir_size,
                FLAG_DIRECTORY, self.stamp)

        pt_iso_l = path_table_records(self.iso_dirs, "iso_id", "iso_index",
                                      "iso_extent", False)
        pt_iso_m = path_table_records(self.iso_dirs, "iso_id", "iso_index",
                                      "iso_extent", True)
        if self.joliet:
            pt_joliet_l = path_table_records(self.joliet_dirs, "joliet_id",
                                             "joliet_index", "joliet_extent",
                                             False)
            pt_joliet_m = path_table_records(self.joliet_dirs, "joliet_id",
                                             "joliet_index", "joliet_extent",
                                             True)

        with open(path, "wb") as handle:
            writer = _Writer(handle)

            # System area: 16 sectors reserved for a boot loader.
            writer.zeros(SYSTEM_AREA_SECTORS * SECTOR_SIZE)

            writer.expect_sector(self.pvd_lba)
            writer.write(build_volume_descriptor(
                VD_PRIMARY, encode_label_primary(self.label),
                self.total_sectors, self.iso_pt_size,
                self.iso_pt_l_lba, self.iso_pt_m_lba, root_record_iso,
                self.stamp, joliet=False))

            if self.joliet:
                writer.expect_sector(self.svd_lba)
                writer.write(build_volume_descriptor(
                    VD_SUPPLEMENTARY, encode_label_joliet(self.label),
                    self.total_sectors, self.joliet_pt_size,
                    self.joliet_pt_l_lba, self.joliet_pt_m_lba,
                    root_record_joliet, self.stamp, joliet=True))

            writer.expect_sector(self.terminator_lba)
            writer.write(build_terminator())

            writer.expect_sector(self.iso_pt_l_lba)
            writer.write(pad_to_sector(pt_iso_l))
            writer.expect_sector(self.iso_pt_m_lba)
            writer.write(pad_to_sector(pt_iso_m))

            if self.joliet:
                writer.expect_sector(self.joliet_pt_l_lba)
                writer.write(pad_to_sector(pt_joliet_l))
                writer.expect_sector(self.joliet_pt_m_lba)
                writer.write(pad_to_sector(pt_joliet_m))

            for directory in self.iso_dirs:
                writer.expect_sector(directory.iso_extent)
                data = build_directory(directory, "iso", self.stamp)
                if len(data) != directory.iso_dir_size:
                    raise IsoError("internal error: directory size changed "
                                   "for %r" % directory.name)
                writer.write(data)

            if self.joliet:
                for directory in self.joliet_dirs:
                    writer.expect_sector(directory.joliet_extent)
                    data = build_directory(directory, "joliet", self.stamp)
                    if len(data) != directory.joliet_dir_size:
                        raise IsoError("internal error: directory size changed "
                                       "for %r" % directory.name)
                    writer.write(data)

            for node in self.files:
                if not node.size:
                    continue
                writer.expect_sector(node.extent)
                self._copy_file(writer, node)
                writer.pad_sector()

            writer.pad_sector()
            if writer.lba != self.total_sectors:
                raise IsoError("internal error: wrote %d sectors, planned %d"
                               % (writer.lba, self.total_sectors))
            handle.flush()

    @staticmethod
    def _copy_file(writer: "_Writer", node: Node) -> None:
        """Stream one source file into the image, checking its length."""
        written = 0
        try:
            with open(node.src_path, "rb") as handle:
                while True:
                    chunk = handle.read(1 << 20)
                    if not chunk:
                        break
                    writer.write(chunk)
                    written += len(chunk)
        except OSError as exc:
            raise IsoError("cannot read %s: %s" % (node.src_path, exc))
        if written != node.size:
            raise IsoError("%s changed size while the image was built "
                           "(%d -> %d)" % (node.src_path, node.size, written))


# --------------------------------------------------------------------------- #
# Command line
# --------------------------------------------------------------------------- #

def build(src: str, out: str, label: str, joliet: bool, stamp: Stamp) -> IsoImage:
    """Build the image and return the object describing what was written."""
    if len(label) > 32:
        raise IsoError("--label is %d characters; the volume identifier "
                       "holds at most 32" % len(label))

    root = scan_tree(src)
    _assign_identifiers(root)
    _sort_children(root)
    _require_shallow(root)

    image = IsoImage(root, label, joliet, stamp)
    image.layout()

    # Refuse to overwrite the source tree with the image.
    out_abs = os.path.abspath(out)
    src_abs = os.path.abspath(src)
    if out_abs == src_abs or out_abs.startswith(src_abs + os.sep):
        raise IsoError("the output image must not be written inside the "
                       "source tree")

    parent_dir = os.path.dirname(out_abs)
    if parent_dir and not os.path.isdir(parent_dir):
        raise IsoError("output directory does not exist: %s" % parent_dir)

    image.write(out_abs)
    return image


def summary(image: IsoImage) -> str:
    files = image.files
    total_bytes = sum(f.size for f in files)
    dirs = len(image.iso_dirs) + len(image.joliet_dirs)
    return ("%d directory extents, %d files (%d bytes) -> %d sectors "
            "(%d bytes), Joliet %s"
            % (dirs, len(files), total_bytes, image.total_sectors,
               image.total_sectors * SECTOR_SIZE,
               "on" if image.joliet else "off"))


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        prog="mkiso.py",
        description="Build an ISO 9660 level 1 (+ Joliet) CD image.")
    parser.add_argument("--src", required=True,
                        help="source directory to master")
    parser.add_argument("--out", required=True, help="output .iso file")
    parser.add_argument("--label", default="SMARTMATE2000",
                        help="volume label (max 32 characters)")
    parser.add_argument("--no-joliet", action="store_true",
                        help="omit the Joliet supplementary volume descriptor")
    parser.add_argument("--date", default="2003-06-01",
                        help="timestamp for every entry, YYYY-MM-DD[THH:MM:SS]")
    parser.add_argument("--boot-image", default="none",
                        help="only 'none' is supported (no El Torito)")
    parser.add_argument("--quiet", action="store_true",
                        help="print nothing on success")
    args = parser.parse_args(argv)

    try:
        if args.boot_image.strip().lower() not in ("none", ""):
            raise IsoError("--boot-image other than 'none' is not supported: "
                           "this builder does not write El Torito boot "
                           "records (the PVD must stay at sector 16)")
        stamp = Stamp.parse(args.date)
        image = build(args.src, args.out, args.label,
                      joliet=not args.no_joliet, stamp=stamp)
    except IsoError as exc:
        sys.stderr.write("mkiso: error: %s\n" % exc)
        return 2

    if not args.quiet:
        sys.stdout.write("%s: %s\n" % (args.out, summary(image)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
