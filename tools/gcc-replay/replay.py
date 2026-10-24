#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""gcc-replay, the machine's half (README.md).

  replay.py DIR [--libexec D] [-j N] [--shard I/N] [--reverse]
            [--timeout S] [-o RESULTS]
  replay.py DIR --command X.cc1

record.sh built the native compiler N with the cross C, through
wrap/driver, which left, beside every object X.o C compiled, the
preprocessed source, C's assembly X.s and X.cc1: the directory and the
compiler proper's command line.  This runs each command again on the
machine with N's compiler proper (cc1, cc1plus in D, by default the one
under DIR/inst) writing X.n.s, and compares X.n.s with X.s, .ident
lines aside.  N is C's source compiled by C, so a difference is our
compiler miscompiling GCC, or GCC's output depending on its host.

Writes one line per command to RESULTS (default DIR/results.txt): same,
DIFFERENT, rc=N with the last line of its messages, or timeout, then the
X.cc1 path relative to DIR.  X.n.s is kept only when it differs.
--shard I/N takes every N-th command from the I-th (0-based), to split
the work between machines; --reverse takes them from the end, for a
faster machine to help a slower one with its shard (stop both when
their results meet).  --command prints the shell command that runs X.cc1
with N, for gdb."""
import argparse
import concurrent.futures
import glob
import os
import shlex
import subprocess
import sys


def ident_free(path):
    with open(path, 'rb') as f:
        return [l for l in f if not l.lstrip().startswith(b'.ident')]


def command(libexec, cc1file):
    with open(cc1file) as f:
        cwd = f.readline().rstrip('\n')
        argv = shlex.split(f.readline())
    argv[0] = os.path.join(libexec, os.path.basename(argv[0]))
    i = argv.index('-o')
    ref = os.path.join(cwd, argv[i + 1])
    argv[i + 1] = ref[:-2] + '.n.s'
    return cwd, argv, ref, argv[i + 1]


def replay(libexec, cc1file, timeout):
    cwd, argv, ref, new = command(libexec, cc1file)
    try:
        p = subprocess.run(argv, cwd=cwd, stdout=subprocess.DEVNULL,
                           stderr=subprocess.PIPE, timeout=timeout)
    except subprocess.TimeoutExpired:
        return 'timeout'
    if p.returncode != 0:
        err = p.stderr.decode(errors='replace').strip().splitlines()
        return 'rc=%d %s' % (p.returncode, err[-1] if err else '')
    if ident_free(ref) == ident_free(new):
        os.unlink(new)
        return 'same'
    return 'DIFFERENT'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('dir')
    ap.add_argument('--libexec')
    ap.add_argument('-j', type=int, default=os.cpu_count())
    ap.add_argument('--shard', default='0/1')
    ap.add_argument('--timeout', type=int, default=3600)
    ap.add_argument('-o')
    ap.add_argument('--reverse', action='store_true')
    ap.add_argument('--command')
    a = ap.parse_args()
    top = os.path.abspath(a.dir)
    libexec = a.libexec
    if not libexec:
        found = glob.glob(os.path.join(top, 'inst/**/libexec/gcc/*/*/cc1plus'),
                          recursive=True)
        if len(found) != 1:
            sys.exit('replay.py: %d cc1plus under %s/inst; give --libexec'
                     % (len(found), top))
        libexec = os.path.dirname(found[0])
    if a.command:
        cwd, argv, _, _ = command(libexec, a.command)
        print('cd %s && %s' % (shlex.quote(cwd), shlex.join(argv)))
        return 0
    files = sorted(os.path.join(r, f)
                   for r, _, fs in os.walk(os.path.join(top, 'build'))
                   for f in fs if f.endswith('.cc1'))
    k, n = map(int, a.shard.split('/'))
    files = files[k::n]
    if a.reverse:
        files.reverse()
    print('%d commands, -j %d, %s' % (len(files), a.j, libexec), flush=True)
    counts = {}
    with open(a.o or os.path.join(top, 'results.txt'), 'w') as out, \
            concurrent.futures.ThreadPoolExecutor(a.j) as ex:
        jobs = {ex.submit(replay, libexec, f, a.timeout): f for f in files}
        for done, fut in enumerate(concurrent.futures.as_completed(jobs), 1):
            res = fut.result()
            out.write('%s %s\n' % (res, os.path.relpath(jobs[fut], top)))
            out.flush()
            key = res.split()[0]
            counts[key] = counts.get(key, 0) + 1
            if done % 100 == 0:
                print(done, counts, flush=True)
    print(counts)
    return 0 if set(counts) <= {'same'} else 1


sys.exit(main())
