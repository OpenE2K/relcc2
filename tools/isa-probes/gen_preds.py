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

The rest return 0/1 (a predicate's value or whether a ct was taken) or a
register, and get a pointer to {9, 11} in %dr0:
mv_*: movep @p0, @p1, @p4 with @p0 = %pred0 (c: 0, 1 or x, diagnostic;
nc: negated in the movep), @p1 = %pred1 (v), and %pred2 (d) before;
returns %pred2.
call_seen_P: the caller sets %pred0-31 to the bits of P and calls a
leaf that returns its own %pred0-31 as a mask; call_kept_P_V: the
caller sets P, calls a leaf that sets all 32 to V, then returns its own.
w2_cA_cB_*: compares in ALC A and B write %pred0 in one instruction;
w2_cmp_clp_*: a compare and a CLP do; returns %pred0.
same_*: a compare makes %pred0 true (from false) and, in the same
instruction, an ELP (elp), an RLP'd addd (rlp), a merge (mrgc) or a
ct (ct) reads it; or a CLP writes %pred2 that an RLP reads (clp_rlp).
spred_*: pass %spredK, @p0 in the instruction of (same) or after (next)
a compare in ALC K (true or false); returns the result, %pred2 having
been true before.  spd_*: the same next to a ,sm addd or ldd in ALC K
with a diagnostic (d) or clean (c) operand or result; spr_*: next to an
addd in ALC K whose operand a load or fmuld of the instruction before
has not delivered yet (or a ready one).
pc_*: %lsr pcnt = 3, then an addd under ? %pcntN, [~]%pred0 (or a merge
on %pcntN); returns 1 if it ran.
le_*: %lsr pcnt = 0 or 3 (vlc = 0, so %NOT_LOOP_END is true), with
semc for *_semc, then a loop_mode ct on ctcond type 6, 7, 14 or 15;
returns 1 if taken.
fm_*: a load or store at address 0 under a false (or true) predicate.
dg_*: %pred1 diagnostic (cmpedb,sm of a failed ldd,sm), then an addd
(,sm or not), ldd,sm, merge or compare,sm under ? [~]%pred1, then a
non-speculative use of the result; 7/9 executed, 5 not, a signal if
diagnostic; *_nouse return 1 unless the operation itself traps.
cr_*: a compare under a false or true predicate; returns %pred2.
callf_*: whether a call under a false predicate keeps %ctpr1 and %ctpr3.
ww_*: two operations write %dr0 in one wide instruction, both enabled.
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


def setp(vals):
    """Compares setting %predK to v for (K, v) in vals, four an instruction."""
    out = ""
    vals = list(vals)
    while vals:
        part, vals = vals[:4], vals[4:]
        out += wi(*[f"cmpesb,{c}\t0x0, {0 if v else 1:#x}, %pred{k}"
                    for (k, v), c in zip(part, (0, 1, 3, 4))])
    return out


def diag(k):
    """Make %predK diagnostic: a compare,sm of a failed ldd,sm."""
    return (wi("ldd,0,sm\t%dr3, 0x0, %dr2") + wi("nop 7")
            + wi(f"cmpedb,0,sm\t%dr2, 0x0, %pred{k}") + wi("nop 2"))


def new(name, body, extra=()):
    """%dr9 = the argument, %dr3 = 16 (an invalid address), %dr4/5 = 0/1."""
    b = wi("setwd\twsz = 0x8, nfx = 0x1, dbl = 0x0", "return\t%ctpr3",
           "addd,0\t0x0, %dr0, %dr9", "addd,1\t0x0, 0x10, %dr3",
           "addd,3\t0x0, 0x0, %dr4", "addd,4\t0x0, 0x1, %dr5")
    b += wi(f"disp\t%ctpr1, .L{name}_t", *extra)
    b += body + wi("nop 2") + wi("ct\t%ctpr3")
    # The taken path also clears %lsr, which le_* cases set.
    b += f".L{name}_t:\n" + wi("addd,1\t0x0, 0x1, %dr0",
                                "rwd,0\t0x0, %lsr") + wi("nop 5")
    b += wi("ct\t%ctpr3")
    return name, fn(name, b)


def rd(k, neg=False):
    return wi("nop 2") + wi(f"merged,0\t%dr4, %dr5, %dr0, {'~' if neg else ''}"
                            f"%pred{k}")


def mask():
    """%dr0 = a mask of %pred0-31 (a diagnostic one traps)."""
    b = wi("addd,0\t0x0, 0x0, %dr0", "addd,1\t0x0, 0x1, %dr7")
    for k in range(32):
        b += wi(f"ord,0\t%dr0, %dr7, %dr0 ? %pred{k}",
                "shld,1\t%dr7, 0x1, %dr7")
    return b


def lsr(v):
    return (wi(f"addd,0\t0x0, {v:#x}, %dr6") + wi("rwd,0\t%dr6, %lsr")
            + wi("nop 5"))


def more_cases():
    cases = []
    leafs = [("leaf_mask", wi("setwd\twsz = 0x8, nfx = 0x1, dbl = 0x0",
                              "return\t%ctpr3") + mask() + wi("ct\t%ctpr3"))]
    for v in (0, 1):
        leafs.append((f"leaf_set{v}",
                      wi("setwd\twsz = 0x8, nfx = 0x1, dbl = 0x0",
                         "return\t%ctpr3")
                      + setp((k, v) for k in range(32)) + wi("nop 5")
                      + wi("ct\t%ctpr3")))
    cases += [(n, fn(n, b)) for n, b in leafs]
    # movep: condition %pred0, value %pred1, destination %pred2.
    for c in ("0", "1", "x", "n0", "n1"):
        for v in (0, 1):
            for d in (0, 1):
                name = f"mv_c{c}_v{v}_d{d}"
                body = setp([(1, v), (2, d)] + ([] if c == "x" else
                                                [(0, c[-1] == "1")]))
                if c == "x":
                    body += diag(0)
                body += wi("nop 2")
                body += wi("pass\t%pred0, @p0", "pass\t%pred1, @p1",
                           f"movep\t{'~' if c[0] == 'n' else ''}@p0, @p1, @p4",
                           "pass\t@p4, %pred2")
                cases.append(new(name, body + rd(2)))
    for v in (0, 1):
        body = setp([(1, v), (2, 0)]) + diag(0) + wi("nop 2")
        body += wi("pass\t%pred0, @p0", "pass\t%pred1, @p1",
                   "movep\t@p0, @p1, @p4", "pass\t@p4, %pred2")
        cases.append(new(f"mv_cx_v{v}_nouse", body + wi("nop 7")
                         + wi("addd,0\t0x0, 0x1, %dr0")))
    # Predicates across calls.
    def call(leaf):
        return (wi(f"disp\t%ctpr1, pr_{leaf}") + wi("nop 4")
                + wi("call\t%ctpr1, wbs = 0x4") + wi("return\t%ctpr3"))
    for pat in (0x5a5a5a5a, 0xa5a5a5a5):
        bits = [(k, pat >> k & 1) for k in range(32)]
        cases.append(new(f"call_seen_{pat:x}", setp(bits) + wi("nop 2")
                         + call("leaf_mask")
                         + wi("addd,0\t0x0, %dr8, %dr0")))
        for v in (0, 1):
            cases.append(new(f"call_kept_{pat:x}_{v}", setp(bits)
                             + call(f"leaf_set{v}") + mask()))
    # A call under a false (p0) or true (p1) predicate: does it keep the
    # CTPRs?  ctpr1: a ct %ctpr1 after it, prepared before it, returns 1
    # (a call that empties it makes the ct trap); ctpr3: the return
    # prepared at entry, used after it, returns 5.
    for v in (0, 1):
        pre = (setp([(0, v)]) + wi("disp\t%ctpr2, pr_leaf") + wi("nop 4")
               + wi("addd,0\t0x0, 0x5, %dr0")
               + wi("call\t%ctpr2, wbs = 0x4 ? %pred0") + wi("nop 2"))
        cases.append(new(f"callf_ctpr1_p{v}", pre + wi("ct\t%ctpr1")))
        cases.append(new(f"callf_ctpr3_p{v}", pre))
    # Two writes of one predicate in one instruction.
    for a, b in ((0, 1), (0, 3), (3, 4), (1, 3)):
        for va in (0, 1):
            cases.append(new(f"w2_c{a}_c{b}_{va}{1 - va}", wi(
                f"cmpesb,{a}\t0x0, {0 if va else 1:#x}, %pred0",
                f"cmpesb,{b}\t0x0, {1 if va else 0:#x}, %pred0") + rd(0)))
    for vc in (0, 1):
        body = setp([(1, 1 - vc)]) + wi("nop 2")
        body += wi(f"cmpesb,0\t0x0, {0 if vc else 1:#x}, %pred0",
                   "pass\t%pred1, @p0", "andp\t@p0, @p0, @p4",
                   "pass\t@p4, %pred0")
        cases.append(new(f"w2_cmp_clp_{vc}{1 - vc}", body + rd(0)))
    for c in (1, 3, 4):
        for vc in (0, 1):
            body = setp([(1, 1 - vc)]) + wi("nop 2")
            body += wi(f"cmpesb,{c}\t0x0, {0 if vc else 1:#x}, %pred0",
                       "pass\t%pred1, @p0", "andp\t@p0, @p0, @p4",
                       "pass\t@p4, %pred0")
            cases.append(new(f"w2_cmp{c}_clp_{vc}{1 - vc}", body + rd(0)))
    for a, b in ((4, 5), (4, 6), (5, 6)):
        for va in (0, 1):
            # CLP a writes va, CLP b writes 1 - va, both to %pred0.
            srcs = {4: ("@p0", "@p0"), 5: ("@p2", "@p2"), 6: ("@p3", "@p3")}
            ops = ["pass\t%pred1, @p0", "pass\t%pred2, @p2",
                   "pass\t%pred3, @p3"]
            val = {a: va, b: 1 - va}
            # %pred1 feeds CLP0 (@p4), %pred2 CLP1 (@p5), %pred3 CLP2 (@p6).
            pv = {4: 1, 5: 2, 6: 3}
            body = setp([(pv[a], val[a]), (pv[b], val[b])]) + wi("nop 2")
            ops += [f"andp\t{srcs[k][0]}, {srcs[k][1]}, @p{k}"
                    for k in (4, 5, 6) if k in (a, b)]
            ops += [f"pass\t@p{k}, %pred0" for k in (a, b)]
            cases.append(new(f"w2_clp{a - 4}_clp{b - 4}_{va}{1 - va}",
                             body + wi(*ops) + rd(0)))
    # Readers in the compare's own instruction.
    pre = setp([(0, 0), (2, 0)]) + wi("nop 2") + wi("addd,0\t0x0, 0x0, %dr0")
    cases.append(new("same_elp", pre + wi(
        "cmpesb,0\t0x0, 0x0, %pred0", "pass\t%pred0, @p0",
        "andp\t@p0, @p0, @p4", "pass\t@p4, %pred2") + rd(2)))
    cases.append(new("same_rlp", pre + wi(
        "cmpesb,0\t0x0, 0x0, %pred0", "addd,1\t0x0, 0x1, %dr0 ? %pred0")))
    cases.append(new("same_mrgc", pre + wi(
        "cmpesb,0\t0x0, 0x0, %pred0", "merged,1\t%dr4, %dr5, %dr0, %pred0")))
    cases.append(new("same_ct", pre + wi("nop 5") + wi(
        "cmpesb,0\t0x0, 0x0, %pred0", "ct\t%ctpr1 ? %pred0")))
    cases.append(new("same_clp_rlp", setp([(1, 1)]) + pre + wi(
        "pass\t%pred1, @p0", "andp\t@p0, @p0, @p4", "pass\t@p4, %pred2",
        "addd,1\t0x0, 0x1, %dr0 ? %pred2")))
    # %spred.
    for k in (0, 1, 3, 4):
        for v in (0, 1):
            for when in ("same", "next"):
                cmp = f"cmpesb,{k}\t0x0, {0 if v else 1:#x}, %pred5"
                elp = ["pass\t%spred" + str(k) + ", @p0",
                       "andp\t@p0, @p0, @p4", "pass\t@p4, %pred2"]
                body = setp([(2, 1)]) + wi("nop 2")
                body += (wi(cmp, *elp) if when == "same"
                         else wi(cmp) + wi(*elp))
                cases.append(new(f"spred{k}_{when}_{v}", body + rd(2)))
    cases.append(new("spred01_same_11", setp([(2, 1)]) + wi("nop 2") + wi(
        "cmpesb,0\t0x0, 0x0, %pred5", "cmpesb,1\t0x0, 0x0, %pred6",
        "pass\t%spred01, @p0", "andp\t@p0, @p0, @p4", "pass\t@p4, %pred2")
        + rd(2)))
    # %spredK next to a ,sm operation in ALC K with a diagnostic (d) or
    # clean (c) operand, a failing (d) or valid (c) ldd,sm.
    for k in range(6):
        for kind in ("add", "ld"):
            if kind == "ld" and k not in (0, 2, 3, 5):
                continue
            for dc in ("d", "c"):
                for when in ("same", "next"):
                    op = ({"d": f"addd,{k},sm\t%dr2, 0x0, %dr6",
                           "c": f"addd,{k},sm\t%dr4, 0x0, %dr6"}[dc]
                          if kind == "add" else
                          {"d": f"ldd,{k},sm\t%dr3, 0x0, %dr6",
                           "c": f"ldd,{k},sm\t%dr9, 0x0, %dr6"}[dc])
                    elp = [f"pass\t%spred{k}, @p0", "andp\t@p0, @p0, @p4",
                           "pass\t@p4, %pred2"]
                    body = (wi("ldd,0,sm\t%dr3, 0x0, %dr2") + wi("nop 7")
                            + setp([(2, 0)]) + wi("nop 2"))
                    body += (wi(op, *elp) if when == "same"
                             else wi(op) + wi(*elp))
                    cases.append(new(f"spd_{kind}{k}_{dc}_{when}",
                                     body + rd(2)))
    # %spredK with ALC K's operand not ready yet (a load or fmuld one
    # instruction before) or ready (control).
    for k in (0, 1, 3, 4):
        for prod, nm in (("ldd,2\t%dr9, 0x0, %dr6", "ld"),
                         ("fmuld,1\t%dr4, %dr4, %dr6", "fmul"),
                         ("nop", "ready")):
            elp = [f"pass\t%spred{k}, @p0", "andp\t@p0, @p0, @p4",
                   "pass\t@p4, %pred2"]
            body = (wi("addd,0\t0x0, 0x0, %dr6") + setp([(2, 0)])
                    + wi("nop 4") + wi(prod)
                    + wi(f"addd,{k}\t%dr6, 0x1, %dr7", *elp))
            cases.append(new(f"spr_{nm}{k}", body + rd(2)))
    for k, op in ((2, "addd,2,sm\t%dr6, 0x1, %dr7"),
                  (5, "addd,5,sm\t%dr6, 0x1, %dr7"),
                  (0, "faddd,0,sm\t%dr6, %dr6, %dr7"),
                  (3, "fmuld,3,sm\t%dr6, %dr6, %dr7")):
        for prod, pn in (("ldd,0\t%dr9, 0x0, %dr6", "ld"),
                         ("fmuld,1\t%dr5, %dr5, %dr6", "fmul")):
            name = f"spr_{op.split(',')[0]}{k}_{pn}"
            elp = [f"pass\t%spred{k}, @p0", "andp\t@p0, @p0, @p4",
                   "pass\t@p4, %pred2"]
            body = (wi("addd,0\t0x0, 0x0, %dr6") + setp([(2, 0)])
                    + wi("nop 4") + wi(prod) + wi(op, *elp))
            cases.append(new(name + "_pred", body + rd(2)))
            cases.append(new(name + "_use", body + wi("nop 7")
                             + wi("addd,0\t%dr7, 0x0, %dr0")))
    for k in (2, 5):
        # A store whose data isn't ready; returns %spred, then mem[2].
        elp = [f"pass\t%spred{k}, @p0", "andp\t@p0, @p0, @p4",
               "pass\t@p4, %pred2"]
        body = (wi("addd,0\t0x0, 0x0, %dr6") + setp([(2, 0)]) + wi("nop 4")
                + wi("ldd,0\t%dr9, 0x0, %dr6")
                + wi(f"std,{k}\t%dr9, 0x10, %dr6", *elp))
        cases.append(new(f"spr_st{k}_pred", body + rd(2)))
        cases.append(new(f"spr_st{k}_mem", body + wi("nop 7")
                         + wi("ldd,0\t%dr9, 0x10, %dr0") + wi("nop 3")))
    # Which part traps: the same with a load, varied.
    for k in (0, 3):
        o = 1 if k == 0 else 4
        pre = (wi("addd,0\t0x0, 0x0, %dr6", "addd,1\t0x0, 0x0, %dr7")
               + setp([(2, 0)]) + wi("nop 4") + wi("ldd,2\t%dr9, 0x0, %dr6"))
        elp = lambda j: [f"pass\t%spred{j}, @p0", "andp\t@p0, @p0, @p4",
                         "pass\t@p4, %pred2"]
        one = wi("nop 7") + wi("addd,0\t0x0, 0x1, %dr0")
        use7 = wi("nop 7") + wi("addd,0\t%dr7, 0x0, %dr0")
        cases.append(new(f"spr_ld{k}_nouse", pre + wi(
            f"addd,{k}\t%dr6, 0x1, %dr7", *elp(k)) + one))
        cases.append(new(f"spr_ld{k}_noelp", pre + wi(
            f"addd,{k}\t%dr6, 0x1, %dr7") + use7))
        cases.append(new(f"spr_ld{k}_sm_pred", pre + wi(
            f"addd,{k},sm\t%dr6, 0x1, %dr7", *elp(k)) + rd(2)))
        cases.append(new(f"spr_ld{k}_sm_use", pre + wi(
            f"addd,{k},sm\t%dr6, 0x1, %dr7", *elp(k)) + use7))
        cases.append(new(f"spr_ld{k}_other", pre + wi(
            f"addd,{k}\t%dr6, 0x1, %dr7", *elp(o)) + use7))
        cases.append(new(f"spr_ld{k}_other_pred", pre + wi(
            f"addd,{k}\t%dr6, 0x1, %dr7", *elp(o)) + rd(2)))
        cases.append(new(f"spr_ld{k}_none", pre + wi(*elp(k)) + rd(2)))
    # Two channels in one %spred: ALC0 waiting (w) or ready (r), ALC1 too.
    for c0 in ("w", "r"):
        for c1 in ("w", "r"):
            src = lambda c: "%dr6" if c == "w" else "%dr8"
            body = (wi("addd,0\t0x0, 0x0, %dr6", "addd,1\t0x0, 0x0, %dr8")
                    + setp([(2, 0)]) + wi("nop 4")
                    + wi("ldd,2\t%dr9, 0x0, %dr6")
                    + wi(f"addd,0,sm\t{src(c0)}, 0x1, %dr7",
                         f"addd,1,sm\t{src(c1)}, 0x1, %dr10",
                         "pass\t%spred01, @p0", "andp\t@p0, @p0, @p4",
                         "pass\t@p4, %pred2"))
            cases.append(new(f"spr_two_{c0}{c1}", body + rd(2)))
    # Cycles for 1000 iterations of a load and a consumer one instruction
    # later, with and without %spred0 in the consumer's instruction.
    for nm, extra in (("spred", ["pass\t%spred0, @p0",
                                 "andp\t@p0, @p0, @p4",
                                 "pass\t@p4, %pred2"]), ("plain", [])):
        lp = f".Lspt_{nm}"
        body = (wi("addd,0\t0x0, 0x3e8, %dr12", f"disp\t%ctpr2, {lp}")
                + wi("nop 5") + wi("rrd\t%clkr, %dr10"))
        body += f"{lp}:\n" + wi("ldd,2\t%dr9, 0x0, %dr6",
                                 "subd,1\t%dr12, 0x1, %dr12")
        body += wi("addd,0,sm\t%dr6, 0x1, %dr7",
                   "cmpledb,1\t%dr12, 0x0, %pred3", *extra)
        body += wi("nop 2") + wi("ct\t%ctpr2 ? ~%pred3")
        body += (wi("rrd\t%clkr, %dr11") + wi("nop 4")
                 + wi("subd,0\t%dr11, %dr10, %dr0"))
        cases.append(new(f"spt_{nm}", body))
    cases.append(new("spred2_none", setp([(2, 1)]) + wi("nop 2") + wi(
        "pass\t%spred2, @p0", "andp\t@p0, @p0, @p4", "pass\t@p4, %pred2")
        + rd(2)))
    # %pcnt with %pred on one operation, %pcnt in a merge.
    p3 = 3 << 48
    for n in (2, 3, 5):
        for v in (0, 1):
            for neg in ("", "~"):
                cases.append(new(f"pc_pcnt{n}_{neg and 'n'}p{v}", setp([(0, v)])
                                 + wi("addd,0\t0x0, 0x0, %dr0") + lsr(p3)
                                 + wi(f"addd,0\t0x0, 0x1, %dr0 ? %pcnt{n}, "
                                      f"{neg}%pred0") + lsr(0)))
    for n in (2, 3):
        cases.append(new(f"pc_mrgc_pcnt{n}", lsr(p3) + wi(
            f"merged,0\t%dr4, %dr5, %dr0, %pcnt{n}") + lsr(0)))
    # ctcond 6, 7, 14, 15: both predicate values, pcnt 0, 1, 3, with
    # semc, and (nolm) without loop_mode.
    conds = {6: "%pred0 || %LOOP_END", 7: "~%pred0 && %NOT_LOOP_END",
             14: "~%pred0 || %LOOP_END", 15: "%pred0 && %NOT_LOOP_END"}
    for t, cond in conds.items():
        for v in (0, 1):
            for pc, semc, lm in ((0, 0, 1), (1, 0, 1), (3, 0, 1), (3, 1, 1),
                                 (3, 0, 0)):
                body = setp([(0, v)]) + wi("addd,0\t0x0, 0x0, %dr0")
                body += lsr((pc << 48) | (semc << 60))
                body += wi(f"ct\t%ctpr1 ? {cond}",
                           *(["loop_mode"] if lm else []))
                nm = (f"le_t{t}_p{v}_pcnt{pc}{'_semc' if semc else ''}"
                      f"{'' if lm else '_nolm'}")
                cases.append(new(nm, body + lsr(0)))
    # Memory operations at address 0 under a predicate.
    for v, neg in ((0, ""), (1, "~"), (1, "")):
        nm = f"{'n' if neg else ''}p{v}"
        cases.append(new(f"fm_ld_{nm}", setp([(0, v)]) + wi(
            "addd,0\t0x0, 0x7, %dr0") + wi(
            f"ldd,0\t0x0, 0x0, %dr0 ? {neg}%pred0") + wi("nop 3")))
        cases.append(new(f"fm_st_{nm}", setp([(0, v)]) + wi(
            "addd,0\t0x0, 0x7, %dr0") + wi(
            f"std,2\t0x0, 0x0, %dr5 ? {neg}%pred0")))
    # ,sm operations under a diagnostic predicate.
    for neg in ("", "~"):
        nm = "n" if neg else "p"
        use = wi("nop 7") + wi("addd,0\t%dr6, 0x0, %dr0")
        init = wi("addd,0\t0x0, 0x5, %dr6")
        cases.append(new(f"dg_addsm_{nm}", init + diag(1) + wi(
            f"addd,0,sm\t0x0, 0x7, %dr6 ? {neg}%pred1") + use))
        cases.append(new(f"dg_add_{nm}", init + diag(1) + wi(
            f"addd,0\t0x0, 0x7, %dr6 ? {neg}%pred1") + use))
        cases.append(new(f"dg_ldsm_{nm}", init + diag(1) + wi(
            f"ldd,0,sm\t%dr9, 0x0, %dr6 ? {neg}%pred1") + use))
        cases.append(new(f"dg_cmpsm_{nm}", setp([(3, 0)]) + diag(1) + wi(
            f"cmpesb,0,sm\t0x0, 0x0, %pred3 ? {neg}%pred1") + rd(3)))
    # The same without a use: 1 if the operation itself doesn't trap.
    for neg in ("", "~"):
        nm = "n" if neg else "p"
        for op in (f"addd,0,sm\t0x0, 0x7, %dr6 ? {neg}%pred1",
                   f"addd,0\t0x0, 0x7, %dr6 ? {neg}%pred1",
                   f"ldd,0,sm\t%dr9, 0x0, %dr6 ? {neg}%pred1",
                   f"cmpesb,0,sm\t0x0, 0x0, %pred3 ? {neg}%pred1"):
            k = op.split("\t")[0].replace(",0", "").replace(",", "")
            cases.append(new(f"dg_{k}_{nm}_nouse", diag(1) + wi(op)
                             + wi("nop 7") + wi("addd,0\t0x0, 0x1, %dr0")))
    for neg, nm in (("", "p"), ("~", "n")):
        cases.append(new(f"dg_mrgsm_{nm}_nouse", diag(1) + wi(
            f"merged,0,sm\t%dr4, %dr5, %dr6, {neg}%pred1") + wi("nop 7")
            + wi("addd,0\t0x0, 0x1, %dr0")))
        cases.append(new(f"dg_mrgsm_{nm}_use", diag(1) + wi(
            f"merged,0,sm\t%dr4, %dr5, %dr6, {neg}%pred1") + wi("nop 7")
            + wi("addd,0\t%dr6, 0x0, %dr0")))
        cases.append(new(f"dg_mrg_{nm}_nouse", diag(1) + wi(
            f"merged,0\t%dr4, %dr5, %dr6, {neg}%pred1") + wi("nop 7")
            + wi("addd,0\t0x0, 0x1, %dr0")))
        # A ct on the diagnostic predicate: taken returns 1, else 0.
        cases.append(new(f"dg_ct_{nm}", diag(1) + wi("nop 5") + wi(
            "addd,0\t0x0, 0x0, %dr0") + wi(f"ct\t%ctpr1 ? {neg}%pred1")))
    cases.append(new("dg_mrg_nouse", diag(1) + wi(
        "merged,0\t%dr4, %dr5, %dr6, %pred1") + wi("nop 7")
        + wi("addd,0\t0x0, 0x1, %dr0")))
    cases.append(new("dg_mrgsm_nouse", diag(1) + wi(
        "merged,0,sm\t%dr4, %dr5, %dr6, %pred1") + wi("nop 7")
        + wi("addd,0\t0x0, 0x1, %dr0")))
    cases.append(new("dg_mrgsm", wi("addd,0\t0x0, 0x5, %dr6") + diag(1) + wi(
        "merged,0,sm\t%dr4, %dr5, %dr6, %pred1") + wi("nop 7")
        + wi("addd,0\t%dr6, 0x0, %dr0")))
    # A compare under a predicate.
    for v in (0, 1):
        cases.append(new(f"cr_p{v}", setp([(0, v), (2, 0)]) + wi("nop 2")
                         + wi("cmpesb,1\t0x0, 0x0, %pred2 ? %pred0") + rd(2)))
    # Two enabled writes of %dr0 in one instruction ({9, 11} at %dr9).
    for name, ops in (
            ("ww_alu0_ld2", ["addd,0\t0x0, 0x7, %dr0", "ldd,2\t%dr9, 0x0, %dr0"]),
            ("ww_ld2_alu3", ["ldd,2\t%dr9, 0x0, %dr0", "addd,3\t0x0, 0x7, %dr0"]),
            ("ww_alu3_ld5", ["addd,3\t0x0, 0x7, %dr0", "ldd,5\t%dr9, 0x0, %dr0"]),
            ("ww_ld0_ld2", ["ldd,0\t%dr9, 0x0, %dr0", "ldd,2\t%dr9, 0x8, %dr0"]),
            ("ww_ld2_ld3", ["ldd,2\t%dr9, 0x0, %dr0", "ldd,3\t%dr9, 0x8, %dr0"])):
        cases.append(new(name, wi(*ops) + wi("nop 5")))
    return cases


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
        cases.append(logic(f"lp_andp_diagfirst_p0{p0}", True, "andp", p0))
    for n in range(16):
        cases.append(tree(n))
    cases += more_cases()
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
