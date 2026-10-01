#!/usr/bin/env python3
"""Check spec/isa/binutils-ops.yaml against what gas actually emits.

For every ALU operation entry, ISA version and allowed channel, this
predicts the ALS and ALES syllables from the database, assembles the
operation with gas, unpacks the resulting wide instruction and compares.
It exercises the operation table, the operand roles of each format and
the wide-instruction layout described in spec/isa/bundle.md.
"""

import argparse
import collections
import subprocess
import sys
import tempfile
from pathlib import Path

import yaml

# Operand roles per ALU operation format: the table's arg formats are listed
# in this order (see print_alf in opcodes/e2k-dis.c).
ROLES = {
    "ALOPF1": ["src1", "src2", "dst"],
    "ALOPF11": ["src1", "src2", "dst"],
    "ALOPF21": ["src1", "src2", "src3", "dst"],
    "ALOPF21_LT_PACK_CODE": ["src1", "src2", "src3", "dst"],
    "ALOPF2": ["src2", "dst"],
    "ALOPF12": ["src2", "dst"],
    "ALOPF22": ["src2", "dst"],
    "ALOPF7": ["src1", "src2", "pdst"],
    "ALOPF17": ["src1", "src2", "pdst"],
    "ALOPF8": ["src2", "pdst"],
    # Stores: src3 (the data) is encoded in the dst byte.
    "ALOPF3": ["src1", "src2", "src3"],
    "ALOPF13": ["src1", "src2", "src3"],
    "ALOPF11_LIT8": ["src1", "src2", "dst"],
    "ALOPF12_PSHUFH": ["src2", "dst"],
    # State registers: rws/rwd write src2 to the register named in dst;
    # rrs/rrd read the register named in src1.
    "ALOPF15": ["src2"],
    "ALOPF16": ["dst"],
    # AAU: staa/aaurw store src3 (in the dst byte); ldaa/aaurr load into dst.
    "ALOPF10": ["src3"],
    "AAURW": ["src3"],
    "ALOPF19": ["dst"],
    "AAURR": ["dst"],
}

# AAU operands used in the tests: %aad3 [ %aasti1 ], and %aasti1 as an AAU
# register. ALS bits 8-23 then hold ind = 1 (bits 7-10), d = 3 (bits 11-15).
AAU_MEM = ("%aad3 [ %aasti1 ]", 1 << 7 | 3 << 11)
AAU_REG = ("%aasti1", 0x84)  # opce1 of %aasti1 in spec/isa/registers.yaml

LIT8 = 0x3          # literal used for ALOPF11_LIT8 and ALOPF12_PSHUFH
LIT32 = 0x12345678  # src3 of v7 _lt3 forms; gas puts it in LTS0
SRC3_LTS0 = 0xd8    # src3 encoding of a 32-bit literal in LTS0
STATE_REG = ("%psr", 0x00)

REG_PREFIX = {"s": "%r", "d": "%dr", "q": "%qr", "p": "%qpr"}
OPC2_VALUES = {"EXT": 0x1, "EXT1": 0x2, "EXT2": 0x3, "FLB": 0x4, "FLH": 0x5,
               "FLW": 0x6, "FLD": 0x7, "ICMB0": 0x8, "ICMB1": 0x9,
               "ICMB2": 0xA, "ICMB3": 0xB, "FCMB0": 0xC, "FCMB1": 0xD,
               "PFCMB0": 0xE, "PFCMB1": 0xF, "LCMBD0": 0x10, "LCMBD1": 0x11,
               "LCMBQ0": 0x12, "LCMBQ1": 0x13, "QPFCMB0": 0x16, "QPFCMB1": 0x17}
NONE = 0xc0
ALES25_DEFAULT = 0x01c0  # opc2 = EXT, opce = NONE

# Window register numbers used for each operand role. Quad registers must be
# even, so use even numbers everywhere.
REGNUM = {"src1": 2, "src2": 4, "src3": 6, "dst": 8, "pdst": 3}


def operand_text(role, fmt):
    if role == "pdst":
        return f"%pred{REGNUM[role]}"
    return f"{REG_PREFIX[fmt]}{REGNUM[role]}"


def operand_code(role):
    return REGNUM[role] if role == "pdst" else 0x80 | REGNUM[role]


def opc2_value(v):
    return OPC2_VALUES[v] if isinstance(v, str) else v


def quad_pair(op, chan, chan_union):
    """Channels used by a quad operation placed in CHAN, or None.

    Returns (lo, hi, same_src1). Quad results (ALOPF2/11/12/22 with a 128-bit
    dst) pair ALC0 with ALC1 and ALC3 with ALC4 when the operation is allowed
    in ALC1, otherwise ALC0 with ALC2 and ALC3 with ALC5. Quad stores and AAU
    quad accesses pair ALC2 with ALC5. See spec/isa/bundle.md.
    """
    f, a = op["alopf"], op["args"]
    if ((f == "ALOPF11" and a[2] == "q") or (f in ("ALOPF2", "ALOPF22", "ALOPF12")
                                            and a[1] == "q")):
        if 1 in chan_union:
            lo, hi = (0, 1) if chan in (0, 1) else (3, 4)
        else:
            lo, hi = (0, 2) if chan in (0, 2) else (3, 5)
        same_src1 = f == "ALOPF11" and "MAS" in op["flags"] and a[0] == "q"
        return lo, hi, same_src1
    if (f == "ALOPF13" and a[2] == "q") or (f in ("ALOPF10", "AAURW", "ALOPF19",
                                                   "AAURR") and a[0] == "q"):
        return 2, 5, a[0] == "q"
    return None


def predict(op, chan, iset, chan_union):
    """Return (asm line, {channel: (expected ALS, expected ALES or None)})."""
    roles = ROLES[op["alopf"]]
    # Predicate destinations have no register format in the table.
    fields = dict(zip(roles, op["args"] + ("-" if roles[-1] == "pdst" else "")))
    f = op["alopf"]
    text = {r: operand_text(r, t) for r, t in fields.items()}
    lt3 = f == "ALOPF21_LT_PACK_CODE" and iset >= 7
    if lt3:
        text["src3"] = f"{LIT32:#x}"
    if f in ("ALOPF3", "ALOPF13"):
        # gas's default store syntax: data, address, offset.
        asm = ", ".join([text["src3"], text["src1"], text["src2"]])
    elif f == "ALOPF11_LIT8":
        asm = ", ".join([text["src1"], text["src2"], f"{LIT8:#x}", text["dst"]])
    elif f == "ALOPF12_PSHUFH":
        asm = ", ".join([text["src2"], f"{LIT8:#x}", text["dst"]])
    elif f == "ALOPF15":
        asm = f"{text['src2']}, {STATE_REG[0]}"
    elif f == "ALOPF16":
        asm = f"{STATE_REG[0]}, {text['dst']}"
    elif f == "ALOPF10":
        asm = f"{text['src3']}, {AAU_MEM[0]}"
    elif f == "ALOPF19":
        asm = f"{AAU_MEM[0]}, {text['dst']}"
    elif f == "AAURW":
        asm = f"{text['src3']}, {AAU_REG[0]}"
    elif f == "AAURR":
        asm = f"{AAU_REG[0]}, {text['dst']}"
    else:
        asm = ", ".join(text.values())
    pair = quad_pair(op, chan, chan_union)

    def syllables(real, upper):
        # In the upper channel of a pair, 128-bit operands name the odd half.
        bump = lambda role: 1 if (upper and fields.get(role) == "q"
                                  and not (role == "src1" and pair[2])) else 0
        als = op["opc"] << 24
        if "src1" in fields:
            als |= (operand_code("src1") + bump("src1")) << 16
        if "src2" in fields:
            als |= (operand_code("src2") + bump("src2")) << 8
        if "dst" in fields:
            als |= operand_code("dst") + bump("dst")
        if op["alopf"] in ("ALOPF10", "ALOPF19"):
            als |= AAU_MEM[1] << 8
        if op["alopf"] in ("AAURW", "AAURR"):
            als |= AAU_REG[1] << 8
        if "src3" in fields and op["alopf"] in ("ALOPF3", "ALOPF13", "ALOPF10",
                                                 "AAURW"):
            als |= operand_code("src3") + bump("src3")
        if op["alopf"] == "ALOPF15":
            als |= op["opce"] << 16 | STATE_REG[1]
        if op["alopf"] == "ALOPF16":
            als |= STATE_REG[1] << 16 | op["src2"] << 8
        if "pdst" in fields:
            als |= operand_code("pdst")
            if op["alopf"] in ("ALOPF7", "ALOPF8"):
                als |= op["cmpopce"] << 5
        if op["alopf"] in ("ALOPF2", "ALOPF12", "ALOPF22", "ALOPF12_PSHUFH"):
            als |= op["opce"] << 16
        if op["alopf"] == "ALOPF8":
            als |= NONE << 16  # unused src1

        ales = None
        if op["alopf"] in ("ALOPF11", "ALOPF12"):
            opc2 = op["ales_opc2"]
            if isinstance(opc2, dict):
                opc2 = opc2.get(real, opc2.get(chan))
            ales = (opc2_value(opc2) << 8) | op["ales_opce"]
        elif op["alopf"] in ("ALOPF21", "ALOPF21_LT_PACK_CODE"):
            src3 = SRC3_LTS0 if lt3 else operand_code("src3") + bump("src3")
            ales = (opc2_value(op["ales_opc2"]) << 8) | src3
        elif op["alopf"] == "ALOPF17":
            ales = 0x0200 | op["cmpopce"]
        elif op["alopf"] in ("ALOPF22", "ALOPF15", "ALOPF16", "AAURW", "AAURR"):
            ales = ALES25_DEFAULT
        elif op["alopf"] in ("ALOPF10", "ALOPF19"):
            ales = 0x02c0 if op["name"] in ("staaqp", "ldaaqp") else ALES25_DEFAULT
        elif op["alopf"] == "ALOPF13":
            ales = (opc2_value(op["ales_opc2"]) << 8) | op["ales_opce"]
        elif op["alopf"] == "ALOPF11_LIT8":
            opc2 = op["ales_opc2"]
            if isinstance(opc2, dict):
                opc2 = opc2.get(real, opc2.get(chan))
            ales = (opc2_value(opc2) << 8) | LIT8
        elif op["alopf"] == "ALOPF12_PSHUFH":
            ales = (opc2_value(op["ales_opc2"]) << 8) | LIT8
        return als, ales

    if pair is None:
        expect = {chan: syllables(chan, False)}
    elif iset < 7:
        expect = {pair[0]: syllables(pair[0], False), pair[1]: syllables(pair[1], True)}
    else:
        # From v7 only the lower channel is encoded; the upper is implied.
        expect = {pair[0]: syllables(pair[0], False)}
    return f"{op['name']},{chan} {asm}", expect


def unpack(buf, off):
    """Split one wide instruction into syllables (see spec/isa/bundle.md)."""
    word = lambda o: int.from_bytes(buf[o:o + 4], "little")
    half = lambda o: int.from_bytes(buf[o:o + 2], "little")
    hs = word(off)
    length = (((hs >> 4) & 0x7) + 1) * 8
    mdl = ((hs & 0xf) + 1) * 4
    pos = 4
    out = {"hs": hs, "als": {}, "ales": {}, "len": length}
    if hs & (1 << 12):
        out["ss"] = word(off + pos)
        pos += 4
    for i in range(6):
        if hs & (1 << (26 + i)):
            out["als"][i] = word(off + pos)
            pos += 4
    if hs & (1 << 14):
        out["cs0"] = word(off + pos)
        pos += 4
    cs1 = bool(hs & (1 << 15))
    # ALES2/ALES5 marked in HS but not allocated hold the default value.
    for i in (2, 5):
        if hs & (1 << (20 + i)):
            out["ales"][i] = ALES25_DEFAULT
    if mdl == pos + (8 if cs1 else 4):
        if hs & (1 << 25):
            out["ales"][5] = half(off + pos)
        if hs & (1 << 22):
            out["ales"][2] = half(off + pos + 2)
        pos += 4
    if cs1:
        out["cs1"] = word(off + pos)
        pos += 4
    k = 0
    for i in (0, 1, 3, 4):
        if hs & (1 << (20 + i)):
            out["ales"][i] = half(off + pos + 2 * ((k & ~1) + 1 - (k & 1)))
            k += 1
    return out


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


def check_iset(binutils, ops, iset, tmp):
    """Assemble all cases for one ISA version; return a Counter and failures."""
    chan_union = collections.defaultdict(set)
    for op in ops:
        if iset in op["isets"]:
            chan_union[op["name"]] |= set(op["channels"])
    cases = []
    for op in ops:
        if iset not in op["isets"] or op["alopf"] not in ROLES:
            continue
        for chan in op["channels"]:
            cases.append((op, chan) + predict(op, chan, iset, chan_union[op["name"]]))

    stats = collections.Counter()
    failures = []
    src = tmp / f"v{iset}.s"
    obj = tmp / f"v{iset}.o"
    binf = tmp / f"v{iset}.bin"

    # Assemble each case on its own line first, to separate gas rejections
    # from encoding mismatches.
    accepted = []
    lines = [f"{{\n  {asm}\n}}\n" for _, _, asm, _ in cases]
    src.write_text("".join(lines))
    res = run([binutils / "e2k-linux-as", f"-mcpu=elbrus-v{iset}", "-o", obj, src])
    rejected_lines = set()
    for line in res.stderr.splitlines():
        parts = line.split(":")
        if len(parts) > 2 and parts[1].isdigit() and "Error" in parts[2]:
            rejected_lines.add((int(parts[1]) - 1) // 3)
    for idx, case in enumerate(cases):
        if idx in rejected_lines:
            stats["rejected by gas"] += 1
            failures.append((iset, case[2], "rejected", res.stderr))
        else:
            accepted.append(case)

    src.write_text("".join(f"{{\n  {c[2]}\n}}\n" for c in accepted))
    res = run([binutils / "e2k-linux-as", f"-mcpu=elbrus-v{iset}", "-o", obj, src])
    if res.returncode != 0:
        sys.exit(f"v{iset}: gas failed on the filtered input:\n{res.stderr}")
    run([binutils / "e2k-linux-objcopy", "-O", "binary", "-j", ".text", obj, binf])
    buf = binf.read_bytes()

    off = 0
    fmt = lambda v: "-" if v is None else f"{v:#x}"
    for op, chan, asm, expect in accepted:
        wi = unpack(buf, off)
        off += wi["len"]
        got = {c: (wi["als"][c], wi["ales"].get(c)) for c in wi["als"]}
        if got != expect:
            stats["mismatch"] += 1
            show = lambda d: "; ".join(f"ALC{c} ALS {fmt(a)} ALES {fmt(e)}"
                                       for c, (a, e) in sorted(d.items()))
            failures.append((iset, asm, "mismatch",
                             f"got {show(got)}, expected {show(expect)}"))
        else:
            stats["ok" if len(expect) == 1 else "ok (channel pair)"] += 1
    if off != len(buf):
        sys.exit(f"v{iset}: consumed {off} of {len(buf)} bytes")
    return stats, failures


def main():
    root = Path(__file__).resolve().parents[2]
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("binutils", type=Path,
                    help="directory containing e2k-linux-as and e2k-linux-objcopy")
    ap.add_argument("--ops", type=Path,
                    default=root / "spec" / "isa" / "binutils-ops.yaml")
    ap.add_argument("--isets", default="2,3,4,5,6,7")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    ops = yaml.safe_load(args.ops.read_text())["alu_ops"]
    skipped = collections.Counter(o["alopf"] for o in ops if o["alopf"] not in ROLES)
    print("formats not checked yet:",
          ", ".join(f"{k} ({v})" for k, v in sorted(skipped.items())))

    total_fail = 0
    with tempfile.TemporaryDirectory() as tmp:
        for iset in map(int, args.isets.split(",")):
            stats, failures = check_iset(args.binutils, ops, iset, Path(tmp))
            print(f"v{iset}: " + ", ".join(f"{k} {v}" for k, v in sorted(stats.items())))
            total_fail += stats["mismatch"]
            for f in failures:
                if f[2] == "mismatch" or args.verbose:
                    print(f"  {f[1]}: {f[2]}" +
                          (f" ({f[3]})" if f[2] == "mismatch" else ""))
    sys.exit(1 if total_fail else 0)


if __name__ == "__main__":
    main()
