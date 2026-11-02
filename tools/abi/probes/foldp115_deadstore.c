/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * foldp115_deadstore.c: a store just before an asm that jumps away.  MCST's
 * decompressor (arch/e2k/boot/decompress.c) lets the boot CPU unpack the
 * kernel while the others wait in
 *
 *     while (unpacking_in_progress) E2K_NOP(7);         (:643-644)
 *
 * on a plain `static int unpacking_in_progress = 1` (:89).  The boot CPU
 * clears it with a plain store (:735) and jumps to the kernel through
 * jump_to_image (:591-605): E2K_JUMP_ABSOLUTE_WITH_ARGUMENTS_2, an asm
 * volatile that clobbers "ctpr1", "r0" and "r1" but not memory, then
 * unreachable () (asm/e2k_api.h:7036-7049), which for GCC is asm
 * volatile ("") and __builtin_unreachable () and for lcc the builtin
 * alone (include/linux/compiler-gcc.h:48-49, 65-70).  wait () and boot ()
 * here have those shapes (spec/abi/kernel-build.md, "A store before a
 * jump").
 *
 * Compile only; read with -S and look for loads and stores of flag:
 *
 *   /opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc -O3 -S \
 *       tools/abi/probes/foldp115_deadstore.c -o - | grep 'flag'
 *   e2k-linux-gnu-gcc -O2 -S tools/abi/probes/foldp115_deadstore.c -o - |
 *       grep 'flag'
 *   e2k-linux-gnu-gcc -O2 -DJUMP_CLOBBERS_MEMORY ...    (the asm with
 *                                                        "memory")
 *   e2k-linux-gnu-gcc -O2 -fdump-tree-dse1-details \
 *       -fdump-ipa-whole-program-details ...  ("Deleted dead store:
 *       flag = 0", "flag/N (read-only)")
 *
 * (also -O1, -fkernel, e2k-v5.8c2, e2k-v6.2c3 and lcc 1.31.05).
 *
 * 2026-11-03: lcc 1.29.16 (v3, v5.8c2, v6.2c3 at -O1, -O3 and -O3
 * -fkernel) and 1.31.05 (v3, v4.8c, v5.8c2, v6.2c3 at -O3) keep the
 * stw of flag and an ldw of it in the wait loop.  The proof-of-concept
 * GCC (0.6, 8C2 cross) at -O2 has no access to flag at all: DSE deletes
 * the store, a store to global memory with no later reader in a block
 * that ends in __builtin_unreachable (gcc/tree-ssa-dse.cc:1157-1180);
 * with no store left the whole-program visibility pass makes flag
 * read-only (ipa_discover_variable_flags, gcc/ipa.cc:789-831), and CCP
 * folds every load to the initializer, so wait () spins on {nop 7}
 * for ever.  The host's x86 GCC 16.2.1 does the same with an x86 jump.
 * With "memory" on the jump asm (-DJUMP_CLOBBERS_MEMORY), or the store
 * written as WRITE_ONCE, GCC keeps both; a volatile read alone keeps the
 * load but not the store, and still spins.
 */

#ifdef JUMP_CLOBBERS_MEMORY
#define JUMP_CLOBBERS "ctpr1", "memory"
#else
#define JUMP_CLOBBERS "ctpr1"
#endif

static int flag = 1;

extern void unpack (void);

static inline __attribute__ ((always_inline)) void
jump_away (unsigned long to)
{
  __asm__ __volatile__ ("{movtd %0, %%ctpr1}\n\tct %%ctpr1"
			:: "r" (to) : JUMP_CLOBBERS);
  __asm__ __volatile__ ("");
  __builtin_unreachable ();
}

__attribute__ ((noinline)) void
wait_or_unpack (int bsp, unsigned long to)
{
  if (!bsp)
    {
      while (flag)
	__asm__ __volatile__ ("{nop 7}" ::: "memory");
      jump_away (to);
    }
  unpack ();
  __asm__ __volatile__ ("" ::: "memory");	/* smp_wmb () */
  flag = 0;
  jump_away (to);
}
