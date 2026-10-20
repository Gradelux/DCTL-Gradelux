#!/usr/bin/env python3
"""Gradelux FilmLab Version 10: the embedded original LUTs.

Gradelux_FilmLab_v10.dctl carries Vista, Lux and Laguna as verbatim
DEFINE_CUBE_LUT blocks. This tool keeps them verbatim:

  python3 tools/v10_luts.py embed   write luts/*.cube into the DCTL (replaces
                                    the @@LUT_DATA@@ marker or the existing
                                    blocks; nothing else in the file changes)
  python3 tools/v10_luts.py check   verify every embedded block is byte-for-byte
                                    identical to its luts/*.cube file
  python3 tools/v10_luts.py strip OUT
                                    write a copy without the LUT blocks, for the
                                    host C tests (C cannot compile .cube text;
                                    tools/filmlab_shim.h emulates APPLY_LUT by
                                    reading the same luts/*.cube files)

Each block is written as
    DEFINE_CUBE_LUT(GlxLutVista)
    {
    <the .cube file, byte for byte>
    }
"""
import hashlib
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DCTL = os.path.join(ROOT, "Gradelux_FilmLab_v10.dctl")
LUTS = [("GlxLutVista", "Vista.cube"), ("GlxLutLux", "Lux.cube"), ("GlxLutLaguna", "Laguna.cube")]
MARKER = "@@LUT_DATA@@"
BLOCK = re.compile(r"DEFINE_CUBE_LUT\((\w+)\)\n\{\n(.*?)\}\n", re.S)


def read(path):
    with open(path, "rb") as f:
        return f.read().decode("utf-8")


def cube(name):
    text = read(os.path.join(ROOT, "luts", name))
    if not text.endswith("\n") or "\r" in text or "}" in text:
        raise SystemExit(f"{name}: unexpected format (needs LF line endings, final newline, no braces)")
    return text


def blocks_text():
    return "\n".join(f"DEFINE_CUBE_LUT({ident})\n{{\n{cube(name)}}}\n" for ident, name in LUTS)


def embed():
    s = read(DCTL)
    data = blocks_text()
    if MARKER in s:
        s = s.replace(MARKER + "\n", data, 1)
    else:
        found = list(BLOCK.finditer(s))
        if len(found) != len(LUTS):
            raise SystemExit("neither the marker nor the three LUT blocks were found")
        s = s[:found[0].start()] + data + s[found[-1].end():]
    with open(DCTL, "wb") as f:
        f.write(s.encode("utf-8"))
    check()


def check():
    s = read(DCTL)
    found = {m.group(1): m.group(2) for m in BLOCK.finditer(s)}
    ok = True
    if sorted(found) != sorted(i for i, _ in LUTS):
        print("FAIL: embedded LUT names", sorted(found))
        ok = False
    for ident, name in LUTS:
        src = cube(name)
        emb = found.get(ident)
        same = (emb == src)
        ok &= same
        print(f"{ident:13s} <- luts/{name:12s} {len(src):7d} bytes  sha1 {hashlib.sha1(src.encode()).hexdigest()}  "
              f"{'IDENTICAL' if same else 'DIFFERENT'}")
    if not ok:
        raise SystemExit(1)


def strip(out):
    s = read(DCTL)
    n = len(BLOCK.findall(s))
    if n != len(LUTS):
        raise SystemExit(f"expected {len(LUTS)} LUT blocks, found {n}")
    s = BLOCK.sub("/* LUT block removed for the host build */\n", s)
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, "w") as f:
        f.write(s)


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else ""
    if cmd == "embed":
        embed()
    elif cmd == "check":
        check()
    elif cmd == "strip" and len(sys.argv) == 3:
        strip(sys.argv[2])
    else:
        print(__doc__)
        sys.exit(2)
