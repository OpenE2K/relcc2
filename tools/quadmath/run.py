#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""tools/quadmath (README.md): libquadmath built by the PoC against x86's
build of the same source and against lcc's, call by call.

  run.py x86 OUT [--src GCC]
      builds GCC/libquadmath (default toolchains/gcc-claude) for x86 with
      the host's GCC, and check.c against it; runs every test into OUT/x86.

  run.py e2k OUT CROSS USER@HOST [--lib DIR] [--src GCC] [--ssh-port PORT]
             [--remote-dir DIR] [-j N]
      builds check.c with CROSS (the PoC's cross toolchain), against
      CROSS's libquadmath or, with --lib, a build directory's (its .libs);
      ships the program and that libquadmath.so.0 to HOST's DIR (default
      relcc2/quadmath/NAME, NAME being OUT's last component) and runs every
      test twice there, with that library (OUT/HOST-ours) and with the
      system's, which lcc built (OUT/HOST-system).  Then compares both
      with OUT/x86 and with each other.

  run.py compare A B
      compares two output directories test by test.

A line differs in its value, only in a NaN's sign or payload, or only in
the flags raised.  Exit status 1 if any value differs."""
import argparse
import os
import shlex
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SSH = os.path.join(ROOT, 'tools/setup/e2k-ssh')
SCP = os.path.join(ROOT, 'tools/setup/e2k-scp')


def sh(cmd, **kw):
    print('+', cmd if isinstance(cmd, str) else shlex.join(cmd), flush=True)
    return subprocess.run(cmd, check=True, **kw)


def tests(prog, wrap=()):
    out = subprocess.run(list(wrap) + [prog, 'list'], check=True,
                         capture_output=True, text=True).stdout
    return out.split()


def x86(a):
    src = os.path.abspath(a.src)
    out = os.path.abspath(a.out)
    lib = os.path.join(out, 'x86-lib')
    os.makedirs(lib, exist_ok=True)
    if not os.path.exists(os.path.join(lib, '.libs/libquadmath.so.0')):
        sh([os.path.join(src, 'libquadmath/configure'), '--disable-multilib'],
           cwd=lib, stdout=subprocess.DEVNULL)
        sh(['make', '-j%d' % os.cpu_count()], cwd=lib, stdout=subprocess.DEVNULL)
    prog = os.path.join(out, 'check-x86')
    sh(['gcc', '-O2', '-I', os.path.join(src, 'libquadmath'),
        os.path.join(HERE, 'check.c'), '-o', prog,
        '-L', os.path.join(lib, '.libs'), '-lquadmath', '-lm',
        '-Wl,-rpath,' + os.path.join(lib, '.libs')])
    res = os.path.join(out, 'x86')
    os.makedirs(res, exist_ok=True)
    bad = 0
    for t in tests(prog):
        with open(os.path.join(res, t + '.txt'), 'w') as f:
            r = subprocess.run([prog, t], stdout=f)
        if r.returncode:
            print('x86: %s exited %d' % (t, r.returncode))
            bad += 1
    print('x86: %d tests in %s' % (len(tests(prog)), res))
    return 1 if bad else 0


REMOTE = r'''
set -u
cd "$1"
j=$2
rm -rf ours system && mkdir ours system
./check list > tests
run() {  # which test
	if [ "$1" = ours ]; then export LD_LIBRARY_PATH=$PWD/lib; else unset LD_LIBRARY_PATH; fi
	timeout 1800 ./check "$2" > "$1/$2.txt" 2> "$1/$2.err"
	echo "$2 $?" >> "$1/status"
}
export -f run
for w in ours system; do
	xargs -P "$j" -I{} bash -c "run $w {}" < tests
done
LD_LIBRARY_PATH=$PWD/lib ldd ./check | grep quadmath > ours/ldd
ldd ./check | grep quadmath > system/ldd
tar -cf - ours system | zstd -q -3
'''


def e2k(a):
    src = os.path.abspath(a.src)
    out = os.path.abspath(a.out)
    cross = os.path.abspath(a.cross)
    gcc = os.path.join(cross, 'bin/e2k-linux-gnu-gcc')
    libdir = (os.path.join(os.path.abspath(a.lib), '.libs') if a.lib
              else os.path.join(cross, 'e2k-linux-gnu/lib64'))
    name = os.path.basename(out.rstrip('/'))
    rdir = a.remote_dir or 'relcc2/quadmath/' + name
    if '/relcc2/' not in '/' + rdir:
        sys.exit('run.py: --remote-dir must be under relcc2/')
    stage = os.path.join(out, 'e2k')
    os.makedirs(os.path.join(stage, 'lib'), exist_ok=True)
    sh([gcc, '-O2', '-I', os.path.join(src, 'libquadmath'),
        os.path.join(HERE, 'check.c'), '-o', os.path.join(stage, 'check'),
        '-L', libdir, '-lquadmath', '-lm'])
    sh(['cp', '-L', os.path.join(libdir, 'libquadmath.so.0'),
        os.path.join(stage, 'lib/')])
    port = ['-p', a.ssh_port] if a.ssh_port else []
    pport = ['-P', a.ssh_port] if a.ssh_port else []
    host = a.host
    sh([SSH] + port + [host, 'rm -rf %s && mkdir -p %s' % (rdir, rdir)])
    sh([SCP] + pport + ['-r', os.path.join(stage, 'check'),
                        os.path.join(stage, 'lib'), '%s:%s/' % (host, rdir)])
    hn = host.split('@')[-1].split('.')[0]
    with open(os.path.join(out, hn + '.tar.zst'), 'wb') as f:
        sh([SSH] + port + [host, 'bash -s %s %d' % (rdir, a.j)],
           input=REMOTE.encode(), stdout=f)
    sh('cd %s && rm -rf %s-ours %s-system && mkdir -p tmp-%s && '
       'zstd -q -d < %s.tar.zst | tar -xf - -C tmp-%s && '
       'mv tmp-%s/ours %s-ours && mv tmp-%s/system %s-system && rmdir tmp-%s'
       % ((shlex.quote(out),) + (hn,) * 10), shell=True)
    for w in ('ours', 'system'):
        d = os.path.join(out, '%s-%s' % (hn, w))
        print('%s: %s' % (w, open(os.path.join(d, 'ldd')).read().strip()))
        st = [l.split() for l in open(os.path.join(d, 'status'))]
        bad = [(t, rc) for t, rc in st if rc != '0']
        print('%s: %d tests, %d exited non-zero%s' % (
            w, len(st), len(bad),
            ''.join('\n  %s rc=%s' % b for b in sorted(bad))))
    rc = 0
    x = os.path.join(out, 'x86')
    pairs = [('%s-ours' % hn, 'x86'), ('%s-system' % hn, 'x86'),
             ('%s-ours' % hn, '%s-system' % hn)]
    for p, q in pairs:
        if os.path.isdir(os.path.join(out, q)):
            print('\n== %s against %s' % (p, q))
            rc |= compare(os.path.join(out, p), os.path.join(out, q))
    if not os.path.isdir(x):
        print('no %s: run "run.py x86 %s" for the x86 comparison' % (x, out))
    return rc


def qnan(tok):
    if len(tok) != 32:
        return False
    try:
        hi = int(tok[:16], 16)
        lo = int(tok[16:], 16)
    except ValueError:
        return False
    return (hi >> 48) & 0x7fff == 0x7fff and ((hi & ((1 << 48) - 1)) or lo)


# ilogbq's results for 0 and NaN, FP_ILOGB0 and FP_ILOGBNAN, are each
# libc's: INT_MIN for both on x86, -INT_MAX and INT_MAX in glibc's
# generic bits/fp-logb.h, which E2K uses.
ILOGB_SPECIAL = {'-2147483648', '-2147483647', '2147483647'}


def classify(x, y):
    """'' if equal, else 'nan', 'flags', 'convention' or 'value'."""
    if x == y:
        return ''
    a, b = x.split(), y.split()
    if len(a) != len(b):
        return 'value'
    if a[:3] == b[:3] and a[-1] == b[-1] and len(a) == 5:
        # signbitq returns any nonzero value for a negative.
        if a[0] == 'signbitq' and (a[3] == '0') == (b[3] == '0'):
            return 'convention'
        if a[0] == 'ilogbq' and a[3] in ILOGB_SPECIAL and b[3] in ILOGB_SPECIAL:
            return 'convention'
    # The flags are the last field, except for quadmath_snprintf, whose
    # lines are LEN FLAGS STRING.
    fa = [i for i, t in enumerate(a) if set(t) <= set('izoux-') and t]
    k = 1 if a[0] == 'quadmath_snprintf' else len(a) - 1
    if k >= len(a):
        return 'value'
    va = a[:k] + a[k + 1:]
    vb = b[:k] + b[k + 1:]
    if va == vb:
        return 'flags'
    if all(s == t or (qnan(s) and qnan(t)) for s, t in zip(va, vb)):
        return 'nan' if a[k] == b[k] else 'nan+flags'
    return 'value'


def compare(da, db):
    names = sorted(f for f in os.listdir(da) if f.endswith('.txt'))
    tot = {'': 0, 'nan': 0, 'nan+flags': 0, 'flags': 0, 'convention': 0,
           'value': 0}
    worst = 0
    rows = []
    for f in names:
        pb = os.path.join(db, f)
        la = open(os.path.join(da, f), errors='replace').read().splitlines()
        lb = (open(pb, errors='replace').read().splitlines()
              if os.path.exists(pb) else [])
        c = dict.fromkeys(tot, 0)
        ex = []
        for i in range(max(len(la), len(lb))):
            x = la[i] if i < len(la) else '<missing>'
            y = lb[i] if i < len(lb) else '<missing>'
            k = classify(x, y)
            c[k] += 1
            if k == 'value' and len(ex) < 3:
                ex.append((x, y))
        for k in tot:
            tot[k] += c[k]
        if any(c[k] for k in c if k) or len(la) != len(lb):
            rows.append((f[:-4], len(la), len(lb), c, ex))
        worst |= bool(c['value'])
    for t, na, nb, c, ex in rows:
        print('%-22s %6d/%-6d lines: %d value, %d flags only, %d NaN bits%s,'
              ' %d convention'
              % (t, na, nb, c['value'], c['flags'], c['nan'] + c['nan+flags'],
                 ' (%d with flags)' % c['nan+flags'] if c['nan+flags'] else '',
                 c['convention']))
        for x, y in ex:
            print('    %s\n    %s' % (x, y))
    print('%d tests: %d lines same, %d value, %d flags only, %d NaN bits,'
          ' %d convention'
          % (len(names), tot[''], tot['value'], tot['flags'],
             tot['nan'] + tot['nan+flags'], tot['convention']))
    return 1 if worst else 0


def main():
    ap = argparse.ArgumentParser()
    sp = ap.add_subparsers(dest='cmd', required=True)
    p = sp.add_parser('x86')
    p.add_argument('out')
    p.add_argument('--src', default=os.path.join(ROOT, 'toolchains/gcc-claude'))
    p = sp.add_parser('e2k')
    p.add_argument('out')
    p.add_argument('cross')
    p.add_argument('host')
    p.add_argument('--lib')
    p.add_argument('--src', default=os.path.join(ROOT, 'toolchains/gcc-claude'))
    p.add_argument('--ssh-port')
    p.add_argument('--remote-dir')
    p.add_argument('-j', type=int, default=4)
    p = sp.add_parser('compare')
    p.add_argument('a')
    p.add_argument('b')
    a = ap.parse_args()
    if a.cmd == 'x86':
        return x86(a)
    if a.cmd == 'e2k':
        return e2k(a)
    return compare(a.a, a.b)


sys.exit(main())
