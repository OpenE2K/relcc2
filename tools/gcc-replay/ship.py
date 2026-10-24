#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""gcc-replay (README.md): sends an Elbrus machine what replay.py needs.

  ship.py DIR USER@HOST [--ssh-port PORT] [--list]

DIR is record.sh's output.  Sends, to DIR's machine directory
(DIR/machine-dir), every X.cc1 under DIR/build with the preprocessed
input and the cross's assembly its command names (the command's paths
are relative to the recorded directory, which maps onto DIR here), all of
DIR/inst (the native compiler), and replay.py into the directory above.
Goes through tools/setup/e2k-ssh, as tar compressed with zstd; the
machine needs python3, tar and zstd.  --list only prints the file list,
relative to DIR."""
import argparse
import os
import shlex
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SSH = os.path.join(HERE, '../setup/e2k-ssh')


def files(top, mdir):
    out = []
    for r, _, fs in os.walk(os.path.join(top, 'build')):
        for f in fs:
            if not f.endswith('.cc1'):
                continue
            p = os.path.join(r, f)
            with open(p) as h:
                cwd = h.readline().rstrip('\n')
                argv = shlex.split(h.readline())
            if not cwd.startswith(mdir):
                sys.exit('%s: directory %s outside %s' % (p, cwd, mdir))
            here = top + cwd[len(mdir):]
            out.append(os.path.relpath(p, top))
            for q in (argv[argv.index('-fpreprocessed') + 1],
                      argv[argv.index('-o') + 1]):
                full = os.path.normpath(os.path.join(here, q))
                if not os.path.exists(full):
                    sys.exit('%s: %s missing' % (p, full))
                out.append(os.path.relpath(full, top))
    for r, _, fs in os.walk(os.path.join(top, 'inst')):
        out += [os.path.relpath(os.path.join(r, f), top) for f in fs]
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('dir')
    ap.add_argument('host', nargs='?')
    ap.add_argument('--ssh-port')
    ap.add_argument('--list', action='store_true')
    a = ap.parse_args()
    top = os.path.abspath(a.dir)
    mdir = open(os.path.join(top, 'machine-dir')).read().strip()
    if '/relcc2/' not in mdir:
        sys.exit('ship.py: %s is not under ~/relcc2 (AGENTS.md)' % mdir)
    lst = files(top, mdir)
    if a.list or not a.host:
        sys.stdout.write(''.join(x + '\n' for x in lst))
        return 0
    ssh = [SSH] + (['-p', a.ssh_port] if a.ssh_port else []) + [a.host]
    print('%d files to %s:%s' % (len(lst), a.host, mdir), flush=True)
    q = shlex.quote(mdir)
    tar = subprocess.Popen(['tar', '-C', top, '-cf', '-', '-T', '-'],
                           stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    zst = subprocess.Popen(['zstd', '-q', '-3', '-T0'], stdin=tar.stdout,
                           stdout=subprocess.PIPE)
    tar.stdout.close()
    rem = subprocess.Popen(ssh + ['rm -rf %s && mkdir -p %s && cd %s && '
                                  'zstd -q -d | tar -xf - && du -sh .'
                                  % (q, q, q)], stdin=zst.stdout)
    zst.stdout.close()
    tar.stdin.write(''.join(x + '\n' for x in lst).encode())
    tar.stdin.close()
    if tar.wait() or zst.wait() or rem.wait():
        sys.exit('ship.py: the copy failed')
    with open(os.path.join(HERE, 'replay.py'), 'rb') as f:
        subprocess.run(ssh + ['cat > %s/replay.py'
                              % shlex.quote(os.path.dirname(mdir))],
                       stdin=f, check=True)
    return 0


sys.exit(main())
