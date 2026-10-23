/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * foldp100_blkasm.c: where GCC stops on an aggregate asm operand that only
 * a register may hold when the aggregate has BLKmode (spec/abi/
 * kernel-build.md, "What a GCC build meets first", "impossible
 * constraint").  MCST's kernel has four asms with an `e2k_qreg_t`
 * (`struct { u64 lo, hi; }`, asm/base_regs_types.h:40-43) "=&r" output
 * printed with %L and %H (asm/e2k_api.h:3894, 3932, 5819, 5971); lcc gives
 * it a register pair (also tools/abi/probes/kernelbuild.py, section
 * `modifiers`).  On a STRICT_ALIGNMENT target GCC gives a 16-byte struct
 * aligned to 8 BLKmode, since TImode wants 16 (gcc/stor-layout.cc
 * compute_record_mode).  The path after that is target-independent, so
 * it shows on any GCC with an aggregate that has no integer mode of its
 * size, here 3 bytes:
 *
 *   gcc -O2 -S -fdump-rtl-expand -fdump-rtl-vregs \
 *       tools/abi/probes/foldp100_blkasm.c -o /dev/null   (also -DIN, -DQ16)
 *
 * and, for lcc and an E2K GCC (-DIN, -DQ16, -DODD likewise):
 *
 *   /opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc -O2 -S \
 *       tools/abi/probes/foldp100_blkasm.c -o - | grep '#'
 *   e2k-linux-gnu-gcc -O2 -S tools/abi/probes/foldp100_blkasm.c -o -
 *
 * 2026-10-24, host GCC 16.2.1 (x86-64):
 *   out_r (default): "impossible constraint in 'asm'".  The expand dump
 *     has the asm writing (mem/c:BLK ... [0 A128]), a stack temporary
 *     with no size (cfgexpand.cc expand_asm_stmt, assign_temp); the
 *     vregs dump no longer has the asm: the error comes from vregs
 *     (function.cc instantiate_virtual_regs_in_insn, check_asm_operands),
 *     after TARGET_MD_ASM_ADJUST has run.
 *   in_r (-DIN): the warning "'asm' operand 1 probably does not match
 *     constraints" (expand, before the hook), then the same error.
 *   out_q16 (-DQ16): compiles on x86-64, where the struct has TImode (no
 *     STRICT_ALIGNMENT).
 * lcc 1.29.16 (e2k-v3) and 1.31.05 (e2k-v3), -O2: every case compiles:
 *   out_r and in_r in one register (%g16), out_q16 and out12 (-DODD) in a
 *   pair (%b[126], %H: %b[127]), out7 (-DODD) in one register.  So lcc
 *   takes any aggregate of 1-16 bytes for "r"; GCC refuses every BLKmode
 *   one, which on a STRICT_ALIGNMENT E2K port includes the kernel's
 *   e2k_qreg_t.
 */

struct s3 { char c[3]; };
struct q16 { unsigned long lo, hi; };

#if defined IN
int in_r (struct s3 v)
{
  int r;
  __asm__ ("# in %1 -> %0" : "=r" (r) : "r" (v));
  return r;
}
#elif defined Q16
struct q16 out_q16 (void)
{
  struct q16 v;
  __asm__ ("# out %0" : "=&r" (v));
  return v;
}
#elif defined ODD
struct s12 { int a, b, c; };
struct s7 { char c[7]; };
struct s12 out12 (void)
{
  struct s12 v;
  __asm__ ("# out %0 %H0" : "=&r" (v));
  return v;
}
struct s7 out7 (void)
{
  struct s7 v;
  __asm__ ("# out %0" : "=&r" (v));
  return v;
}
#else
struct s3 out_r (void)
{
  struct s3 v;
  __asm__ ("# out %0" : "=&r" (v));
  return v;
}
#endif
