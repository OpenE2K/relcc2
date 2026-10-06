#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check spec/abi/lcc-builtins.yaml against lcc: compile a call of every
operation builtin (group op) with the lcc of its ISA version and record
the operations lcc emits for it (spec/abi/lcc-builtins.md).

  python3 tools/abi/probes/gen_builtins.py [-o DIR]

For each ISA version N with an lcc in /opt/mcst (v3 for v1-v3, v4.8c,
v5.8c2, v6.2c3; lcc 1.29.16 has no v7 target) it writes DIR/bN.c, one
function per builtin taking the builtin's arguments as parameters
(constants where lcc requires them), compiles it with -O2 -S, and reads
each function's operations.  Results go to spec/abi/lcc-builtins-check.yaml
and are summarised on stdout; lcc-builtins.md describes them.
"""

import argparse
import re
import subprocess
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[3]
LCC = "/opt/mcst/lcc-1.29.16.e2k-{}.linux-6.1/bin/lcc"
TARGET = {1: "v3", 2: "v3", 3: "v3", 4: "v4.8c", 5: "v5.8c2", 6: "v6.2c3"}
CTYPE = {"uint64": "unsigned long long", "uint32": "unsigned int",
         "uint16": "unsigned short", "uint8": "unsigned char",
         "int64": "long long", "int": "int", "long": "long",
         "unsigned": "unsigned", "unsigned int": "unsigned int",
         "v2di": "v2di", "uint128": "v2di", "float32": "float",
         "float64": "double", "long double": "long double",
         "void*": "void *", "const void*": "const void *",
         "v2di*": "v2di *", "const v2di*": "const v2di *",
         "void": "void", "N": "int"}
# Operations a function may contain besides the builtin's own.
FRAME = {"setwd", "return", "ct", "nop", "disp", "ipd", "setbn", "setbp",
         "addd", "adds", "ord", "ors", "movtd", "qppackdl", "movfi", "movif",
         "ldd", "std", "ldqp", "stqp", "sxt", "andd", "ands", "getsp", "wait",
         # (splitting a v2di result into its two return registers)
         "qpswitchd"}


def ctype(t):
    return CTYPE.get(t.split(" | ")[0], "unsigned long long")


def functions(rows, consts):
    out = ["typedef long long v2di __attribute__ ((vector_size (16)));\n"
           "#include <e2kintrin.h>\n"]
    for i, r in enumerate(rows):
        params, call = [], []
        for k, a in enumerate(r["args"]):
            if r["name"] in consts and k in consts[r["name"]]:
                call.append("1")
            else:
                params.append(f"{ctype(a['type'])} a{k}")
                call.append(f"a{k}")
        ret = ctype(r["returns"])
        body = (f"{r['name']} ({', '.join(call)});" if ret == "void"
                else f"return {r['name']} ({', '.join(call)});")
        out.append(f"{ret} f{i} ({', '.join(params) or 'void'}) {{ {body} }}\n")
    return "".join(out)


def first_error(err):
    """The text of lcc's first error message, on one line."""
    m = re.search(r"error[^:]*:(.*?)(?:\n\S|\Z)", err, re.S)
    return " ".join((m.group(1) if m else err).split())


def fix(r, msg, consts):
    """Change R's call so that lcc accepts it; False if nothing helps."""
    tried = r.setdefault("_tried", set())
    m = re.search(r"argument (\d+) in call", msg)
    if m and ("constant" in msg or "literal" in msg or "immediate" in msg
              or "константн" in msg):
        # (A builtin defined as a macro reports its expansion's argument.)
        k = min(int(m.group(1)) - 1, len(r["args"]) - 1)
        if k not in consts.setdefault(r["name"], set()):
            consts[r["name"]].add(k)
            return True
    m = re.search(r'argument of type "([^"]+)" is incompatible with parameter'
                  r' of type "([^"]+)"', msg)
    if m:
        have, want = m.group(1), m.group(2)
        for x in r["args"]:
            if ctype(x["type"]) == have and ("type", x["name"]) not in tried:
                tried.add(("type", x["name"]))
                x["type"] = {"unsigned long long": "uint64",
                             "unsigned int": "uint32", "int": "int",
                             "long long": "int64"}.get(want, "v2di")
                return True
    if "return value type" in msg or "incompatible with the function" in msg \
            or "value of type" in msg:
        for alt in {"v2di": ["uint64", "void"],
                    "uint64": ["v2di", "uint32", "void"]}.get(
                        r["returns"], ["void"]):
            if ("ret", alt) not in tried:
                tried.add(("ret", alt))
                r["returns"] = alt
                return True
    m = re.search(r"argument (\d+) in call", msg)
    if m:
        k = int(m.group(1)) - 1
        if k not in consts.setdefault(r["name"], set()):
            consts[r["name"]].add(k)
            return True
    return False


def compile_(target, src, asm):
    p = subprocess.run([LCC.format(target), "-O2", "-S", str(src), "-o",
                        str(asm)], capture_output=True, text=True)
    return p.returncode, p.stdout + p.stderr


def ops_of(asm_text):
    res, cur = {}, None
    for line in asm_text.splitlines():
        m = re.match(r"^(f\d+):", line)
        if m:
            cur = int(m.group(1)[1:])
            res[cur] = []
            continue
        if cur is None:
            continue
        m = re.match(r"^\s+([a-z][a-z0-9_]*)[\s,]", line + " ")
        if m and not line.lstrip().startswith("."):
            res[cur].append(m.group(1))
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-o", type=Path, default=ROOT / "toolchains" / "builtins")
    a = ap.parse_args()
    a.o.mkdir(parents=True, exist_ok=True)
    rows = [r for r in yaml.safe_load((ROOT / "spec/abi/lcc-builtins.yaml")
                                      .read_text())["builtins"]
            if r["group"] == "op"]
    for r in rows:
        r["_doc_args"] = [x["type"] for x in r["args"]]
        r["_doc_ret"] = r["returns"]
    results, later = {}, []
    for iset in sorted(TARGET):
        group = [r for r in rows if r["iset"] == iset] + \
            [r for r in later if r["iset"] == iset]
        if not group:
            continue
        target, consts = TARGET[iset], {}
        src, asm = a.o / f"b{iset}.c", a.o / f"b{iset}.s"
        lines_of = {}
        for attempt in range(len(group) + 8):
            text = functions(group, consts)
            src.write_text(text)
            lines_of = {n + 1: l for n, l in enumerate(text.splitlines())}
            rc, err = compile_(target, src, asm)
            bad = {int(m.group(1)) for m in
                   re.finditer(r'"[^"]*b%d\.c", line (\d+)' % iset, err)}
            bad |= {int(m.group(1)) for m in
                    re.finditer(r'at line (\d+) of\s+"[^"]*b%d\.c"' % iset, err)}
            if rc == 0 and not bad:
                break
            changed = False
            for ln in sorted(bad):
                m = re.match(r".* f(\d+) ", lines_of.get(ln, ""))
                if not m:
                    continue
                r = group[int(m.group(1))]
                msg = first_error(err)
                fixed = fix(r, msg, consts)
                if not fixed:
                    if "built-in function" in msg and r["iset"] < 6:
                        later.append(dict(r, iset=r["iset"] + 1,
                                          listed_under=r.get("listed_under",
                                                             r["iset"])))
                    else:
                        results[r["name"]] = {"iset": iset, "target": target,
                                              "error": msg[:300]}
                    group = [x for x in group if x is not r]
                changed = True
                break
            if not changed:
                break
        found = ops_of(asm.read_text()) if asm.exists() else {}
        for i, r in enumerate(group):
            ops = [o for o in found.get(i, [])
                   if o not in FRAME or o == r["op"]]
            entry = {"iset": iset, "target": target, "ops": sorted(set(ops))}
            if consts.get(r["name"]):
                entry["constant_args"] = sorted(consts[r["name"]])
            if "listed_under" in r:
                entry["manual_iset"] = r["listed_under"]
            sig = [x["type"] for x in r["args"]]
            if sig != r["_doc_args"] or r["returns"] != r["_doc_ret"]:
                entry["lcc_signature"] = {"returns": r["returns"], "args": sig}
            results[r["name"]] = entry
    for r in rows:
        if r["iset"] not in TARGET:
            results[r["name"]] = {"iset": r["iset"], "untested": "no lcc for v7"}
        results.setdefault(r["name"], {"iset": r["iset"],
                                       "error": "not accepted up to v6"})
    out = ROOT / "spec" / "abi" / "lcc-builtins-check.yaml"
    out.write_text(
        "# SPDX-License-Identifier: CC-BY-SA-4.0\n"
        "# Generated by tools/abi/probes/gen_builtins.py: what lcc 1.29.16\n"
        "# emits for a call of each operation builtin of lcc-builtins.yaml\n"
        "# (target: the lcc build used; ops: the operations in the function\n"
        "# besides frame and argument moves; constant_args: argument indices\n"
        "# lcc requires as constants; lcc_signature: where lcc's types\n"
        "# differ from the manual's; manual_iset: where the builtin first\n"
        "# compiles for a later ISA than the manual lists it under).\n"
        + yaml.safe_dump({"builtins": results}, sort_keys=True,
                         default_flow_style=None, width=100))
    same = sum(1 for r in rows if r["op"] in results[r["name"]].get("ops", []))
    other = [(r["name"], results[r["name"]].get("ops"))
             for r in rows if "ops" in results[r["name"]]
             and r["op"] not in results[r["name"]]["ops"]]
    errs = [n for n, e in results.items() if "error" in e]
    print(f"{len(rows)} op builtins: {same} emit their operation,"
          f" {len(other)} emit something else, {len(errs)} don't compile,"
          f" {sum(1 for e in results.values() if 'untested' in e)} untested")
    for n, o in other:
        print("  other:", n, o)
    for n in errs:
        print("  error:", n, results[n]["error"][:120].replace("\n", " "))


if __name__ == "__main__":
    main()
