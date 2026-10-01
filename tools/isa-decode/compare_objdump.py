#!/usr/bin/env python3
"""Compare e2kdis.py with MCST objdump on E2K object files.

Usage: compare_objdump.py [--binutils DIR] FILE.o...

The ISA version is taken from "-vN" in each file name (as produced by
build_corpus.sh). For every wide instruction, the lines printed by both
decoders are compared as multisets, after mapping alias mnemonics to their
canonical names. Any difference means the spec, the database or the decoder
disagrees with binutils.
"""

import collections
import re
import subprocess
import sys
import tempfile
from pathlib import Path

import e2kdis

BUNDLE_RX = re.compile(r"^\s+([0-9a-f]+):\s*$")

# Disagreements caused by known objdump bugs, as (regex matching the objdump
# line, explanation). Bundles where every difference is explained by these
# are counted separately rather than failing.
KNOWN_OBJDUMP_BUGS = [
    # (objdump line, our corresponding lines, explanation)
    (re.compile(r"^aaur[rw]\w*,\d .*%unknown_aau_reg"),
     re.compile(r"^(staa\w*|ldaa\w*|incr),\d "),
     "print_alf accepts AAURR/AAURW when ALES is 0x01c0 even without MAS "
     "0x3f, so plain staa/ldaa print as aaurw/aaurr (and their incr is lost)"),
]


def objdump_bundles(binutils, obj, iset):
    out = subprocess.run([binutils / "e2k-linux-objdump", "-d", f"-melbrus-v{iset}",
                          "-j", ".text", obj], capture_output=True, text=True,
                         check=True).stdout
    bundles, cur = {}, None
    for line in out.splitlines():
        m = BUNDLE_RX.match(line)
        if m:
            cur = bundles.setdefault(int(m.group(1), 16), [])
        elif cur is not None and line.startswith("  ") and line.strip():
            cur.append(line.strip())
        elif not line.strip() or line.startswith("0"):
            cur = None if line.startswith("0") else cur
    return bundles


def normalize(lines, canonical):
    out = collections.Counter()
    for line in lines:
        line = re.sub(r"\s+", " ", line.strip())
        m = re.match(r"([a-z0-9_]+)(.*)", line)
        if m and m.group(1) in canonical:
            line = canonical[m.group(1)] + m.group(2)
        out[line] += 1
    return out


def main():
    args = sys.argv[1:]
    binutils = Path(__file__).resolve().parents[2] / "toolchains" / "binutils" / "bin"
    if args[:1] == ["--binutils"]:
        binutils, args = Path(args[1]), args[2:]
    total = collections.Counter()
    shown = 0
    for obj in map(Path, args):
        iset = int(re.findall(r"-v(\d)", obj.name)[-1])
        canonical = e2kdis.Spec(iset).canonical
        with tempfile.NamedTemporaryFile() as tmp:
            subprocess.run([binutils / "e2k-linux-objcopy", "-O", "binary",
                            "-j", ".text", obj, tmp.name], check=True)
            data = Path(tmp.name).read_bytes()
        ref = objdump_bundles(binutils, obj, iset)
        ours = dict(e2kdis.decode(data, iset))
        for addr in sorted(set(ref) | set(ours)):
            a = normalize(ref.get(addr, []), canonical)
            b = normalize(ours.get(addr) or [], canonical)
            if a == b:
                total["match"] += 1
                continue
            only_ref, only_ours = list((a - b).elements()), list((b - a).elements())
            if only_ref and all(any(r.search(l) for r, _, _ in KNOWN_OBJDUMP_BUGS)
                                for l in only_ref) \
                    and all(any(o.search(l) for _, o, _ in KNOWN_OBJDUMP_BUGS)
                            for l in only_ours):
                total["known objdump bug"] += 1
                continue
            total["differ"] += 1
            if shown < 30:
                shown += 1
                print(f"{obj.name} {addr:#x}:")
                for line in sorted((a - b).elements()):
                    print(f"  objdump: {line}")
                for line in sorted((b - a).elements()):
                    print(f"  ours:    {line}")
    print(", ".join(f"{k} {v}" for k, v in sorted(total.items())))
    sys.exit(1 if total["differ"] else 0)


if __name__ == "__main__":
    main()
