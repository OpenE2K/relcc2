#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Find every __builtin_e2k_* name lcc knows, so that
spec/abi/lcc-builtins.yaml (from lcc's manual) misses none.

  python3 tools/abi/probes/find_builtins.py [--lcc DIR] [-o DIR]

Candidates (about 60,000): every mnemonic and alias of
spec/isa/binutils-ops.yaml (ALU and control operations), alone and with
the suffixes lcc and binutils use (_lt3, _c, _be, _8u, ...); every name
the manual lists; every E2K-looking __builtin_ name in the manuals and
changelogs of the lcc trees on this machine (lcc-doc); every
__builtin_e2k_ name that appears in lcc's include directory or in
Unipro's e2k_wrappers headers (names only: the headers are MCST's, all
rights reserved, and nothing else is taken from them); and the
non-operation names with their first word replaced (get_, set_, is_,
load_, ..., with and without e2k_), which is how
__builtin_e2k_get_cpu_iset, misspelt in the manual, was found.  Each candidate is called with no arguments in a C file,
compiled by lcc for each target (and for v7 with -march=elbrus-v7, as
lcc 1.31 has no v7 build), with and without <e2kintrin.h>, and in
protected (-mptr128) and 32-bit (-mptr32) mode: lcc warns
"declared implicitly" for a name it doesn't know and reports anything
else (usually "too few arguments") for one it knows.  It also lists
what <e2kintrin.h> adds, from lcc's preprocessor (-dM -E): macro names
with their parameter lists, typedef, enumerator and inline function
names, and the values of its constants as lcc computes them; never
macro bodies or other header text.  Writes
spec/abi/lcc-builtins-names.yaml and prints the names the manual lacks.
"""

import argparse
import re
import subprocess
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[3]
VERSION = "1.31.05"
# Target directory suffix and extra options, by ISA version.
TARGETS = {3: ("v3", []), 4: ("v4.8c", []), 5: ("v5.8c2", []),
           6: ("v6.2c3", []), 7: ("v6.2c3", ["-march=elbrus-v7"])}
PREFIX = "__builtin_e2k_"
# Names earlier manuals listed (1.29.16's machine-independent barriers).
OLD = ["__builtin_read_barrier", "__builtin_write_barrier",
       "__builtin_mem_barrier", "__builtin_barrier"]


def lcc_dir(base, target):
    """The lcc tree: /opt/mcst if installed there, else toolchains/."""
    name = f"lcc-{VERSION}.e2k-{target}.linux-6.1"
    for d in (Path("/opt/mcst") / name,
              base / f"lcc-{VERSION}-e2k-{target}" / "opt" / "mcst" / name):
        if d.exists():
            return d
    raise SystemExit(f"no lcc {VERSION} for {target} (tools/setup/fetch-lcc.sh)")


def lcc_cmd(base, target):
    if (Path("/opt/mcst") / f"lcc-{VERSION}.e2k-{target}.linux-6.1").exists():
        return [f"/opt/mcst/lcc-{VERSION}.e2k-{target}.linux-6.1/bin/lcc"]
    return [str(base / f"lcc-{VERSION}-e2k-{target}" / "lcc")]


UNIPRO = ROOT / "toolchains" / "unipro" / "pkg" / "usr" / "lib"
# Suffixes lcc and binutils put on operation names.
SUFFIXES = ["_lt3", "_c", "_imm", "_hi", "_lo", "_sm", "_be", "_nt", "_t1",
            "_t2", "_nta", "_cleartag", "_8u", "_16u", "_32u", "_64u", "_128v"]
# First words of the non-operation builtins' names.
WORDS = ["get", "set", "is", "load", "store", "pstore", "read", "write",
         "create", "cpu"]
E2KISH = re.compile(r"e2k|mas|asi|cpu|barrier|tag|ptr|_ap_|rwap|rwsap|rrq")


def doc_names():
    """__builtin_ names in every lcc manual and changelog on this machine
    that look E2K-specific (lcc-doc; names only)."""
    dirs = list(Path("/opt/mcst").glob("lcc-*/doc/lcc")) + \
        list((ROOT / "toolchains").glob("lcc-*/opt/mcst/lcc-*/doc/lcc"))
    names = set()
    for d in dirs:
        for f in d.glob("*.htm*"):
            names.update(re.findall(r"__builtin_\w+", f.read_text(errors="replace")))
    return {n for n in names if E2KISH.search(n) and not n.endswith("_")}


def candidates(manual, include):
    ops = yaml.safe_load((ROOT / "spec/isa/binutils-ops.yaml").read_text())
    mnemonics = set()
    for o in ops["alu_ops"] + ops["manual_ops"]:
        mnemonics.add(o["name"])
        mnemonics.update(o.get("aliases") or [])
    names = {PREFIX + m + s for m in mnemonics for s in [""] + SUFFIXES}
    other = {b["name"] for b in manual if b["group"] != "op"} | set(OLD) \
        | {n for n in doc_names() if n[len(PREFIX):] not in mnemonics}
    names.update(b["name"] for b in manual)
    names |= other
    # Names that occur in lcc's and Unipro's headers (names only).
    for d in [include] + list(UNIPRO.glob("llvm-*/lib/clang/*/include/e2k_wrappers")):
        for h in d.rglob("*.h"):
            names.update(m for m in re.findall(PREFIX + r"\w+", h.read_text(
                errors="replace")) if m != PREFIX)
    # The non-operation names with their first word replaced, with and
    # without "e2k_" (cpu_iset -> get_cpu_iset, cpu_init -> e2k_cpu_init).
    stems = set()
    for n in other:
        rest = n[len("__builtin_"):]
        rest = rest[4:] if rest.startswith("e2k_") else rest
        stems.add(rest)
        stems.add(rest.split("_", 1)[1] if "_" in rest else rest)
    for st in stems:
        for w in [""] + [x + "_" for x in WORDS]:
            for pre in ("__builtin_", PREFIX):
                names.add(pre + w + st)
    return sorted(n for n in names if re.fullmatch(r"\w+", n))


def known(cmd, opts, names, work, header):
    src = work / "names.c"
    lines = (["#include <e2kintrin.h>"] if header else [""])
    lines += [f"void f{i} (void) {{ {n} (); }}" for i, n in enumerate(names)]
    src.write_text("\n".join(lines) + "\n")
    # (lcc stops after 100 errors unless told otherwise.)
    p = subprocess.run(cmd + opts + ["-S", "-fmax-errors=100000", str(src),
                                     "-o", "/dev/null"],
                       capture_output=True, text=True)
    implicit = {int(m.group(1)) for m in re.finditer(
        r'line (\d+): warning #223:', p.stdout + p.stderr)}
    return {n for i, n in enumerate(names) if i + 2 not in implicit}


def header_interface(cmd, opts, work):
    """What <e2kintrin.h> declares, by name: macros (with their parameter
    lists), typedefs, enumerators, and functions it defines; the values of
    its object-like macros and enumerators, as lcc computes them."""
    plain, inc = work / "h0.c", work / "h1.c"
    plain.write_text("int x;\n")
    inc.write_text("#include <e2kintrin.h>\nint x;\n")

    def run(args):
        return subprocess.run(cmd + opts + args, capture_output=True,
                              text=True, check=True).stdout
    before = set(run(["-dM", "-E", str(plain)]).splitlines())
    macros = {}
    for line in sorted(set(run(["-dM", "-E", str(inc)]).splitlines()) - before):
        m = re.match(r"#define (\w+)(\([^)]*\))?", line)
        macros[m.group(1)] = (m.group(2) or "").replace(" ", "")
    text = re.sub(r"^#.*$", "", run(["-E", str(inc)]), flags=re.M)
    types = set(re.findall(r"\btypedef\b[^;{]*?(\w+)\s*(?:\[[^\]]*\])?\s*"
                           r"(?:__attribute__\s*\(\(.*?\)\))?\s*;", text))
    enums = {}
    for name, body in re.findall(r"\benum\s+(\w+)\s*\{([^}]*)\}", text):
        types.add("enum " + name)
        for e in body.split(","):
            if e.strip():
                enums[e.split("=")[0].strip()] = name
    funcs = set()
    skip = {"__attribute__", "__always_inline__", "__inline__",
            "__gnu_inline__", "__artificial__", "__unused__"}
    for m in re.finditer(r"\)\s*\{", text):
        names = [n for n in re.findall(r"\b(\w+)\s*\(",
                                       text[max(0, m.start() - 300):m.start() + 1])
                 if n not in skip]
        if names:
            funcs.add(names[-1])
    # Values: object-like macros and enumerators, as 64-bit initialisers.
    consts = [n for n, p in macros.items() if not p] + sorted(enums)
    values = {}
    src = work / "h2.c"
    for n in consts:
        src.write_text(f"#include <e2kintrin.h>\nlong long v = (long long) ({n});\n")
        p = subprocess.run(cmd + opts + ["-S", "-O0", str(src), "-o", "-"],
                           capture_output=True, text=True)
        m = re.search(r"^v:\n\s+\.\w+\s+(\S+)", p.stdout, re.M)
        if p.returncode == 0 and m and re.fullmatch(r"-?(0x)?[0-9a-fA-F]+", m.group(1)):
            values[n] = int(m.group(1), 0)
    return macros, types, enums, funcs, values


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lcc", type=Path, default=ROOT / "toolchains",
                    help="where fetch-lcc.sh put lcc trees (if not in /opt/mcst)")
    ap.add_argument("-o", type=Path, default=ROOT / "toolchains" / "builtins131")
    a = ap.parse_args()
    a.o.mkdir(parents=True, exist_ok=True)
    manual = yaml.safe_load((ROOT / "spec/abi/lcc-builtins.yaml").read_text())
    manual = manual["builtins"]
    v3 = lcc_dir(a.lcc, "v3")
    include = next((v3 / "lcc-home" / VERSION).glob("*/include"))
    names = candidates(manual, include)
    found = {}
    for iset, (target, opts) in TARGETS.items():
        cmd = lcc_cmd(a.lcc, target)
        with_h = known(cmd, opts, names, a.o, True)
        without = known(cmd, opts, names, a.o, False)
        p128 = known(cmd, opts + ["-mptr128"], names, a.o, True)
        p32 = known(cmd, opts + ["-mptr32"], names, a.o, True)
        for n in with_h | without | p128 | p32:
            e = found.setdefault(n, {"isets": [], "needs_header": [],
                                     "ptr128_only": [], "ptr32_only": []})
            e["isets"].append(iset)
            if n in with_h and n not in without:
                e["needs_header"].append(iset)
            if n not in with_h | without | p32:
                e["ptr128_only"].append(iset)
            if n not in with_h | without | p128:
                e["ptr32_only"].append(iset)
            if n in with_h | without and n not in p32:
                e.setdefault("not_ptr32", []).append(iset)
    header = {}
    for iset, (target, opts) in TARGETS.items():
        macros, types, enums, funcs, values = header_interface(
            lcc_cmd(a.lcc, target), opts, a.o)
        for kind, items in (("macro", macros), ("type", {t: None for t in types}),
                            ("enumerator", enums), ("function", {f: None for f in funcs})):
            for n, extra in items.items():
                e = header.setdefault(n, {"kind": kind, "isets": []})
                e["isets"].append(iset)
                if kind == "macro" and extra:
                    e["params"] = extra
                if kind == "enumerator":
                    e["enum"] = extra
                if n in values:
                    e.setdefault("values", {})[iset] = values[n]
    for e in header.values():
        vals = e.pop("values", None)
        if vals:
            e["value"] = (next(iter(vals.values()))
                          if len(set(vals.values())) == 1 else vals)
    documented = {b["name"] for b in manual}
    out = {}
    for n in sorted(found):
        e = {"isets": found[n]["isets"]}
        for k in ("needs_header", "ptr128_only", "ptr32_only", "not_ptr32"):
            if found[n].get(k):
                e[k] = found[n][k]
        if n not in documented:
            e["undocumented"] = True
        out[n] = e
    (ROOT / "spec/abi/lcc-builtins-names.yaml").write_text(
        "# SPDX-License-Identifier: CC-BY-SA-4.0\n"
        f"# Generated by tools/abi/probes/find_builtins.py: every\n"
        f"# {PREFIX}* name lcc {VERSION} knows among {len(names)} candidates\n"
        "# (operation mnemonics and aliases from binutils-ops.yaml, the\n"
        "# manual's names, names appearing in lcc's include directory).\n"
        "# isets: targets where lcc knows the name (7: -march=elbrus-v7);\n"
        "# needs_header: where it is known only after <e2kintrin.h>;\n"
        "# ptr128_only: where it is known only in protected mode (-mptr128);\n"
        "# ptr32_only: only in 32-bit mode (-mptr32); not_ptr32: unknown\n"
        "# in 32-bit mode;\n"
        "# undocumented: not in lcc's manual (lcc-builtins.yaml).\n"
        "# header: what <e2kintrin.h> adds, by name only (macro with\n"
        "# its parameter list, type, enumerator, function it defines),\n"
        "# with isets and, for constants, the value lcc computes.\n"
        + yaml.safe_dump({"builtins": out, "header": header}, sort_keys=True,
                         default_flow_style=None, width=100))
    missing = [n for n in documented if n not in found]
    print(f"{len(names)} candidates, {len(found)} known to lcc {VERSION}")
    print("undocumented:", " ".join(n for n in sorted(found) if n not in documented))
    print("documented but unknown:", " ".join(sorted(missing)))


if __name__ == "__main__":
    main()
