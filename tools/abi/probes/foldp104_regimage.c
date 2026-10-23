/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * foldp104_regimage.c: what a check_stack function (MCST's kernel spells
 * it __interrupt) must keep in registers.  The kernel's register images
 * (e2k_psp_t, e2k_pcsp_t, e2k_cr0_t, e2k_gd_t in
 * arch/e2k/include/asm/cpu_regs_types.h) are 16-byte unions of bit-field
 * structs over two u64, aligned to 8; e2k_setjmp, the fast system calls
 * and relocate_kernel read, change and write them in functions that must
 * not touch the data stack (spec/abi/kernel-build.md, "Functions without
 * a data-stack frame").  Each function below is one shape those
 * functions have:
 *
 *   save_psp   read a register image with two rrd, change a bit-field,
 *              store it through a pointer (e2k_setjmp, incr_psp_ind)
 *   write_gd   build one from a compound literal that leaves bytes out
 *              (new_gd_v6: Base is 48 bits, Size the top 32) and write
 *              both halves with rwd (relocate_kernel's jump_kernel)
 *   across     a register image live across a call
 *   hex        constants used across calls in a loop (dump_u64_no_stack,
 *              dump_vprintk)
 *
 * Compile only; read with -S (nothing here may run in user mode):
 *
 *   /opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc -O3 -S \
 *       tools/abi/probes/foldp104_regimage.c -o -
 *   (also -O3 -fkernel -fno-global-regs, and e2k-v5.8c2, e2k-v6.2c3)
 *
 * 2026-10-24: lcc 1.29.16 (e2k-v3, e2k-v5.8c2, e2k-v6.2c3; -O2, -O3,
 * -O3 -fkernel with -fno-global-regs or with the kernel's -ffixed-g16
 * ... g25) and 1.31.05 (e2k-v3, -O3) compile all four with no getsp and
 * no error: the unions stay in register pairs (save_psp changes Ind
 * with getfd/insfd on the rrd results; write_gd builds lo with andd
 * and hi with shld, the left-out bytes zero).  At -O0 lcc refuses
 * save_psp, write_gd and across (the kernel builds at -O3).
 *
 * A compiler that keeps such a union in memory has to refuse them
 * ("stack access in function with attribute 'check_stack'"): GCC gives
 * a 16-byte aggregate aligned to 8 BLKmode under STRICT_ALIGNMENT when
 * TImode's alignment is 16 (gcc/stor-layout.cc:1943-1953), and builds
 * the zero for the bytes write_gd leaves out (DSE trims the
 * gimplifier's "g = {}" to "MEM <char[6]> [&g + 6B] = {}") in a stack
 * temporary even when g is in registers (gcc/expr.cc:8372, 9573; host
 * GCC 16.2 x86-64 -O2 -Wno-attributes: "movl $0, -24(%rsp)",
 * "movw %ax, -20(%rsp)", "movq -24(%rsp), %rax").  The proof-of-concept
 * back end before it kept such unions in TImode (v0.5-17) refused
 * save_psp and write_gd at -O2.  A GCC port:
 *
 *   e2k-linux-gnu-gcc -O2 -S tools/abi/probes/foldp104_regimage.c -o -
 *
 * must compile every function without that error and without getsp.
 * across and hex are the shapes of the other two causes (a value live
 * across a call in a call-clobbered register saved on the stack by
 * caller saves; a constant with a REG_EQUIV left without a register
 * class); whether a GCC meets them here depends on its costs (the
 * v0.5-17 back end compiled both).
 */

typedef unsigned long u64;
typedef struct { u64 lo, hi; } qreg_t;

/* e2k_psp_t, with the kernel's anonymous e2k_qreg_t member (which needs
   -fms-extensions) spelled as an anonymous struct.  */
typedef union {
	struct {
		struct { u64 Base : 48; };
		struct { u64 Ind : 32; u64 Size : 32; };
	};
	struct { u64 lo, hi; };
	qreg_t word;
} psp_t;

/* e2k_gd_t.  */
typedef union {
	struct {
		struct { u64 Base : 48; };
		struct { u64 : 32; u64 Size : 32; };
	};
	struct { u64 lo, hi; };
	qreg_t word;
} gd_t;

struct jb { psp_t psp; long x; };

#define CS __attribute__ ((check_stack))

static inline __attribute__ ((always_inline)) psp_t read_psp (void)
{
	psp_t r;
	__asm__ __volatile__ ("rrd %%psp.lo, %0" : "=r" (r.lo));
	__asm__ __volatile__ ("rrd %%psp.hi, %0" : "=r" (r.hi));
	return r;
}

CS void save_psp (struct jb *jb, u64 n)
{
	psp_t p = read_psp ();
	p.Ind += n;
	jb->psp = p;
}

CS void write_gd (u64 base, u64 size)
{
	gd_t g = (gd_t) { .Base = base, .Size = size };
	__asm__ __volatile__ ("rwd %0, %%osgd.lo\n\trwd %1, %%osgd.hi"
			      : : "r" (g.lo), "r" (g.hi) : "memory");
}

extern void ext (u64);

CS u64 across (u64 n)
{
	psp_t p = read_psp ();
	ext (n);
	return p.Base + p.Ind;
}

extern void put (int);

CS void hex (u64 v)
{
	static const char d[] = "0123456789abcdef";
	for (int i = 60; i >= 0; i -= 4)
		put (d[(v >> i) & 15]);
	put ('\n');
}
