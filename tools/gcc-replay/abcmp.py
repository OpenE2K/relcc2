#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""gcc-replay (README.md): a compiler change's effect on real code, from
the commands record.sh recorded.

  abcmp.py BEFORE AFTER STRIDE DIR...

BEFORE and AFTER are directories holding cc1 and cc1plus (the compilers
proper to compare, both x86-hosted).  Every STRIDE-th X.cc1 of each
DIR/build is run with both, here; prints how many outputs differ and,
over all, the wide instructions and the loads and stores before and
after, then the eight files that grew most."""
import concurrent.futures
import os
import re
import shlex
import subprocess
import sys
import tempfile

before, after, stride = sys.argv[1], sys.argv[2], int(sys.argv[3])
jobs = []
for top in sys.argv[4:]:
    top = os.path.abspath(top)
    mdir = open(os.path.join(top, 'machine-dir')).read().strip()
    files = sorted(os.path.join(r, f) for r, _, fs in os.walk(os.path.join(top, 'build'))
                   for f in fs if f.endswith('.cc1'))
    jobs += [(top, mdir, f) for f in files[::stride]]
mem = re.compile(rb'^\s*(ld[a-z]*|st[a-z]*),')
tmp = tempfile.mkdtemp(prefix='abcmp-', dir=os.environ.get('TMPDIR'))


def run(job, k):
    top, mdir, f = job
    with open(f) as h:
        cwd = top + h.readline().rstrip('\n')[len(mdir):]
        argv = shlex.split(h.readline())
    out = []
    for n, d in enumerate((before, after)):
        a = list(argv)
        a[0] = os.path.join(d, os.path.basename(a[0]))
        o = os.path.join(tmp, '%d.%d.s' % (k, n))
        a[a.index('-o') + 1] = o
        subprocess.run(a, cwd=cwd, stderr=subprocess.DEVNULL, check=True)
        lines = [l for l in open(o, 'rb') if not l.lstrip().startswith(b'.ident')]
        os.unlink(o)
        out.append((lines, sum(l.strip() == b'{' for l in lines),
                    sum(bool(mem.match(l)) for l in lines)))
    return f, out[0][0] != out[1][0], out[0][1:], out[1][1:]


with concurrent.futures.ThreadPoolExecutor(os.cpu_count()) as ex:
    res = list(ex.map(lambda p: run(*p), [(j, k) for k, j in enumerate(jobs)]))
os.rmdir(tmp)
diff = [r for r in res if r[1]]
tot = lambda i, j: sum(r[i][j] for r in res)
print('%d compiles, %d differ' % (len(res), len(diff)))
print('wide instructions %d -> %d, loads and stores %d -> %d'
      % (tot(2, 0), tot(3, 0), tot(2, 1), tot(3, 1)))
for f, _, b, a in sorted(diff, key=lambda r: r[3][0] - r[2][0])[-8:]:
    print('  %+d wide, %+d ld/st  %s' % (a[0] - b[0], a[1] - b[1], f.split('/build/', 1)[1]))
