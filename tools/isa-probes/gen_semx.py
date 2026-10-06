#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write semx.s and semx-ops.h: the v3 scalar ALU operations no other
probe runs (32-bit compares, single and register-result FP compares,
conversions, fsub, single fmin/fmax, fscale, insfs, mpsadbh, the 32x32
and 64/32 multiplies and divides, the x86-translator flag forms and
cctop*, the combined merge_*), for semx.c (spec/isa/semantics.md,
"Other scalar operations").  semp.c covers packed operations, sem5.c v5,
fcomb.c and icomb.c the other combined ones; the 80-bit (fx*) family,
memory, tag and protected-mode operations are probed elsewhere.

  python3 tools/isa-probes/gen_semx.py

Each function is  void sx_N(const u64 in[3], u64 out[2]): it loads
in[0..2] into %dr2..%dr4, clears the %pfpfr flags, presets %dr5 to
0x5555555555555555, runs the operation in its first allowed channel and
stores the whole %dr5 in out[0] (for a predicate result: 1 if the
predicate is true, else 0) and %pfpfr in out[1].  merge_* runs twice,
with its predicate false (_f) and true (_t).
"""

import json
from pathlib import Path

import yaml

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
REG = {"d": "%dr", "s": "%r", "q": "%qr", "p": "%qpr"}
SKIP_PREFIX = ("fx", "movx", "movtc", "movtr", "gettag", "gettc", "gettst",
               "invtc", "puttag", "aptoap", "cudtoap", "gdtoap", "getsap",
               "getpl", "vfsi", "ld", "st", "aaur", "idtofx", "istofx",
               "fstofx")
KINDS = {"ALOPF1", "ALOPF2", "ALOPF11", "ALOPF12", "ALOPF7", "ALOPF8",
         "ALOPF21", "ALOPF21_MERGE"}
# Named in another probe's text but never run there.
FORCE = {"insfd", "insfs", "mpsadbh"}
# This sweep's own files (semx, semfx, semc and their helpers): not "other
# probes".
SWEEP = ("semx", "semfx", "semc", "divx", "stmqp")
# v4 and later only: in semx-v4.s, for the 8C2.
V4 = {"fscaled", "fscales"}


def covered():
    """Operations other probes already run (see tools/isa-probes)."""
    import glob
    import re
    words = set()
    for f in (glob.glob(str(HERE / "*.s")) + glob.glob(str(HERE / "gen_*.py"))
              + glob.glob(str(ROOT / "tools/abi/probes/*.s"))
              + [str(ROOT / "tools/isa-timing/bench.py")]):
        if any(k in Path(f).name for k in SWEEP):
            continue
        words |= set(re.findall(r"\b[a-z][a-z0-9_]*\b", Path(f).read_text()))
    return words


def ops(v4=False):
    data = yaml.safe_load((ROOT / "spec/isa/binutils-ops.yaml").read_text())
    done = covered()
    out = {}
    for e in data["alu_ops"]:
        n = e["name"]
        i = set(e.get("isets") or [])
        if n in V4 and v4:
            out.setdefault(n, (e["args"], e["channels"][0], e["alopf"]))
            continue
        if v4 or (3 not in i or (n in done and n not in FORCE)
                  or n.startswith(SKIP_PREFIX)
                  or e.get("alopf") not in KINDS or n.startswith("p")
                  or min(i) > 3):
            continue
        out.setdefault(n, (e["args"], e["channels"][0], e["alopf"]))
    res = []
    for n, (args, ch, alopf) in sorted(out.items()):
        if alopf == "ALOPF21_MERGE":
            res += [(f"{n}_f", n, args, ch, alopf, 0),
                    (f"{n}_t", n, args, ch, alopf, 1)]
        else:
            res.append((n, n, args, ch, alopf, None))
    return res


def body(name, args, ch, alopf, pred):
    srcs = [f"{REG[a]}{2 + k}" for k, a in enumerate(args[:-1])]
    dst = f"{REG[args[-1]]}5"
    pre, post = [], []
    if alopf == "ALOPF7":               # compare: two sources -> predicate
        srcs = [f"{REG[a]}{2 + k}" for k, a in enumerate(args)]
        op = f"{name},{ch}\t{', '.join(srcs)}, %pred1"
        post = ["addd,0\t0x0, 0x0, %dr5", "addd,0\t0x0, 0x1, %dr5 ? %pred1"]
    elif alopf == "ALOPF8":             # cctop: one source -> predicate
        op = f"{name},{ch}\t{REG[args[0]]}2, %pred1"
        post = ["addd,0\t0x0, 0x0, %dr5", "addd,0\t0x0, 0x1, %dr5 ? %pred1"]
    elif alopf == "ALOPF21_MERGE":
        pre = [f"cmpesb,0\t0x0, {0 if pred else 1:#x}, %pred1"]
        op = f"{name},{ch}\t{', '.join(srcs)}, {dst}, %pred1"
    elif alopf in ("ALOPF2", "ALOPF12"):
        op = f"{name},{ch}\t{REG[args[0]]}2, {dst}"
    else:
        op = f"{name},{ch}\t{', '.join(srcs)}, {dst}"
    return pre, op, post


def fn(i, label, name, args, ch, alopf, pred):
    pre, op, post = body(name, args, ch, alopf, pred)
    w = lambda *o: "\t{\n" + "".join(f"\t  {x}\n" for x in o) + "\t}\n"
    b = w("setwd\twsz = 0x8, nfx = 0x1, dbl = 0x0", "return\t%ctpr3")
    b += w("rrs,0\t%pfpfr, %r8")
    b += w("ldd,0\t%dr0, 0x0, %dr2", "ldd,2\t%dr0, 0x8, %dr3",
           "ldd,3\t%dr0, 0x10, %dr4",
           "addd,1\t0x0, _f64,_lts0 0x5555555555555555, %dr5")
    b += w("ands,0\t%r8, 0xffffffc0, %r9")
    b += w("rws,0\t%r9, %pfpfr")
    b += "".join(w(p) for p in pre)
    b += w("nop 7")
    b += w(op)
    b += w("nop 7") + w("nop 7")
    b += "".join(w(p) for p in post)
    b += w("nop 2", "rrs,0\t%pfpfr, %r9")
    b += w("std,2\t%dr1, 0x0, %dr5", "stw,5\t%dr1, 0x8, %r9")
    b += w("rws,0\t%r8, %pfpfr") + w("nop 5") + w("ct\t%ctpr3")
    return (f"\n\t.global\tsx_{i}\n\t.type\tsx_{i}, #function\n"
            f"\t.align\t8\nsx_{i}:\n{b}\t.size\tsx_{i}, .-sx_{i}\n")


def main():
    ls = ops()
    l4 = ops(v4=True)
    for path, cases, base in (("semx", ls, 0), ("semx-v4", l4, 1000)):
        (HERE / f"{path}.s").write_text(
            "! SPDX-License-Identifier: GPL-3.0-or-later\n"
            "! Generated by tools/isa-probes/gen_semx.py; see semx.c.\n"
            "\t.ignore\tld_st_style\n\t.text\n"
            + "".join(fn(base + i, *o) for i, o in enumerate(cases)))
        (HERE / f"{path}-ops.h").write_text(
            "/* SPDX-License-Identifier: GPL-3.0-or-later */\n"
            "/* Generated by tools/isa-probes/gen_semx.py.  */\n"
            + "".join(f'OP ({base + i}, "{o[0]}")\n'
                      for i, o in enumerate(cases)))
    print(len(ls), "v3 cases,", len(l4), "v4+ cases")


if __name__ == "__main__":
    main()
