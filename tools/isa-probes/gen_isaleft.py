#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write isaleft.s, isaleft-v5.s and their -cases.h files for isaleft.c:
the ISA leftovers of the 2026-10-12 audit (spec/isa/predicates.md,
speculation.md, x86-support.md, apb.md, loops.md, hwbugs.md;
spec/abi/registers.md).

  python3 tools/isa-probes/gen_isaleft.py

Every function is  long il_NAME(long, long, long, long); the kind
(second field of CASE in the -cases.h files) says what the arguments
are and what isaleft.c prints:

  D  gen_spec.py's shape (out, a, b, c): out[0] = %dr8 with its tag
     cleared, out[1] its tag, out[2] = 1 if an rbranch (%MLOCK) was
     taken; returns the %clkr cycles of the groups.  dl_*: a ,sm load
     from a under %pred1 = a diagnostic predicate (p; n: negated), a
     clean true (t) or false (f) one, or none (u); sm is MAS 0 (semi-
     speculative), s3 MAS 0x3 (fully speculative).  dk_*: the same for
     a lock load (MAS 0x4) of b[0] into %dr8, then a check load and
     rbranch: no MLOCK means the lock load recorded a DAM entry.
  T  (out, bad1, bad2): the caller sets %pred0-31 to 0x5a5a5a5a and
     makes %pred1, %pred5, %pred30 diagnostic (pt_mk: none), records
     each predicate's value and tag (addd,sm under it, then gettagd)
     into out[128..191], calls a leaf, records them again into
     out[64..127].  The leaves record theirs into out[0..63] (tags),
     set all 32 true (set1), make %pred2 and %pred7 diagnostic first
     (mkdiag), use %pred1 without ,sm (use: %dr0 = 1 if it ran; the
     caller stores it in out[200]), or load from bad2 (sig: a SIGSEGV
     whose handler records sigcontext.cr0_lo; pt_sig's caller first
     loads from bad1).
  A  gen_apb.py's shape (out, b1, b2): value and tag of each mova (and
     128 bits of movaqp at out[192..]); *_sg cases load from b2 between
     movas 1 and 2 (isaleft.c makes it a PROT_NONE page whose SIGSEGV
     handler records the AAU state the kernel saved in uc_extra).
     q_*: movaqp from areas of fmt F and mrng M; m_*: narrower movas
     against mrng (m_gap*: bap to first mova distance); bf*: %aabfN
     writes (channel 2 or 5) before the trap, then (bf0_*) an AAU
     register write; aa_*: no program, only register writes, then the
     trap.  Not cases: il_ct1, il_ct2 (a taken ct through %ctpr1 or
     %ctpr2; isaleft.c's seq calls them between programs).
  U  (out, base, on, off): %upsr = on before ldisp/bap (pre) or after
     bap and the gap (post), the movas, %upsr = off; results as A.
  S  (a, lsr, out, d): loop_mode loop of %lsr (to set ldovl, ldmc), one
     movad of a[i] per iteration (area of asz 0, 2 or 5); iteration i
     stores 0x5555555555555555 into a[i + d]; out[2i], out[2i + 1] get
     what movad returned and its tag; returns the iterations.
  M  gen_apb_stream.py's stream (a, lsr, &cycles) with %lsr as given.
  Helpers, not cases (v5+): il_qcall_nfxN, il_qsig_nfxN keep a 128-bit
  value in a window register of a nfx = N window across a deep call or
  a signal handler (isaleft.c's qspill).
"""

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import gen_apb          # noqa: E402  case(), fapb(), mova(), seq()
import gen_apb_stream   # noqa: E402  fn(): one-area streaming loop
import gen_preds        # noqa: E402  setp(): compares into predicates
import gen_spec         # noqa: E402  case(): %clkr-timed groups

NOP = ["nop 7"]
GROUPS = {"": [], "-v5": []}


def wi(*ops):
    return "\t{\n" + "".join(f"\t  {o}\n" for o in ops) + "\t}\n"


def nops(n):
    out = ""
    while n > 0:
        k = min(n, 8)
        out += wi(f"nop {k - 1}" if k > 1 else "nop")
        n -= k
    return out


def add(group, name, kind, text, r=0, a=0):
    GROUPS[group].append((name, kind, r, a, text))


def func(name, body, wsz=0x8):
    fn = f"il_{name}"
    return (f"\n\t.global\t{fn}\n\t.type\t{fn}, #function\n\t.align\t8\n{fn}:\n"
            + wi(f"setwd\twsz = {wsz:#x}, nfx = 0x1, dbl = 0x0",
                 "return\t%ctpr3")
            + body + wi("nop 5") + wi("ct\t%ctpr3")
            + f"\t.size\t{fn}, .-{fn}\n")


# --- 1. ,sm loads under a diagnostic predicate (D) -----------------------

def spec_case(name, groups):
    """gen_spec.case(), renamed to il_NAME; '%%NEXT' in an operation is a
    label on the next wide instruction (an rbranch that goes on either
    way)."""
    text = gen_spec.case(name, groups).replace(f"spec_{name}", f"il_{name}")
    k = 0
    while "%%NEXT" in text:
        lab = f".L{name}_n{k}"
        i = text.index("%%NEXT")
        text = text[:i] + lab + text[i + len("%%NEXT"):]
        j = text.index("\t}\n", i) + 3
        text = text[:j] + f"{lab}:\n" + text[j:]
        k += 1
    return text


# %pred1 diagnostic: a ,sm compare of a failed ,sm load (address 16).
DP = [["addd,0\t0x0, 0x10, %dr20"], ["ldd,0,sm\t%dr20, 0x0, %dr21"], NOP,
      ["cmpedb,0,sm\t%dr21, 0x0, %pred1"], ["nop 2"]]
PT = [["cmpesb,0\t0x0, 0x0, %pred1"], ["nop 2"]]
PF = [["cmpesb,0\t0x0, 0x1, %pred1"], ["nop 2"]]
CONDS = {"p": (DP, " ? %pred1"), "n": (DP, " ? ~%pred1"),
         "t": (PT, " ? %pred1"), "f": (PF, " ? %pred1"), "u": (DP, "")}


def section_pred():
    for kind, mas in (("sm", ""), ("s3", ", mas=0x3")):
        for c, (pre, cond) in CONDS.items():
            add("", f"dl_{kind}_{c}", "D", spec_case(
                f"dl_{kind}_{c}",
                pre + [[f"ldd,0,sm\t%dr1, 0x0, %dr8{mas}{cond}"], NOP, NOP]))
    # Other load channels and a word load under the diagnostic predicate.
    for ch in (2, 3, 5):
        add("", f"dl_sm{ch}_p", "D", spec_case(
            f"dl_sm{ch}_p", DP + [[f"ldd,{ch},sm\t%dr1, 0x0, %dr8 ? %pred1"],
                                  NOP, NOP]))
    add("", "dl_smw_p", "D", spec_case(
        "dl_smw_p", DP + [["ldw,0,sm\t%dr1, 0x0, %r8 ? %pred1"], NOP, NOP]))
    # DAM: clear any entry of %dr8 (a check, either way on), make the
    # predicate, lock-load b[0] under it, check.
    clr = [["ldd,5\t%dr2, 0x0, %dr8, mas=0x3", "rbranch\t%%NEXT"], NOP]
    chk = [["ldd,5\t%dr2, 0x0, %dr8, mas=0x3", "rbranch\t%%LOCKED"], NOP]
    for c, (pre, cond) in CONDS.items():
        add("", f"dk_{c}", "D", spec_case(
            f"dk_{c}", clr + pre
            + [[f"ldd,0,sm\t%dr2, 0x0, %dr8, mas=0x4{cond}"], NOP] + chk))
    add("", "dk_none", "D", spec_case("dk_none", clr + DP + [NOP] + chk))


# --- 2. Predicate tags across a call (T) ----------------------------------

PAT = 0x5a5a5a5a


def rdtags(off):
    """out (%dr0) + OFF: each predicate's addd,sm result (0 false, 1
    true, diagnostic) with its tag cleared, then the tags at + 256."""
    b = ""
    for k in range(32):
        b += wi("addd,0\t0x0, 0x0, %dr4")
        b += wi(f"addd,0,sm\t0x0, 0x1, %dr4 ? %pred{k}")
        b += wi("gettagd,2\t%dr4, %r5", "puttagd,5\t%dr4, 0x0, %dr6")
        b += wi("nop 2")
        b += wi(f"std,2\t%dr0, {off + 8 * k:#x}, %dr6",
                f"stw,5\t%dr0, {off + 256 + 8 * k:#x}, %r5")
    return b


def mkdp(preds, tmp):
    b = wi(f"addd,0\t0x0, 0x10, {tmp}") + wi(f"ldd,0,sm\t{tmp}, 0x0, {tmp}")
    b += wi("nop 7")
    b += wi(*[f"cmpedb,{c},sm\t{tmp}, 0x0, %pred{k}"
              for k, c in zip(preds, (0, 1, 3, 4))])
    return b + wi("nop 2")


def section_ptag():
    leaves = {
        "tags": rdtags(0),
        "set1": gen_preds.setp((k, 1) for k in range(32)) + wi("nop 2"),
        "mkdiag": mkdp((2, 7), "%dr3") + rdtags(0),
        "use": wi("addd,0\t0x0, 0x0, %dr0")
        + wi("addd,0\t0x0, 0x1, %dr0 ? %pred1") + wi("nop 2"),
        "sig": rdtags(0) + wi("ldd,0\t%dr1, 0x0, %dr2") + wi("nop 7"),
    }
    for nm, body in leaves.items():
        add("", f"leaf_{nm}", "-", func(f"leaf_{nm}", body))
    for name, leaf, dp, sig in (("pt_seen", "tags", True, False),
                                ("pt_kept", "set1", True, False),
                                ("pt_mk", "mkdiag", False, False),
                                ("pt_use", "use", True, False),
                                ("pt_sig", "sig", True, True)):
        b = gen_preds.setp((k, PAT >> k & 1) for k in range(32))
        if dp:
            b += mkdp((1, 5, 30), "%dr7")
        b += rdtags(0x400)
        if sig:
            b += wi("ldd,0\t%dr1, 0x0, %dr7") + wi("nop 7")
        b += wi("addd,0\t0x0, %dr0, %dr8", "addd,1\t0x0, %dr2, %dr9")
        b += wi(f"disp\t%ctpr1, il_leaf_{leaf}") + wi("nop 4")
        b += wi("call\t%ctpr1, wbs = 0x4") + wi("return\t%ctpr3")
        b += wi("std,2\t%dr0, 0x640, %dr8")
        b += rdtags(0x200)
        add("", name, "T", func(name, b))


# --- 3. APB: movaqp and mrng (A), %aabf (A) -------------------------------

BASE = [("%aad0", "%dr1")]
LOAD = [["ldd,0\t%dr2, 0x0, %dr3"], NOP, NOP]


def apb(group, name, *a, **kw):
    text = gen_apb.case(name, *a, **kw).replace(f"apb_{name}", f"il_{name}")
    # A diagnostic movaqp result must not trap its readout.
    text = text.replace("stqp,2\t%dr0, 0x6", "stqp,2,sm\t%dr0, 0x6")
    r, n = gen_apb.case.counts
    add(group, name, "A", text, r, n)


def section_apbq():
    seq = gen_apb.seq
    # Narrower movas against mrng (v3 file: every machine).
    for name, fmt, mrng, op, n in (("m_d4_m4", 4, 4, 4, 2),
                                   ("m_d3_m4", 3, 4, 4, 2),
                                   ("m_d3_m8", 3, 8, 4, 3),
                                   ("m_d3_m16", 3, 16, 4, 3),
                                   ("m_w2_m2", 2, 2, 3, 2),
                                   ("m_w2_m4", 2, 4, 3, 3),
                                   ("m_d1_m8", 1, 8, 4, 3),
                                   ("m_d2_m8", 2, 8, 4, 3)):
        apb("", name, [({"fmt": fmt, "mrng": mrng}, None)], seq(n, fmt=op),
            BASE)
    # The bap -> first mova distance (2C3's stand-alone SIGILL).
    for gap in (21, 60, 200, 1000):
        apb("", f"m_gap{gap}", [({}, None)], seq(3), BASE, gap=gap)
    # The same ended as lcc ends its loops, with an %aabf0 write.
    apb("", "m_gap21_bf", [({}, None)], seq(3), BASE,
        between={2: aabf_writes([("aabf0", 2, 0)])})
    # movaqp (v5+): fmt F, mrng M (0 = 32), ind.
    for f, m in ((4, 8), (4, 16), (4, 24), (4, 0), (5, 8), (5, 16), (5, 0),
                 (3, 16), (2, 16), (1, 16), (3, 8)):
        apb("-v5", f"q_f{f}_m{m}", [({"fmt": f, "mrng": m}, None)],
            seq(3, fmt=5), BASE)
    for m in (24, 16, 0):
        apb("-v5", f"q_f4_m{m}_i8", [({"fmt": 4, "mrng": m}, None)],
            seq(2, fmt=5, ind=8), BASE)
    apb("-v5", "q_f4_m16_mis4", [({"fmt": 4, "mrng": 16}, None)],
        seq(3, fmt=5), [("%aad0", ("%dr1", 4))])
    apb("-v5", "q_f4_m8_sg", [({"fmt": 4, "mrng": 8}, None)], seq(3, fmt=5),
        BASE, between={0: LOAD})
    apb("-v5", "q_f4_m16_sg", [({"fmt": 4, "mrng": 16}, None)],
        seq(3, fmt=5), BASE, between={0: LOAD})


def aabf_writes(writes):
    ops = []
    for reg, ch, v in writes:
        ops += [[f"addd,0\t0x0, _f64,_lts0 {v:#x}, %dr3"],
                [f"aaurw,{ch}\t%r3, %{reg}"]]
    return ops


def section_aabf():
    seq = gen_apb.seq
    bf = lambda name, writes, extra=(), **kw: apb(
        "", name, [({}, None)], seq(4), BASE,
        between={1: aabf_writes(writes) + [NOP] + list(extra) + LOAD}, **kw)
    bf("bf_none_sg", [])
    for n in range(16):
        bf(f"bf{n}_sg", [(f"aabf{n}", 2, 0)])
    for n in range(4):
        bf(f"bf{n}c5_sg", [(f"aabf{n}", 5, 0)])
    bf("bf2f_sg", [("aabf2", 2, 0xffffffff)])
    bf("bf0_2_sg", [("aabf0", 2, 0), ("aabf2", 2, 0)])
    bf("bf2_0_sg", [("aabf2", 2, 0), ("aabf0", 2, 0)])
    bf("bf0_bap_sg", [("aabf0", 2, 0)],
       [["ldisp\t%ctpr2, .Lbf0_bap_sg_prog"], ["bap"], NOP, NOP, NOP])
    bf("bf0_eapbap_sg", [("aabf0", 2, 0)],
       [["eap"], NOP, ["bap"], NOP, NOP, NOP])
    # Written before bap (gen_apb's setup), then the trap.
    apb("", "bf_pre_sg", [({}, None)], seq(4), BASE + [("%aabf0", 0)],
        between={1: LOAD})
    # The right half (area 32, channel 2): the write in channel 2 or 5.
    for ch in (2, 5):
        apb("", f"bfr0c{ch}_sg", [({}, {"disp": 512})],
            [[gen_apb.mova(2)]] * 4, BASE,
            between={1: aabf_writes([("aabf0", ch, 0)]) + [NOP] + LOAD})
    # What sets the flags again after %aabf0: an AAU register write
    # between it and the trap.
    for nm, op in (("aad", "aaurwd,2\t%dr1, %aad0"),
                   ("aaind", "aaurwd,2\t%dr3, %aaind1"),
                   ("aaincr", "aaurw,2\t%r3, %aaincr1"),
                   ("aasti", "aaurwd,2\t%dr3, %aasti1")):
        bf(f"bf0_{nm}_sg", [("aabf0", 2, 0)],
           [["addd,0\t0x0, 0x0, %dr3"], [op], NOP])
    # No program at all: the AAU state a trap saves after only register
    # writes (or none).
    for nm, ops in (("none", []), ("aad", ["aaurwd,2\t%dr1, %aad0"]),
                    ("aasti", ["aaurwd,2\t%dr1, %aasti1"]),
                    ("aaind", ["aaurwd,2\t%dr1, %aaind1"])):
        body = "".join(wi(o) for o in ops) + "".join(wi(*g) for g in LOAD)
        add("", f"aa_{nm}_sg", "A", func(f"aa_{nm}_sg", body, wsz=0x10))
    # Both halves streaming, one write: which halves stop?
    apb("", "bflr_sg", [({}, {"disp": 512})],
        [[gen_apb.mova(0), gen_apb.mova(2)]] * 4, BASE,
        between={1: aabf_writes([("aabf0", 2, 0)]) + [NOP] + LOAD})
    apb("", "bflr_none_sg", [({}, {"disp": 512})],
        [[gen_apb.mova(0), gen_apb.mova(2)]] * 4, BASE, between={1: LOAD})


# --- 4. UPSR.ac and APB (U) ------------------------------------------------

def ac_case(group, name, fields, rows, off=0, when="pre", setup=(), extra=()):
    """(out, base, on, off): the area at base + OFF; %upsr = on (%r2)
    before ldisp/bap (pre) or after the gap (post), then the movas, then
    %upsr = off (%r3)."""
    on = wi("rws,0\t%r2, %upsr") + wi("wait\tall_c = 1")
    b = wi("setwd\twsz = 0x20, nfx = 0x1, dbl = 0x0", "return\t%ctpr3")
    b += wi(f"addd,0\t%dr1, {off:#x}, %dr7")
    for reg, v in setup:
        b += wi(f"addd,0\t0x0, _f64,_lts0 {v:#x}, %dr6")
        b += wi(f"aaurwd,2\t%dr6, {reg}")
    if when == "pre":
        b += on
    b += wi(f"ldisp\t%ctpr2, .L{name}_prog", "aaurwd,2\t%dr7, %aad0")
    b += wi("bap") + nops(21)
    if when == "post":
        b += on
    b += wi("rrd,0\t%clkr, %dr4")
    dst, quads = 8, []
    for row in rows:
        ops = []
        for ch, text in row:
            q = text.startswith("movaqp")
            ops.append(f"{text}, %qpr{dst}" if q else f"{text}, %dr{dst}")
            if q:
                quads.append(dst)
            dst += 1
        b += wi(*ops)
    b += wi("rrd,0\t%clkr, %dr5")
    b += "".join(wi(o) for o in extra)
    b += wi("rws,0\t%r3, %upsr") + wi("wait\tall_c = 1")
    b += wi("nop 7")
    for k in range(8, dst):
        b += wi(f"gettagd,2\t%dr{k}, %r6", f"puttagd,5\t%dr{k}, 0x0, %dr7")
        b += wi("nop 2")
        b += wi(f"std,2\t%dr0, {(k - 8) * 16:#x}, %dr7",
                f"std,5\t%dr0, {(k - 8) * 16 + 8:#x}, %dr6")
    for k in quads:
        b += wi(f"stqp,2\t%dr0, {0x600 + (k - 8) * 16:#x}, %qpr{k}")
    b += wi("disp\t%ctpr2, disp=0x0", "subd,0\t%dr5, %dr4, %dr0")
    b += wi("ct\t%ctpr3")
    prog = wi(gen_apb.fapb("l", True, fields))
    fn = f"il_{name}"
    add(group, name, "U",
        f"\n\t.global\t{fn}\n\t.type\t{fn}, #function\n\t.align\t8\n{fn}:\n"
        + b + f"\t.align\t8\n.L{name}_prog:\n" + prog
        + f"\t.size\t{fn}, .-{fn}\n", dst - 8, 0)


def section_ac():
    seq = gen_apb.seq
    d = {"fmt": 4, "mrng": 8}
    ac_case("", "ac_d_al", d, seq(3))
    ac_case("", "ac_d_m4", d, seq(3), off=4)
    ac_case("", "ac_d_m1", d, seq(3), off=1)
    ac_case("", "ac_d_m4_post", d, seq(3), off=4, when="post")
    ac_case("", "ac_w_m2", {"fmt": 3, "mrng": 4}, seq(3, fmt=3), off=2)
    ac_case("", "ac_h_m1", {"fmt": 2, "mrng": 2}, seq(3, fmt=2), off=1)
    ac_case("", "ac_b_m1", {"fmt": 1, "mrng": 1}, seq(3, fmt=1), off=1)
    ac_case("", "ac_w12", {"fmt": 1, "mrng": 8, "incr": 1}, seq(3, fmt=4),
            setup=[("%aaincr1", 12)])
    ac_case("", "ac_ind4", {"fmt": 1, "mrng": 0, "incr": 1},
            seq(3, fmt=4, ind=4), setup=[("%aaincr1", 16)])
    # Control: the same with a misaligned ldd while %upsr = on.
    ac_case("", "ac_ctl_ldd", d, seq(3), off=4,
            extra=["ldd,0\t%dr1, 0x4, %dr20", "nop 7"])
    q = {"fmt": 5, "mrng": 16}
    ac_case("-v5", "ac_q_al", q, seq(3, fmt=5))
    ac_case("-v5", "ac_q_m8", q, seq(3, fmt=5), off=8)
    ac_case("-v5", "ac_q_m4", q, seq(3, fmt=5), off=4)
    # Helpers (not cases): a taken ct through %ctpr1 or %ctpr2, the way
    # glibc's v6 string functions branch (seq's @ct1, @ct2).
    for c in (1, 2):
        add("", f"ct{c}", "-", func(f"ct{c}", wi(f"disp\t%ctpr{c}, .Lct{c}_t")
                                    + wi("nop 4") + wi(f"ct\t%ctpr{c}")
                                    + wi("addd,0\t0x0, 0x1, %dr0")
                                    + f".Lct{c}_t:\n"))
    # Helpers (not cases): %upsr read and write.
    add("", "rd_upsr", "-", func("rd_upsr", wi("rrs,0\t%upsr, %r0")))
    add("", "wr_upsr", "-", func("wr_upsr", wi("rws,0\t%r0, %upsr")
                                  + wi("wait\tall_c = 1")))


# --- 5. ldovl (S, M) -------------------------------------------------------

def ovst(asz):
    name = f"ovst_a{asz}"
    b = wi(f"ldisp\t%ctpr2, .L{name}_prog", "aaurwd,2\t%dr0, %aad0",
           "addd,1\t0x0, 0x0, %dr10", "addd,3\t%dr2, 0x0, %dr14",
           "shld,4\t%dr3, 0x3, %dr15")
    b += wi("addd,0\t%dr0, %dr15, %dr15",
            "addd,1\t0x0, _f64,_lts0 0x5555555555555555, %dr16")
    b += wi(f"disp\t%ctpr1, .L{name}_loop", "rwd,0\t%dr1, %lsr")
    b += wi("nop 7", "bap") + wi("nop 7") + wi("nop 7")
    b += f".L{name}_loop:\n"
    b += wi("loop_mode", "movad,0\tarea=0, ind=0, am=1, be=0, %dr8",
            "addd,1\t%dr10, 0x1, %dr10")
    b += wi("std,2\t%dr15, 0x0, %dr16", "addd,0\t%dr15, 0x8, %dr15")
    b += wi("nop 3")
    b += wi("gettagd,2\t%dr8, %r12", "puttagd,5\t%dr8, 0x0, %dr13")
    b += wi("nop 2")
    b += wi("std,2\t%dr14, 0x0, %dr13", "stw,5\t%dr14, 0x8, %r12")
    b += wi("loop_mode", "alc\talcf=1, alct=1", "ct\t%ctpr1 ? %NOT_LOOP_END",
            "addd,0\t%dr14, 0x10, %dr14")
    b += wi("nop 7", "disp\t%ctpr2, disp=0x0")
    b += wi("addd,0\t%dr10, 0x0, %dr0")
    b += wi("ct\t%ctpr3")
    fn = f"il_{name}"
    prog = wi(gen_apb.fapb("l", True, {"fmt": 4, "mrng": 8, "asz": asz}))
    add("", name, "S",
        f"\n\t.global\t{fn}\n\t.type\t{fn}, #function\n\t.align\t8\n{fn}:\n"
        + wi("setwd\twsz = 0x10, nfx = 0x1, dbl = 0x0", "return\t%ctpr3")
        + b + f"\t.align\t8\n.L{name}_prog:\n" + prog
        + f"\t.size\t{fn}, .-{fn}\n")


def section_ldovl():
    for asz in (0, 2, 5):
        ovst(asz)
    for asz, step in ((0, 1), (3, 1), (5, 1), (5, 8)):
        name = f"ovs{step}_a{asz}"
        text = gen_apb_stream.fn(name, asz, 0, step)
        add("", name, "M", text.replace(f"stream_{name}", f"il_{name}"))


# --- 6. A 128-bit register across a window spill (v5+, helpers) -----------

def qkeep(name, nfx, how):
    """il_NAME(out, hi, deep, depth): %qpr6 = (hi, ~hi) in a window with
    nfx = NFX; then a call of deep(depth) (HOW 'call': deep enough to
    spill this window) or a load from deep (HOW 'sig': a PROT_NONE page
    whose SIGSEGV handler makes it readable); then out[0..1] = %qpr6
    (low, high)."""
    fn = f"il_{name}"
    b = wi(f"setwd\twsz = 0x8, nfx = {nfx:#x}, dbl = 0x0",
           "setbn\trsz = 0x3, rbs = 0x4, rcur = 0x0")
    b += wi("xord,1\t%dr1, _f64,_lts0 0xffffffffffffffff, %dr5")
    b += wi("qppackdl,1\t%dr1, %dr5, %qpr6")
    if how == "call":
        b += wi("movtd,0\t%dr2, %ctpr1", "addd,3\t0x0, %dr3, %db[0]")
        b += wi("nop 7") + wi("call\t%ctpr1, wbs = 0x4")
    else:
        b += wi("ldd,0\t%dr2, 0x0, %dr7") + wi("nop 7")
    b += wi("return\t%ctpr3", "stqp,2,sm\t%dr0, 0x0, %qpr6")
    b += wi("nop 5") + wi("ct\t%ctpr3")
    add("-v5", name, "-",
        f"\n\t.global\t{fn}\n\t.type\t{fn}, #function\n\t.align\t8\n{fn}:\n"
        + b + f"\t.size\t{fn}, .-{fn}\n")


def section_qspill():
    for nfx in (1, 0):
        for how in ("call", "sig"):
            qkeep(f"q{how}_nfx{nfx}", nfx, how)


def main():
    section_pred()
    section_ptag()
    section_apbq()
    section_aabf()
    section_ac()
    section_ldovl()
    section_qspill()
    for g, cs in GROUPS.items():
        (HERE / f"isaleft{g}.s").write_text(
            "! SPDX-License-Identifier: GPL-3.0-or-later\n"
            "! Generated by tools/isa-probes/gen_isaleft.py; see isaleft.c.\n"
            "\t.ignore\tld_st_style\n\t.text\n"
            + "".join(t for *_, t in cs))
        (HERE / f"isaleft{g}-cases.h").write_text(
            "/* SPDX-License-Identifier: GPL-3.0-or-later */\n"
            "/* Generated by tools/isa-probes/gen_isaleft.py.  */\n"
            + "".join(f"CASE ({n}, \"{k}\", {r}, {a})\n"
                      for n, k, r, a, _ in cs if k != "-"))


if __name__ == "__main__":
    main()
