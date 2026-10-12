#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check a compiler's lcc-compatible builtins against lcc's: build ops.c
(gen.py) and models.c with each compiler, run them on an Elbrus machine
(or under RUNNER), and compare.

Usage:
  run.py [-o OUTDIR] [--target T] [--cc NAME=COMMAND]...
         [--runner COMMAND | --remote [USER@]HOST [--ssh-key FILE]
          [--ssh-port N] [--remote-dir DIR]]

The first compiler is the reference (by default lcc 1.31.05 for TARGET,
from /opt/mcst or toolchains/lcc-1.31.05-e2k-TARGET as fetch-lcc.sh
installs it). Every compiler's models program must report no failures
(models.c holds reference models of the builtins that are not one
operation), and its ops program must print the reference's hash for
every builtin (one line per operation builtin). COMMAND is a shell-style
command that accepts SRC -o OUT, e.g.
  --cc gcc='e2k-linux-gnu-gcc -O2 -march=elbrus-v3'
TARGET (default v3) is the lcc directory suffix: v3, v5.8c2, v6.2c3, ...
With --remote the programs go to REMOTE_DIR (default relcc2/builtins) in
the remote home; remove it when done. The exit status is 1 on any
difference.
"""

import argparse
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
VERSION = "1.31.05"
sys.path.insert(0, str(ROOT / "tools" / "setup"))
import sshkey  # noqa: E402  (never connect without relcc2's key)


def default_lcc(target):
    opt = Path(f"/opt/mcst/lcc-{VERSION}.e2k-{target}.linux-6.1/bin/lcc")
    if opt.exists():
        return str(opt)
    local = ROOT / "toolchains" / f"lcc-{VERSION}-e2k-{target}" / "lcc"
    if local.exists():
        return str(local)
    sys.exit(f"no lcc {VERSION} for {target}: tools/setup/fetch-lcc.sh "
             f"{VERSION} e2k-{target}")


def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("-o", type=Path, default=ROOT / "toolchains" / "builtins")
    ap.add_argument("--target", default="v3")
    ap.add_argument("--cc", action="append", default=[])
    ap.add_argument("--runner")
    ap.add_argument("--remote")
    ap.add_argument("--ssh-key", type=Path,
                    help="default: $RELCC2_SSH_KEY or secrets/id_ed25519")
    ap.add_argument("--ssh-port", type=int)
    ap.add_argument("--remote-dir", default="relcc2/builtins")
    a = ap.parse_args()
    a.o.mkdir(parents=True, exist_ok=True)
    ccs = [("lcc", default_lcc(a.target) + " -O2")]
    for c in a.cc:
        name, _, cmd = c.partition("=")
        ccs = [x for x in ccs if x[0] != name] + [(name, cmd)]
    p = sh([sys.executable, str(HERE / "gen.py"), "-o", str(a.o)])
    if p.returncode:
        sys.exit(p.stderr)
    progs = []
    for name, cmd in ccs:
        for prog, src in (("ops", a.o / "ops.c"), ("models", HERE / "models.c")):
            out = a.o / f"{prog}-{name}"
            p = sh(shlex.split(cmd) + [str(src), "-o", str(out)])
            if p.returncode:
                sys.exit(f"{name}: {src.name} does not build:\n{p.stdout}{p.stderr}")
            progs.append(out)
    if a.remote:
        opts = sshkey.options(sshkey.find_key(a.ssh_key), a.ssh_port)
        ssh = ["ssh", *opts]
        scp = ["scp", "-q", *opts]
        sh(ssh + [a.remote, f"mkdir -p {a.remote_dir}"])
        p = sh(scp + [str(x) for x in progs] + [f"{a.remote}:{a.remote_dir}/"])
        if p.returncode:
            sys.exit(p.stderr)

    def run(prog):
        if a.remote:
            p = sh(ssh + [a.remote, f"cd {a.remote_dir} && ./{prog.name}"],
                   timeout=600)
        else:
            runner = shlex.split(a.runner) if a.runner else []
            p = sh(runner + [str(prog)], timeout=600)
        return p.returncode, p.stdout
    bad = 0
    ref = None
    for name, _ in ccs:
        rc, out = run(a.o / f"models-{name}")
        print(f"{name} models: {out.strip().splitlines()[-1] if out.strip() else 'no output'}")
        for line in out.splitlines():
            if line.startswith(("FAIL", "info")):
                print("  " + line)
        bad += rc != 0
        rc, out = run(a.o / f"ops-{name}")
        (a.o / f"ops-{name}.txt").write_text(out)
        res = dict(line.split() for line in out.splitlines() if " " in line)
        if rc:
            print(f"{name} ops: exit status {rc}")
            bad += 1
        if ref is None:
            ref = res
            print(f"{name} ops: {len(res)} builtins (reference)")
            continue
        diff = sorted(k for k in ref if res.get(k) != ref[k])
        print(f"{name} ops: {len(ref) - len(diff)} of {len(ref)} builtins agree")
        for k in diff:
            print(f"  differs: {k}")
        bad += bool(diff)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
