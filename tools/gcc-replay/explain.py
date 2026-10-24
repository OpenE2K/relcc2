#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""gcc-replay (README.md): after a fix, whether it explains the
differences a replay found.

  explain.py DIR LIBEXEC USER@HOST [--ssh-port PORT] [-r RESULTS]

For every DIFFERENT line of replay.py's RESULTS on HOST (default
results.txt in DIR's machine directory), fetches the native compiler's
X.n.s from HOST, runs the command recorded in X.cc1 here with LIBEXEC's
compiler proper (cc1, cc1plus: the fixed cross's, an install's
libexec/gcc/e2k-linux-gnu/VERSION or a build's gcc directory) and
compares, .ident lines aside.  "explained": the fixed cross gives what
the native compiler gave, so the difference was the cross's own (what
the fix removed), not a miscompile of the native compiler.  Exits 1 if
any is UNEXPLAINED."""
import argparse
import os
import shlex
import subprocess
import sys

SSH = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   '../setup/e2k-ssh')


def strip(b):
    return [l for l in b.split(b'\n') if not l.lstrip().startswith(b'.ident')]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('dir')
    ap.add_argument('libexec')
    ap.add_argument('host')
    ap.add_argument('--ssh-port')
    ap.add_argument('-r', default='results.txt')
    a = ap.parse_args()
    top = os.path.abspath(a.dir)
    mdir = open(os.path.join(top, 'machine-dir')).read().strip()
    ssh = [SSH] + (['-p', a.ssh_port] if a.ssh_port else []) + [a.host]
    fetch = lambda p: subprocess.run(
        ssh + ['cat ' + shlex.quote(os.path.join(mdir, p))],
        capture_output=True, check=True).stdout
    bad = n = 0
    for line in fetch(a.r).decode().splitlines():
        kind, cc1 = line.split()[0], line.split()[-1]
        if kind != 'DIFFERENT':
            continue
        n += 1
        with open(os.path.join(top, cc1)) as f:
            there = f.readline().rstrip('\n')
            argv = shlex.split(f.readline())
        cwd = top + there[len(mdir):]
        argv[0] = os.path.join(a.libexec, os.path.basename(argv[0]))
        i = argv.index('-o')
        nat = fetch(os.path.join(there, argv[i + 1][:-2] + '.n.s'))
        argv[i + 1] = '/dev/stdout'
        mine = subprocess.run(argv, cwd=cwd, capture_output=True,
                              check=True).stdout
        ok = strip(mine) == strip(nat)
        bad += not ok
        print('explained' if ok else 'UNEXPLAINED', cc1, flush=True)
    print('%d DIFFERENT, %d unexplained' % (n, bad))
    return 1 if bad else 0


sys.exit(main())
