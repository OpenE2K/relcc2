/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Which channel lcc gives the wait-unlock store of each read-modify-write,
   by memory order and target (spec/abi/atomics.md, "Read-modify-write";
   docs/spec-poc-findings.md A29).  Read the assembly; nothing is run:

     for t in v3 v4.8c v4.1c+ v5.8c2 v6.2c3 v6.16c; do
       /opt/mcst/lcc-1.29.16.e2k-$t.linux-6.1/bin/lcc -O3 -S \
         tools/abi/probes/pocabi1_ord.c -o - |
       awk '/^[a-z]+[0-9]:/ { f = $1 } /mas = 0x2/ { print t, f, $1 }' t=$t
     done

   Names: x exchange, f fetch-add, o fetch-or (int), t test-and-set,
   c compare-exchange; the digit is the order (0 relaxed, 1 consume,
   2 acquire, 3 release, 4 acq_rel, 5 seq_cst).  lcc 1.29.16: ALC5
   (std,5 / stw,5 / stb,5) for x, f, o, t at orders 0 and 2 on v5.8c2,
   v6.2c3 and v6.16c; ALC2 for every other order, for every c, and for
   everything on v3, v4.8c and v4.1c+.  (xa1 draws lcc's warning that
   consume is not a valid order for an exchange.)  */

long xa0 (long *p, long v) { return __atomic_exchange_n (p, v, 0); }
long fa0 (long *p, long v) { return __atomic_fetch_add (p, v, 0); }
int fo0 (int *p, int v) { return __atomic_fetch_or (p, v, 0); }
int ta0 (char *p) { return __atomic_test_and_set (p, 0); }
int ca0 (long *p, long *e, long d)
{ return __atomic_compare_exchange_n (p, e, d, 0, 0, 0); }

long xa1 (long *p, long v) { return __atomic_exchange_n (p, v, 1); }
long fa1 (long *p, long v) { return __atomic_fetch_add (p, v, 1); }
int fo1 (int *p, int v) { return __atomic_fetch_or (p, v, 1); }
int ta1 (char *p) { return __atomic_test_and_set (p, 1); }
int ca1 (long *p, long *e, long d)
{ return __atomic_compare_exchange_n (p, e, d, 0, 1, 1); }

long xa2 (long *p, long v) { return __atomic_exchange_n (p, v, 2); }
long fa2 (long *p, long v) { return __atomic_fetch_add (p, v, 2); }
int fo2 (int *p, int v) { return __atomic_fetch_or (p, v, 2); }
int ta2 (char *p) { return __atomic_test_and_set (p, 2); }
int ca2 (long *p, long *e, long d)
{ return __atomic_compare_exchange_n (p, e, d, 0, 2, 0); }

long xa3 (long *p, long v) { return __atomic_exchange_n (p, v, 3); }
long fa3 (long *p, long v) { return __atomic_fetch_add (p, v, 3); }
int fo3 (int *p, int v) { return __atomic_fetch_or (p, v, 3); }
int ta3 (char *p) { return __atomic_test_and_set (p, 3); }

long xa4 (long *p, long v) { return __atomic_exchange_n (p, v, 4); }
long fa4 (long *p, long v) { return __atomic_fetch_add (p, v, 4); }
int fo4 (int *p, int v) { return __atomic_fetch_or (p, v, 4); }
int ta4 (char *p) { return __atomic_test_and_set (p, 4); }
int ca4 (long *p, long *e, long d)
{ return __atomic_compare_exchange_n (p, e, d, 0, 4, 0); }

long xa5 (long *p, long v) { return __atomic_exchange_n (p, v, 5); }
long fa5 (long *p, long v) { return __atomic_fetch_add (p, v, 5); }
int fo5 (int *p, int v) { return __atomic_fetch_or (p, v, 5); }
int ta5 (char *p) { return __atomic_test_and_set (p, 5); }
int ca5 (long *p, long *e, long d)
{ return __atomic_compare_exchange_n (p, e, d, 0, 5, 5); }
