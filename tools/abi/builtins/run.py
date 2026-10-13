#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check a compiler's lcc-compatible builtins against lcc's: build ops.c
(gen.py) and models.c with each compiler, run them on an Elbrus machine
(or under RUNNER), and compare.

Usage:
  run.py [-o OUTDIR] [--target T] [--cc NAME=COMMAND]...
         [--runner COMMAND | --remote [USER@]HOST [--ssh-key FILE]
          [--ssh-port N] [--remote-dir DIR] [--keep-remote]]

The first compiler is the reference (by default lcc 1.31.05 for TARGET,
from /opt/mcst or toolchains/lcc-1.31.05-e2k-TARGET as fetch-lcc.sh
installs it). Every compiler's models program must report no failures
(models.c holds reference models of the builtins that are not one
operation), and its ops program must print the reference's hash for
every builtin (one line per operation builtin). COMMAND is a shell-style
command that accepts SRC -o OUT, e.g.
  --cc gcc='e2k-linux-gnu-gcc -O2 -march=elbrus-v3'
TARGET (default v3) is the lcc directory suffix: v3, v5.8c2, v6.2c3, ...

A compiler that has only some of the builtins (or no <e2kintrin.h>)
still builds both programs: a builtin it lacks (neither __has_builtin
nor a macro of its name) is reported as missing, and so is a models
check whose builtins it lacks. A builtin that the compiler's header
defines as an inline function counts as missing, since the preprocessor
can't see it. A compiler whose programs don't build is reported and
the others still run.

With --remote the programs go to REMOTE_DIR (default relcc2/builtins) in
the remote home and are removed afterwards, with REMOTE_DIR if that
leaves it empty (--keep-remote keeps them). The exit status is 1 on any
difference, missing builtin or build failure.
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
    ap.add_argument("--keep-remote", action="store_true",
                    help="leave the programs in REMOTE_DIR on the remote")
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
    bad = 0
    for name, cmd in ccs:
        for prog, src in (("ops", a.o / "ops.c"), ("models", HERE / "models.c")):
            out = a.o / f"{prog}-{name}"
            out.unlink(missing_ok=True)
            p = sh(shlex.split(cmd) + [str(src), "-o", str(out)])
            if p.returncode:
                log = (p.stdout + p.stderr).splitlines()
                msg = f"{name}: {src.name} does not build:\n" + "\n".join(log[-20:])
                if name == ccs[0][0]:
                    sys.exit(msg)  # the reference
                print(msg)
                bad += 1
                continue
            progs.append(out)
    if a.remote:
        opts = sshkey.options(sshkey.find_key(a.ssh_key), a.ssh_port)
        ssh = ["ssh", *opts]
        scp = ["scp", "-q", *opts]
        sh(ssh + [a.remote, f"mkdir -p {a.remote_dir}"])
        p = sh(scp + [str(x) for x in progs] + [f"{a.remote}:{a.remote_dir}/"])
        if p.returncode:
            sys.exit(p.stderr)

    def cleanup():
        if a.remote and not a.keep_remote:
            d = shlex.quote(a.remote_dir)
            names = " ".join(shlex.quote(x.name) for x in progs)
            sh(ssh + [a.remote, f"cd {d} && rm -f {names} && cd && "
                      f"{{ rmdir {d} 2>/dev/null; true; }}"])

    def run(prog):
        if a.remote:
            p = sh(ssh + [a.remote, f"cd {a.remote_dir} && ./{prog.name}"],
                   timeout=600)
        else:
            runner = shlex.split(a.runner) if a.runner else []
            p = sh(runner + [str(prog)], timeout=600)
        return p.returncode, p.stdout
    ref = None
    try:
        for name, _ in ccs:
            if a.o / f"models-{name}" in progs:
                rc, out = run(a.o / f"models-{name}")
                last = out.strip().splitlines()[-1] if out.strip() else "no output"
                print(f"{name} models: {last}")
                for line in out.splitlines():
                    if line.startswith(("FAIL", "info", "missing")):
                        print("  " + line)
                bad += rc != 0
            if a.o / f"ops-{name}" not in progs:
                continue
            rc, out = run(a.o / f"ops-{name}")
            (a.o / f"ops-{name}.txt").write_text(out)
            res = dict(line.split() for line in out.splitlines() if " " in line)
            if rc:
                print(f"{name} ops: exit status {rc}")
                bad += 1
            if ref is None:
                ref = {k: v for k, v in res.items() if v != "missing"}
                print(f"{name} ops: {len(ref)} builtins (reference)")
                if len(ref) < len(res):
                    print(f"  the reference lacks {len(res) - len(ref)}: "
                          + " ".join(k for k in res if k not in ref))
                    bad += 1
                continue
            missing = sorted(k for k in ref if res.get(k) == "missing")
            noline = sorted(k for k in ref if k not in res)
            diff = sorted(k for k in ref if k in res and res[k] not in
                          (ref[k], "missing"))
            print(f"{name} ops: {len(ref) - len(diff) - len(missing) - len(noline)}"
                  f" of {len(ref)} builtins agree"
                  + (f", {len(missing)} missing (ops-{name}.txt)"
                     if missing else ""))
            for k in diff:
                print(f"  differs: {k}")
            for k in noline:
                print(f"  no result: {k}")
            bad += bool(diff or missing or noline)
    finally:
        cleanup()
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
