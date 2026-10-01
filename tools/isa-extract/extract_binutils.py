#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Extract the E2K operation database from MCST binutils opcode tables.

Reads the generated ALU operation table (opcodes/e2k-opc.h), the hand-written
control operation entries (opcodes/e2k-opc.c) and the field layouts from
include/opcode/e2k.h, and writes a neutral YAML description of every
operation: encoding format, opcode, ISA versions, allowed channels and
operand formats.

Only facts about the instruction set are extracted. No binutils code is
copied into the output.
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

import yaml

ISA_VERSIONS = range(1, 9)  # iset_mask bit (N - 1) selects elbrus-vN

FMT_LETTERS = {"SINGLE": "s", "DOUBLE": "d", "QUAD": "q", "QPACKED": "p"}

# Field layout of each template struct, after the common part
# (name, iset_mask, parse_args) and the ALF part
# (next, alopf, flags, opc, allowed_channels[6]).
TEMPLATE_FIELDS = {
    "alf1": ["arg_fmt"],
    "alf2": ["arg_fmt", "opce"],
    "alf3": ["arg_fmt"],
    "alf7": ["arg_fmt", "cmpopce", "implicit_nops", "int_comparison"],
    "alf8": ["arg_fmt", "cmpopce"],
    "alf9": ["arg_fmt"],
    "alf10": ["arg_fmt"],
    "alopf11": ["arg_fmt", "ales_opce", "ales_opc2", "explicit_ales25_v4"],
    "alopf11_lit8": ["arg_fmt", "ales_opce", "ales_opc2", "explicit_ales25_v4",
                     "max_lit8", "warn"],
    "alopf12": ["arg_fmt", "opce", "ales_opce", "ales_opc2"],
    "alopf13": ["arg_fmt", "ales_opce", "ales_opc2"],
    "alopf15": ["arg_fmt", "opce"],
    "alopf16": ["arg_fmt", "src2"],
    "alopf21": ["arg_fmt", "ales_opc2"],
}

# ALES.opc2 values have symbolic names worth keeping in the output.
OPC2_NAMES = {"EXT", "EXT1", "EXT2", "FLB", "FLH", "FLW", "FLD",
              "ICMB0", "ICMB1", "ICMB2", "ICMB3", "FCMB0", "FCMB1",
              "PFCMB0", "PFCMB1", "LCMBD0", "LCMBD1", "LCMBQ0", "LCMBQ1",
              "QPFCMB0", "QPFCMB1"}


def split_top(s):
    """Split a C initializer body on top-level commas."""
    parts, depth, cur, in_str = [], 0, [], False
    i = 0
    while i < len(s):
        c = s[i]
        if in_str:
            cur.append(c)
            if c == "\\":
                cur.append(s[i + 1])
                i += 1
            elif c == '"':
                in_str = False
        elif c == '"':
            in_str = True
            cur.append(c)
        elif c in "{(":
            depth += 1
            cur.append(c)
        elif c in "})":
            depth -= 1
            cur.append(c)
        elif c == "," and depth == 0:
            parts.append("".join(cur).strip())
            cur = []
        else:
            cur.append(c)
        i += 1
    if "".join(cur).strip():
        parts.append("".join(cur).strip())
    return parts


def strip_braces(s):
    s = s.strip()
    if s.startswith("{") and s.endswith("}"):
        return s[1:-1]
    return s


class Defines:
    """Object-like #defines from include/opcode/e2k.h and e2k-opc.c."""

    def __init__(self, *texts):
        self.map = {}
        for text in texts:
            text = re.sub(r"\\\n", " ", text)
            for m in re.finditer(r"^#define\s+(\w+)\s+(.+?)\s*$", text, re.M):
                name, val = m.group(1), m.group(2)
                if "(" in name:
                    continue
                self.map[name] = re.sub(r"/\*.*?\*/", "", val).strip()

    def expand(self, tok):
        seen = set()
        while tok in self.map and tok not in seen:
            seen.add(tok)
            tok = self.map[tok]
        return tok

    def value(self, tok):
        """Evaluate an integer expression made of defines, '|' and literals."""
        tok = self.expand(tok.strip())
        if "|" in tok:
            v = 0
            for part in tok.split("|"):
                v |= self.value(part)
            return v
        return int(tok, 0)


def iset_list(mask):
    return [v for v in ISA_VERSIONS if mask & (1 << (v - 1))]


def parse_flags(tok):
    names = [p.strip() for p in tok.split("|")]
    return sorted(n for n in names if n != "NO_MAS")


# Length of the arg_fmt array in each template struct. Shorter initializers
# leave the remaining elements zero, i.e. SINGLE.
ARG_FMT_LEN = {"alf1": 3, "alf2": 2, "alf3": 3, "alf7": 2, "alf8": 1,
               "alf9": 1, "alf10": 1, "alopf11": 3, "alopf11_lit8": 3,
               "alopf12": 2, "alopf13": 3, "alopf15": 1, "alopf16": 1,
               "alopf21": 4, "aaopf1": 1}


def parse_arg_fmt(tok, defs, templ):
    tok = defs.expand(tok)
    items = split_top(strip_braces(tok))
    fmt = "".join(FMT_LETTERS[defs.expand(i)] for i in items)
    return fmt.ljust(ARG_FMT_LEN[templ], "s")


def parse_opc2(tok, defs):
    tok = tok.strip()
    if tok in OPC2_NAMES:
        return tok
    return defs.value(tok)


def parse_alu_table(path, defs):
    ops = []
    text = path.read_text()
    for m in re.finditer(r"static e2k_(\w+)_opcode_templ dummy =\s*(\{.*?\});",
                         text, re.S):
        templ, body = m.group(1), m.group(2)
        fields = split_top(strip_braces(body))
        name = fields[0].strip('"')
        iset_mask = defs.value(fields[1])
        assert fields[3] == "NULL", (name, fields)
        op = {
            "name": name,
            "template": templ,
            "alopf": fields[4],
            "isets": iset_list(iset_mask),
            "flags": parse_flags(fields[5]),
            "opc": defs.value(fields[6]),
            "channels": [i for i, c in enumerate(split_top(strip_braces(fields[7])))
                         if int(c) != 0],
        }
        extra = fields[8:]
        layout = TEMPLATE_FIELDS[templ]
        if len(extra) != len(layout):
            sys.exit(f"{name}: {templ} expects {len(layout)} extra fields, "
                     f"got {len(extra)}: {extra}")
        for key, tok in zip(layout, extra):
            if key == "arg_fmt":
                op["args"] = parse_arg_fmt(tok, defs, templ)
            elif key == "ales_opc2":
                if tok.startswith("{"):
                    vals = split_top(strip_braces(tok))
                    op[key] = {i: parse_opc2(v, defs)
                               for i, v in enumerate(vals) if i in op["channels"]}
                else:
                    op[key] = parse_opc2(tok, defs)
            elif key == "warn":
                op[key] = tok.strip('"')
            else:
                op[key] = defs.value(tok)
        ops.append(op)
    return ops


VEC_MACROS = {
    "E2K_OPCODE_VEC_ENTRY": ["name", "iset_mask", "parser"],
    "COPF2_VEC_ENTRY": ["name", "iset_mask", "ctp_opc", "ctprs", "label"],
    "COPF4_VEC_ENTRY": ["name", "iset_mask"],
    "AAOPF1_VEC_ENTRY": ["name", "iset_mask", "opc", "args"],
    "SETCMD_VEC_ENTRY": ["name", "iset_mask", "id"],
}


def parse_manual_entries(path, defs):
    """Hand-written entries added after the generated table."""
    text = path.read_text()
    body = text[text.index('#include "e2k-opc.h"'):]
    ops = []
    for m in re.finditer(r"^\s*(\w+_VEC_ENTRY)\s*\((.*?)\);", body, re.M | re.S):
        macro, args = m.group(1), split_top(m.group(2))
        layout = VEC_MACROS[macro]
        f = dict(zip(layout, args))
        op = {"name": f["name"], "template": macro[:-len("_VEC_ENTRY")].lower(),
              "isets": iset_list(defs.value(f["iset_mask"]))}
        if "parser" in f:
            op["parser"] = f["parser"]
        if "ctp_opc" in f:
            op["ctp_opc"] = defs.value(f["ctp_opc"])
            op["ctprs"] = [i + 1 for i, c in
                           enumerate(split_top(strip_braces(defs.expand(f["ctprs"]))))
                           if int(c)]
            op["label"] = f["label"] == "EXPECT_LABEL"
        if "opc" in f:
            op["opc"] = defs.value(f["opc"])
            op["args"] = parse_arg_fmt(f["args"], defs, op["template"])
        if "id" in f:
            op["id"] = defs.value(f["id"])
        ops.append(op)
    # MMURR/MMURW are written out as full ALF3 structs.
    for m in re.finditer(r"static const e2k_alf3_opcode_templ (\w+) =\s*(\{.*?\});",
                         body, re.S):
        fields = split_top(strip_braces(m.group(2)))
        ops.append({
            "name": fields[0].strip('"'),
            "template": "alf3",
            "alopf": fields[4],
            "isets": iset_list(defs.value(fields[1])),
            "flags": parse_flags(fields[5]),
            "opc": defs.value(fields[6]),
            "channels": [i for i, c in enumerate(split_top(strip_braces(fields[7])))
                         if int(c) != 0],
            "args": parse_arg_fmt(fields[8], defs, "alf3"),
        })
    return ops


def git_describe(tree):
    try:
        rev = subprocess.run(["git", "-C", str(tree), "rev-parse", "HEAD"],
                             capture_output=True, text=True, check=True).stdout.strip()
        subj = subprocess.run(["git", "-C", str(tree), "log", "-1", "--format=%s"],
                              capture_output=True, text=True, check=True).stdout.strip()
        return rev, subj
    except (OSError, subprocess.CalledProcessError):
        return None, None


class FlowList(list):
    pass


class HexInt(int):
    pass


HEX_KEYS = {"opc", "opce", "ales_opce", "ales_opc2", "ctp_opc", "src2"}


def hexify(op):
    for key in HEX_KEYS & op.keys():
        v = op[key]
        if isinstance(v, dict):
            op[key] = {k: HexInt(x) if isinstance(x, int) else x for k, x in v.items()}
        elif isinstance(v, int):
            op[key] = HexInt(v)


def flow_list_representer(dumper, data):
    return dumper.represent_sequence("tag:yaml.org,2002:seq", data, flow_style=True)


def hex_int_representer(dumper, data):
    return dumper.represent_scalar("tag:yaml.org,2002:int", f"{data:#x}")


CANONICAL_RX = re.compile(r"_0x[0-9a-f]{2}(_lt3)?$")
ENCODING_IGNORE = {"name", "warn", "max_lit8"}


def fold_aliases(ops):
    """Merge entries whose encoding is identical into one canonical entry.

    binutils spells e.g. the PLOG truth table as plog_128, plog_0x80 and
    plog_and; all three assemble to the same bits. The _0xNN spelling is
    kept as the canonical name, others go to 'aliases'.
    """
    groups = {}
    for op in ops:
        key = repr(sorted((k, repr(v)) for k, v in op.items()
                          if k not in ENCODING_IGNORE))
        groups.setdefault(key, []).append(op)
    folded = []
    for op in ops:
        key = repr(sorted((k, repr(v)) for k, v in op.items()
                          if k not in ENCODING_IGNORE))
        group = groups.pop(key, None)
        if group is None:
            continue
        canon = next((o for o in group if CANONICAL_RX.search(o["name"])), group[0])
        aliases = [o["name"] for o in group if o is not canon]
        if aliases:
            canon["aliases"] = aliases
        folded.append(canon)
    return folded


def main():
    here = Path(__file__).resolve().parent
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--binutils", type=Path,
                    default=here.parents[1] / "OpenE2K" / "binutils-gdb")
    ap.add_argument("-o", "--output", type=Path,
                    default=here.parents[1] / "spec" / "isa" / "binutils-ops.yaml")
    args = ap.parse_args()

    bu = args.binutils
    defs = Defines((bu / "include/opcode/e2k.h").read_text(),
                   (bu / "opcodes/e2k-opc.c").read_text())
    alu = parse_alu_table(bu / "opcodes/e2k-opc.h", defs)
    n_raw = len(alu)
    alu = fold_aliases(alu)
    manual = parse_manual_entries(bu / "opcodes/e2k-opc.c", defs)

    for op in alu + manual:
        hexify(op)
        for key in ("isets", "flags", "channels", "ctprs", "aliases"):
            if key in op:
                op[key] = FlowList(op[key])

    rev, subj = git_describe(bu)
    doc = {
        "source": {
            "project": "MCST binutils (OpenE2K/binutils-gdb)",
            "commit": rev,
            "release": subj,
            "files": ["opcodes/e2k-opc.h", "opcodes/e2k-opc.c",
                      "include/opcode/e2k.h"],
        },
        "notes": [
            "isets: ISA versions (elbrus-vN) for which this entry is valid; "
            "iset_mask bit N-1. gas accepts -mcpu values 2..7 only; bit 7 (v8) "
            "is set by the generator for future versions.",
            "One mnemonic may have several entries (different ISA versions, "
            "channels or encodings); gas links them by name.",
            "aliases: other mnemonics binutils accepts for the identical "
            "encoding (e.g. plog_128 and plog_and for plog_0x80).",
            "args: operand register formats in table order "
            "(s=32-bit, d=64-bit, q=128-bit, p=packed 128-bit), padded to "
            "the template's array length as the C struct does. Operand roles "
            "per format are described in spec/isa/bundle.md.",
        ],
        "alu_ops": alu,
        "manual_ops": manual,
    }

    yaml.add_representer(FlowList, flow_list_representer)
    yaml.add_representer(HexInt, hex_int_representer)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w") as f:
        f.write("# SPDX-License-Identifier: CC-BY-SA-4.0\n"
                "# Generated by tools/isa-extract; see spec/NOTICE.md.\n")
        yaml.dump(doc, f, sort_keys=False, allow_unicode=True, width=100)
    print(f"{n_raw} ALU table entries, {len(alu)} after alias folding "
          f"({len({o['name'] for o in alu})} mnemonics), "
          f"{len(manual)} manual entries -> {args.output}")


if __name__ == "__main__":
    main()
