/* SPDX-License-Identifier: GPL-3.0-or-later */
/* lcc's inline 16-byte atomics on v5+ (docs/spec-poc-findings.md A29),
   read as assembly, one function per operation and order:
     /opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1/bin/lcc -O3 -S pocabi1_atom16.c
     /opt/mcst/lcc-1.29.16.e2k-v6.2c3.linux-6.1/bin/lcc -O3 -S pocabi1_atom16.c
   (and lcc 1.31.05; -DWITH_SYNC adds the __sync builtins).  */

typedef __int128 T;
T q;

T ld_rlx (T *p) { return __atomic_load_n (p, __ATOMIC_RELAXED); }
T ld_con (T *p) { return __atomic_load_n (p, __ATOMIC_CONSUME); }
T ld_acq (T *p) { return __atomic_load_n (p, __ATOMIC_ACQUIRE); }
T ld_sc (T *p) { return __atomic_load_n (p, __ATOMIC_SEQ_CST); }

void st_rlx (T *p, T v) { __atomic_store_n (p, v, __ATOMIC_RELAXED); }
void st_rel (T *p, T v) { __atomic_store_n (p, v, __ATOMIC_RELEASE); }
void st_sc (T *p, T v) { __atomic_store_n (p, v, __ATOMIC_SEQ_CST); }

T xchg_rlx (T *p, T v) { return __atomic_exchange_n (p, v, __ATOMIC_RELAXED); }
T xchg_sc (T *p, T v) { return __atomic_exchange_n (p, v, __ATOMIC_SEQ_CST); }

T add_rlx (T *p, T v) { return __atomic_fetch_add (p, v, __ATOMIC_RELAXED); }
T add_sc (T *p, T v) { return __atomic_fetch_add (p, v, __ATOMIC_SEQ_CST); }
T and_sc (T *p, T v) { return __atomic_fetch_and (p, v, __ATOMIC_SEQ_CST); }
T or_sc (T *p, T v) { return __atomic_fetch_or (p, v, __ATOMIC_SEQ_CST); }
T addf_sc (T *p, T v) { return __atomic_add_fetch (p, v, __ATOMIC_SEQ_CST); }

int
cas_sc (T *p, T *e, T d)
{
  return __atomic_compare_exchange_n (p, e, d, 0, __ATOMIC_SEQ_CST,
				      __ATOMIC_SEQ_CST);
}

int
cas_rlx (T *p, T *e, T d)
{
  return __atomic_compare_exchange_n (p, e, d, 0, __ATOMIC_RELAXED,
				      __ATOMIC_RELAXED);
}

int
casw_acq (T *p, T *e, T d)
{
  return __atomic_compare_exchange_n (p, e, d, 1, __ATOMIC_ACQUIRE,
				      __ATOMIC_ACQUIRE);
}

/* Constant operands: what a 16-byte constant becomes.  */
T add1_sc (T *p) { return __atomic_fetch_add (p, 1, __ATOMIC_SEQ_CST); }
void st0_rlx (T *p) { __atomic_store_n (p, 0, __ATOMIC_RELAXED); }

/* A global object: whether the address form changes anything.  */
T gld_rlx (void) { return __atomic_load_n (&q, __ATOMIC_RELAXED); }
T gld_sc (void) { return __atomic_load_n (&q, __ATOMIC_SEQ_CST); }

#ifdef WITH_SYNC
T sync_vcas (T *p, T e, T d) { return __sync_val_compare_and_swap (p, e, d); }
T sync_add (T *p, T v) { return __sync_fetch_and_add (p, v); }
#endif

int lf (void) { return __atomic_always_lock_free (16, 0); }
int lfr (T *p) { return __atomic_is_lock_free (16, p); }
