#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check spec/abi/lcc-builtins.yaml against lcc 1.31.05: compile a call of
every operation builtin (group op) with the lcc of its ISA version and
record what lcc emits for it (spec/abi/lcc-builtins.md).

  python3 tools/abi/probes/gen_builtins.py [-o DIR]

For each ISA version N (v3 lcc for v1-v3, v4.8c, v5.8c2, v6.2c3, and
the v6.2c3 lcc with -march=elbrus-v7 for v7) it writes DIR/bN.c, one
function per builtin taking the builtin's arguments as parameters
(constants where lcc requires them), compiles it with -O2 -S, and traces
each function symbolically: arguments are a0, a1, ... (a v2di argument
arrives as two halves that qppackdl joins), copies and extensions are
followed, and every other operation is written with its operands in
those names, its result a temporary (t1, ...) or "ret" for the returned
value.  For the arguments lcc requires as constants it also finds the
values lcc accepts.  And it compiles the header's short form _e2k_<op>
of each builtin and compares the trace.  Results go to
spec/abi/lcc-builtins-check.yaml; lcc-builtins.md describes them.
"""

import argparse
import re
import subprocess
from pathlib import Path

import yaml

from find_builtins import lcc_cmd

ROOT = Path(__file__).resolve().parents[3]
VERSION = "1.31.05"
TARGET = {1: ("v3", []), 2: ("v3", []), 3: ("v3", []), 4: ("v4.8c", []),
          5: ("v5.8c2", []), 6: ("v6.2c3", []),
          7: ("v6.2c3", ["-march=elbrus-v7"])}
CTYPE = {"uint64": "unsigned long long", "uint32": "unsigned int",
         "uint16": "unsigned short", "uint8": "unsigned char",
         "int64": "long long", "int": "int", "long": "long",
         "unsigned": "unsigned", "unsigned int": "unsigned int",
         "v2di": "v2di", "uint128": "v2di", "float32": "float",
         "float64": "double", "long double": "long double",
         "void*": "void *", "const void*": "const void *",
         "v2di*": "v2di *", "const v2di*": "const v2di *",
         "void": "void", "N": "int", "__e2k_hint": "enum __e2k_hint"}
# Values tried for arguments lcc requires as constants.
CONST_TRIES = [-1, 0, 1, 2, 3, 4, 5, 7, 8, 15, 16, 31, 32, 63, 64, 127, 128,
               255, 256, 1000, 65536]
# Operations that only build the frame or return.
FRAME = {"setwd", "return", "iret", "ct", "nop", "ipd", "setbn", "setbp",
         "getsp"}


def ctype(t):
    return CTYPE.get(t.split(" | ")[0], "unsigned long long")


def kind(t):
    c = ctype(t)
    return ("pair" if c in ("v2di", "long double") else
            "void" if c == "void" else "int")


def call_args(r, consts, values):
    params, call = [], []
    for k, a in enumerate(r["args"]):
        if k in consts.get(r["name"], set()):
            call.append(str(values.get((r["name"], k), 1)))
        else:
            params.append(f"{ctype(a['type'])} a{k}")
            call.append(f"a{k}")
    return params, call


def functions(rows, consts, values, short=False):
    out = ["typedef long long v2di __attribute__ ((vector_size (16)));\n"
           "#include <e2kintrin.h>\n"]
    for i, r in enumerate(rows):
        params, call = call_args(r, consts, values)
        name = ("_e2k_" + r["name"][len("__builtin_e2k_"):]) if short else r["name"]
        ret = ctype(r["returns"])
        body = (f"{name} ({', '.join(call)});" if ret == "void"
                else f"return {name} ({', '.join(call)});")
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
        # (A builtin defined as a macro reports its expansion's argument;
        # lcc's later phases number the _lt3 builtins' arguments from 0.)
        n = int(m.group(1))
        for k in (n - 1, n):
            if k < len(r["args"]) and ctype(r["args"][k]["type"]) != "v2di" \
                    and k not in consts.setdefault(r["name"], set()):
                consts[r["name"]].add(k)
                return True
        k = min(n - 1, len(r["args"]) - 1)
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
                             "long long": "int64",
                             "enum __e2k_hint": "__e2k_hint"}.get(want, "v2di")
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


def compile_(iset, src, asm):
    target, opts = TARGET[iset]
    p = subprocess.run(lcc_cmd(ROOT / "toolchains", target) + opts +
                       ["-O2", "-S", "-fmax-errors=100000", str(src), "-o",
                        str(asm)], capture_output=True, text=True)
    return p.returncode, p.stdout + p.stderr


def error_lines(err, name):
    bad = {int(m.group(1)) for m in
           re.finditer(r'"[^"]*%s", line (\d+)' % re.escape(name), err)}
    bad |= {int(m.group(1)) for m in
            re.finditer(r'at line (\d+) of\s+"[^"]*%s"' % re.escape(name), err)}
    return bad


# Symbolic tracing of a compiled function.

def reg(o):
    """A register's canonical name (%dr3 -> r3, %db[2] -> b2), or None."""
    m = re.fullmatch(r"%[dqx]?(r|g|b)\[?(\d+)\]?", o)
    if m:
        return f"{m.group(1)}{m.group(2)}"
    m = re.fullmatch(r"%(pred\d+)", o)
    return m.group(1) if m else None


def operands(text):
    """Split an operand list, joining literal prefixes (_f32s,_lts1 0x5)
    and writing a bracketed literal ([ _f64,_lts1 sym +4 ]) as [sym+4]."""
    text = re.sub(r"\[\s*(?:_\w+\s*,\s*)*_\w+\s+([^\]]*?)\s*\]",
                  lambda m: "[" + m.group(1).replace(" ", "") + "]", text)
    parts, out = [p.strip() for p in text.split(",")], []
    while parts:
        p = parts.pop(0)
        while re.fullmatch(r"_\w+", p) and parts:
            p = parts.pop(0)
        out.append(p.split()[-1] if p.startswith("_") else p)
    return out


def bundles(asm_text):
    """{function index: [[(op, [operands]), ...] per wide instruction]}."""
    res, cur, wide = {}, None, None
    for line in asm_text.splitlines():
        m = re.match(r"^f(\d+):", line)
        if m:
            cur = int(m.group(1))
            res[cur] = []
            continue
        if cur is None:
            continue
        s = line.strip()
        if s == "{":
            wide = []
        elif s == "}":
            if wide is not None:
                res[cur].append(wide)
            wide = None
        elif wide is None and s and not s.startswith(".") and \
                not s.endswith(":") and "=" not in s.split()[0] and \
                not s.startswith("elbrus_"):
            # An instruction outside braces: inline asm (between
            # .accept strict_delay and .ignore strict_delay).
            m = re.match(r"([a-z][a-z0-9_]*)\s*(.*)", s)
            if m:
                res[cur].append([(m.group(1), "asm",
                                  operands(m.group(2)) if m.group(2) else [])])
        elif wide is not None and s and not s.startswith("."):
            for piece in s.split(";"):
                m = re.match(r"([a-z][a-z0-9_]*)((?:,[a-z0-9]+)*)\s*(.*)", piece.strip())
                if m:
                    ops = re.sub(r"\s*\?.*$", "", m.group(3))
                    wide.append((m.group(1), m.group(2).lstrip(","),
                                 operands(ops) if ops else []))
    return res


def trace(func, args, ret_type, channels=False):
    """Symbolic list of the operations in FUNC (bundles()), given the C
    argument kinds ARGS ("int", "pair" for a v2di or long double, None
    for a constant argument) and the return kind."""
    val, slot = {}, 0
    for k, t in enumerate(args):
        if t is None:
            continue
        if t == "pair":
            slot += slot % 2
            val[f"r{slot}"], val[f"r{slot + 1}"] = f"a{k}.lo", f"a{k}.hi"
            slot += 2
        else:
            val[f"r{slot}"] = f"a{k}"
            slot += 1
    out, temps = [], 0

    def v(o):
        r = reg(o)
        return val.get(r, o) if r else o

    def joined(hi, lo):
        m = re.fullmatch(r"(.+)\.hi", hi)
        return m.group(1) if m and lo == m.group(1) + ".lo" else None
    for wide in func:
        writes = []
        for op, mods, opnds in wide:
            if op in FRAME or (op == "disp" and "ctpr" in opnds[0]
                               and len(opnds) == 1):
                continue
            attrs = [o for o in opnds if "=" in o]
            opnds = [o for o in opnds if "=" not in o]
            src = [v(o) for o in opnds]
            # The destination: the last operand, or the one before a
            # merge's condition.
            d = -2 if op.startswith("merge") and len(opnds) == 4 else -1
            dst = reg(opnds[d]) if opnds else None
            if op in ("addd", "ord", "adds", "ors") and len(src) == 3 \
                    and "0x0" in src[:2]:
                writes.append((dst, src[1] if src[0] == "0x0" else src[0]))
            elif op in ("sxt", "movtd") and len(src) in (2, 3):
                writes.append((dst, src[-2]))
            elif op in ("qppackdl", "movif") and joined(src[0], src[1]):
                writes.append((dst, joined(src[0], src[1])))
            elif op == "qpswitchd" and len(src) == 2:
                writes.append((dst, f"{src[0]}.hi" if not src[0].endswith(".lo")
                               else src[0][:-3] + ".hi"))
            elif op == "movfi" and len(src) == 2:
                writes.append((dst, f"{src[0]}.hi"))
            elif op.startswith("st") or dst is None:
                out.append([op, mods, src, attrs])
            else:
                temps += 1
                t = f"t{temps}"
                src[d] = t
                out.append([op, mods, src, attrs])
                writes.append((dst, t))
        for dst, x in writes:
            if dst:
                val[dst] = x
    # The returned value: r0, and r1 for a 16-byte one (its high half).
    lo, hi = val.get("r0"), val.get("r1")
    names = {}
    if ret_type == "pair":
        m = re.fullmatch(r"(.+)\.lo", lo or "")
        whole = m.group(1) if m else lo
        if hi == f"{whole}.hi":
            names[whole] = "ret"
        else:
            names[lo], names[hi] = "ret.lo", "ret.hi"
    elif ret_type != "void":
        names[lo] = "ret"
    lines = []
    for op, mods, src, attrs in out:
        src = [names.get(x, x) for x in src]
        sm = ",sm" if "sm" in mods.split(",") else ""
        if channels:
            sm = "".join("," + m for m in mods.split(",") if m.isdigit()) + sm
        text = f"{op}{sm} {', '.join(src + attrs)}".rstrip()
        lines.append(f"asm: {text}" if mods == "asm" else text)
    copies = [f"{n} = {x}" for x, n in sorted(names.items(), key=lambda i: i[1])
              if x and not re.fullmatch(r"t\d+", x)]
    return lines + copies


def ranges(vals):
    """Runs of consecutive CONST_TRIES entries in VALS, as "a..b"."""
    out = []
    for i, x in enumerate(CONST_TRIES):
        if x not in vals:
            continue
        if out and out[-1][2] == i - 1:
            out[-1][1:] = [x, i]
        else:
            out.append([x, x, i])
    return [a if a == b else f"{a}..{b}" for a, b, _ in out]


def accepted_constants(iset, r, k, consts, types, work):
    """{value: what lcc emits for it} for the values of CONST_TRIES lcc
    accepts as argument K of R (one compilation each: lcc checks some
    ranges only after the front end, reporting one error)."""
    out = {}
    for c in CONST_TRIES:
        params, call = call_args(r, consts, {(r["name"], k): c})
        ret = ctype(r["returns"])
        body = (f"{r['name']} ({', '.join(call)});" if ret == "void"
                else f"return {r['name']} ({', '.join(call)});")
        src, asm = work / "c.c", work / "c.s"
        src.write_text("typedef long long v2di __attribute__ ((vector_size (16)));\n"
                       "#include <e2kintrin.h>\n"
                       f"{ret} f0 ({', '.join(params) or 'void'}) {{ {body} }}\n")
        if asm.exists():
            asm.unlink()
        rc, _ = compile_(iset, src, asm)
        if rc == 0:
            out[c] = trace(bundles(asm.read_text()).get(0, []), types,
                           kind(r["returns"]))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-o", type=Path, default=ROOT / "toolchains" / "builtins131")
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
        consts, values = {}, {}
        for r in group:
            for k, x in enumerate(r["args"]):
                if x["type"] == "N":
                    consts.setdefault(r["name"], set()).add(k)
        src, asm = a.o / f"b{iset}.c", a.o / f"b{iset}.s"
        for attempt in range(len(group) + 8):
            text = functions(group, consts, values)
            src.write_text(text)
            lines_of = {n + 1: l for n, l in enumerate(text.splitlines())}
            rc, err = compile_(iset, src, asm)
            bad = error_lines(err, src.name)
            if rc == 0 and not bad:
                break
            changed = False
            for ln in sorted(bad):
                m = re.match(r".* f(\d+) ", lines_of.get(ln, ""))
                if not m:
                    continue
                r = group[int(m.group(1))]
                at = err.find(f", line {ln}:")
                msg = first_error(err[at:] if at >= 0 else err)
                if not fix(r, msg, consts):
                    if "built-in function" in msg and r["iset"] < 7:
                        later.append(dict(r, iset=r["iset"] + 1,
                                          listed_under=r.get("listed_under",
                                                             r["iset"])))
                    else:
                        results[r["name"]] = {"iset": iset,
                                              "target": TARGET[iset][0],
                                              "error": msg[:300]}
                    group = [x for x in group if x is not r]
                changed = True
                break
            if not changed:
                break
        # Accepted constants, then compile again with a typical one.
        accepted = {}
        for r in group:
            types = [None if k in consts.get(r["name"], ()) else kind(x["type"])
                     for k, x in enumerate(r["args"])]
            for k in sorted(consts.get(r["name"], ())):
                ok = accepted_constants(iset, r, k, consts, types, a.o)
                accepted[(r["name"], k)] = ok
                values[(r["name"], k)] = 5 if 5 in ok else min(ok, default=1)
        src.write_text(functions(group, consts, values))
        compile_(iset, src, asm)
        found = bundles(asm.read_text()) if asm.exists() else {}
        # The header's short forms.
        ssrc, sasm = a.o / f"s{iset}.c", a.o / f"s{iset}.s"
        ssrc.write_text(functions(group, consts, values, short=True))
        _, serr = compile_(iset, ssrc, sasm)
        sbad = error_lines(serr, ssrc.name)
        if sbad:
            # Compile the rest again without the calls lcc rejects.
            keep = [r if i + 3 not in sbad else dict(r, name="__builtin_e2k_" + r["op"])
                    for i, r in enumerate(group)]
            text = functions(keep, consts, values, short=True)
            for i, r in enumerate(group):
                if i + 3 in sbad:
                    text = text.replace(f" _e2k_{r['op']} (", f" {r['name']} (")
            ssrc.write_text(text)
            compile_(iset, ssrc, sasm)
        sabsent = {int(m.group(1)) for m in re.finditer(
            r'"[^"]*%s", line (\d+): warning #223' % re.escape(ssrc.name), serr)}
        sfound = bundles(sasm.read_text()) if sasm.exists() else {}
        for i, r in enumerate(group):
            types = [None if k in consts.get(r["name"], ()) else kind(x["type"])
                     for k, x in enumerate(r["args"])]
            ops = trace(found.get(i, []), types, kind(r["returns"]))
            entry = {"iset": iset, "target": " ".join(
                [TARGET[iset][0]] + TARGET[iset][1]), "asm": ops}
            if consts.get(r["name"]):
                entry["constant_args"] = {}
                for k in sorted(consts[r["name"]]):
                    ok = accepted[(r["name"], k)]
                    entry["constant_args"][k] = {
                        "accepted": ranges(ok), "probed_with": values[(r["name"], k)],
                        "rejected": [c for c in CONST_TRIES if c not in ok]}
                    # The literal lcc emits for each accepted value, where
                    # it isn't the value itself.
                    lit = {}
                    for c, t in ok.items():
                        if not re.search(r"(?<![\w=])0x0*%x(?![\w])" % (c & 0xffffffff),
                                         " ".join(t)):
                            lit[c] = " ".join(t)
                    if lit:
                        entry["constant_args"][k]["emitted"] = lit
            if "listed_under" in r:
                entry["manual_iset"] = r["listed_under"]
            sig = [x["type"] for x in r["args"]]
            if sig != r["_doc_args"] or r["returns"] != r["_doc_ret"]:
                entry["lcc_signature"] = {"returns": r["returns"], "args": sig}
            line = i + 3
            if line in sabsent:
                pass
            elif line in sbad:
                entry["short_form"] = "error"
            elif i in sfound:
                st = trace(sfound[i], types, kind(r["returns"]))
                entry["short_form"] = "same" if st == ops else st
            results[r["name"]] = entry
    for r in rows:
        results.setdefault(r["name"], {"iset": r["iset"],
                                       "error": "not accepted up to v7"})
    out = ROOT / "spec" / "abi" / "lcc-builtins-check.yaml"
    out.write_text(
        "# SPDX-License-Identifier: CC-BY-SA-4.0\n"
        f"# Generated by tools/abi/probes/gen_builtins.py: what lcc {VERSION}\n"
        "# emits for a call of each operation builtin of lcc-builtins.yaml\n"
        "# at -O2. target: the lcc build used. asm: the operations besides\n"
        "# the frame, with the arguments as a0, a1, ... in C order (a0.lo,\n"
        "# a0.hi: the halves of a 16-byte one), temporaries t1, ..., the\n"
        "# returned value ret; 'asm:' marks inline asm. constant_args: the\n"
        "# arguments lcc requires as constants (index from 0): the values\n"
        "# it accepts among -1..65536 (runs of tried values), those it\n"
        "# rejects, and what it emits for a value where that isn't the value\n"
        "# itself. short_form: what the header's _e2k_<op> does (same: the\n"
        "# same operations; error: lcc rejects the call; no key: no such\n"
        "# macro). lcc_signature: where lcc's types differ from the\n"
        "# manual's. manual_iset: where the builtin first compiles for a\n"
        "# later ISA than the manual lists it under.\n"
        + yaml.safe_dump({"builtins": results}, sort_keys=True,
                         default_flow_style=None, width=100))
    same = sum(1 for r in rows if results[r["name"]].get("asm") ==
               [f"{r['op']} " + ", ".join(
                   [f"a{k}" for k in range(len(r["args"]))] + ["ret"])])
    errs = [n for n, e in results.items() if "error" in e]
    print(f"{len(rows)} op builtins: {same} are their operation with the"
          f" arguments in order, {len(errs)} don't compile")
    for r in rows:
        e = results[r["name"]]
        if "asm" in e and e["asm"] != [f"{r['op']} " + ", ".join(
                [f"a{k}" for k in range(len(r["args"]))] + ["ret"])]:
            print("  ", r["name"], e["asm"])
    for n in errs:
        print("  error:", n, results[n]["error"][:120])


if __name__ == "__main__":
    main()
