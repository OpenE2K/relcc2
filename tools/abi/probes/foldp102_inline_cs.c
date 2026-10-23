/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * foldp102_inline_cs.c: does lcc inline a function that carries
 * check_stack, and which copy does the attribute's check apply to?
 * MCST's kernel declares helpers `static __always_inline __interrupt`
 * (__interrupt is __attribute__((__check_stack__)) for lcc and
 * __attribute__((__interrupt__)) for other compilers, asm/sections.h:11-15):
 * native_read_UPSR_reg (asm/native_cpu_regs_access.h:36),
 * dump_debug_info_no_stack (kernel/ttable.c:205) and four in
 * kvm/paravirt_sw/cpu.h; kvm/pv-emul.h:111 has a `static inline
 * __interrupt` one (spec/abi/kernel-build.md, "Functions without a
 * data-stack frame").  GCC inlines a function that has an attribute from
 * the target's attribute table only if TARGET_FUNCTION_ATTRIBUTE_INLINABLE_P
 * says so, and the default says no (gcc/tree-inline.cc
 * function_attribute_inlinable_p; gcc/target.def, hook_bool_const_tree_false),
 * so an always_inline one is an error ("function not inlinable").
 *
 * Compile only; read with -S:
 *
 *   for t in v3 v5.8c2 v6.2c3; do for o in -O0 -O2 -O3; do
 *     /opt/mcst/lcc-1.29.16.e2k-$t.linux-6.1/bin/lcc $o -S \
 *         tools/abi/probes/foldp102_inline_cs.c -o - | grep -E '^[a-z_0-9]+:|disp|getsp'
 *   done; done
 *   (also toolchains/lcc-1.31.05-e2k-v3/lcc; -DSTACKY, -DSTACKY_CS and
 *   -DMANYARGS add the cases below; -fkernel changes nothing here)
 *
 * 2026-10-24, lcc 1.29.16 (v3, v5.8c2, v6.2c3) and 1.31.05 (v3), the same
 * on all:
 *   -O1 and up: rd_upsr is inlined into both callers, the check_stack
 *     caller1cs included, and no out-of-line copy is emitted; plain is
 *     inlined from -O2 on (-O1 calls it).  -O3 -fkernel as -O3.
 *   -O0: nothing is inlined, always_inline included (callers `disp` the
 *     helpers, which are compiled out of line).
 *   -DSTACKY: -O1 and up compile with no error; caller3 gets a getsp for
 *     the local whose address escapes and there is no out-of-line stacky.
 *     -O0: "stack access in function with attribute 'check_stack'" (the
 *     out-of-line stacky).
 *   -DSTACKY_CS: the same helper inlined into a check_stack caller is
 *     that error at every level.
 *   -DMANYARGS: as -DSTACKY (a 10-argument call).
 * So check_stack is checked on each function's own code: once inlined, a
 * helper's code is its caller's and follows the caller's attribute.  A
 * GCC port's attribute must let such helpers inline (the hook answering
 * true for it), or every file that reads %upsr fails.
 */

#define CS __attribute__((__check_stack__))
#define AI __attribute__((__always_inline__)) inline

extern void ext (long *);

/* The kernel's shape: always_inline + check_stack, a state-register
   read.  */
static AI CS long rd_upsr (void)
{
  long v;
  __asm__ ("rrd %%upsr, %0" : "=r" (v));
  return v;
}
long caller1 (void) { return rd_upsr () + 1; }
CS long caller1cs (void) { return rd_upsr () + 2; }

/* Plain inline + check_stack.  */
static inline CS long plain (long a, long b) { return a * b + 3; }
long caller2 (long a, long b) { return plain (a, b) + plain (b, a); }

#if defined STACKY || defined STACKY_CS
/* A check_stack helper that needs the data stack: the address of a
   local escapes.  */
static AI CS long stacky (void)
{
  long x = 5;
  ext (&x);
  return x;
}
#endif
#ifdef STACKY
long caller3 (void) { return stacky (); }
#endif
#ifdef STACKY_CS
CS long caller3cs (void) { return stacky (); }
#endif

#ifdef MANYARGS
extern long ext10 (long, long, long, long, long, long, long, long, long,
                   long);
static AI CS long many (void) { return ext10 (1, 2, 3, 4, 5, 6, 7, 8, 9, 10); }
long caller4 (void) { return many (); }
#endif
