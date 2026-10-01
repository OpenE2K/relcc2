#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Cross-check spec/isa/binutils-ops.yaml against the e2k_opcodes_new sheet.

The sheet was generated from an older binutils that still shipped the
opcode table generator. Fetch it with:

  curl -L -o e2k_opcodes_new.csv 'https://docs.google.com/spreadsheets/d/\
1p8K5DWBsXTpCDk3TdFwMaTFIadAFuxaH4zfnJXUxK94/gviz/tq?tqx=out:csv&sheet=e2k_opcodes_new'

For every mnemonic and ISA version present in both, compares the opcode
and the set of allowed channels, and prints every disagreement.
"""

import argparse
import collections
import csv
from pathlib import Path

import yaml

MAX_ISET = 7  # the sheet's "N.." ranges end at the newest released version


def load_ours(path):
    """name -> iset -> opc -> set(channels), aliases included."""
    data = yaml.safe_load(path.read_text())
    out = collections.defaultdict(lambda: collections.defaultdict(
        lambda: collections.defaultdict(set)))
    for op in data["alu_ops"]:
        for name in [op["name"], *op.get("aliases", [])]:
            for v in op["isets"]:
                if v <= MAX_ISET:
                    out[name][v][op["opc"]] |= set(op["channels"])
    return out


def load_sheet(path):
    out = collections.defaultdict(lambda: collections.defaultdict(
        lambda: collections.defaultdict(set)))
    for row in csv.DictReader(path.open()):
        if not row["opc1"]:
            continue  # plog_0xXX/qplog_0xXX templates, expanded in binutils
        lo, hi = row["ver"].split("..")
        channels = {int(c) for c in row["channels"] if c != "_"}
        for v in range(int(lo), int(hi or MAX_ISET) + 1):
            out[row["name"]][v][int(row["opc1"], 16)] |= channels
    return out


def fmt(by_opc):
    return ", ".join(f"{opc:#x}:{''.join(map(str, sorted(ch)))}"
                     for opc, ch in sorted(by_opc.items())) or "-"


def main():
    root = Path(__file__).resolve().parents[2]
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("sheet", type=Path, help="CSV export of e2k_opcodes_new")
    ap.add_argument("--ops", type=Path,
                    default=root / "spec" / "isa" / "binutils-ops.yaml")
    args = ap.parse_args()

    ours, sheet = load_ours(args.ops), load_sheet(args.sheet)
    common = sorted(set(ours) & set(sheet))
    print(f"mnemonics: binutils {len(ours)}, sheet {len(sheet)}, common {len(common)}")
    print(f"only in sheet: {sorted(set(sheet) - set(ours)) or 'none'}")

    new_by_iset = collections.Counter(
        min(ours[n]) for n in set(ours) - set(sheet) if ours[n])
    print("only in binutils, by first ISA version: "
          + ", ".join(f"v{v}: {c}" for v, c in sorted(new_by_iset.items())))

    print("\ndisagreements (iset: binutils | sheet), as opc:channels")
    count = 0
    for name in common:
        for v in sorted(set(ours[name]) | set(sheet[name])):
            a, b = ours[name].get(v, {}), sheet[name].get(v, {})
            if a != b:
                print(f"  {name:12} v{v}: {fmt(a):24} | {fmt(b)}")
                count += 1
    print(f"{count} disagreements")


if __name__ == "__main__":
    main()
