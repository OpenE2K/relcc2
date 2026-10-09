#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write preds.s and preds-cases.h for preds.c: predicate timing and logic
(spec/isa/timing.md, semantics.md).

  python3 tools/isa-probes/gen_preds.py

i17_*: %pred0 starts false; a compare makes it true (i17l_*: predicate
logic, pass/landp/pass of two true predicates); a reader D
instructions later writes the return value: a predicated addd (1 if it
saw the new value, else 0) or a merged (9 new, 7 old).  The reader's
instruction also has a taken return (ret), a taken ct to the next label
(branch) or over three instructions (far), a call (call), a ct whose
condition is
false (ctfalse), or no transfer.
lp_*: predicate logic with a diagnostic predicate: %pred1 is cmpedb,sm
of an ldd,sm from address 16 (diagnostic), %pred0 a clean false or true;
the result goes to %pred2 and a ct on it returns 1 (taken) or 0; a trap
is a signal.  tree_N: landp (landp (c1, c2), landp (c3, c4)) on the four
low bits of N, returned as 0 or 1.
"""

from pathlib import Path

HERE = Path(__file__).resolve().parent


def wi(*ops):
    return "\t{\n" + "".join(f"\t  {o}\n" for o in ops) + "\t}\n"


def fn(name, body):
    return (f"\n\t.global\tpr_{name}\n\t.type\tpr_{name}, #function\n"
            f"\t.align\t8\npr_{name}:\n{body}\t.size\tpr_{name}, "
            f".-pr_{name}\n")


def i17(kind, d, reader, writer="cmp"):
    name = f"i17{'l' if writer == 'landp' else ''}_{kind}_{reader}_d{d}"
    b = wi("setwd\twsz = 0x8, nfx = 0x1, dbl = 0x0", "return\t%ctpr3",
           "addd,0\t0x0, 0x0, %dr0", "addd,1\t0x0, 0x7, %dr4",
           "addd,2\t0x0, 0x9, %dr5", "addd,3\t0x0, 0x0, %dr3")
    b += wi("cmpesb,0\t0x0, 0x1, %pred0", "cmpesb,1\t0x0, 0x1, %pred1",
            "cmpesb,3\t0x0, 0x0, %pred2", "cmpesb,4\t0x0, 0x0, %pred3",
            "disp\t%ctpr1, pr_leaf" if kind == "call"
            else f"disp\t%ctpr1, .L{name}_t")
    b += wi("nop 7") + wi("nop 7")
    if writer == "landp":
        # %pred0 = %pred2 and %pred3, both true, by predicate logic.
        b += wi("pass\t%pred2, @p0", "pass\t%pred3, @p1",
                "landp\t@p0, @p1, @p4", "pass\t@p4, %pred0")
    else:
        b += wi("cmpedb,0\t%dr3, 0x0, %pred0")
    if d == 2:
        b += wi("nop")
    op = ("addd,0\t0x0, 0x1, %dr0 ? %pred0" if reader == "add"
          else "merged,0\t%dr4, %dr5, %dr0, %pred0")
    ct = {"ret": ["ct\t%ctpr3"], "branch": ["ct\t%ctpr1"],
          "far": ["ct\t%ctpr1"], "call": ["call\t%ctpr1, wbs = 0x4"],
          "ctfalse": ["ct\t%ctpr1 ? %pred1"], "none": []}[kind]
    b += wi(op, *ct)
    if kind == "far":
        # Skipped by the taken branch: would spoil the result.
        b += wi("addd,0\t0x0, 0x63, %dr0") * 3
    if kind == "call":
        # The call empties the CTPRs: prepare the return again.
        b += wi("return\t%ctpr3") + wi("nop 5")
    b += f".L{name}_t:\n" + wi("nop 5") + wi("ct\t%ctpr3")
    return name, fn(name, b)


def logic(name, first_diag, op, p0):
    # %dr2 = ldd,sm from 16 (diagnostic); %pred1 = cmpedb,sm of it.
    b = wi("setwd\twsz = 0x8, nfx = 0x1, dbl = 0x0", "return\t%ctpr3",
           "addd,0\t0x0, 0x10, %dr3", "addd,1\t0x0, 0x0, %dr0")
    b += wi("ldd,0,sm\t%dr3, 0x0, %dr2", f"disp\t%ctpr1, .L{name}_t")
    b += wi("nop 7")
    b += wi("cmpedb,0,sm\t%dr2, 0x0, %pred1",
            f"cmpesb,1\t0x0, {0 if p0 else 1:#x}, %pred0")
    b += wi("nop 2")
    a, c = ("%pred1", "%pred0") if first_diag else ("%pred0", "%pred1")
    b += wi(f"pass\t{a}, @p0", f"pass\t{c}, @p1", f"{op}\t@p0, @p1, @p4",
            "pass\t@p4, %pred2")
    b += wi("nop 2")
    b += wi("ct\t%ctpr1 ? %pred2")
    b += wi("ct\t%ctpr3")
    b += f".L{name}_t:\n" + wi("addd,0\t0x0, 0x1, %dr0") + wi("ct\t%ctpr3")
    return name, fn(name, b)


def tree(n):
    name = f"tree_{n}"
    bits = [(n >> k) & 1 for k in range(4)]
    b = wi("setwd\twsz = 0x8, nfx = 0x1, dbl = 0x0", "return\t%ctpr3",
           "addd,0\t0x0, 0x0, %dr0")
    b += wi(f"disp\t%ctpr1, .L{name}_t",
            *[f"cmpesb,{c}\t0x0, {0 if bits[k] else 1:#x}, %pred{k}"
              for k, c in enumerate((0, 1, 3, 4))])
    b += wi("nop 2")
    # Two chained levels: @p4 = c1 && c2, @p5 = c3 && c4, @p6 = @p4 && @p5.
    b += wi("pass\t%pred0, @p0", "pass\t%pred1, @p1", "pass\t%pred2, @p2",
            "pass\t%pred3, @p3", "landp\t@p0, @p1, @p4",
            "landp\t@p2, @p3, @p5", "landp\t@p4, @p5, @p6",
            "pass\t@p6, %pred4")
    b += wi("nop 2")
    b += wi("ct\t%ctpr1 ? %pred4")
    b += wi("ct\t%ctpr3")
    b += f".L{name}_t:\n" + wi("addd,0\t0x0, 0x1, %dr0") + wi("ct\t%ctpr3")
    return name, fn(name, b)


def main():
    cases = [("leaf", fn("leaf", wi("setwd\twsz = 0x4, nfx = 0x1, dbl = 0x0",
                                       "return\t%ctpr3")
                                    + wi("nop 5") + wi("ct\t%ctpr3")))]
    for d in (1, 2):
        for kind in ("ret", "branch", "far", "call", "ctfalse", "none"):
            for reader in ("add", "merge"):
                cases.append(i17(kind, d, reader))
                cases.append(i17(kind, d, reader, "landp"))
    for p0 in (0, 1):
        cases.append(logic(f"lp_landp_p0{p0}", False, "landp", p0))
        cases.append(logic(f"lp_andp_p0{p0}", False, "andp", p0))
        cases.append(logic(f"lp_landp_diagfirst_p0{p0}", True, "landp", p0))
    for n in range(16):
        cases.append(tree(n))
    (HERE / "preds.s").write_text(
        "! SPDX-License-Identifier: GPL-3.0-or-later\n"
        "! Generated by tools/isa-probes/gen_preds.py; see preds.c.\n"
        "\t.ignore\tld_st_style\n\t.text\n"
        + "".join(t for _, t in cases))
    (HERE / "preds-cases.h").write_text(
        "/* SPDX-License-Identifier: GPL-3.0-or-later */\n"
        "/* Generated by tools/isa-probes/gen_preds.py.  */\n"
        + "".join(f"CASE ({n})\n" for n, _ in cases))


if __name__ == "__main__":
    main()
