#!/usr/bin/env python3
"""
Checks that every function a GSC script calls exists in the game.

BO2 links a script's calls to engine builtins by name when it loads the script;
one unknown name is a "script error" that the PS5 build turns into a fatal
crash. This reads the real builtin tables out of the decrypted executable
(re/codzm.plain.elf, from `ps5-native-tool self --extract`) and checks every
call that is not defined in the script itself and not compiled to an opcode.

    python3 check_gsc.py <script.gsc> [codzm.plain.elf]
"""
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PARSER = os.path.join(HERE, "..", "source", "compiler", "t6", "parser.hpp")


def load_segments(d):
    phoff, = struct.unpack_from("<Q", d, 0x20)
    phnum, = struct.unpack_from("<H", d, 0x38)
    segs, dyn = [], None
    for i in range(phnum):
        t, _, off, va, _, fs, _, _ = struct.unpack_from("<IIQQQQQQ", d, phoff + i * 56)
        if t == 1:
            segs.append((off, va, fs))
        elif t == 2:
            dyn = (off, fs)
    return segs, dyn


def builtin_names(elf_path):
    """Names in the engine's builtin tables: 40-byte records
    { name*, id, 0, func*, flags } whose pointers are RELATIVE relocations."""
    d = open(elf_path, "rb").read()
    segs, dyn = load_segments(d)
    text_end = max(va + fs for off, va, fs in segs if off == min(o for o, _, _ in segs))

    def va2off(va):
        for off, base, size in segs:
            if base <= va < base + size:
                return off + va - base
        return None

    tags = {}
    off, size = dyn
    for j in range(0, size, 16):
        tag, val = struct.unpack_from("<qQ", d, off + j)
        tags.setdefault(tag, val)
    rela, relasz = tags[7], tags[8]
    o = va2off(rela)
    rel = {}
    for k in range(relasz // 24):
        r_off, r_info, r_add = struct.unpack_from("<QQq", d, o + k * 24)
        if r_info & 0xFFFFFFFF == 8:
            rel[r_off] = r_add

    def ident(va):
        o = va2off(va)
        if o is None:
            return None
        e = d.find(b"\0", o, o + 64)
        s = d[o:e] if e > o else b""
        return s.decode() if re.fullmatch(rb"[a-z_][a-z0-9_]*", s) else None

    records = {}
    for slot, target in rel.items():
        name = ident(target)
        fn = rel.get(slot + 0x18)
        if name and fn is not None and 0 < fn < text_end:
            records[slot] = name
    names = set()
    for slot in records:
        # keep records that sit in a table (a neighbour 40 bytes away)
        if slot + 40 in records or slot - 40 in records:
            names.add(records[slot])
    return names


def compiler_keywords():
    """Words DeadOps' T6 compiler turns into opcodes or syntax, not calls."""
    text = open(PARSER, encoding="utf-8").read()
    return set(re.findall(r'//\s+"([a-z_][a-z0-9_]*)"', text))


def script_calls(path):
    src = open(path, encoding="utf-8", errors="replace").read()
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    src = re.sub(r"//[^\n]*", "", src)
    src = re.sub(r'"(\\.|[^"\\])*"', '""', src)
    defined = {m.group(1).lower()
               for m in re.finditer(r"^\s*(\w+)\s*\([^;{]*\)\s*\{", src, re.M)}
    calls = {}
    for n, line in enumerate(src.splitlines(), 1):
        for m in re.finditer(r"\b(\w+)\s*\(", line):
            calls.setdefault(m.group(1).lower(), n)
    return defined, calls


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    gsc = sys.argv[1]
    elf = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "..", "re", "codzm.plain.elf")
    builtins = builtin_names(elf)
    keywords = compiler_keywords()
    defined, calls = script_calls(gsc)

    bad = []
    for name, line in sorted(calls.items(), key=lambda kv: kv[1]):
        if name in defined or name in keywords or name in builtins:
            continue
        bad.append((line, name))
    print(f"{len(builtins)} engine builtins, {len(keywords)} compiler keywords, "
          f"{len(calls)} names called, {len(defined)} defined in script")
    for line, name in bad:
        print(f"  line {line}: '{name}' is not a builtin in this game build")
    print("OK: every call resolves" if not bad else f"FAIL: {len(bad)} unknown name(s)")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
