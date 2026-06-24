#!/usr/bin/env python3
"""
gsl_fw_compile.py

Converts a Silead GSLx680-family vendor firmware header (the C array of
{offset, value} register writes shipped by panel/touch vendors) into the
flat binary format expected by the Linux kernel's silead.c touchscreen
driver (drivers/input/touchscreen/silead.c), which loads the file via
request_firmware() and reinterprets it directly as:

    struct silead_fw_data {
        u32 offset;
        u32 val;
    };

Each entry becomes 8 bytes on disk: a little-endian u32 offset followed
by a little-endian u32 value, in the same order they appear in the
source header. C block comments (/* ... */) and line comments (// ...)
are stripped before parsing, so any vendor-disabled blocks are correctly
excluded.

Usage:
    python3 gsl_fw_compile.py INPUT.h OUTPUT.fw

Exit status is non-zero if no entries are found, so this is safe to use
as a build step / CI gate.
"""

import argparse
import re
import struct
import sys


def parse_entries(text: str):
    # Drop block comments first (handles vendor-disabled register ranges).
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    # Drop line comments.
    text = re.sub(r"//.*", "", text)

    pairs = re.findall(
        r"\{\s*0x([0-9a-fA-F]+)\s*,\s*0x([0-9a-fA-F]+)\s*\}", text
    )

    entries = []
    for off_hex, val_hex in pairs:
        offset = int(off_hex, 16)
        val = int(val_hex, 16)
        if offset > 0xFF:
            print(
                f"warning: offset 0x{off_hex} exceeds a single byte; "
                f"double check this is really a register write entry",
                file=sys.stderr,
            )
        if val > 0xFFFFFFFF:
            raise ValueError(f"value 0x{val_hex} does not fit in 32 bits")
        entries.append((offset, val))
    return entries


def compile_fw(src_path: str, dst_path: str) -> int:
    with open(src_path, "r", encoding="utf-8", errors="replace") as f:
        text = f.read()

    entries = parse_entries(text)
    if not entries:
        print(f"error: no {{0x.., 0x..}} entries found in {src_path}", file=sys.stderr)
        return 1

    with open(dst_path, "wb") as out:
        for offset, val in entries:
            out.write(struct.pack("<II", offset, val))

    print(f"{src_path}: {len(entries)} entries -> {dst_path} ({len(entries) * 8} bytes)")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("input", help="vendor firmware header (.h)")
    ap.add_argument("output", help="output binary firmware file (.fw)")
    args = ap.parse_args()
    sys.exit(compile_fw(args.input, args.output))


if __name__ == "__main__":
    main()
