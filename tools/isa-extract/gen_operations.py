#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write spec/isa/operations-v{3,5,6,7}.md: every E2K operation, what it
does, and what is known about it, one page per hardware level.

Joins five sources:

  spec/isa/operations-v3.yaml  what each operation computes (hand-written
  spec/isa/operations-v5.yaml  rules): v3 has everything up to v3 (the 4C),
  spec/isa/operations-v6.yaml  v5 what v4 and v5 add (the 8C2), v6 and v7
  spec/isa/operations-v7.yaml  what those add; every operation must match
                               exactly one rule, in the file of the
                               version that introduced it
  spec/isa/binutils-ops.yaml ISA versions, channels, operand formats
  spec/isa/timing-e2s.yaml,  measured scheduled latencies (4C, 8C2,
  spec/isa/timing-e8c2.yaml, 2C3)
  spec/isa/timing-e2c3.yaml
  spec/isa/lcc-usage.yaml    how often lcc's system libraries use it
  tools/**/probes .s files   which hardware probes execute it (the
                             generators in GENERATED write theirs first:
                             those files are too large to keep in git)

  python3 tools/isa-extract/gen_operations.py          # write the pages
  python3 tools/isa-extract/gen_operations.py --check  # only check rules
"""

import argparse
import glob
import re
import subprocess
import sys
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[2]
ISA = ROOT / "spec" / "isa"

# Probe assembly -> (probe, machines it ran on).  "4C" is the Elbrus 4C
# (v3), "8C2" the Elbrus-8C2 (v5), "2C3" the Elbrus-2C3 (v6).
MACHINE_ISET = {"4C": 3, "8C2": 5, "2C3": 6}
PROBES = {
    "semantics.s": ("semantics.c", "4C, 8C2, 2C3"),
    "semp.s": ("semp.c", "4C, 8C2, 2C3"),
    "sem5.s": ("sem5.c", "8C2, 2C3"),
    "semx.s": ("semx.c", "4C, 8C2, 2C3"),
    "semx-v4.s": ("semx.c", "8C2, 2C3"),
    "semfx.s": ("semfx.c", "4C, 8C2, 2C3"),
    "semc.s": ("semc.c", "4C, 8C2, 2C3"),
    "semc-v5.s": ("semc.c", "8C2, 2C3"),
    "fcomb.s": ("fcomb.c", "4C, 8C2, 2C3"),
    "icomb.s": ("icomb.c", "4C, 8C2, 2C3"),
    "sqrt.s": ("sqrt.c", "4C, 8C2, 2C3"),
    "getf.s": ("getf.c", "4C, 8C2, 2C3"),
    "divx.s": ("divx.c", "4C, 8C2, 2C3"),
    "stmqp.s": ("stmqp.c", "8C2, 2C3"),
    "mas_be.s": ("mas_be.c", "4C, 8C2, 2C3"),
    "mas_be-v5.s": ("mas_be.c", "8C2, 2C3"),
    "mas_hint.s": ("mas_hint.c", "4C, 8C2, 2C3"),
    "mas_mod.s": ("mas_mod.c", "4C, 8C2, 2C3"),
    "preds.s": ("preds.c", "4C, 8C2, 2C3"),
    "rndpred.s": ("rndpred.c", "4C, 8C2, 2C3"),
    "rp.s": ("rp.c", "4C, 8C2, 2C3"),
    "native.s": ("native.c", "4C, 8C2, 2C3"),
    "native-rot.s": ("native.c", "4C, 8C2"),
    "native6.s": ("native.c", "2C3"),
    "loops.s": ("loops.c", "4C, 8C2, 2C3"),
    "loops-v5.s": ("loops.c", "8C2, 2C3"),
    "lsrw.s": ("lsrw.c", "4C, 8C2, 2C3"),
    "lsrw-v5.s": ("lsrw.c", "8C2, 2C3"),
    "apb.s": ("apb.c", "4C, 8C2, 2C3"),
    "apb-v5.s": ("apb.c", "8C2, 2C3"),
    "apb-loop.s": ("apb-loop.c", "4C, 8C2, 2C3"),
    "apb-stream.s": ("apb-stream.c", "4C, 8C2, 2C3"),
    "spec.s": ("spec.c", "4C, 8C2, 2C3"),
    "spec-v5.s": ("spec.c", "8C2, 2C3"),
    "spec-v6.s": ("spec.c", "2C3"),
    "samewi.s": ("samewi.c", "4C, 8C2, 2C3"),
    "stld.s": ("stld.c", "4C, 8C2, 2C3"),
    "stldv.s": ("stldv.c", "4C, 8C2, 2C3"),
    "p28.s": ("p28.c", "4C, 8C2, 2C3"),
    "pocchk.s": ("pocchk.c", "4C, 8C2, 2C3"),
    "pocchk2.s": ("pocchk2.c", "4C, 8C2, 2C3"),
    "pocchk3.s": ("pocchk3.c", "4C, 8C2, 2C3"),
    "pocchk4.s": ("pocchk4.c", "4C, 8C2, 2C3"),
    "pocchk5.s": ("pocchk5.c", "4C, 8C2, 2C3"),
    "pocchk6.s": ("pocchk6.c", "4C, 8C2, 2C3"),
    "foldisa.s": ("foldisa.c", "8C2, 2C3"),
    "foldisa-v3.s": ("foldisa.c", "4C"),
    "foldp93.s": ("foldp93.c", "8C2, 2C3"),
    "damwb.s": ("damwb.c", "4C, 8C2, 2C3"),
    "abg.s": ("abg.c", "4C, 8C2"),
    "pmu.s": ("pmu.c", "4C, 8C2, 2C3"),
    "pmu2.s": ("pmu2.c", "4C, 8C2, 2C3"),
    "v2ops.s": ("v2ops.c", "4C, 8C2, 2C3"),
    "ctlops.s": ("ctlops.c", "4C, 8C2, 2C3"),
    "semfx-v4.s": ("semfx.c", "8C2, 2C3"),
    "sem6.s": ("sem6.c", "2C3"),
    "ctl6.s": ("ctl6.c", "2C3"),
    "mas6.s": ("mas6.c", "2C3"),
    "memx.s": ("memx.c", "4C, 8C2, 2C3"),
    "memx-v5.s": ("memx.c", "8C2, 2C3"),
    "fpenv.s": ("fpenv.c", "4C, 8C2, 2C3"),
    "fpenv-v6.s": ("fpenv.c", "2C3"),
    "fpcost.s": ("fpcost.c", "4C, 8C2, 2C3"),
    "pf.s": ("pf.c", "4C, 8C2, 2C3"),
    "aaurwct.s": ("aaurwct.c", "4C, 8C2"),
    "sregs.s": ("sregs.c", "4C, 8C2, 2C3"),
    "tagx.s": ("tagx.c", "4C, 8C2, 2C3"),
    "tagx-v5.s": ("tagx.c", "8C2, 2C3"),
    "memleft.s": ("memleft.c", "4C, 8C2, 2C3"),
    "memleft-v5.s": ("memleft.c", "8C2, 2C3"),
    "memleft-v6.s": ("memleft.c", "2C3"),
    "ctlleft.s": ("ctlleft.c", "4C, 8C2, 2C3"),
    "ctlleft6.s": ("ctlleft.c", "2C3"),
    "isaleft.s": ("isaleft.c", "4C, 8C2, 2C3"),
    "isaleft-v5.s": ("isaleft.c", "8C2, 2C3"),
    "apbleft.s": ("apbleft.c", "4C, 8C2, 2C3"),
    "apbleft-v5.s": ("apbleft.c", "8C2, 2C3"),
    "apbst.s": ("apbst.c", "4C, 8C2, 2C3"),
    "apbst-v5.s": ("apbst.c", "8C2, 2C3"),
    "stbase.s": ("stbase.c", "4C, 8C2, 2C3"),
    "timeleft.s": ("timeleft.c", "4C, 8C2, 2C3"),
    "timeleft-v6.s": ("timeleft.c", "2C3"),
    "timeleftv.s": ("timeleftv.c", "4C, 8C2, 2C3"),
    "damlist.s": ("damlist.c", "4C, 8C2, 2C3"),
    "memorycxx_dam.s": ("memorycxx_dam.c", "4C, 8C2, 2C3"),
    "memorycxx_stld.s": ("memorycxx_stld.c", "4C, 2C3"),
    "memorycxx_stld-v5.s": ("memorycxx_stld.c", "2C3"),
    "pocabi2.s": ("abi/pocabi2.s", "4C, 8C2, 2C3"),
    "kernleft.s": ("abi/kernleft.s", "4C, 8C2, 2C3"),
    "pocabi1_mp.s": ("abi/pocabi1_mp.s", "4C, 8C2, 2C3"),
    "pocabi1_tear.s": ("abi/pocabi1_tear.s", "8C2, 2C3"),
    "lat32.s": ("abi/mode32/lat32.c", "4C, 8C2, 2C3"),
    "lat32-v5.s": ("abi/mode32/lat32.c", "8C2, 2C3"),
    "pmmin.s": ("abi/protected/pmmin.s", "4C, 8C2, 2C3"),
    "callsstack_pcall.s": ("callsstack_pcall.c", "4C, 8C2, 2C3"),
    "callsstack_tail.s": ("abi/callsstack_tail.s", "4C, 8C2, 2C3"),
    "callsstack_clash.s": ("abi/callsstack_clash.s", "4C, 8C2, 2C3"),
    "debuginfo.s": ("abi/debuginfo.s", "4C, 8C2, 2C3"),
    "glibcbuild_entry.s": ("abi/glibcbuild_entry.s", "4C, 8C2, 2C3"),
    "foldi56_undef.s": ("abi/foldi56_undef.c", "4C, 8C2"),
}
ABI_PROBES = "tools/abi/probes"


def expand(spec):
    """'add{s,d} sub{s,d}' -> ['adds', 'addd', 'subs', 'subd']."""
    out = []
    for word in spec.split():
        parts = [""]
        for m in re.finditer(r"\{([^}]*)\}|([^{]+)", word):
            alts = m.group(1).split(",") if m.group(1) is not None \
                else [m.group(2)]
            parts = [p + a for p in parts for a in alts]
        out += parts
    return out


def load_ops():
    data = yaml.safe_load((ISA / "binutils-ops.yaml").read_text())
    ops = {}
    for e in data["alu_ops"]:
        o = ops.setdefault(e["name"], {"isets": set(), "channels": set(),
                                       "args": set(), "unit": "alu"})
        o["isets"] |= set(e.get("isets") or [])
        o["channels"] |= set(e.get("channels") or [])
        if e.get("args"):
            o["args"].add(e["args"])
    for e in data["manual_ops"]:
        o = ops.setdefault(e["name"], {"isets": set(), "channels": set(),
                                       "args": set(), "unit": "control"})
        o["isets"] |= set(e.get("isets") or [])
    # v1 (Elbrus-3M) is gone: no working machine exists, so operations
    # only it had are left out.
    v1 = {n for n, o in ops.items() if o["isets"] and max(o["isets"]) == 1}
    return {n: o for n, o in ops.items() if n not in v1}, v1


# Rule files and the first ISA versions their operations may have.
LEVELS = (("v3", 1, 3), ("v5", 4, 5), ("v6", 6, 6), ("v7", 7, 8))


def assign(docs, ops, v1=()):
    """Map every operation to its rule; report misses, doubles and
    operations in the wrong level's file."""
    owner, errors = {}, []
    for level, lo, hi in LEVELS:
        if level in docs:
            _assign(docs[level], ops, v1, owner, errors, level, lo, hi)
    missing = sorted(n for n in ops if n not in owner)
    return owner, errors, missing


def _assign(rules_doc, ops, v1, owner, errors, level, lo, hi):
    for g in rules_doc["groups"]:
        for r in g["rules"]:
            names = []
            if "ops" in r:
                names = expand(r["ops"])
                for n in names:
                    if n not in ops and n not in v1:
                        errors.append(f"{g['title']}: unknown operation {n}")
            if "match" in r:
                rx = re.compile(r["match"])
                names += [n for n in ops if rx.fullmatch(n) and n not in names]
            r["_names"] = [n for n in names if n in ops]
            for n in r["_names"]:
                if n in owner:
                    errors.append(f"{n}: matched by '{owner[n]['does'][:40]}'"
                                  f" and '{r['does'][:40]}'")
                first = min(ops[n]["isets"])
                if not lo <= first <= hi:
                    errors.append(f"{n} (from v{first}) is in "
                                  f"operations-{level}.yaml")
                owner.setdefault(n, r)


# Generators of probe assembly that is not kept in the repository
# (.gitignore: several MB each); probe_runs() runs them before the scan.
GENERATED = ["gen_timeleft.py", "gen_timeleftv.py", "gen_memorycxx_stld.py"]


def probe_runs(ops):
    runs = {}
    for gen in GENERATED:
        subprocess.run([sys.executable,
                        str(ROOT / "tools" / "isa-probes" / gen)],
                       check=True, stdout=subprocess.DEVNULL)
    files = glob.glob(str(ROOT / "tools" / "isa-probes" / "*.s"))
    files += glob.glob(str(ROOT / ABI_PROBES / "*.s"))
    files += glob.glob(str(ROOT / ABI_PROBES / "*" / "*.s"))
    for f in files:
        name = Path(f).name
        tag = PROBES.get(name)
        if tag is None:
            if ABI_PROBES not in f:
                sys.exit(f"{f}: add it to PROBES")
            tag = (f"abi/{Path(f).relative_to(ROOT / ABI_PROBES)}", "4C")
        for line in Path(f).read_text().splitlines():
            m = re.match(r"\s+([a-z][a-z0-9_]*)(,|\s|$)", line)
            if m and m.group(1) in ops:
                runs.setdefault(m.group(1), set()).add(tag)
    bench = (ROOT / "tools" / "isa-timing" / "bench.py").read_text()
    timed = {n for n in ops if re.search(r'["(\s]' + n + r",\d", bench)}
    return runs, timed


def latencies():
    """Operation -> latency cells: the first machine's value, and the
    8C2's or 2C3's where v5 or v6 differs from those before it."""
    out = {}
    for path, tag in ((ISA / "timing-e2s.yaml", ""),
                      (ISA / "timing-e8c2.yaml", " (v5)"),
                      (ISA / "timing-e2c3.yaml", " (v6)")):
        data = yaml.safe_load(path.read_text())
        for k, v in (data.get("scheduled_latency") or {}).items():
            name = re.sub(r"^v[356]op_", "", k)
            if v.get("latency") is None:
                continue
            if name not in out:
                out[name] = [f"{v['latency']}{tag}"]
            elif tag and not any(
                    c.split()[0] == str(v["latency"]) for c in out[name]):
                out[name].append(f"{v['latency']}{tag}")
    return out


def isets_text(s):
    s = sorted(s)
    if not s:
        return ""
    runs, start = [], s[0]
    for a, b in zip(s, s[1:] + [None]):
        if b != a + 1:
            runs.append(f"{start}" if start == a else f"{start}–{a}")
            if b is not None:
                start = b
    return ", ".join(runs)


def chans_text(s):
    if not s:
        return ""
    s = sorted(s)
    if s == list(range(6)):
        return "0–5"
    return ",".join(map(str, s))


def kfmt(n):
    if n >= 10000:
        return f"{n // 1000}k"
    if n >= 1000:
        return f"{n / 1000:.1f}k"
    return str(n)


def cell(text):
    return text.replace("|", "\\|")


def render(rules_doc, ops, owner):
    lat = latencies()
    usage = yaml.safe_load((ISA / "lcc-usage.yaml").read_text())["counts"]
    runs, timed = probe_runs(ops)
    timed |= {n for n in ops if n in lat}       # bench.py's generated chains
    out = [l.rstrip() for l in rules_doc["header"].splitlines()]
    out.append("")
    for g in rules_doc["groups"]:
        out += [f"## {g['title']}", ""]
        if g.get("intro"):
            out += [g["intro"].strip(), ""]
        out += ["| Operations | ISA | ALC | Does | Lat | lcc v3 / v5 | Run on hardware |",
                "|---|---|---|---|---|---|---|"]
        for r in g["rules"]:
            names = r["_names"]
            if not names:
                continue
            isets = set().union(*(ops[n]["isets"] for n in names))
            chans = set().union(*(ops[n]["channels"] for n in names))
            if r.get("compact") or len(names) > 12:
                shown = ", ".join(f"`{n}`" for n in names[:6])
                opcell = f"{shown}, … ({len(names)} operations)"
            else:
                opcell = ", ".join(f"`{n}`" for n in names)
            does = r["does"].strip().replace("\n", " ")
            if r.get("use"):
                does += f" **Use:** {r['use'].strip()}"
            if r.get("see"):
                does += f" ({r['see'].strip()})"
            lats = sorted({c for n in names for c in lat.get(n, [])},
                          key=lambda x: (len(x), x))
            latcell = ", ".join(lats[:3]) + ("…" if len(lats) > 3 else "")
            if r.get("lat"):
                latcell = str(r["lat"]).strip()
            u3 = sum(usage.get(n, [0, 0])[0] for n in names)
            u5 = sum(usage.get(n, [0, 0])[1] for n in names)
            ucell = f"{kfmt(u3)} / {kfmt(u5)}" if u3 or u5 else ""
            tags = set().union(*(runs.get(n, set()) for n in names))
            probes = sorted({t[0] for t in tags})
            # An operation runs only on machines of its version or later.
            machines = sorted({m for n in names for t in runs.get(n, ())
                               for m in t[1].split(", ")
                               if MACHINE_ISET[m] >= min(ops[n]["isets"])},
                              key=MACHINE_ISET.get)
            ran = sum(1 for n in names if n in runs)
            if r.get("status"):
                chk = r["status"].strip()
            elif probes:
                chk = ", ".join(p.replace(".c", "") for p in probes[:4])
                chk += "…" if len(probes) > 4 else ""
                chk += f" ({', '.join(machines)})"
                if ran < len(names):
                    chk += f"; {ran} of {len(names)}"
            elif any(n in timed for n in names):
                chk = "timed only (bench.py)"
            elif min(isets or {0}) >= 7:
                chk = "v7: no hardware"
            else:
                chk = "—"
            out.append(f"| {opcell} | {isets_text(isets)} | {chans_text(chans)}"
                       f" | {cell(does)} | {latcell} | {ucell} | {cell(chk)} |")
        out.append("")
    return "\n".join(out) + "\n"


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    ops, v1 = load_ops()
    docs = {level: yaml.safe_load((ISA / f"operations-{level}.yaml")
                                  .read_text())
            for level, _, _ in LEVELS}
    owner, errors, missing = assign(docs, ops, v1)
    for e in errors:
        print("error:", e)
    if missing:
        print(f"{len(missing)} operations without a rule:", " ".join(missing))
    if errors or missing:
        sys.exit(1)
    nrules = sum(len(g["rules"]) for d in docs.values() for g in d["groups"])
    print(f"{len(ops)} operations, all covered by {nrules} rules")
    if not args.check:
        for level, doc in docs.items():
            out = ISA / f"operations-{level}.md"
            out.write_text(render(doc, ops, owner))
            print("written", out)


if __name__ == "__main__":
    main()
