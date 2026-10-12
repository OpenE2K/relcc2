#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare e2kdis.py with MCST objdump on E2K object files.

Usage: compare_objdump.py [--binutils DIR] FILE.o...

The ISA version is taken from "-vN" in each file name (as produced by
build_corpus.sh). For every wide instruction, the lines printed by both
decoders are compared as multisets, after mapping alias mnemonics to their
canonical names. Any difference means the spec, the database or the decoder
disagrees with binutils. Bundles that differ only by a known objdump bug are
counted apart; where objdump drops a wait field, the line we print is
assembled with gas and must give the same CS1.
"""

import collections
import re
import subprocess
import sys
import tempfile
from pathlib import Path

import yaml

import e2kdis

_LOADER = getattr(yaml, "CSafeLoader", yaml.SafeLoader)
_YAML = {}


def load_yaml(text):
    """Parse TEXT once: e2kdis.Spec reads the large opcode table per file."""
    if text not in _YAML:
        _YAML[text] = yaml.load(text, Loader=_LOADER)
    return _YAML[text]


e2kdis.yaml = type("yaml", (), {"safe_load": staticmethod(load_yaml)})

BUNDLE_RX = re.compile(r"^\s+([0-9a-f]+):\s*$")

# Disagreements caused by known objdump bugs (bundle.md, "Known objdump
# bugs"). Bundles where every difference is explained by these, or that
# print as objdump does once our lines go through as_objdump_prints, are
# counted separately rather than failing.
KNOWN_OBJDUMP_BUGS = [
    # (objdump line, our corresponding lines, explanation)
    (re.compile(r"^aaur[rw]\w*,\d .*%unknown_aau_reg"),
     re.compile(r"^(staa\w*|ldaa\w*|incr),\d "),
     "print_alf accepts AAURR/AAURW when ALES is 0x01c0 even without MAS "
     "0x3f, so plain staa/ldaa print as aaurw/aaurr (and their incr is lost)"),
]

WAIT_RX = re.compile(r"^wait (.*)$")
WAIT_HIDDEN = {"las", "lal", "mt", "int", "mem_mod", "macp", "fl_c_mode"}


def as_objdump_prints(line):
    """Our LINE as objdump's known bugs would print it.

    objdump's wait prints none of CS1 bits 9-15 and prints sal/sas only
    when ld_c/st_c are set (bundle.md, "Known objdump bugs").
    """
    m = WAIT_RX.match(line)
    if not m:
        return line
    fields = [f.split(" = ") for f in m.group(1).split(", ")]
    values = dict(fields)
    kept = [f"{n} = {v}" for n, v in fields
            if n not in WAIT_HIDDEN
            and not (n == "sal" and values.get("ld_c") != "1")
            and not (n == "sas" and values.get("st_c") != "1")]
    return "wait " + ", ".join(kept)


def gas_cs1(binutils, iset, line, tmpdir):
    """CS1 of the wide instruction gas makes of LINE, or None if gas rejects it.

    objdump cannot show what as_objdump_prints drops, so the fields our
    wait line prints are checked against gas's encoder instead.
    """
    src, obj, raw = (Path(tmpdir) / n for n in ("w.s", "w.o", "w.bin"))
    src.write_text(f"\t.text\n{{\n\t{line}\n}}\n")
    if subprocess.run([binutils / "e2k-linux-as", f"-mcpu=elbrus-v{iset}", "-o", obj, src],
                      capture_output=True).returncode:
        return None
    subprocess.run([binutils / "e2k-linux-objcopy", "-O", "binary", "-j", ".text", obj, raw],
                   check=True)
    return e2kdis.Bundle(raw.read_bytes()).cs1


def gas_cannot_write(cs1):
    """Whether gas rejects every spelling of the wait CS1: sas or las without
    st_c, sal or lal without ld_c, fl_c_mode without fl_c (bundle.md, "wait
    fields"). Only raw words give these."""
    return bool(cs1 & 0x280 and not cs1 & 0x4 or cs1 & 0x500 and not cs1 & 0x8
                or cs1 & 0x8000 and not cs1 & 0x10)


def explained(only_ref, only_ours):
    """Whether KNOWN_OBJDUMP_BUGS explain every line that differs."""
    return (only_ref
            and all(any(r.search(l) for r, _, _ in KNOWN_OBJDUMP_BUGS) for l in only_ref)
            and all(any(o.search(l) for _, o, _ in KNOWN_OBJDUMP_BUGS) for l in only_ours))


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
    gas_checked = shown = 0
    tmpdir = tempfile.TemporaryDirectory()
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
            lines = ours.get(addr) or []
            a = normalize(ref.get(addr, []), canonical)
            b = normalize(lines, canonical)
            if a == b:
                total["match"] += 1
                continue
            seen = normalize(map(as_objdump_prints, lines), canonical)
            bad = []
            if a == seen:
                # objdump hides what differs: check it against gas instead.
                cs1 = e2kdis.Bundle(data[addr:addr + (((data[addr] >> 4) & 7) + 1) * 8]).cs1
                for line in lines:
                    if as_objdump_prints(line) == line:
                        continue
                    c = gas_cs1(binutils, iset, line, tmpdir.name)
                    if c is None and not gas_cannot_write(cs1):
                        bad.append(f"gas rejects it, but can write CS1 0x{cs1:08x}")
                    elif c is not None and c != cs1:
                        bad.append(f"gas encodes it with CS1 0x{c:08x}, not 0x{cs1:08x}")
                if not bad:
                    total["known objdump bug"] += 1
                    gas_checked += not gas_cannot_write(cs1)
                    continue
            elif explained(list((a - seen).elements()), list((seen - a).elements())):
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
                for line in bad:
                    print(f"  {line}")
    print(", ".join(f"{k} {v}" + (f" ({gas_checked} checked with gas)"
                                  if k == "known objdump bug" and gas_checked else "")
                    for k, v in sorted(total.items())))
    sys.exit(1 if total["differ"] else 0)


if __name__ == "__main__":
    main()
