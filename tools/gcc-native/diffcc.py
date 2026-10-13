#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile the same sources with two builds of one GCC revision and
compare what they produce: tools/gcc-native's check for a miscompiled
compiler.

Two builds of the same source must turn the same input into the same
assembly, whoever compiled them (GCC's own bootstrap comparison rests on
this). When one build was compiled by lcc and the other by the port
under test, a difference means the port miscompiled GCC somewhere on that
input's path.  remote.sh runs this on the Elbrus machine.

  diffcc.py compare --a CMD --b CMD [--a-cxx CMD --b-cxx CMD] --out DIR
            [--jobs N] [--flags SET]... [--cxx-flags SET]... SOURCE...
      Compile every *.c (and with --a-cxx, *.C/*.cc) under each SOURCE
      (a file or directory) with each flag set, by both compilers, to
      assembly; compare exit status, assembly and diagnostics.
  diffcc.py triage --a CMD --b CMD --a-cxx CMD --b-cxx CMD --out DIR
            --testsuite DIR [--jobs N] SUMFILE...
      For every FAIL, XPASS and UNRESOLVED line of the .sum files,
      compile that test with the options the line names (and its
      dg-options) by both compilers: the same output means the failure
      is not a miscompiled GCC (a back-end bug, or the test's), a
      different output points at one.

CMD is a compiler driver command line, e.g. "/x/inst-N/bin/gcc" or
"/x/build-N/gcc/xgcc -B/x/build-N/gcc/".  Every compile gets
-S -w -frandom-seed=0 so that nothing but the compiler decides the
output.  Results: DIR/summary.txt, DIR/diffs.txt (one line per
difference) and DIR/cases/ (both outputs of the first differences).
"""

import argparse
import concurrent.futures as cf
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

CXX_SUFFIXES = (".C", ".cc", ".cpp", ".cxx")
KEEP_CASES = 200


def sources(paths, cxx):
    out = []
    for p in paths:
        if os.path.isdir(p):
            for root, _, files in os.walk(p):
                for f in sorted(files):
                    if f.endswith(".c") or (cxx and f.endswith(CXX_SUFFIXES)):
                        out.append(os.path.join(root, f))
        elif os.path.isfile(p):
            out.append(p)
    return sorted(out)


def compile_one(cmd, flags, src, timeout):
    """(status, assembly bytes, diagnostics) of one compile."""
    with tempfile.TemporaryDirectory(prefix="diffcc-") as tmp:
        out = os.path.join(tmp, "out.s")
        argv = (["nice", "-n", "19"] + shlex.split(cmd) + shlex.split(flags)
                + ["-S", "-w", "-frandom-seed=0", "-fno-diagnostics-color",
                   src, "-o", out])
        try:
            p = subprocess.run(argv, cwd=tmp, capture_output=True,
                               timeout=timeout)
        except subprocess.TimeoutExpired:
            return "timeout", b"", b""
        asm = open(out, "rb").read() if p.returncode == 0 and os.path.exists(out) else b""
        diag = (p.stdout + p.stderr).replace(tmp.encode(), b"TMP")
        return p.returncode, asm, diag


def compare(job):
    a, b, flags, src, timeout = job
    ra = compile_one(a, flags, src, timeout)
    rb = compile_one(b, flags, src, timeout)
    if ra[0] == "timeout" or rb[0] == "timeout":
        kind = "timeout" if ra[0] == rb[0] else "timeout-one"
    elif ra[0] != rb[0]:
        kind = "status"
    elif ra[0] == 0:
        kind = "same" if ra[1] == rb[1] else "asm"
    else:
        kind = "same-failure" if ra[2] == rb[2] else "diagnostics"
    return kind, flags, src, ra, rb


def run(jobs, njobs, out, label):
    """Run (a, b, flags, src, timeout) jobs; write the results under OUT."""
    os.makedirs(os.path.join(out, "cases"), exist_ok=True)
    counts, kept = {}, 0
    with open(os.path.join(out, "diffs.txt"), "a") as diffs, \
            cf.ThreadPoolExecutor(max_workers=njobs) as pool:
        for kind, flags, src, ra, rb in pool.map(compare, jobs):
            key = (flags, kind)
            counts[key] = counts.get(key, 0) + 1
            if kind in ("same", "same-failure", "timeout"):
                continue
            diffs.write(f"{kind}\t{flags}\t{src}\t{ra[0]}\t{rb[0]}\n")
            diffs.flush()
            if kept < KEEP_CASES:
                kept += 1
                base = os.path.join(out, "cases", f"{kept:03d}")
                for tag, r in (("a", ra), ("b", rb)):
                    with open(f"{base}.{tag}.s", "wb") as f:
                        f.write(r[1])
                    with open(f"{base}.{tag}.diag", "wb") as f:
                        f.write(r[2])
                with open(f"{base}.what", "w") as f:
                    f.write(f"{kind}\t{flags}\t{src}\n")
    lines = [f"== {label}"]
    for flags in sorted({k[0] for k in counts}):
        row = {k[1]: v for k, v in counts.items() if k[0] == flags}
        lines.append(f"{flags or '(no flags)':40} " + ", ".join(
            f"{k} {row[k]}" for k in sorted(row)))
    bad = sum(v for k, v in counts.items()
              if k[1] not in ("same", "same-failure", "timeout"))
    lines.append(f"differences: {bad} (diffs.txt; first {KEEP_CASES} in cases/)")
    text = "\n".join(lines) + "\n"
    with open(os.path.join(out, "summary.txt"), "a") as f:
        f.write(text)
    print(text, end="")
    return bad


# "FAIL: gcc.c-torture/execute/pr1.c   -O2  execution test"
# "FAIL: g++.dg/x.C  -std=gnu++17 (test for excess errors)"
SUM_LINE = re.compile(r"^(FAIL|XPASS|UNRESOLVED): (\S+\.(?:c|C|cc|cpp))(.*)$")
DG_OPTS = re.compile(r'\{\s*dg-(?:additional-)?options\s+"([^"]*)"')


def triage_jobs(args):
    seen, jobs = set(), []
    for sumfile in args.inputs:
        for line in open(sumfile, errors="replace"):
            m = SUM_LINE.match(line.rstrip("\n"))
            if not m:
                continue
            path, rest = m.group(2), m.group(3).split()
            flags = []
            for tok in rest:
                if not tok.startswith("-"):
                    break
                flags.append(tok)
            src = os.path.join(args.testsuite, path)
            if not os.path.isfile(src):
                continue
            try:
                text = open(src, errors="replace").read()
            except OSError:
                continue
            for opts in DG_OPTS.findall(text):
                flags += opts.split()
            cxx = src.endswith(CXX_SUFFIXES)
            key = (src, " ".join(flags))
            if key in seen:
                continue
            seen.add(key)
            a = args.a_cxx if cxx else args.a
            b = args.b_cxx if cxx else args.b
            jobs.append((a, b, key[1], src, args.timeout))
    return jobs


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("mode", choices=["compare", "triage"])
    ap.add_argument("--a", required=True)
    ap.add_argument("--b", required=True)
    ap.add_argument("--a-cxx")
    ap.add_argument("--b-cxx")
    ap.add_argument("--out", required=True)
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--timeout", type=int, default=600)
    ap.add_argument("--flags", action="append")
    ap.add_argument("--cxx-flags", action="append")
    ap.add_argument("--testsuite")
    ap.add_argument("inputs", nargs="+")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    with open(os.path.join(args.out, "summary.txt"), "a") as f:
        f.write(f"a: {args.a}\nb: {args.b}\n"
                f"a-cxx: {args.a_cxx}\nb-cxx: {args.b_cxx}\n")

    if args.mode == "triage":
        if not (args.testsuite and args.a_cxx and args.b_cxx):
            sys.exit("triage needs --testsuite, --a-cxx and --b-cxx")
        jobs = triage_jobs(args)
        bad = run(jobs, args.jobs, args.out, f"triage of {len(jobs)} failing tests")
        print("same output: a back-end or test problem, not a miscompiled GCC;"
              " a difference points at one")
        return 0 if bad == 0 else 1

    bad = 0
    srcs_c = [s for s in sources(args.inputs, False) if s.endswith(".c")]
    for flags in args.flags or ["-O2"]:
        bad += run([(args.a, args.b, flags, s, args.timeout) for s in srcs_c],
                   args.jobs, args.out, f"C, {flags}, {len(srcs_c)} sources")
    if args.a_cxx and args.b_cxx:
        srcs_x = [s for s in sources(args.inputs, True) if s.endswith(CXX_SUFFIXES)]
        for flags in args.cxx_flags or ["-O2"]:
            bad += run([(args.a_cxx, args.b_cxx, flags, s, args.timeout)
                        for s in srcs_x],
                       args.jobs, args.out, f"C++, {flags}, {len(srcs_x)} sources")
    if bad == 0:
        shutil.rmtree(os.path.join(args.out, "cases"), ignore_errors=True)
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
