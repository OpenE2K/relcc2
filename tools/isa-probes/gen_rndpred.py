#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write rndpred.s for rndpred.c: the random predicates %rndpred1-15 and
their state register %rndpr (spec/isa/predicates.md).

  python3 tools/isa-probes/gen_rndpred.py

rp_rrd() returns %rndpr; rp_rwd(v) writes it.
rp_seqN(out, n): n reads of %rndpredN, one a loop iteration, each
stored as a byte 0 or 1.
rp_trN(st, out, n): the same, also storing %rndpr before each read.
rp_mix1(st, out, n): rp_tr1 with two instructions of other predicate
logic (a %pred ELP and CLP, a %spred ELP and CLP) before each read.
rp_twoN(st, out, n): two reads of %rndpredN in one wide instruction
(@p0 and @p1), stored as bytes 2i and 2i+1, %rndpr before each pair.
rp_tri(st, out, n): %rndpred3, 7 and 11 in one wide instruction,
bytes 3i..3i+2.
rp_unused(n): n ELPs reading %rndpred1 whose result nothing uses;
returns %rndpr afterwards.
rp_idle(n): n iterations of an empty loop; returns %rndpr afterwards.
"""

from pathlib import Path

HERE = Path(__file__).resolve().parent


def wi(*ops):
    return "\t{\n" + "".join(f"\t  {o}\n" for o in ops) + "\t}\n"


def fn(name, body):
    return (f"\n\t.global\trp_{name}\n\t.type\trp_{name}, #function\n"
            f"\t.align\t8\nrp_{name}:\n{body}\t.size\trp_{name}, "
            f".-rp_{name}\n")


def head(name, *extra):
    return wi("setwd\twsz = 0x8, nfx = 0x1, dbl = 0x0", "return\t%ctpr3",
              "addd,0\t0x0, 0x0, %dr4", "addd,1\t0x0, 0x1, %dr5",
              *extra) + wi(f"disp\t%ctpr1, .L{name}")


def bits(srcs):
    # @pK from %rndpred srcs[K], through a CLP to %predK (at most three).
    return ([f"pass\t%rndpred{x}, @p{k}" for k, x in enumerate(srcs)]
            + [f"andp\t@p{k}, @p{k}, @p{4 + k}" for k in range(len(srcs))]
            + [f"pass\t@p{4 + k}, %pred{k}" for k in range(len(srcs))])


def seq(n, trace, mix=False):
    name = f"{'mix' if mix else 'tr' if trace else 'seq'}{n}"
    # %dr0 out (or st with trace: %dr0 st, %dr1 out, %dr2 count).
    out, cnt = ("%dr1", "%dr2") if trace else ("%dr0", "%dr1")
    b = head(name)
    b += f".L{name}:\n"
    if trace:
        b += wi("rrd\t%rndpr, %dr7")
        b += wi("nop 2")
    if mix:
        # Other predicate logic between the reads: a %pred ELP with a CLP,
        # and a %spred ELP.
        b += wi("pass\t%pred5, @p0", "andp\t@p0, @p0, @p4",
                "pass\t@p4, %pred6")
        b += wi("pass\t%spred0, @p1", "andp\t@p1, @p1, @p4",
                "pass\t@p4, %pred7")
    b += wi(*bits([n]), f"subd,1\t{cnt}, 0x1, {cnt}")
    b += wi(f"cmpledb,0\t{cnt}, 0x0, %pred1",
            "merged,1\t%dr4, %dr5, %dr6, %pred0")
    st = [f"stb,2\t{out}, 0x0, %dr6", f"addd,1\t{out}, 0x1, {out}"]
    if trace:
        st += ["std,5\t%dr0, 0x0, %dr7", "addd,0\t%dr0, 0x8, %dr0"]
    b += wi(*st)
    b += wi("ct\t%ctpr1 ? ~%pred1")
    b += wi("ct\t%ctpr3")
    return name, fn(name, b)


def multi(name, srcs):
    # %dr0 st, %dr1 out, %dr2 count; len(srcs) bytes an iteration.
    k = len(srcs)
    b = head(name)
    b += f".L{name}:\n"
    b += wi("rrd\t%rndpr, %dr7")
    b += wi("nop 2")
    b += wi(*bits(srcs), "subd,1\t%dr2, 0x1, %dr2")
    b += wi("cmpledb,0\t%dr2, 0x0, %pred3",
            *[f"merged,{c}\t%dr4, %dr5, %dr{8 + j}, %pred{j}"
              for j, c in zip(range(k), (1, 3, 4))])
    st = ([f"stb\t%dr1, {j:#x}, %dr{8 + j}" for j in range(k)]
          + ["std\t%dr0, 0x0, %dr7"])
    while st:
        pair, st = st[:2], st[2:]
        ops = [f"{o.split(chr(9))[0]},{c}\t{o.split(chr(9))[1]}"
               for o, c in zip(pair, (2, 5))]
        if not st:
            ops += ["addd,0\t%dr0, 0x8, %dr0", f"addd,1\t%dr1, {k:#x}, %dr1",
                    "ct\t%ctpr1 ? ~%pred3"]
        b += wi(*ops)
    b += wi("ct\t%ctpr3")
    return name, fn(name, b)


def main():
    cases = [("rrd", fn("rrd", wi("setwd\twsz = 0x4, nfx = 0x1, dbl = 0x0",
                                  "return\t%ctpr3")
                         + wi("rrd\t%rndpr, %dr0") + wi("nop 4")
                         + wi("ct\t%ctpr3"))),
             ("rwd", fn("rwd", wi("setwd\twsz = 0x4, nfx = 0x1, dbl = 0x0",
                                  "return\t%ctpr3")
                         + wi("rwd\t%dr0, %rndpr") + wi("wait\tall_e = 1")
                         + wi("nop 4") + wi("ct\t%ctpr3")))]
    for n in range(1, 16):
        cases.append(seq(n, False))
        cases.append(seq(n, True))
    cases.append(seq(1, True, mix=True))
    for n in (1, 8, 15):
        cases.append(multi(f"two{n}", [n, n]))
    cases.append(multi("tri", [3, 7, 11]))
    # n ELPs whose result nothing reads.
    b = head("unused")
    b += ".Lunused:\n" + wi("pass\t%rndpred1, @p0", "subd,0\t%dr0, 0x1, %dr0")
    b += wi("cmpledb,0\t%dr0, 0x0, %pred1") + wi("ct\t%ctpr1 ? ~%pred1")
    b += wi("rrd\t%rndpr, %dr0") + wi("nop 4") + wi("ct\t%ctpr3")
    cases.append(("unused", fn("unused", b)))
    b = head("idle")
    b += ".Lidle:\n" + wi("subd,0\t%dr0, 0x1, %dr0")
    b += wi("cmpledb,0\t%dr0, 0x0, %pred1") + wi("ct\t%ctpr1 ? ~%pred1")
    b += wi("rrd\t%rndpr, %dr0") + wi("nop 4") + wi("ct\t%ctpr3")
    cases.append(("idle", fn("idle", b)))
    (HERE / "rndpred.s").write_text(
        "! SPDX-License-Identifier: GPL-3.0-or-later\n"
        "! Generated by tools/isa-probes/gen_rndpred.py; see rndpred.c.\n"
        "\t.ignore\tld_st_style\n\t.text\n"
        + "".join(t for _, t in cases))


if __name__ == "__main__":
    main()
