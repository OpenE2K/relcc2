/* SPDX-License-Identifier: GPL-3.0-or-later */
/* How lcc maps the C11/GNU atomic builtins to E2K code (atomics.md).
   Read the assembly; nothing is run.

     LCC3=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     LCC5=/opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1/bin/lcc
     $LCC3 -O2 -S tools/abi/probes/atomics.c -o -   # 16-byte: libatomic calls
     $LCC5 -O2 -S tools/abi/probes/atomics.c -o -   # 16-byte: ldqp/stqp loops

   Memory orders: 0 relaxed, 2 acquire, 3 release, 5 seq_cst.  */

long g;
__int128 q;

long ld_relaxed (void) { return __atomic_load_n (&g, 0); }
long ld_acquire (void) { return __atomic_load_n (&g, 2); }
long ld_seq_cst (void) { return __atomic_load_n (&g, 5); }

void st_relaxed (long v) { __atomic_store_n (&g, v, 0); }
void st_release (long v) { __atomic_store_n (&g, v, 3); }
void st_seq_cst (long v) { __atomic_store_n (&g, v, 5); }

long xchg_relaxed (long v) { return __atomic_exchange_n (&g, v, 0); }
long xchg_seq_cst (long v) { return __atomic_exchange_n (&g, v, 5); }
long add8 (long v) { return __atomic_fetch_add (&g, v, 5); }
short add2 (short *p, short v) { return __atomic_fetch_add (p, v, 0); }
char add1 (char *p, char v) { return __atomic_fetch_add (p, v, 5); }

int
cas4 (int *p, int e, int d)
{
  return __atomic_compare_exchange_n (p, &e, d, 0, 5, 5);
}

int tas (char *p) { return __atomic_test_and_set (p, 5); }

void fence_acquire (void) { __atomic_thread_fence (2); }
void fence_release (void) { __atomic_thread_fence (3); }
void fence_seq_cst (void) { __atomic_thread_fence (5); }
void sync_synchronize (void) { __sync_synchronize (); }

__int128 ld16 (void) { return __atomic_load_n (&q, 5); }

int
cas16 (__int128 *p, __int128 *e, __int128 d)
{
  return __atomic_compare_exchange_n (p, e, d, 0, 5, 5);
}
