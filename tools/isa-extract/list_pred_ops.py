#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""List every E2K operation that writes or reads a predicate, from
spec/isa/binutils-ops.yaml, grouped by family, with per-version counts.

Writers: formats with a predicate destination (ALOPF7, ALOPF17, ALOPF8).
Readers listed here: MRGC formats (MERGE, ALOPF11_MERGE, ALOPF21_MERGE),
ctcond-controlled ALU ops (ALOPF12_IBRANCHD/ICALLD), AAU stores whose
`incr` takes a predicate (ALOPF10), and the manual (control/PLS/SS) ops.
Any other ALU op reads a predicate only through an optional RLP
(`? %predN`), which gas accepts on every ALS except ibranchd/icalld.

Usage: python3 tools/isa-extract/list_pred_ops.py [--list]
"""
import collections, os, re, sys
import yaml

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../..')
Y = os.path.join(ROOT, 'spec/isa/binutils-ops.yaml')
VERS = range(1, 9)

WRITER_FMTS = ('ALOPF7', 'ALOPF17', 'ALOPF8')
MRGC_FMTS = ('MERGE', 'ALOPF11_MERGE', 'ALOPF21_MERGE')
CT_FMTS = ('ALOPF12_IBRANCHD', 'ALOPF12_ICALLD')
MANUAL_READERS = ('ct', 'call', 'ibranch', 'rbranch', 'done', 'iret', 'hret',
                  'glaunch', 'abgc', 'rbgc', 'srp', 'crp', 'slrp', 'incr',
                  'pass', 'andp', 'landp', 'movep', 'clpandp', 'clplandp',
                  'alc', 'abn', 'abp', 'setbp')

def family(o):
    n = o['name']
    for pat, fam in ((r'^cmpand', 'cmpand<c><s|d>b (int AND-test)'),
                     (r'^cmp', 'cmp<c><s|d>b (int subtract-test)'),
                     (r'^fcmp', 'fcmp<c><s|d>b (FP)'),
                     (r'^fxcmp', 'fxcmp<c><s|d|x>b (80-bit FP)'),
                     (r'^cctop', 'cctop<c> (x86 flags in a register)'),
                     (r'^pcmp', 'pcmp<eq|gt><b|h|w|d><op|ap> (64-bit packed)'),
                     (r'^qpcmp', 'qpcmp<eq|gt><b|h|w|d><op|ap> (128-bit packed)')):
        if re.match(pat, n):
            return fam
    return 'other'

def mrgc_family(o):
    n = o['name']
    if o['alopf'] == 'MERGE':
        return 'merge<s|d>'
    if o['alopf'] == 'ALOPF11_MERGE':
        return 'pmrgp/qpmrgp'
    if n.startswith('merge_'):
        return 'merge_<op> (merge first, then op)'
    return '<op>_merge (op first, then merge)'

def vcount(ops):
    return ' '.join('v%d:%d' % (v, sum(v in o['isets'] for o in ops)) for v in VERS)

def table(title, ops, famf, show_cmpopce):
    print('## ' + title)
    print('total entries: %d   per version: %s' % (len(ops), vcount(ops)))
    fams = collections.OrderedDict()
    for o in ops:
        fams.setdefault(famf(o), []).append(o)
    for fam, l in fams.items():
        chans = sorted({tuple(o['channels']) for o in l})
        isets = sorted({tuple(o['isets']) for o in l})
        opcs = sorted({o['opc'] for o in l})
        print('- %s: %d ops; opc %s; channels %s; isets %s; %s' % (
            fam, len(l), ','.join(hex(x) for x in opcs),
            ' | '.join(','.join(map(str, c)) for c in chans),
            ' | '.join('v%d-v%d' % (i[0], i[-1]) for i in isets), vcount(l)))
        if show_cmpopce:
            m = collections.OrderedDict()
            for o in l:
                stem = re.sub(r'^(f?x?cmpand|f?x?cmp|cctop|q?pcmp)', '', o['name'])
                m.setdefault(o.get('cmpopce'), []).append(o['name'])
            print('    cmpopce: ' + '; '.join('%s=%s' % (hex(k), '/'.join(v))
                                           for k, v in m.items()))
        if '--list' in sys.argv:
            for o in l:
                print('    %s %s opc=%#x ch=%s args=%s cmpopce=%s nops=%s isets=%s' % (
                    o['name'], o['alopf'], o['opc'], o['channels'], o['args'],
                    o.get('cmpopce'), o.get('implicit_nops'), o['isets']))
    print()

def main():
    d = yaml.safe_load(open(Y))
    print('source: %s (binutils %s, commit %s)\n' % (
        os.path.relpath(Y, ROOT), d['source']['release'], d['source']['commit'][:12]))
    alu = d['alu_ops']
    w = [o for o in alu if o['alopf'] in WRITER_FMTS]
    # Sanity: no other template carries cmpopce (a predicate destination).
    other = [o['name'] for o in alu if 'cmpopce' in o and o['alopf'] not in WRITER_FMTS]
    assert not other, other
    for fmt in WRITER_FMTS:
        l = [o for o in w if o['alopf'] == fmt]
        print('%s: %d entries, per version %s' % (fmt, len(l), vcount(l)))
    print()
    table('Predicate writers (ALOPF7/17/8)', w, family, True)
    m = [o for o in alu if o['alopf'] in MRGC_FMTS]
    table('MRGC readers (merge condition)', m, mrgc_family, False)
    c = [o for o in alu if o['alopf'] in CT_FMTS]
    table('ALU ops controlled by SS.ctcond', c, lambda o: o['name'], False)
    a = [o for o in alu if o['alopf'] == 'ALOPF10']
    table('AAU stores (staa*; `incr` may carry an RLP on AM2/AM5)', a,
          lambda o: 'staa*', False)
    print('## Manual (non-ALU) operations that read predicates or rotate them')
    for o in d['manual_ops']:
        if o['name'] in MANUAL_READERS:
            print('- %s: isets %s' % (o['name'], ','.join('v%d' % v for v in o['isets'])))

if __name__ == '__main__':
    main()
