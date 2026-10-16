#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write apbst.s, apbst-v5.s and their -cases.h: which stores into a
streamed array a later mova sees, by store kind, loop length and
%lsr.ldovl, for apbst.c (spec/isa/loops.md, "Stores into a streamed
array"; apb.md).

  python3 tools/isa-probes/gen_apbst.py

Every case is  long apbst_NAME(unsigned long *a, unsigned long lsr,
unsigned long *out, long d): a 64-iteration (lcnt from LSR) loop_mode
loop whose iteration i reads a[i] with movad (area 0, fmt 4) and stores
0x5555555555555555 into a[i + d]; out[2i] gets what the movad read
(slow cases: tag cleared, and out[2i + 1] the tag; fast ones: as read,
out[2i + 1] 0).  It
returns the iterations run; apbst_clkr returns %clkr.

  std   the store is std (isaleft.c's ovst loop)
  staa  the store is staad through %aad1 with incr %aaincr0, as lcc's
        APB loops store
  slow  12 cycles an iteration (ovst's: the movad's tag is recorded)
  fast  the movad's value is stored at once (8 cycles: the recording
        store waits for the movad)
  aN    the area's asz (0: 64 bytes; 5: 2 KiB, lcc's)
  l1    (apbst-v5.s) %lsr1 = n written after %lsr, as lcc's v5+ loops do
  ix    (apbst-v5.s) the area's base 0 and the address in %aaind1
        (fapb ind=1), as lcc's loops address it
  lm    (apbst-v5.s) the store in the loop_mode instruction with alc and
        ct, as lcc puts it (it then obeys %lsr.strmd)
  rot   (apbst-v5.s) two instructions an iteration, both loop_mode: the
        movad writes %b[0], and %b[6], the value three iterations old,
        is recorded at out - 48 + 16i (the caller passes out + 8 words
        and checks iterations below n - 4: the last ones' records are
        lost or dropped); nothing waits for a movad, so an iteration
        takes 2 cycles (rot2) or, with a nop 3, 5 (rot5, lcc's pace)

Each case starts on a 512-byte boundary, which keeps its loop label out
of offsets 0x1c0-0x1ff of each 1 KiB block (gas's CODE_PLACEMENT
padding, spec/isa/hwbugs.md).
"""

from pathlib import Path

import gen_apb

HERE = Path(__file__).resolve().parent


def kernel(store, fast, asz, l1=False, ix=False, lm=False):
    name = (f"{store}_{'fast' if fast else 'slow'}_a{asz}"
            + ("_l1" if l1 else "") + ("_ix" if ix else "")
            + ("_lm" if lm else ""))
    fn = f"apbst_{name}"
    out = [f"\n\t.global\t{fn}\n\t.type\t{fn}, #function\n"
           f"\t.align\t512\n{fn}:\n"]

    def wi(*ops):
        out.append("\t{\n" + "".join(f"\t  {o}\n" for o in ops) + "\t}\n")

    wi("setwd\twsz = 0x10, nfx = 0x1, dbl = 0x0", "return\t%ctpr3")
    wi(f"ldisp\t%ctpr2, .L{name}_prog",
       "aaurwd,5\t%dr0, %aaind1" if ix else "aaurwd,2\t%dr0, %aad0",
       "addd,1\t0x0, 0x0, %dr10", "addd,3\t%dr2, 0x0, %dr14",
       "shld,4\t%dr3, 0x3, %dr15",
       "andd,0\t%dr1, _f64,_lts0 0xffffffff, %dr11")
    wi("addd,0\t%dr0, %dr15, %dr15",
       "addd,1\t0x0, _f64,_lts0 0x5555555555555555, %dr16")
    if store == "staa":
        wi("aaurwd,2\t%dr15, %aad1", "aaurwd,5\t%dr10, %aasti1")
    if ix:
        wi("aaurwd,2\t%dr10, %aad0")
    wi(f"disp\t%ctpr1, .L{name}_loop", "rwd,0\t%dr1, %lsr")
    if l1:
        wi("rwd,0\t%dr11, %lsr1")
    wi("nop 7", "bap")
    wi("nop 7")
    wi("nop 7")
    out.append(f".L{name}_loop:\n")
    wi("loop_mode", "movad,0\tarea=0, ind=0, am=1, be=0, %dr8",
       "addd,1\t%dr10, 0x1, %dr10")
    st = (["std,2\t%dr15, 0x0, %dr16"] if store == "std" else
          ["staad,2\t%dr16, %aad1[ %aasti1 ]", "incr,2\t%aaincr0"])
    if lm:
        wi("nop 1")
        st.append("addd,3\t%dr15, 0x8, %dr15")
    else:
        wi(*st, "addd,0\t%dr15, 0x8, %dr15")
    if fast:
        wi("std,2\t%dr14, 0x0, %dr8")
    else:
        wi("nop 3")
        wi("gettagd,2\t%dr8, %r12", "puttagd,5\t%dr8, 0x0, %dr13")
        wi("nop 2")
        wi("std,2\t%dr14, 0x0, %dr13", "stw,5\t%dr14, 0x8, %r12")
    wi("loop_mode", "alc\talcf=1, alct=1", "ct\t%ctpr1 ? %NOT_LOOP_END",
       "addd,0\t%dr14, 0x10, %dr14", *(st if lm else []))
    wi("nop 7", "disp\t%ctpr2, disp=0x0")
    wi("addd,0\t%dr10, 0x0, %dr0")
    wi("ct\t%ctpr3")
    out.append(f"\t.align\t8\n.L{name}_prog:\n")
    wi(gen_apb.fapb("l", True, {"fmt": 4, "mrng": 8, "asz": asz,
                                "ind": int(ix)}))
    out.append(f"\t.size\t{fn}, .-{fn}\n")
    return name, "".join(out)


def rot(store, asz, gap=0):
    name = f"{store}_rot{2 + gap}_a{asz}"
    fn = f"apbst_{name}"
    out = [f"\n\t.global\t{fn}\n\t.type\t{fn}, #function\n"
           f"\t.align\t512\n{fn}:\n"]

    def wi(*ops):
        out.append("\t{\n" + "".join(f"\t  {o}\n" for o in ops) + "\t}\n")

    wi("setwd\twsz = 0x20, nfx = 0x1, dbl = 0x0",
       "setbn\trsz = 0x3, rbs = 0x10, rcur = 0x0", "return\t%ctpr3")
    wi(f"ldisp\t%ctpr2, .L{name}_prog", "aaurwd,5\t%dr0, %aaind1",
       "addd,1\t0x0, 0x0, %dr10",
       "addd,3\t%dr2, _f32s,_lts0 0xffffffd0, %dr14",
       "shld,4\t%dr3, 0x3, %dr15",
       "andd,0\t%dr1, _f64,_lts1 0xffffffff, %dr11")
    wi("addd,0\t%dr0, %dr15, %dr15",
       "addd,1\t0x0, _f64,_lts0 0x5555555555555555, %dr16")
    wi("aaurwd,2\t%dr15, %aad1", "aaurwd,5\t%dr10, %aasti1")
    wi("aaurwd,2\t%dr10, %aad0")
    wi(f"disp\t%ctpr1, .L{name}_loop", "rwd,0\t%dr1, %lsr")
    wi("rwd,0\t%dr11, %lsr1")
    wi("nop 7", "bap")
    wi("nop 7")
    wi("nop 7")
    out.append(f".L{name}_loop:\n")
    st = (["std,5\t%dr15, 0x0, %dr16"] if store == "std" else
          ["staad,5\t%dr16, %aad1[ %aasti1 ]", "incr,5\t%aaincr0"])
    wi("loop_mode", "movad,0\tarea=0, ind=0, am=1, be=0, %b[0]",
       "std,2\t%dr14, 0x0, %b[6]", "addd,1\t%dr14, 0x10, %dr14", *st,
       *([f"nop {gap}"] if gap else []))
    wi("loop_mode", "alc\talcf=1, alct=1", "abn\tabnf=1, abnt=1",
       "ct\t%ctpr1 ? %NOT_LOOP_END", "addd,0\t%dr10, 0x1, %dr10",
       "addd,3\t%dr15, 0x8, %dr15")
    wi("nop 7", "disp\t%ctpr2, disp=0x0")
    wi("addd,0\t%dr10, 0x0, %dr0")
    wi("ct\t%ctpr3")
    out.append(f"\t.align\t8\n.L{name}_prog:\n")
    wi(gen_apb.fapb("l", True, {"fmt": 4, "mrng": 8, "asz": asz, "ind": 1}))
    out.append(f"\t.size\t{fn}, .-{fn}\n")
    return name, "".join(out)


def main():
    names, text = [], []
    for asz in (5, 0):
        for fast in (False, True):
            for store in ("std", "staa"):
                n, t = kernel(store, fast, asz)
                names.append(n)
                text.append(t)
    v5names, v5text = [], []
    for l1, ix, lm in ((True, False, False), (False, True, False),
                       (True, True, False), (True, True, True)):
        for store in ("std", "staa"):
            n, t = kernel(store, True, 5, l1, ix, lm)
            v5names.append(n)
            v5text.append(t)
    for gap in (0, 3):
        for store in ("std", "staa"):
            n, t = rot(store, 5, gap)
            v5names.append(n)
            v5text.append(t)
    clkr = ("\n\t.global\tapbst_clkr\n\t.type\tapbst_clkr, #function\n"
            "\t.align\t8\napbst_clkr:\n"
            "\t{\n\t  setwd\twsz = 0x4, nfx = 0x1, dbl = 0x0\n"
            "\t  return\t%ctpr3\n\t}\n"
            "\t{\n\t  rrd,0\t%clkr, %dr0\n\t}\n"
            "\t{\n\t  ct\t%ctpr3\n\t}\n"
            "\t.size\tapbst_clkr, .-apbst_clkr\n")
    head = ("! SPDX-License-Identifier: GPL-3.0-or-later\n"
            "! Generated by tools/isa-probes/gen_apbst.py; see apbst.c.\n"
            "\t.ignore\tld_st_style\n\t.text\n")
    (HERE / "apbst.s").write_text(head + "".join(text) + clkr)
    (HERE / "apbst-cases.h").write_text(
        "/* SPDX-License-Identifier: GPL-3.0-or-later */\n"
        "/* Generated by tools/isa-probes/gen_apbst.py.  */\n"
        + "".join(f"CASE ({n})\n" for n in names))
    (HERE / "apbst-v5.s").write_text(head + "".join(v5text))
    (HERE / "apbst-v5-cases.h").write_text(
        "/* SPDX-License-Identifier: GPL-3.0-or-later */\n"
        "/* Generated by tools/isa-probes/gen_apbst.py.  */\n"
        + "".join(f"CASE ({n})\n" for n in v5names))


if __name__ == "__main__":
    main()
