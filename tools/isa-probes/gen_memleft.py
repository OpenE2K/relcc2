#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write memleft.s, memleft-v5.s, memleft-v6.s and their -cases.h files
for memleft.c: memory leftovers of the 2026-10-12 user-mode gap audit
(AAU accesses with a MAS, the v6 MAS forms mas6.c left out, UPSR.ac with
stores and 16-byte accesses, and the wait fields).

  python3 tools/isa-probes/gen_memleft.py

Every function is  long ml_NAME(long a0, long a1, long a2, long a3);
what the arguments mean depends on the case's kind (memleft.c):

  L   load value: (out, buf, v): out[0] the value with its tag cleared,
      out[1] its tag, out[2..3] the raw 16 bytes (quad cases); buf[0] =
      0x1122334455667788, buf[1] = 0x99aabbccddeeff00
  S   store value: (out, buf, v): stores v (or v * 1..4) into buf
  M   lock test: (out, buf, v): out[0] = 1 if the wait-unlock store
      failed (%MLOCK), else 0
  P   APB loop: (dst, src, ldsrc): copies src[0..7] to dst through movad
      and staad, sums ldsrc[0..7] through ldaad; returns the sum
  H2, H4, K2, F1, F2  stream: (p, lsr): one access per 64-byte line, 2 or 4 lines
      an iteration, a loop counted by %lsr (K: loop_mode, APB running)
  A, a  UPSR.ac: (base, upsr with ac, upsr without, out): sets %upsr,
      makes one access at a fixed offset from base, restores %upsr;
      loads (A) leave value, tag (and raw 16 bytes) in out
  w32, W64  wait cost: (buf): %clkr cycles of 32 or 64 repetitions
  B   store-buffering litmus side: (a, b): *a = 1, a wait, return *b
  T   cycles of 32 dependent loads (buf[0] = buf) or 32 stores
  Q   watch: (p, v): if *p == v, wait for a change of its line; cycles
  X   one wait trap = 1: cycles
"""

from pathlib import Path

HERE = Path(__file__).resolve().parent
groups = {"": [], "-v5": [], "-v6": []}


def wi(*ops):
    return "\t{\n" + "".join(f"\t  {o}\n" for o in ops) + "\t}\n"


def raw_wait(bits):
    """A wide instruction of HS + CS1 wait with exactly BITS (gas forces
    st_c/ld_c on with sas/sal/las/lal; MCST's kernel encodes its soft
    barriers this way, asm/e2k_api.h __E2K_WAIT)."""
    return f"\t.word\t0x00008001\n\t.word\t{0x30000000 | bits:#010x}\n"


def nops(n):
    out = ""
    while n > 0:
        k = min(n, 8)
        out += wi(f"nop {k - 1}" if k > 1 else "nop")
        n -= k
    return out


def lit(v):
    return f"_f64,_lts0 {v & (1 << 64) - 1:#x}"


def func(group, name, kind, body, wsz=0x10):
    fn = f"ml_{name}"
    groups[group].append(
        (name, kind,
         f"\n\t.global\t{fn}\n\t.type\t{fn}, #function\n\t.align\t8\n{fn}:\n"
         + wi(f"setwd\twsz = {wsz:#x}, nfx = 0x1, dbl = 0x0", "return\t%ctpr3")
         + body + wi("nop 5") + wi("ct\t%ctpr3")
         + f"\t.size\t{fn}, .-{fn}\n"))


def m(mas):
    return f", mas={mas:#x}" if mas else ""


def result(out="%dr0", lo="%dr4", quad=None):
    """Store LO's value (tag cleared) and tag to out[0], out[1]; QUAD (a
    %qpr or the pair's %qr) raw to out[2..3] with a speculative store."""
    b = (wi("nop 7") + wi(f"gettagd,2\t{lo}, %r20") + wi("nop 2")
         + wi(f"puttagd,5\t{lo}, 0x0, %dr21") + wi("nop 2")
         + wi(f"std,2\t{out}, 0x0, %dr21", f"std,5\t{out}, 0x8, %dr20"))
    if quad and quad.startswith("%qpr"):
        b += wi(f"stqp,2,sm\t{out}, 0x10, {quad}")
    elif quad:
        n = int(quad[3:])
        b += wi(f"std,2,sm\t{out}, 0x10, %dr{n}", f"std,5,sm\t{out}, 0x18, %dr{n + 1}")
    return b


# --- 1. AAU accesses with a MAS ---------------------------------------

AAU0 = (wi("addd,0\t0x0, 0x0, %dr24", "addd,1\t0x0, 0x0, %dr4",
           "addd,3\t0x0, 0x0, %dr5")
        + wi("aaurwd,2\t%dr1, %aad0", "aaurwd,5\t%dr24, %aasti1") + nops(8))


def aau_load(name, op, mas, sm=False, ch=5, group="", dst="%dr4"):
    s = ",sm" if sm else ""
    quad = dst if dst.startswith("%q") else None
    func(group, name, "L", AAU0
         + wi(f"{op},{ch}{s}\t%aad0[ %aasti1 ], {dst}{m(mas)}")
         + result(quad=quad))


def aau_store(name, op, mas, ch=5, group="", src="%dr2", n=4):
    """N stores of v * 1..N with incr %aaincr0 after each."""
    b = AAU0 + wi("addd,0\t%dr2, %dr2, %dr21", "addd,1\t%dr2, 0x0, %dr20",
                  "addd,3\t%dr2, 0x0, %dr26", "addd,4\t%dr2, %dr2, %dr27")
    b += wi("addd,0\t%dr21, %dr2, %dr22") + wi("addd,0\t%dr22, %dr2, %dr23")
    b += nops(3)
    regs = ["%dr20", "%dr21", "%dr22", "%dr23"]
    for r in regs[:n]:
        if src.startswith("%r"):
            r = r.replace("%dr", "%r")
        elif src.startswith("%q"):
            r = src
        b += wi(f"{op},{ch}\t{r}, %aad0[ %aasti1 ]{m(mas)}", f"incr,{ch}\t%aaincr0")
    func(group, name, "S", b + nops(4))


for mas in (0x0, 0x20, 0x40, 0x60, 0x8):
    for ch in (2, 5):
        aau_store(f"staad_{mas:02x}_c{ch}", "staad", mas, ch)
aau_store("staaw_60", "staaw", 0x60, src="%r2")
aau_store("staaq_60", "staaq", 0x60, src="%qr26", n=2)
aau_store("staaq_00", "staaq", 0x0, src="%qr26", n=2)
for mas in (0x0, 0x2, 0x3, 0x20, 0x40, 0x60, 0x8):
    for ch in (2, 5):
        aau_load(f"ldaad_{mas:02x}_c{ch}", "ldaad", mas, ch=ch)
for mas in (0x0, 0x3, 0x4, 0x7):
    for ch in (2, 5):
        aau_load(f"ldaad_{mas:02x}sm_c{ch}", "ldaad", mas, sm=True, ch=ch)
for mas, sm in ((0x0, False), (0x3, False), (0x4, True), (0x7, True), (0x0, True)):
    aau_load(f"ldaaq_{mas:02x}{'sm' if sm else ''}", "ldaaq", mas, sm=sm,
             ch=2, dst="%qr4")
# Two ldaad: does incr step a load's index?
func("", "ldaad_incr", "L", AAU0
     + wi("ldaad,5\t%aad0[ %aasti1 ], %dr5, mas=0x60", "incr,5\t%aaincr0")
     + wi("ldaad,5\t%aad0[ %aasti1 ], %dr4, mas=0x60") + result())
# v5: the 128-bit AAU accesses.
for mas, sm in ((0x0, False), (0x60, False), (0x3, False), (0x4, True),
                (0x7, True), (0x0, True)):
    aau_load(f"ldaaqp_{mas:02x}{'sm' if sm else ''}", "ldaaqp", mas, sm=sm,
             ch=2, dst="%qpr4", group="-v5")
for mas in (0x0, 0x60):
    b = AAU0 + wi("qppackdl,0\t%dr2, %dr2, %qpr26") + nops(2)
    b += wi(f"staaqp,5\t%qpr26, %aad0[ %aasti1 ]{m(mas)}", "incr,5\t%aaincr0")
    b += wi(f"staaqp,5\t%qpr26, %aad0[ %aasti1 ]{m(mas)}", "incr,5\t%aaincr0")
    func("-v5", f"staaqp_{mas:02x}", "S", b + nops(4))


def apb_loop(name, st_mas, ld_mas, ld_sm=False):
    """dst[i] = src[i] (movad from %aad0, staad to %aad1) and sum of
    ldsrc[i] (ldaad from %aad2), i < 8, in a loop_mode loop with the
    APB program running."""
    s = ",sm" if ld_sm else ""
    lsr = (1 << 37) | (0xff << 40) | (1 << 53) | 8
    b = (wi("addd,0\t0x0, 0x0, %dr24", "addd,1\t0x0, 0x0, %dr10",
            f"addd,3\t0x0, {lit(lsr)}, %dr3")
         + wi(f"ldisp\t%ctpr2, .L{name}_prog", "aaurwd,2\t%dr1, %aad0",
              "aaurwd,5\t%dr0, %aad1")
         + wi("aaurwd,2\t%dr2, %aad2", "aaurwd,5\t%dr24, %aasti1")
         + wi("aaurwd,2\t%dr24, %aasti2")
         + wi(f"disp\t%ctpr1, .L{name}_loop", "rwd,0\t%dr3, %lsr")
         + wi("nop 7", "bap") + wi("nop 7") + wi("nop 7")
         + f".L{name}_loop:\n"
         + wi("loop_mode", "movad,0\tarea=0, ind=0, am=1, be=0, %dr8",
              f"ldaad,2{s}\t%aad2[ %aasti2 ], %dr9{m(ld_mas)}", "incr,2\t%aaincr0")
         + wi("nop 5")
         + wi("loop_mode", "alc\talcf=1, alct=1", "ct\t%ctpr1 ? %NOT_LOOP_END",
              f"staad,5\t%dr8, %aad1[ %aasti1 ]{m(st_mas)}", "incr,5\t%aaincr0",
              "addd,0\t%dr10, %dr9, %dr10")
         + wi("nop 7", "disp\t%ctpr2, disp=0x0")
         + wi("addd,0\t%dr10, 0x0, %dr0") + nops(2))
    prog = (f"\t.align\t8\n.L{name}_prog:\n"
            + wi("fapb\tct=1, dcd=0, fmt=4, mrng=8, d=0, incr=0, ind=0, "
                 "asz=5, abs=0, disp=0"))
    func("", name, "P", b + wi("ct\t%ctpr3") + prog)


for st, ld in ((0x0, 0x0), (0x20, 0x20), (0x40, 0x40), (0x60, 0x60),
               (0x60, 0x2), (0x60, 0x3)):
    apb_loop(f"apb_st{st:02x}_ld{ld:02x}", st, ld)
for ld in (0x0, 0x3, 0x4, 0x7):
    apb_loop(f"apb_st60_ld{ld:02x}sm", 0x60, ld, True)


# --- streams for the cache hints (mas_hint.c's measurement) -------------

def stream(group, name, kind, ops, setup="", loop_mode=False, after="",
           start="", gap=0):
    """START runs after the %lsr write (bap: apb.md wants %lsr first).
    GAP: a nop GAP in the loop's instruction (GAP + 1 cycles an
    iteration)."""
    lm = ["loop_mode"] if loop_mode else []
    g = [f"nop {gap}"] if gap else []
    b = (setup + wi(f"disp\t%ctpr1, .L{name}_loop", "rwd,0\t%dr1, %lsr")
         + nops(6) + start + f".L{name}_loop:\n"
         + wi(*g, *lm, "alc\talcf=1, alct=1", "ct\t%ctpr1 ? %NOT_LOOP_END",
              *ops)
         + nops(6) + after)
    func(group, name, kind, b)


AAUS = (wi("addd,0\t0x0, 0x0, %dr24", "addd,1\t0x0, 0x10, %dr25",
           "addd,3\t0x0, 0x1, %dr3")
        + wi("aaurwd,2\t%dr0, %aad0", "aaurwd,5\t%dr24, %aasti1")
        + wi("aaurwd,2\t%dr25, %aaincr2") + nops(4))
for mas in (0x0, 0x20, 0x40, 0x60):
    stream("", f"h_staad_{mas:02x}", "H2",
           [f"staad,2\t%dr3, %aad0[ %aasti1 + 0x40 ]{m(mas)}",
            f"staad,5\t%dr3, %aad0[ %aasti1 ]{m(mas)}", "incr,5\t%aaincr2"],
           AAUS)
    stream("", f"h_ldaad_{mas:02x}", "H2",
           [f"ldaad,2\t%aad0[ %aasti1 + 0x40 ], %dr8{m(mas)}",
            f"ldaad,5\t%aad0[ %aasti1 ], %dr9{m(mas)}", "incr,5\t%aaincr2"],
           AAUS)
    # The same shape with ordinary accesses (two a wide instruction).
    stream("", f"h_std_{mas:02x}", "H2",
           ["addd,1\t%dr0, 0x80, %dr0", f"std,2\t%dr0, 0x40, %dr3{m(mas)}",
            f"std,5\t%dr0, 0x0, %dr3{m(mas)}"], wi("addd,0\t0x0, 0x1, %dr3"))
    stream("", f"h_ldd_{mas:02x}", "H2",
           ["addd,1\t%dr0, 0x80, %dr0", f"ldd,2\t%dr0, 0x40, %dr8{m(mas)}",
            f"ldd,5\t%dr0, 0x0, %dr9{m(mas)}"])
    # lcc's #pragma vector nontemporal shape: loop_mode, the APB program
    # running (one movad an iteration from an area with step 0), staad.
    apbs = AAUS + wi(f"ldisp\t%ctpr2, .Lk_staad_{mas:02x}_prog",
                     "aaurwd,2\t%dr0, %aad1", "aaurwd,5\t%dr24, %aaincr3")
    stream("", f"k_staad_{mas:02x}", "K2",
           ["movad,0\tarea=0, ind=0, am=1, be=0, %dr8",
            f"staad,2\t%dr3, %aad0[ %aasti1 + 0x40 ]{m(mas)}",
            f"staad,5\t%dr3, %aad0[ %aasti1 ]{m(mas)}", "incr,5\t%aaincr2"],
           apbs, loop_mode=True,
           start=wi("nop 7", "bap") + wi("nop 7") + wi("nop 7"),
           after=wi("disp\t%ctpr2, disp=0x0") + nops(2) + wi("ct\t%ctpr3")
           + f"\t.align\t8\n.Lk_staad_{mas:02x}_prog:\n"
           + wi("fapb\tct=1, dcd=0, fmt=4, mrng=8, d=1, incr=3, ind=0, "
                "asz=0, abs=0, disp=0"))
# The same at one iteration (two lines) every 8 cycles, for `fill`: on
# the 4C a faster stream of misses leaves only part of its lines in L1.
for mas in (0x0, 0x20, 0x40, 0x60):
    stream("", f"f_ldd_{mas:02x}", "F2",
           ["addd,1\t%dr0, 0x80, %dr0", f"ldd,2\t%dr0, 0x40, %dr8{m(mas)}",
            f"ldd,5\t%dr0, 0x0, %dr9{m(mas)}"], gap=7)
    stream("", f"f_ldaad_{mas:02x}", "F2",
           [f"ldaad,2\t%aad0[ %aasti1 + 0x40 ], %dr8{m(mas)}",
            f"ldaad,5\t%aad0[ %aasti1 ], %dr9{m(mas)}", "incr,5\t%aaincr2"],
           AAUS, gap=7)
    stream("", f"f_std_{mas:02x}", "F2",
           ["addd,1\t%dr0, 0x80, %dr0", f"std,2\t%dr0, 0x40, %dr3{m(mas)}",
            f"std,5\t%dr0, 0x0, %dr3{m(mas)}"], wi("addd,0\t0x0, 0x1, %dr3"),
           gap=7)
    stream("", f"f_staad_{mas:02x}", "F2",
           [f"staad,2\t%dr3, %aad0[ %aasti1 + 0x40 ]{m(mas)}",
            f"staad,5\t%dr3, %aad0[ %aasti1 ]{m(mas)}", "incr,5\t%aaincr2"],
           AAUS, gap=7)
# One load an iteration (ALC0 or ALC3), one line every 8 cycles: does a
# second load in the instruction keep its line out of L1?
stream("", "f_ldd_00_one0", "F1",
       ["addd,1\t%dr0, 0x40, %dr0", "ldd,0\t%dr0, 0x0, %dr8"], gap=7)
stream("", "f_ldd_00_one3", "F1",
       ["addd,1\t%dr0, 0x40, %dr0", "ldd,3\t%dr0, 0x0, %dr8"], gap=7)
# Both loads in one cluster (ALC0 and 2, or ALC3 and 5): one L1 per
# cluster?
stream("", "f_ldd_00_cl0", "F2",
       ["addd,1\t%dr0, 0x80, %dr0", "ldd,0\t%dr0, 0x40, %dr8",
        "ldd,2\t%dr0, 0x0, %dr9"], gap=7)
stream("", "f_ldd_00_cl1", "F2",
       ["addd,1\t%dr0, 0x80, %dr0", "ldd,3\t%dr0, 0x40, %dr8",
        "ldd,5\t%dr0, 0x0, %dr9"], gap=7)
for mas in (0x30, 0x50, 0x70):
    stream("-v6", f"f6_ldd_{mas:02x}", "F2",
           ["addd,1\t%dr0, 0x80, %dr0", f"ldd,2\t%dr0, 0x40, %dr8{m(mas)}",
            f"ldd,5\t%dr0, 0x0, %dr9{m(mas)}"], gap=7)
    stream("-v6", f"f6_std_{mas:02x}", "F2",
           ["addd,1\t%dr0, 0x80, %dr0", f"std,2\t%dr0, 0x40, %dr3{m(mas)}",
            f"std,5\t%dr0, 0x0, %dr3{m(mas)}"], wi("addd,0\t0x0, 0x1, %dr3"),
           gap=7)
# v6: the cache hints with m1 (bit 4), against mas_hint.c's shape.
for mas in (0x0, 0x20, 0x40, 0x60, 0x30, 0x50, 0x70):
    stream("-v6", f"h6_ldd_{mas:02x}", "H4",
           ["addd,1\t%dr0, 0x100, %dr0"]
           + [f"ldd,{c}\t%dr0, {o:#x}, %dr{8 + k}{m(mas)}"
              for k, (c, o) in enumerate(zip((0, 2, 3, 5), (0, 64, 128, 192)))])
    stream("-v6", f"h6_std_{mas:02x}", "H2",
           ["addd,1\t%dr0, 0x80, %dr0", f"std,2\t%dr0, 0x40, %dr3{m(mas)}",
            f"std,5\t%dr0, 0x0, %dr3{m(mas)}"], wi("addd,0\t0x0, 0x1, %dr3"))


# --- 2. v6 MAS forms (2C3) ----------------------------------------------

def load(group, name, mas, ch, sm=False, quad=False):
    s = ",sm" if sm else ""
    if quad:
        func(group, name, "L", wi("addd,0\t0x0, 0x0, %dr4")
             + wi(f"ldqp,{ch}{s}\t%dr1, 0x0, %qpr4{m(mas)}")
             + result(quad="%qpr4"))
    else:
        func(group, name, "L", wi("addd,0\t0x0, 0x0, %dr4")
             + wi(f"ldd,{ch}{s}\t%dr1, 0x0, %dr4{m(mas)}") + result())


def store(group, name, mas, ch):
    func(group, name, "S", wi(f"std,{ch}\t%dr1, 0x8, %dr2{m(mas)}") + nops(8))


for mas in (0x33, 0x73, 0x1b, 0x5b):
    for ch in (0, 2, 3, 5):
        load("-v6", f"ld_{mas:02x}_c{ch}", mas, ch)
    for ch in (2, 5):
        store("-v6", f"st_{mas:02x}_c{ch}", mas, ch)
for dc in (1, 2, 3):
    base = 0x10 | dc << 5
    for ch in (0, 2):
        load("-v6", f"ld_{base:02x}_c{ch}", base, ch)
        load("-v6", f"ld_{base | 4:02x}_c{ch}", base | 4, ch)
        load("-v6", f"ld_{base | 2:02x}sm_c{ch}", base | 2, ch, sm=True)
        load("-v6", f"ld_{base | 6:02x}sm_c{ch}", base | 6, ch, sm=True)
    load("-v6", f"ld_{base | 1:02x}_c0", base | 1, 0)
    load("-v6", f"ld_{base | 1:02x}sm_c0", base | 1, 0, sm=True)
    load("-v6", f"ld_{base | 5:02x}sm_c0", base | 5, 0, sm=True)
    store("-v6", f"st_{base:02x}_c2", base, 2)
    store("-v6", f"st_{base | 4:02x}_c2", base | 4, 2)
load("-v6", "ld_17_c0", 0x17, 0)
load("-v6", "ld_17sm_c0", 0x17, 0, sm=True)
load("-v6", "ld_07sm_c0", 0x7, 0, sm=True)
# The watch-for-modification load on a quad register.
for ch in (0, 2, 3, 5):
    load("-v6", f"ldqp_11_c{ch}", 0x11, ch, quad=True)
load("-v6", "ldqp_00_c0", 0x0, 0, quad=True)


def lock(group, name, ldmas, ch=0):
    """Load with LDMAS, nop 4, then the wait-unlock store (MAS 0x2) of v
    to the same word with rbranch: out[0] = 1 when %MLOCK was set."""
    b = (wi("addd,0\t0x0, 0x0, %dr6")
         + wi(f"ldd,{ch}\t%dr1, 0x0, %dr4{m(ldmas)}", "nop 4")
         + wi("std,2\t%dr1, 0x0, %dr2, mas=0x2", f"rbranch\t.L{name}_fail")
         + wi("ibranch\t.L{0}_done".format(name))
         + f".L{name}_fail:\n" + wi("addd,0\t0x0, 0x1, %dr6")
         + f".L{name}_done:\n" + nops(2)
         + wi("std,2\t%dr0, 0x0, %dr6", "std,5\t%dr0, 0x8, %dr4"))
    func(group, name, "M", b)


lock("", "lock_07", 0x7)
lock("", "lock_00", 0x0)
lock("-v6", "lock_17", 0x17)


def lock2(group, name, stmas, ldmas=0x7, off=0, again=False):
    """The lock-wait load (LDMAS), then a store with STMAS to buf + OFF
    and rbranch: is STMAS a wait-unlock store?  AGAIN: a second
    lock-wait load after it (SIGBUS if the store left the lock held)."""
    b = (wi("addd,0\t0x0, 0x0, %dr6")
         + wi(f"ldd,0\t%dr1, 0x0, %dr4{m(ldmas)}", "nop 4")
         + wi(f"std,2\t%dr1, {off:#x}, %dr2{m(stmas)}", f"rbranch\t.L{name}_fail")
         + wi(f"ibranch\t.L{name}_done")
         + f".L{name}_fail:\n" + wi("addd,0\t0x0, 0x1, %dr6")
         + f".L{name}_done:\n" + nops(2)
         + (wi("ldd,0\t%dr1, 0x0, %dr7, mas=0x7", "nop 4")
            + wi("std,2\t%dr1, 0x0, %dr7, mas=0x2") + nops(2) if again else "")
         + wi("std,2\t%dr0, 0x0, %dr6", "std,5\t%dr0, 0x8, %dr4"))
    func(group, name, "M", b)


def dam(group, name, ldmas=0x3, stmas=None, brk=False, other=0x8, lkmas=0x4,
        aau=False, lock=True):
    """buf[0] = A, buf[1] = B; lock load of %dr4 from buf[0] (LKMAS,sm;
    none when not LOCK);
    optionally a store with STMAS of %dr4 to buf[2] (or, BRK, of v to
    buf[0]); then a load with LDMAS in ALC5 from buf + OTHER into %dr4
    with rbranch (AAU: an ldaad from %aad0 = buf, %aasti1 = 0).  out[0] =
    1 if rbranch was taken (%MLOCK), out[1] = %dr4: A if the entry held
    (a check), B (or v) if it reloaded."""
    b = (wi("addd,0\t0x0, 0x0, %dr6",
            f"addd,1\t0x0, {lit(0xaaaa000000000001)}, %dr20",
            "addd,3\t0x0, 0x0, %dr24")
         + wi(f"addd,0\t0x0, {lit(0xbbbb000000000002)}, %dr21")
         + wi("std,2\t%dr1, 0x0, %dr20", "std,5\t%dr1, 0x8, %dr21"))
    if aau:
        b += wi("aaurwd,2\t%dr1, %aad0", "aaurwd,5\t%dr24, %aasti1")
    b += nops(8)
    b += (wi(f"ldd,0,sm\t%dr1, 0x0, %dr4{m(lkmas)}") if lock else wi("nop")) + nops(3)
    if brk:
        b += wi("std,2\t%dr1, 0x0, %dr2") + nops(2)
    if stmas is not None:
        b += wi(f"std,2\t%dr1, 0x10, %dr4{m(stmas)}") + nops(2)
    if aau:
        ld = f"ldaad,5\t%aad0[ %aasti1 + {other:#x} ], %dr4{m(ldmas)}"
    else:
        ld = f"ldd,5\t%dr1, {other:#x}, %dr4{m(ldmas)}"
    b += (wi(ld, f"rbranch\t.L{name}_fail")
          + wi(f"ibranch\t.L{name}_done")
          + f".L{name}_fail:\n" + wi("addd,0\t0x0, 0x1, %dr6")
          + f".L{name}_done:\n" + nops(4)
          + wi("std,2\t%dr0, 0x0, %dr6", "std,5\t%dr0, 0x8, %dr4"))
    func(group, name, "M", b)


# Is an AAU load with mod 2 or 3 a DAM check (qemu: an ordinary load)?
# The reference: ldd mod 3 and mod 0 in ALC5 after the same lock load.
for ldm in (0x3, 0x2, 0x0):
    dam("", f"dam_aa{ldm:02x}_other", ldmas=ldm, aau=True)
    dam("", f"dam_aa{ldm:02x}_lost", ldmas=ldm, brk=True, other=0x0, aau=True)
dam("", "dam_ld03_ref_other", ldmas=0x3)
dam("", "dam_ld00_ref_other", ldmas=0x0)


# m3 (bit 5) with mod 3 and m1: a DAM check, a lock-wait store, an unlock?
for ldm in (0x3, 0x33, 0x73, 0x0, 0x13):
    dam("-v6", f"dam_ld{ldm:02x}_other", ldmas=ldm)
    dam("-v6", f"dam_ld{ldm:02x}_lost", ldmas=ldm, brk=True, other=0x0)
for stm in (0x0, 0x3, 0x5, 0x33, 0x73, 0x13):
    dam("-v6", f"dam_st{stm:02x}", stmas=stm)
for stm in (0x33, 0x73, 0x13):
    lock2("-v6", f"lockw_st{stm:02x}", stm)
for stm in (0x2, 0x33, 0x73, 0x13, 0x0):
    lock2("-v6", f"nolock_st{stm:02x}", stm, ldmas=0x0)
    lock2("-v6", f"lockw_other_st{stm:02x}", stm, off=0x8)
    lock2("-v6", f"lockw_again_st{stm:02x}", stm, again=True)
# m1 (and m3, mt) with mod 7 in ALC0: a lock-wait load (acquire)?  With
# the wait-unlock store and with the m3 store found above.
for ldm in (0x37, 0x77, 0x57):
    for stm in (0x2, 0x33):
        lock2("-v6", f"lk{ldm:02x}_st{stm:02x}", stm, ldmas=ldm, again=True)
# Lock loads with the v6 MAS forms: do they record a DAM entry?
for lkm in (0x17, 0x31, 0x13, 0x11):
    dam("-v6", f"dam_lk{lkm:02x}_other", lkmas=lkm)
# (Verifier's addition.)  The m3 checks with no lock load at all: does
# 0x33 set %MLOCK and reload as mas=0x3 does?  And the m1 lock-wait
# forms as plain loads (SIGILL from the load itself?), 0x31 in ALC2.
for ldm in (0x3, 0x33, 0x73):
    dam("-v6", f"dam_nl_ld{ldm:02x}", ldmas=ldm, lock=False)
for ldm in (0x37, 0x57, 0x77):
    load("-v6", f"ld_{ldm:02x}_c0", ldm, 0)
load("-v6", "ld_31_c2", 0x31, 2)
# The register-pair ldq with the watch MAS (the kernel: "ALC0/ALC2 for
# quadro"; ldq takes ALC0+2 or 3+5).
for ch in (0, 3):
    func("-v6", f"ldq_11_c{ch}", "L", wi("addd,0\t0x0, 0x0, %dr4")
         + wi(f"ldq,{ch}\t%dr1, 0x0, %qr4, mas=0x11") + result(quad="%qr4"))


def chase(group, name, mas, sm=False, ch=0):
    """32 dependent loads through a self-pointer (buf[2] = &buf[2]; for a
    byte-swapped MAS it is stored byte-swapped first)."""
    s = ",sm" if sm else ""
    body = wi("addd,0\t%dr1, 0x10, %dr4") + nops(2)
    body += wi(f"std,2\t%dr1, 0x10, %dr4{', mas=0x8' if mas & 8 else ''}")
    body += nops(8) + wi("rrd,0\t%clkr, %dr10")
    for _ in range(32):
        body += wi(f"ldd,{ch}{s}\t%dr4, 0x0, %dr4{m(mas)}")
    body += wi("addd,0\t%dr4, 0x0, %dr5") + wi("rrd,0\t%clkr, %dr11")
    body += nops(3) + wi("subd,0\t%dr11, %dr10, %dr0")
    func(group, name, "T", body)


def stores(group, name, mas, ch=2):
    body = wi("rrd,0\t%clkr, %dr10")
    for i in range(32):
        body += wi(f"std,{ch}\t%dr1, {8 * (i % 8):#x}, %dr2{m(mas)}")
    body += wi("rrd,0\t%clkr, %dr11") + nops(3)
    body += wi("subd,0\t%dr11, %dr10, %dr0")
    func(group, name, "T", body)


for mas in (0x0, 0x13, 0x1b, 0x5b, 0x30, 0x50, 0x70, 0x34, 0x8):
    chase("-v6", f"t_chase_{mas:02x}", mas)
# The m3 loads are DAM checks (ALC2/5 only); with no entry they reload.
for mas in (0x0, 0x3, 0x33, 0x73):
    chase("-v6", f"t_chase_{mas:02x}_c2", mas, ch=2)
for mas in (0x0, 0x13, 0x33, 0x73, 0x1b, 0x5b, 0x30, 0x50, 0x70):
    stores("-v6", f"t_st_{mas:02x}", mas)


# lcc's release store, back to back (32 each): v3/v6 form, v5 form (a
# nop for the E8C2 soft-wait erratum, then the soft barriers), and the v5
# form without the nop; and the seq_cst store (release + wait st_c).
def rel(group, name, insns):
    body = wi("addd,0\t0x0, 0x7, %dr3") + nops(8) + wi("rrd,0\t%clkr, %dr10")
    for i in range(32):
        body += insns.replace("OFF", f"{8 * (i % 8):#x}")
    body += wi("rrd,0\t%clkr, %dr11") + nops(3)
    body += wi("subd,0\t%dr11, %dr10, %dr0")
    func(group, name, "T", body)


rel("", "t_rel_v3", wi("wait\tld_c = 1, st_c = 1", "std,2\t%dr1, OFF, %dr3"))
rel("", "t_sc_v3", wi("wait\tld_c = 1, st_c = 1", "std,2\t%dr1, OFF, %dr3")
    + wi("wait\tst_c = 1"))
rel("", "t_st_plain", wi("std,2\t%dr1, OFF, %dr3"))
rel("", "t_st_nop", wi("nop") + wi("std,2\t%dr1, OFF, %dr3"))
rel("-v5", "t_rel_v5", wi("nop") + wi("wait\tld_c = 1, sal = 1, st_c = 1, sas = 1",
                                       "std,2\t%dr1, OFF, %dr3"))
rel("-v5", "t_rel_v5_nonop", wi("wait\tld_c = 1, sal = 1, st_c = 1, sas = 1",
                                "std,2\t%dr1, OFF, %dr3"))
rel("-v5", "t_sc_v5", wi("nop") + wi("wait\tld_c = 1, sal = 1, st_c = 1, sas = 1",
                                     "std,2\t%dr1, OFF, %dr3")
    + wi("wait\tst_c = 1"))


# --- 3. UPSR.ac -----------------------------------------------------------

def ac(group, name, access, kind="A", setup="", quad=None, lo="%dr4"):
    """(base, on, off, out): %dr10 = 0x1122334455667788, %dr11 =
    0x99aabbccddeeff00 (%qr10 / %qpr10 hold both), then %upsr = on, the
    access, %upsr = off."""
    b = (wi(f"addd,0\t0x0, {lit(0x1122334455667788)}, %dr10")
         + wi(f"addd,0\t0x0, {lit(0x99aabbccddeeff00)}, %dr11",
              "addd,1\t0x0, 0x0, %dr4", "addd,3\t0x0, 0x0, %dr5")
         + setup + nops(4)
         + wi("rws,0\t%r1, %upsr") + wi("wait\tall_c = 1")
         + wi(access) + wi("wait\tall_c = 1") + nops(4)
         + wi("rws,0\t%r2, %upsr") + wi("wait\tall_c = 1"))
    if kind == "A":
        b += result(out="%dr3", lo=lo, quad=quad)
    func(group, name, kind, b)


for op, off in (("ldh", 1), ("ldw", 2), ("ldd", 4), ("ldd", 1), ("ldd", 0),
                ("ldb", 1)):
    dst = {"ldb": "%r4", "ldh": "%r4", "ldw": "%r4"}.get(op, "%dr4")
    ac("", f"ac_{op}_{off}", f"{op},0\t%dr0, {off:#x}, {dst}")
for op, off in (("stb", 1), ("sth", 1), ("stw", 2), ("stw", 1), ("std", 4),
                ("std", 1), ("std", 0)):
    src = "%dr10" if op == "std" else "%r10"
    ac("", f"ac_{op}_{off}", f"{op},2\t%dr0, {off:#x}, {src}", kind="a")
for mas in (0x0, 0x3):
    ac("", f"ac_lddsm{mas:x}_4", f"ldd,0,sm\t%dr0, 0x4, %dr4{m(mas)}")
    ac("", f"ac_lddsm{mas:x}_0", f"ldd,0,sm\t%dr0, 0x0, %dr4{m(mas)}")
ac("", "ac_ldwsm_2", "ldw,0,sm\t%dr0, 0x2, %r4")
AACS = (wi("addd,0\t0x0, 0x0, %dr24")
        + wi("aaurwd,2\t%dr0, %aad0", "aaurwd,5\t%dr24, %aasti1") + nops(6))
ac("", "ac_staad_4", "staad,5\t%dr10, %aad0[ %aasti1 + 0x4 ]", kind="a",
   setup=AACS)
ac("", "ac_staaw_2", "staaw,5\t%r10, %aad0[ %aasti1 + 0x2 ]", kind="a",
   setup=AACS)
ac("", "ac_ldaad_4", "ldaad,5\t%aad0[ %aasti1 + 0x4 ], %dr4", setup=AACS)
ac("", "ac_staad_0", "staad,5\t%dr10, %aad0[ %aasti1 ]", kind="a",
   setup=AACS)
# v5: register-pair 16-byte accesses, ldqp/stqp.
for off in (0, 8, 4):
    ac("-v5", f"ac_ldq_{off}", f"ldq,0\t%dr0, {off:#x}, %qr4", quad="%qr4")
    ac("-v5", f"ac_stq_{off}", f"stq,2\t%dr0, {off:#x}, %qr10", kind="a")
ac("-v5", "ac_ldqp_0", "ldqp,0\t%dr0, 0x0, %qpr4", quad="%qpr4",
   setup=wi("qppackdl,0\t%dr11, %dr10, %qpr10"))
ac("-v5", "ac_stqp_0", "stqp,2\t%dr0, 0x0, %qpr10", kind="a",
   setup=wi("qppackdl,0\t%dr11, %dr10, %qpr10"))
# v6: misaligned ldqp/stqp work without ac.
for off in (8, 4, 1):
    ac("-v6", f"ac_ldqp_{off}", f"ldqp,0\t%dr0, {off:#x}, %qpr4",
       quad="%qpr4")
    ac("-v6", f"ac_stqp_{off}", f"stqp,2\t%dr0, {off:#x}, %qpr10", kind="a",
       setup=wi("qppackdl,0\t%dr11, %dr10, %qpr10"))
ac("-v6", "ac_ldqpsm_4", "ldqp,0,sm\t%dr0, 0x4, %qpr4", quad="%qpr4")
ac("-v6", "ac_ldqpsm_0", "ldqp,0,sm\t%dr0, 0x0, %qpr4", quad="%qpr4")


# --- 4. wait ----------------------------------------------------------------

def wait_cost(group, name, pattern):
    """PATTERN: text of one repetition."""
    for n in (32, 64):
        body = (wi("addd,0\t0x0, 0x7, %dr3") + nops(8)
                + wi("rrd,0\t%clkr, %dr10") + pattern * n
                + wi("rrd,0\t%clkr, %dr11") + nops(3)
                + wi("subd,0\t%dr11, %dr10, %dr0"))
        func(group, f"{name}_{n}", "w32" if n == 32 else "W64", body)


WAITS = {
    "": [("all_c", "all_c = 1"), ("ma_c", "ma_c = 1"), ("fl_c", "fl_c = 1"),
         ("all_c+fl_c", "all_c = 1, fl_c = 1"),
         ("ld_c+st_c", "ld_c = 1, st_c = 1"),
         ("st_c+ma_c", "st_c = 1, ma_c = 1"), ("ma_c+fl_c", "ma_c = 1, fl_c = 1"),
         ("all", "all_c = 1, all_e = 1, st_c = 1, ld_c = 1, fl_c = 1, ma_c = 1"),
         ("all_e", "all_e = 1"), ("st_c", "st_c = 1"), ("ld_c", "ld_c = 1")],
    # The raw v6 bits are here so that the 8C2 runs them too (MCST's
    # kernel puts lal into rmb() on every CPU; e2k_api.h, barrier.h).
    "-v5": [("st_c+sas", "st_c = 1, sas = 1"), ("ld_c+sal", "ld_c = 1, sal = 1"),
            ("ld_c+st_c+sal+sas", "ld_c = 1, st_c = 1, sal = 1, sas = 1"),
            ("raw_las", 0x200), ("raw_lal", 0x400), ("raw_mt", 0x800),
            ("raw_ld_c_lal", 0x408), ("raw_st_c_las", 0x204)],
    "-v6": [("st_c+las", "st_c = 1, las = 1"), ("ld_c+lal", "ld_c = 1, lal = 1"),
            ("mt", "mt = 1"), ("ld_c+st_c+mt", "ld_c = 1, st_c = 1, mt = 1"),
            ("ld_c+st_c+las+lal", "ld_c = 1, st_c = 1, las = 1, lal = 1"),
            # MCST's kernel's v6 smp_mb() (asm/barrier.h).
            ("kmb", "ld_c = 1, st_c = 1, sas = 1, sal = 1, las = 1, lal = 1, "
             "mt = 1"),
            ("raw_sas", 0x80), ("raw_sal", 0x100)],
}


def wait_text(w):
    return raw_wait(w) if isinstance(w, int) else wi(f"wait\t{w}")


for group, ws in WAITS.items():
    for name, w in ws:
        t = wait_text(w)
        tag = name.replace("+", "_")
        wait_cost(group, f"w_{tag}", t)
        wait_cost(group, f"w_std_{tag}", wi("std,2\t%dr0, 0x100, %dr3") + t)
        wait_cost(group, f"w_ldd_{tag}", wi("ldd,0\t%dr0, 0x100, %dr8") + t)
# Without a wait, for the pair costs.
wait_cost("", "w_std_none", wi("std,2\t%dr0, 0x100, %dr3") + wi("nop"))
wait_cost("", "w_ldd_none", wi("ldd,0\t%dr0, 0x100, %dr8") + wi("nop"))


def sb(group, name, w, between=None):
    body = (wi("addd,0\t0x0, 0x1, %dr4") + nops(6)
            + wi("std,2\t%dr0, 0x0, %dr4")
            + (wait_text(w) if w else "") + (between or "")
            + wi("ldd,0\t%dr1, 0x0, %dr0") + nops(2))
    func(group, name, "B", body)


sb("", "sb_none", None)
# Plain delays between the store and the load, as controls for the waits'
# own cost (a wait with nothing outstanding takes 6-8 cycles).
sb("", "sb_nop1", None, wi("nop"))
for d in range(2, 8):
    sb("", f"sb_nop{d}", None, wi(f"nop {d - 1}"))
sb("", "sb_nop8", None, wi("nop 7"))
sb("", "sb_nop16", None, nops(16))
sb("", "sb_nop32", None, nops(32))
for group, ws in WAITS.items():
    for name, w in ws:
        if name in ("all", "all_e", "ld_c"):
            continue
        sb(group, "sb_" + name.replace("+", "_"), w)
sb("", "sb_all_e", "all_e = 1")


# The wait in the load's own instruction (as lcc pairs a release wait
# with its store): a soft barrier that doesn't stall shows whether it
# orders the load after the store.
def sbw(group, name, w):
    body = (wi("addd,0\t0x0, 0x1, %dr4") + nops(6)
            + wi("std,2\t%dr0, 0x0, %dr4")
            + wi(f"wait\t{w}", "ldd,0\t%dr1, 0x0, %dr0") + nops(2))
    func(group, name, "B", body)


for group, ws in WAITS.items():
    for name, w in ws:
        if isinstance(w, int) or name in ("all", "all_e", "ld_c"):
            continue
        sbw(group, "sbw_" + name.replace("+", "_"), w)
# (Verifier's addition.)  What the sbw shape costs: a store, then the
# wait in the load's own instruction, repeated (wait_cost's harness): how
# far each wait pushes the load behind the store.
for name, w in (("none", None), ("ld_c+st_c", "ld_c = 1, st_c = 1"),
                ("st_c+sas", "st_c = 1, sas = 1"), ("ld_c+sal", "ld_c = 1, sal = 1"),
                ("ld_c+st_c+sal+sas", "ld_c = 1, st_c = 1, sal = 1, sas = 1"),
                ("st_c+las", "st_c = 1, las = 1"), ("ld_c+lal", "ld_c = 1, lal = 1"),
                ("mt", "mt = 1"),
                ("kmb", "ld_c = 1, st_c = 1, sas = 1, sal = 1, las = 1, lal = 1, "
                 "mt = 1")):
    ops = ([f"wait\t{w}"] if w else []) + ["ldd,0\t%dr0, 0x140, %dr8"]
    wait_cost("-v6", "w_sbw_" + name.replace("+", "_"),
              wi("std,2\t%dr0, 0x100, %dr3") + wi(*ops))


# trap = 1 alone (MCST's kernel: the v3-v5 C1 idle state; v6 replaced it
# with int = 1, mem_mod = 1).
func("", "wtrap", "X", wi("rrd,0\t%clkr, %dr4") + wi("wait\ttrap = 1")
     + wi("rrd,0\t%clkr, %dr5") + nops(3) + wi("subd,0\t%dr5, %dr4, %dr0"))


# --- watch for modification (v6), quad and doubleword --------------------

def watch(name, load, wait="wait\tmem_mod = 1, int = 1"):
    body = (wi("rrd,0\t%clkr, %dr4")
            + wi("nop 4", f"disp\t%ctpr1, .L{name}_w", load)
            + wi("nop 2", "cmpedb,0\t%dr2, %dr1, %pred0")
            + wi("ct\t%ctpr1 ? ~%pred0") + wi(wait)
            + f".L{name}_w:\n" + wi("rrd,0\t%clkr, %dr5") + nops(3)
            + wi("subd,0\t%dr5, %dr4, %dr0"))
    func("-v6", name, "Q", body)


watch("watch_spin", "ldd,0\t%dr0, 0x0, %dr2", wait="nop")
watch("watch_ldd11", "ldd,0\t%dr0, 0x0, %dr2, mas=0x11")
watch("watch_ldqp11_c0", "ldqp,0\t%dr0, 0x0, %qpr2, mas=0x11")
watch("watch_ldqp11_c2", "ldqp,2\t%dr0, 0x0, %qpr2, mas=0x11")
watch("watch_ldd00", "ldd,0\t%dr0, 0x0, %dr2")
watch("watch_ldq11_c0", "ldq,0\t%dr0, 0x0, %qr2, mas=0x11")
# (Verifier's addition.)  Does 0x31 (dc_ch 1 with m1, mod 1) arm the watch?
watch("watch_ldd31", "ldd,0\t%dr0, 0x0, %dr2, mas=0x31")


# Load to use through the AAU against ldd, data in L1 (buf, just read):
# 32 pairs of { load into %dr8 } { addd %dr8 }; also an ldaad from a
# line ldd just read, and an ldd from a line only ldaad has read.
def use_pairs(group, name, load, setup="", gap=0):
    """GAP empty cycles between each load and its use."""
    body = setup + wi("ldd,0\t%dr1, 0x0, %dr20", "ldd,2\t%dr1, 0x8, %dr21")
    body += nops(8) + wi("rrd,0\t%clkr, %dr10")
    for _ in range(32):
        body += wi(load) + nops(gap) + wi("addd,0\t%dr8, %dr9, %dr9")
    body += wi("rrd,0\t%clkr, %dr11") + nops(3)
    body += wi("subd,0\t%dr11, %dr10, %dr0")
    func(group, name, "T", body)


AAUT = (wi("addd,0\t0x0, 0x0, %dr24", "addd,1\t0x0, 0x0, %dr9")
        + wi("aaurwd,2\t%dr1, %aad0", "aaurwd,5\t%dr24, %aasti1") + nops(8))
use_pairs("", "t_use_ldd5", "ldd,5\t%dr1, 0x0, %dr8", wi("addd,1\t0x0, 0x0, %dr9"))
use_pairs("", "t_use_ldd2", "ldd,2\t%dr1, 0x0, %dr8", wi("addd,1\t0x0, 0x0, %dr9"))
use_pairs("", "t_use_ldaad5", "ldaad,5\t%aad0[ %aasti1 ], %dr8", AAUT)
use_pairs("", "t_use_ldaad2", "ldaad,2\t%aad0[ %aasti1 ], %dr8", AAUT)
use_pairs("", "t_use_ldaad5_60", "ldaad,5\t%aad0[ %aasti1 ], %dr8, mas=0x60", AAUT)
for g in (2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14):
    use_pairs("", f"t_use_ldd5_g{g}", "ldd,5\t%dr1, 0x0, %dr8",
              wi("addd,1\t0x0, 0x0, %dr9"), gap=g)
    use_pairs("", f"t_use_ldaad5_g{g}", "ldaad,5\t%aad0[ %aasti1 ], %dr8",
              AAUT, gap=g)
# (Verifier's addition.)  The same with the load in ALC2, the consumer's
# cluster: the ALC5 series above includes the cross-cluster cost.
for g in (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14):
    use_pairs("", f"t_use_ldd2_g{g}", "ldd,2\t%dr1, 0x0, %dr8",
              wi("addd,1\t0x0, 0x0, %dr9"), gap=g)
    use_pairs("", f"t_use_ldaad2_g{g}", "ldaad,2\t%aad0[ %aasti1 ], %dr8",
              AAUT, gap=g)


# Helpers (not cases): %upsr read and write as sysabi.s does, %clkr.
# chaseN (p): %clkr cycles of 128 dependent loads in ALC N, 3 cycles a
# step when they hit L1 (a load, then nop 1).
for ch in (0, 3):
    body = wi("rrd,0\t%clkr, %dr10")
    for _ in range(128):
        body += wi(f"ldd,{ch}\t%dr0, 0x0, %dr0") + wi("nop 1")
    # The addd waits for the last load, so the rrd beside it reads after.
    body += (wi("addd,1\t%dr0, 0x0, %dr2", "rrd,0\t%clkr, %dr11") + nops(3)
             + wi("subd,0\t%dr11, %dr10, %dr0"))
    func("", f"chase{ch}", "-", body)
func("", "rd_upsr", "-", wi("rrs,0\t%upsr, %r0"))
func("", "wr_upsr", "-", wi("rws,0\t%r0, %upsr") + wi("wait\tall_c = 1"))
func("", "clkr", "-", wi("rrd,0\t%clkr, %dr0"))


def main():
    for g, cases in groups.items():
        (HERE / f"memleft{g}.s").write_text(
            "! SPDX-License-Identifier: GPL-3.0-or-later\n"
            "! Generated by tools/isa-probes/gen_memleft.py; see memleft.c.\n"
            "\t.ignore\tld_st_style\n\t.text\n"
            + "".join(t for _, _, t in cases))
        (HERE / f"memleft{g}-cases.h").write_text(
            "/* SPDX-License-Identifier: GPL-3.0-or-later */\n"
            "/* Generated by tools/isa-probes/gen_memleft.py.  */\n"
            + "".join(f'CASE ({n}, "{k}")\n' for n, k, _ in cases
                      if k != "-"))
        print(f"memleft{g}.s: {len(cases)} functions")


if __name__ == "__main__":
    main()
