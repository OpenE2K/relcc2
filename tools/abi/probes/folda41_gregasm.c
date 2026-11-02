/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * folda41_gregasm.c: what lcc and GCC assume an asm does to a global
 * register variable that the asm's template names but its operands and
 * clobbers don't, or that it lists only as a clobber (spec/abi/
 * source-compat.md, "What lcc assumes an asm does"; kernel-build.md,
 * "Global registers").  MCST's kernel has three such patterns: the v5+
 * ASM_RESTORE_GREG_V5 loads %g16-%g31 in its template with no clobbers
 * (asm/e2k_api.h:361-371); barrier() clobbers %g18-%g20 to say "may
 * change" (asm/compiler.h:131-132, 152-157); WRITE_UACCESS_REGS_CACHED
 * reads %g20 and %g21 in its template and lists them only as clobbers
 * (asm/e2k_api.h:2141-2160).  The proof-of-concept back end's finding
 * A41.
 *
 * Compile only and read the assembly:
 *
 *   L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc   (also v5.8c2, and
 *                                     toolchains/lcc-1.31.05-e2k-v3/lcc)
 *   $L -O2 -S tools/abi/probes/folda41_gregasm.c -o -
 *   $L -O3 -fkernel -masm-inline -ffixed-g16 ... -ffixed-g25 -S ...
 *   (-DNOINL puts lcc's _Pragma ("no_asm_inline") before every asm)
 *   gcc -O2 -DVOL -S tools/abi/probes/folda41_gregasm.c -o -   (host GCC:
 *                         the same shapes on x86-64's %r12-%r15; -DVOL
 *                         makes every asm volatile, which GCC would
 *                         otherwise delete for its unused output)
 *
 * copy_*: a copy of the register variable taken before the asm, used
 * after it (the kernel's `ti`, a copy of `current` in %g17).  The right
 * code reads the copy, not the register: if the function uses %g17
 * after the asm, the compiler took the copy for the register.
 *   _named   the template writes %g17 (no operand, no clobber)
 *   _other   the template writes %g18 instead
 *   _clob    _named with a "g17" clobber
 *   _out     the asm has the variable as a "+r" output
 * reads_*: the variable read before and after such an asm, the
 * difference returned; folded to 0 means both reads were merged.
 * store_*: a store to the variable just before an asm that lists it
 * only as a clobber; a deleted store is a missing write to the register.
 *   _barrier  barrier()'s shape: no reference in the template
 *   _mmurw    WRITE_UACCESS_REGS_CACHED's: the template reads it
 *
 * 2026-11-03, lcc 1.29.16 (e2k-v3, e2k-v5.8c2) and 1.31.05 (e2k-v3,
 * e2k-v5.8c2) alike (v5 drops only "ipd 2"):
 *   opaque asm (-O2; -O3 -fkernel -masm-inline with -DNOINL):
 *     copy_*: the copy goes to a temporary before the asm (%g16 at -O2,
 *       %g26 with -ffixed-g16..g25, a window register under -fkernel)
 *       and is read after it, whatever the template names;
 *     reads_named folds to 0, as GCC's; reads_clob and reads_out read
 *       %g17 again;
 *     store_*: the store is kept.
 *   parsed asm (-O3 -fkernel -masm-inline, the kernel's options): the
 *     template's addd becomes lcc's own operation; copy_named and
 *     copy_clob copy %g17 to %r2 first, copy_other reads %g17 after the
 *     asm (the template writes only %g18), reads_named and reads_clob
 *     give a - 1; the stores are kept.
 * Host GCC 16.2.1 -O2 -DVOL (x86-64): copy_named reads %r15 after the
 *   asm (the copy taken for the register), copy_clob copies first,
 *   copy_out is right; reads_named and reads_clob fold to 0, reads_out
 *   reads twice; store_barrier and store_mmurw delete the store.
 *   Without -DVOL it deletes every one of these asms (unused output).
 */

#if defined __e2k__
# define G1 "%g17"
# define G2 "%g18"
# define G3 "%g20"
# define SET1 "addd 0x0, 0x1, %%dg17"
# define SET2 "addd 0x0, 0x1, %%dg18"
# define USE3 "{mmurw %%g20, %%cont}"
# define C1 "g17"
# define C3 "g20", "g21"
# define CBAR "g19", "g18", "g20"
#else
# define G1 "r15"
# define G2 "r14"
# define G3 "r13"
# define SET1 "movq $1, %%r15"
# define SET2 "movq $1, %%r14"
# define USE3 "movq %%r13, %%cr3"
# define C1 "r15"
# define C3 "r13", "r12"
# define CBAR "r12", "r14", "r13"
#endif

#ifdef VOL
# define ASM __asm__ __volatile__
#elif defined NOINL
# define ASM _Pragma ("no_asm_inline") __asm__
#else
# define ASM __asm__
#endif

struct ti { long pad; long next; };
register struct ti *cur __asm__ (G1);
register unsigned long other __asm__ (G2);
register unsigned long ctx3 __asm__ (G3);

long
copy_named (long *buf)
{
  struct ti *ti = cur;
  int unused;
  ASM (SET1 : "=r" (unused) : "r" (buf), "m" (*buf));
  return ti->next;
}

long
copy_other (long *buf)
{
  struct ti *ti = cur;
  int unused;
  ASM (SET2 : "=r" (unused) : "r" (buf), "m" (*buf));
  return ti->next;
}

long
copy_clob (long *buf)
{
  struct ti *ti = cur;
  int unused;
  ASM (SET1 : "=r" (unused) : "r" (buf), "m" (*buf) : C1);
  return ti->next;
}

long
copy_out (long *buf)
{
  struct ti *ti = cur;
  ASM ("" : "+r" (cur) : "r" (buf), "m" (*buf));
  return ti->next;
}

long
reads_named (long *buf)
{
  long a = (long) cur;
  int unused;
  ASM (SET1 : "=r" (unused) : "r" (buf), "m" (*buf));
  return a - (long) cur;
}

long
reads_clob (long *buf)
{
  long a = (long) cur;
  int unused;
  ASM (SET1 : "=r" (unused) : "r" (buf), "m" (*buf) : C1);
  return a - (long) cur;
}

long
reads_out (long *buf)
{
  long a = (long) cur;
  ASM ("" : "+r" (cur) : "r" (buf), "m" (*buf));
  return a - (long) cur;
}

void
store_barrier (void)
{
  int unused;
  other |= 4;
  ASM ("" : "=r" (unused) : : "memory", CBAR);
}

void
store_mmurw (unsigned long ctx)
{
  int unused;
  ctx3 = ctx;
  ASM (USE3 : "=r" (unused) : : "memory", C3);
}
