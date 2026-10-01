#!/usr/bin/env python3
"""A reference E2K decoder built from spec/isa.

It decodes wide instructions using only spec/isa/bundle.md,
spec/isa/binutils-ops.yaml and spec/isa/registers.yaml, and prints them in
the same textual form as MCST objdump, so the two can be compared line by
line (see compare_objdump.py).
"""

import sys
from pathlib import Path

import yaml

SPEC = Path(__file__).resolve().parents[2] / "spec" / "isa"

OPC2_VALUES = {"EXT": 0x1, "EXT1": 0x2, "EXT2": 0x3, "FLB": 0x4, "FLH": 0x5,
               "FLW": 0x6, "FLD": 0x7, "ICMB0": 0x8, "ICMB1": 0x9,
               "ICMB2": 0xA, "ICMB3": 0xB, "FCMB0": 0xC, "FCMB1": 0xD,
               "PFCMB0": 0xE, "PFCMB1": 0xF, "LCMBD0": 0x10, "LCMBD1": 0x11,
               "LCMBQ0": 0x12, "LCMBQ1": 0x13, "QPFCMB0": 0x16, "QPFCMB1": 0x17}
ALES25_DEFAULT = 0x01c0
FMT_PREFIX = {"s": "", "d": "d", "q": "q", "p": "qp"}
SPECIAL_DST = {13: "%tst", 14: "%tc", 15: "%tcd", 17: "%ctpr1", 18: "%ctpr2",
               19: "%ctpr3", 30: "%empty.lo", 31: "%empty.hi"}
MAS_SHIFT = {0: 21, 2: 14, 3: 7, 5: 0}

SHORT_FORMATS = {"ALOPF1", "MERGE", "ALOPF2", "ALOPF3", "ALOPF7", "ALOPF8"}

# Quad operations occupying two channels: only the lower channel is printed.
PAIRED_HI = {
    (1, 4): {"movtq", "gdtoap", "cast", "odtoap", "aptoap", "aptoapb"},
    (2, 5): {"ldgdq", "ldcudq", "ldapq", "ldq", "ldcsq", "lddsq", "ldesq",
             "ldfsq", "ldgsq", "ldssq"},
    (5,): {"staaq", "stgdq", "stapq", "aaurwq", "aaurrq", "stcsq", "stdsq",
           "stesq", "stfsq", "stgsq", "stssq"},
    (1,): {"getsap"},
}


def opc2_value(v):
    return OPC2_VALUES[v] if isinstance(v, str) else v


class Spec:
    def __init__(self, iset):
        self.iset = iset
        ops = yaml.safe_load((SPEC / "binutils-ops.yaml").read_text())["alu_ops"]
        regs = yaml.safe_load((SPEC / "registers.yaml").read_text())
        # Candidates by opcode, latest table entry first.
        self.by_opc = {}
        for op in ops:
            if iset in op["isets"]:
                self.by_opc.setdefault(op["opc"], []).insert(0, op)
        self.canonical = {}
        for op in ops:
            for alias in op.get("aliases", []):
                self.canonical[alias] = op["name"]
        self.state_regs = {}
        for r in regs["state_registers"]:
            if iset in r["isets"] and not r.get("assembler_only"):
                self.state_regs.setdefault(r["num"], r["name"])
        self.aau_regs = {r["opce1"]: r["name"] for r in regs["aau_registers"]}


class Bundle:
    """Syllables of one wide instruction (spec/isa/bundle.md)."""

    def __init__(self, buf):
        w = lambda o: int.from_bytes(buf[o:o + 4], "little")
        h = lambda o: int.from_bytes(buf[o:o + 2], "little")
        self.hs = hs = w(0)
        self.length = (((hs >> 4) & 7) + 1) * 8
        self.valid = False
        self.ss = self.cs0 = self.cs1 = None
        self.als, self.ales, self.ales_marked = {}, {}, set()
        self.aas, self.lts, self.pls, self.cds = {}, [], [], []

        pos = 4
        if hs & (1 << 12):
            self.ss, pos = w(pos), pos + 4
        for i in range(6):
            if hs & (1 << (26 + i)):
                self.als[i], pos = w(pos), pos + 4
        if hs & (1 << 14):
            self.cs0, pos = w(pos), pos + 4
        for i in range(6):
            if hs & (1 << (20 + i)):
                self.ales_marked.add(i)
        for i in (2, 5):
            if i in self.ales_marked:
                self.ales[i] = ALES25_DEFAULT
        mdl = ((hs & 0xf) + 1) * 4
        cs1 = bool(hs & (1 << 15))
        if mdl == pos + (8 if cs1 else 4):
            if 5 in self.ales_marked:
                self.ales[5] = h(pos)
            if 2 in self.ales_marked:
                self.ales[2] = h(pos + 2)
            pos += 4
        if cs1:
            self.cs1, pos = w(pos), pos + 4
        if mdl != pos:
            return

        halves = [i for i in (0, 1, 3, 4) if i in self.ales_marked]
        if self.ss is not None and not self.ss & (1 << 20):
            aa = [i for i in range(4) if self.ss & (1 << (12 + i))]
            aas_idx = sorted({i >> 1 for i in aa}) + [2 + i for i in aa]
        else:
            aas_idx = []
        k = 0
        for i in halves:
            self.ales[i] = h(pos + 2 * ((k & ~1) + 1 - (k & 1)))
            k += 1
        for i in aas_idx:
            self.aas[i] = h(pos + 2 * ((k & ~1) + 1 - (k & 1)))
            k += 1
        k += k & 1
        gap = pos + 2 * k

        end = self.length - 4
        for _ in range((hs >> 16) & 3):
            self.cds.append(w(end))
            end -= 4
        for _ in range((hs >> 18) & 3):
            self.pls.append(w(end))
            end -= 4
        if end < gap and end != gap - 4:
            return
        while len(self.lts) < 4 and end >= gap:
            self.lts.append(w(end))
            end -= 4
        self.valid = True


class Printer:
    def __init__(self, spec):
        self.spec = spec

    # Operands ---------------------------------------------------------------

    def reg(self, v, fmt):
        p = FMT_PREFIX[fmt]
        if v & 0x80 == 0:
            return f"%{p}b[{v & 0x7f}]"
        if v & 0xc0 == 0x80:
            return f"%{p}r{v & 0x3f}"
        return f"%{p}g{v & 0x1f}"

    def lit32(self, b, v):
        n = v & 3
        if n >= len(b.lts):
            return f"<nonexistent _f32s,_lts{n}>"
        return f"_f32s 0x{b.lts[n]:x}"

    def src1(self, v, fmt):
        if v & 0xe0 == 0xc0:
            return f"0x{v & 0x1f:x}"
        return self.reg(v, fmt)

    def src2(self, b, v, fmt):
        if v & 0xf0 == 0xc0:
            return f"0x{v & 0xf:x}"
        if v & 0xf8 == 0xd0:
            n, hi = v & 3, bool(v & 4)
            if n > 1:
                return f"<illegal _f16s,_lts{n}{'hi' if hi else 'lo'}>"
            if n >= len(b.lts):
                return f"<nonexistent _f16s,_lts{n}{'hi' if hi else 'lo'}>"
            return f"_f16s 0x{(b.lts[n] >> (16 if hi else 0)) & 0xffff:x}"
        if v & 0xfc == 0xd8:
            return self.lit32(b, v)
        if v & 0xfc == 0xdc:
            n = v & 3
            if n == 3:
                return f"<illegal _f64,_lts{n}>"
            if n + 1 >= len(b.lts):
                return f"<nonexistent _f64,_lts{n}>"
            lo, hi = b.lts[n], b.lts[n + 1]
            return f"_f64 0x{lo:x}" if hi == 0 else f"_f64 0x{hi:x}{lo:08x}"
        return self.reg(v, fmt)

    def src3(self, b, v, fmt, lit_ok):
        if v & 0xe0 != 0xc0:
            return self.reg(v, fmt)
        if not lit_ok:
            return "<illegal operand>"
        if v & 0xf0 == 0xc0:
            return "0x0" if v & 0xf == 0 else "<illegal operand>"
        if v & 0xf8 == 0xd0:
            return "0x0" if v & 7 == 0 else "<illegal operand>"
        if v & 0xfc == 0xd8:
            return self.lit32(b, v)
        return "0x0" if v & 3 == 0 else "<illegal operand>"

    def dst(self, v, fmt):
        if v & 0xe0 == 0xc0:
            return SPECIAL_DST.get(v & 0x1f, "<reserved special register>")
        return self.reg(v, fmt)

    # Predicates (CDS) -----------------------------------------------------

    def predicates(self, b, chn, mrgc):
        if chn <= 2 or chn == 6:
            mask_bit, neg_bit = (3, 2) if chn == 6 else (chn, chn)
            want = 2 if mrgc else 0
        else:
            mask_bit, neg_bit = (3, 2) if chn == 7 else (chn - 3, chn - 3)
            want = 3 if mrgc else 1
        out = ""
        first = True
        for cds in b.cds:
            for half in (cds & 0xffff, cds >> 16):
                opc, mask = half >> 14, (half >> 10) & 0xf
                if opc != want or not mask & (1 << mask_bit):
                    continue
                neg, pred = (half >> 7) & 7, half & 0x7f
                out += (", " if mrgc else " ? ") if first else (" " if mrgc else ", ")
                first = False
                if neg & (1 << neg_bit):
                    out += "~"
                if pred & 0x60 == 0x40:
                    out += f"%pcnt{pred & 0x1f}"
                elif pred & 0x60 == 0x60:
                    out += f"%pred{pred & 0x1f}"
                else:
                    out += "<invalid predicate>"
        return out

    # ALU syllables ------------------------------------------------------------

    def match(self, b, chn):
        als = b.als[chn]
        opc, src1 = (als >> 24) & 0x7f, (als >> 16) & 0xff
        ales = b.ales.get(chn)
        marked = chn in b.ales_marked
        found = None
        for op in self.spec.by_opc.get(opc, []):
            if chn not in op["channels"]:
                continue
            f = op["alopf"]
            if f in SHORT_FORMATS:
                if marked:
                    continue
                if f == "ALOPF2" and src1 != op["opce"]:
                    continue
                if f in ("ALOPF7", "ALOPF8") and (als >> 5) & 7 != op["cmpopce"]:
                    continue
            elif not marked:
                continue
            elif f in ("ALOPF10", "ALOPF19"):
                want = 0x02c0 if op["name"] in ("staaqp", "ldaaqp") else 0x01c0
                if ales != want:
                    continue
            elif f in ("AAURR", "AAURW"):
                # AAU register access is staa/ldaa with MAS 0x3f (special
                # MMU/AAU mode, AAU register operation). objdump accepts any
                # ALES of 0x01c0 instead; see KNOWN_OBJDUMP_BUGS in
                # compare_objdump.py.
                mas_set = b.cs1 is not None and b.cs1 >> 28 == 6
                mas = (b.cs1 >> MAS_SHIFT[chn]) & 0x7f if mas_set else 0
                if ales != 0x01c0 or mas != 0x3f:
                    continue
            elif f in ("ALOPF11", "ALOPF11_MERGE"):
                opc2 = op["ales_opc2"]
                opc2 = opc2[chn] if isinstance(opc2, dict) else opc2
                if ales != (opc2_value(opc2) << 8) | op["ales_opce"]:
                    continue
            elif f == "ALOPF11_LIT8":
                opc2 = op["ales_opc2"]
                opc2 = opc2[chn] if isinstance(opc2, dict) else opc2
                if ales & 0xff00 != opc2_value(opc2) << 8:
                    continue
            elif f in ("ALOPF12", "ALOPF12_IBRANCHD", "ALOPF12_ICALLD"):
                if f != "ALOPF12_ICALLD" and src1 != op["opce"]:
                    continue
                if ales != (opc2_value(op["ales_opc2"]) << 8) | op["ales_opce"]:
                    continue
            elif f == "ALOPF12_PSHUFH":
                if src1 != op["opce"] or ales & 0xff00 != opc2_value(op["ales_opc2"]) << 8:
                    continue
            elif f == "ALOPF13":
                if ales != (opc2_value(op["ales_opc2"]) << 8) | op["ales_opce"]:
                    continue
            elif f in ("ALOPF15", "ALOPF16"):
                if ales != 0x01c0:
                    continue
            elif f in ("ALOPF21", "ALOPF21_LT_PACK_CODE", "ALOPF21_MERGE"):
                if ales & 0xff00 != opc2_value(op["ales_opc2"]) << 8:
                    continue
            elif f == "ALOPF22":
                if ales != 0x01c0 or src1 != op["opce"]:
                    continue
            elif f == "ALOPF17":
                if ales != 0x0200 | op["cmpopce"]:
                    continue
            else:
                raise ValueError(f"unknown format {f}")

            if found is None:
                found = op
                continue
            if ((found["alopf"] == "ALOPF10" and f == "AAURW")
                    or (found["alopf"] == "ALOPF19" and f == "AAURR")):
                found = op
            elif found["alopf"] == "ALOPF16" and (als >> 8) & 0xff != found["src2"]:
                found = op
            break
        return found

    def alu(self, b, chn):
        als = b.als[chn]
        op = self.match(b, chn)
        name = op["name"] if op else "unrecognized"
        for chans, names in PAIRED_HI.items():
            if chn in chans and name in names:
                return []
        if name.endswith("_lt3"):
            name = name[:-4]
        head = f"{name},{chn}{',sm' if als >> 31 else ''} "
        if op is None:
            return [head]

        f, a = op["alopf"], op["args"]
        s1, s2, d = (als >> 16) & 0xff, (als >> 8) & 0xff, als & 0xff
        ales = b.ales.get(chn, 0)
        if f in ("ALOPF1", "MERGE", "ALOPF11", "ALOPF11_LIT8", "ALOPF11_MERGE",
                 "ALOPF21", "ALOPF21_LT_PACK_CODE", "ALOPF21_MERGE"):
            parts = [self.src1(s1, a[0]), self.src2(b, s2, a[1])]
            if f == "ALOPF11_LIT8":
                parts.append(f"0x{ales & 0xff:x}")
            elif f.startswith("ALOPF21"):
                parts.append(self.src3(b, ales & 0xff, a[2],
                                       self.spec.iset >= 7 and f == "ALOPF21_LT_PACK_CODE"))
            dfmt = a[3] if f in ("ALOPF21", "ALOPF21_LT_PACK_CODE") else a[2]
            text = ", ".join(parts + [self.dst(d, dfmt)])
        elif f in ("ALOPF2", "ALOPF12", "ALOPF12_PSHUFH", "ALOPF12_IBRANCHD",
                   "ALOPF12_ICALLD", "ALOPF22"):
            text = f"wbs = 0x{s1:x}, " if f == "ALOPF12_ICALLD" else ""
            text += self.src2(b, s2, a[0]) + ", "
            if f == "ALOPF12_PSHUFH":
                text += f"0x{ales & 0xff:x}, "
            text += self.dst(d, a[1])
            if f in ("ALOPF12_IBRANCHD", "ALOPF12_ICALLD"):
                text += self.ctcond(b.ss & 0x1ff)
        elif f in ("ALOPF3", "ALOPF13"):
            text = ", ".join([self.src1(s1, a[0]), self.src2(b, s2, a[1]),
                              self.src3(b, d, a[2], False)])
        elif f == "ALOPF15":
            text = f"{self.src2(b, s2, a[0])}, {self.spec.state_regs.get(d)}"
        elif f == "ALOPF16":
            text = f"{self.spec.state_regs.get(s1)}, {self.dst(d, a[0])}"
        elif f in ("ALOPF7", "ALOPF17"):
            text = f"{self.src1(s1, a[0])}, {self.src2(b, s2, a[1])}, %pred{d & 0x1f}"
        elif f == "ALOPF8":
            text = f"{self.src2(b, s2, a[0])}, %pred{d & 0x1f}"
        elif f in ("ALOPF10", "ALOPF19"):
            opce1 = (als >> 8) & 0xffff
            lt, ind, dd = opce1 & 3, (opce1 >> 7) & 0xf, (opce1 >> 11) & 0x1f
            text = (self.src3(b, d, a[0], False) + ", ") if f == "ALOPF10" else ""
            text += f"%aad{dd} [ %aasti{ind}"
            if lt:
                text += (f" + _f32s,_lts{lt - 1} 0x{b.lts[lt - 1]:x}"
                         if lt - 1 < len(b.lts) else " + <non-existent literal>")
            text += " ]"
            if f == "ALOPF19":
                text += ", " + self.dst(d, a[0])
        elif f in ("AAURR", "AAURW"):
            reg = self.spec.aau_regs.get((als >> 8) & 0xffff, "%unknown_aau_reg")
            text = (self.src3(b, d, a[0], False) + ", " + reg) if f == "AAURW" \
                else (reg + ", " + self.dst(d, a[0]))
        else:
            raise ValueError(f)

        if ("MAS" in op["flags"] and chn in MAS_SHIFT and b.cs1 is not None
                and b.cs1 >> 28 == 6):
            mas = (b.cs1 >> MAS_SHIFT[chn]) & 0x7f
            if mas:
                text += f", mas = 0x{mas:x}"
        if f in ("MERGE", "ALOPF11_MERGE", "ALOPF21_MERGE"):
            text += self.predicates(b, chn, True)
        if f not in ("ALOPF12_IBRANCHD", "ALOPF12_ICALLD"):
            text += self.predicates(b, chn, False)
        lines = [head + text]
        if f == "ALOPF10":
            opce1 = (als >> 8) & 0xffff
            if opce1 & 4:
                lines.append(f"incr,{chn} %aaincr{(opce1 >> 4) & 7}"
                             + self.predicates(b, 6 if chn == 2 else 7, False))
        return lines

    # Control ------------------------------------------------------------------

    def ctcond(self, cc):
        t, psrc = (cc >> 5) & 0xf, cc & 0x1f
        if t == 1:
            return ""
        s = " ? "
        if t in (3, 7, 0xe):
            s += "~"
        if t in (2, 3, 6, 7, 0xe, 0xf):
            s += f"%pred{psrc}"
        if t in (4, 6, 0xe):
            s += (" || " if t in (6, 0xe) else "") + "%LOOP_END"
        if t in (5, 7, 0xf):
            s += (" && " if t in (7, 0xf) else "") + "%NOT_LOOP_END"
        if t == 8:
            s += "%MLOCK"
            if psrc & 0xf:
                s += " || %dt_al" + "".join(str(c) for i, c in enumerate((0, 1, 3, 4))
                                             if psrc & (1 << i))
        if t == 9 or (self.spec.iset >= 7 and t == 11):
            lock = "%MLOCK || " if t == 9 else ""
            kind = (psrc >> 3) & 3
            if kind == 0:
                s += f"{lock}{'~' if psrc & 1 else ''}%cmp{(0, 1, 3, 4)[(psrc >> 1) & 3]}"
            elif kind == 1:
                jk = (psrc >> 2) & 1
                s += (f"{lock}{'~' if psrc & 2 else ''}%cmp{3 if jk else 0} || "
                      f"{'~' if psrc & 1 else ''}%cmp{4 if jk else 1}")
            elif kind == 2:
                s += f"{lock}{'~' if psrc & 1 else ''}%clp{(psrc >> 1) & 3}"
        if 0xa <= t <= 0xd and not (self.spec.iset >= 7 and t == 0xb):
            s += "<reserved condition type>"
        return s

    def hs(self, b):
        out = []
        if b.hs & 0x400:
            out.append("loop_mode")
        if b.hs & 0x380:
            out.append(f"nop {(b.hs >> 7) & 7}")
        if b.hs & ~0x70 == 0:
            out.append("nop")
        return out

    def ss_lines(self, b):
        ss, out = b.ss, []
        ctop, cc = (ss >> 10) & 3, ss & 0x1ff
        is_call = b.cs1 is not None and b.cs1 >> 28 == 5
        if (cc >> 5) & 0xf != 0 and not is_call and ctop != 0:
            out.append(f"ct %ctpr{ctop}" + self.ctcond(cc))
        out.append(f"ipd {ss >> 30}")
        t0 = not ss & (1 << 20)
        if t0 and ss & (1 << 29):
            out.append("eap")
        if t0 and ss & (1 << 28):
            out.append("bap")
        rp = ((ss >> 27) & 1) << 1 | ((ss >> 25) & 1)
        if rp:
            out.append(("", "crp", "srp", "slrp")[rp])
        if t0:
            if ss & (1 << 26):
                out.append("vfdi")
            for name, sh, a, c in (("abg", 23, "abgi", "abgd"), ("abn", 21, "abnf", "abnt"),
                                   ("abp", 18, "abpf", "abpt"), ("alc", 16, "alcf", "alct")):
                v = (ss >> sh) & 3
                if v:
                    out.append(f"{name} {a}={v >> 1}, {c}={v & 1}")
        return out

    def cs0_lines(self, b, addr):
        cs0 = b.cs0
        table = (("ibranch", "pref", "puttsd", "done"),
                 ("disp", None, "sdisp", "gettsd"),
                 ("disp", "ldisp", "sdisp", "gettsd"),
                 ("disp", None, "sdisp", "return"))
        ctpr, ctp_opc, ptype = cs0 >> 30, (cs0 >> 28) & 3, cs0 & 7
        kind = table[ctpr][ctp_opc] or ""
        if kind == "return" and ptype == 1:
            kind = "gettsd"
        elif kind == "done":
            kind = {2: "iret", 3: "hret", 4: "glaunch"}.get(ptype, "done")
        disp = lambda: addr + (((cs0 & 0x0fffffff) ^ 0x08000000) - 0x08000000) * 8

        if kind in ("ibranch", "done", "iret", "hret", "glaunch"):
            ss = b.ss
            if ss is None or ss & 0xc00:
                return [f"invalid {kind}"]
            hicall = (b.cs1 is not None and b.cs1 >> 28 == 5
                      and (b.cs1 >> 7) & 7 in (1, 2))
            if ss & 0x1ff and not hicall:
                text = kind
                if kind == "ibranch":
                    text += f" 0x{disp() & 0xffffffffffffffff:x}"
                return [text + self.ctcond(ss & 0x1ff)]
            return []
        text = ("invalid " if kind == "gettsd" and ptype != 1 else "") + kind + " "
        if kind in ("disp", "sdisp", "ldisp", "return", "gettsd"):
            text += f"%ctpr{ctpr}"
        if kind == "sdisp":
            text += f", 0x{cs0 & 0x1f:x}"
        elif kind in ("disp", "ldisp", "puttsd"):
            text += ("" if kind == "puttsd" else ", ") + f"0x{disp() & 0xffffffffffffffff:x}"
        if kind == "pref":
            text += (f"%ipr{cs0 & 7}, disp = 0x{(cs0 >> 4) & 0xffffff:x}, "
                     f"ipd = {(cs0 >> 3) & 1}")
        return [text]

    def cs1_lines(self, b):
        cs1, iset, out = b.cs1, self.spec.iset, []
        opc = cs1 >> 28
        if opc in (0, 1, 4):
            if opc == 1:
                out.append("<bogus vfrpsz>" if not b.lts
                           else f"vfrpsz rpsz = 0x{(b.lts[0] >> 12) & 0x1f:x}")
            if opc in (0, 1):
                if not b.lts:
                    out.append("<bogus setwd>")
                else:
                    l0 = b.lts[0]
                    s = f"setwd wsz = 0x{(l0 >> 5) & 0x7f:x}, nfx = 0x{(l0 >> 4) & 1:x}"
                    if iset >= 3:
                        s += f", dbl = 0x{(l0 >> 3) & 1:x}"
                    out.append(s)
            if cs1 & (1 << 26):
                out.append(f"setbn rbs = 0x{cs1 & 0x3f:x}, rsz = 0x{(cs1 >> 6) & 0x3f:x}, "
                           f"rcur = 0x{(cs1 >> 12) & 0x3f:x}")
            if cs1 & (1 << 27):
                out.append(f"setbp psz = 0x{(cs1 >> 18) & 0x1f:x}")
        elif opc == 2:
            if cs1 & (1 << 27):
                out.append("setsft" if iset >= 2 else "unimp")
            else:
                out.append(f"setei 0x{cs1 & 0xff:x}")
        elif opc == 3:
            s = "wait "
            if iset >= 5:
                if cs1 & 8:
                    s += f"sal = {(cs1 >> 8) & 1}, "
                if cs1 & 4:
                    s += f"sas = {(cs1 >> 7) & 1}, "
            if iset >= 2:
                s += f"trap = {(cs1 >> 6) & 1}, "
            s += ", ".join(f"{n} = {(cs1 >> i) & 1}" for n, i in
                           (("ma_c", 5), ("fl_c", 4), ("ld_c", 3), ("st_c", 2),
                            ("all_e", 1), ("all_c", 0)))
            out.append(s)
        elif opc == 5:
            ctop, wbs = (b.ss >> 10) & 3 if b.ss is not None else 0, cs1 & 0x7f
            if ctop:
                out.append(f"call %ctpr{ctop}, wbs = 0x{wbs:x}" + self.ctcond(b.ss & 0x1ff))
            else:
                ctopc = (cs1 >> 7) & 7
                if ctopc in (1, 2) and b.cs0 is not None:
                    if b.cs0 >> 28 == 0:
                        hcall = ctopc == 2
                        disp = (b.cs0 >> 1) & 0xf if hcall else b.cs0 & 0xfffffff
                        out.append(f"{'h' if hcall else 'i'}call 0x{disp:x}, wbs = 0x{wbs:x}"
                                   + self.ctcond(b.ss & 0x1ff))
                else:
                    out.append("<bogus call>")
        elif opc == 6:
            pass  # MAS values are printed with the operations they apply to
        elif opc == 7:
            for bit, name in ((0, "flushr"), (1, "flushc"), (2, "fillr"), (3, "fillc")):
                if cs1 & (1 << bit):
                    out.append(("unimp " if bit >= 2 and iset < 6 else "") + name)
        elif opc == 8:
            out.append(f"vfbg umask = 0x{cs1 & 0xff:x}, dmask = 0x{(cs1 >> 8) & 0xff:x}, "
                       f"chkm4 = 0x{(cs1 >> 16) & 1:x}")
        else:
            out.append("unimp")
        return out

    def aas_lines(self, b):
        out = []
        names = ("reserved", "movab", "movah", "movaw", "movad", "movaq",
                 "reserved", "movaqp")
        for i in sorted(b.aas):
            if i < 2:
                continue
            j = i - 2
            dst = (b.aas[j // 2] >> ((1 - j % 2) * 8)) & 0xff
            v = b.aas[i]
            opc = (v >> 12) & 7
            out.append(f"{names[opc]},{j} area = {(v >> 6) & 0x1f}, ind = {(v >> 1) & 0x1f}, "
                       f"am = {v & 1}, be = {v >> 15}, " + self.dst(dst, "d" if opc < 5 else "q"))
        return out

    def pls_lines(self, b):
        need = [0] * 7
        for i, p in enumerate(b.pls):
            if p & 0x20:
                need[4 + i] = 1
        again = True
        while again:
            again = False
            for i, p in enumerate(b.pls):
                if need[4 + i] == 1:
                    for q in ((p >> 10) & 7, (p >> 6) & 7):
                        if q < 7 and need[q] == 0:
                            need[q] = 1
                            again |= q >= 4
                    need[4 + i] = 2
        out = []
        for i in reversed(range(len(b.pls))):
            p = b.pls[i]
            if i < 2:
                for k, elp in ((2 * i, (p >> 24) & 0x7f), (2 * i + 1, (p >> 16) & 0x7f)):
                    if need[k] or (elp & 0x60 == 0x40 and 1 <= elp & 0x1f <= 15):
                        out.append(f"pass {self.elp(elp)}, @p{k}")
            if need[4 + i]:
                c = p & 0xffff
                name = ("andp", "landp", "reserved", "movep")[c >> 14]
                out.append(f"{name} {'~' if c & 0x2000 else ''}@p{(c >> 10) & 7}, "
                           f"{'~' if c & 0x200 else ''}@p{(c >> 6) & 7}, @p{4 + i}")
                if c & 0x20:
                    out.append(f"pass @p{4 + i}, %pred{c & 0x1f}")
        return out

    def elp(self, v):
        if v & 0x40 == 0:
            if v & 0x3f == 0:
                return "%lcntex"
            return "%spred" + "".join(str(i) for i in range(5) if v & (1 << i))
        if v & 0x60 == 0x40:
            n = v & 0x1f
            return "%bgrpred" if n == 0 else (f"%rndpred{n}" if n <= 15 else "<invalid pred>")
        return f"%pred{v & 0x1f}"

    def bundle(self, b, addr):
        lines = self.hs(b)
        if b.ss is not None:
            lines += self.ss_lines(b)
        for chn in sorted(b.als):
            lines += self.alu(b, chn)
        if b.cs0 is not None:
            lines += self.cs0_lines(b, addr)
        if b.cs1 is not None:
            lines += self.cs1_lines(b)
        lines += self.aas_lines(b)
        lines += self.pls_lines(b)
        return lines


def decode(buf, iset, base=0):
    """Yield (address, lines) for every wide instruction in BUF."""
    pr = Printer(Spec(iset))
    off = 0
    while off + 8 <= len(buf):
        length = (((buf[off] >> 4) & 7) + 1) * 8
        b = Bundle(buf[off:off + length]) if off + length <= len(buf) else None
        if b is not None and b.valid:
            yield base + off, pr.bundle(b, base + off)
            off += length
        else:
            yield base + off, None  # APB or invalid; not decoded yet
            off += 16


if __name__ == "__main__":
    data = Path(sys.argv[1]).read_bytes()
    for addr, lines in decode(data, int(sys.argv[2])):
        print(f"{addr:x}:")
        for line in lines or ["<not a wide instruction>"]:
            print("  " + line)
