/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * foldp106_coldshift.c: where a compiler calls the 128-bit shift helpers
 * (__ashlti3, __lshrti3, __ashrti3), which MCST's kernel doesn't have
 * (it links no libgcc; spec/abi/kernel-build.md, "128-bit arithmetic
 * and no libgcc").  The kernel shifts a 128-bit product by a variable
 * count in mul_u64_u32_shr and mul_u64_u64_shr
 * (include/linux/math64.h:157-171; kernel/sched/fair.c:345), and under
 * CONFIG_MCST its unlikely() is __builtin_expect_with_probability
 * (!!(x), 1, 0.0001) (include/linux/compiler.h:76-84), so every
 * unlikely() path is cold code to GCC.
 *
 * Compile only; read with -S and list the helpers each function calls:
 *
 *   for o in -O0 -O1 -O2 -O3 -Os; do
 *     /opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc $o -S \
 *         tools/abi/probes/foldp106_coldshift.c -o - |
 *       awk '/^[a-z_0-9]+:/ { f = $1 }
 *            /disp.*__(ashl|lshr|ashr|mul)ti3/ { print f, $3 }'
 *   done
 *
 * (also e2k-v5.8c2, e2k-v6.2c3, and a GCC in place of lcc).
 *
 * 2026-10-24: lcc 1.29.16 (v3, v5.8c2, v6.2c3) and 1.31.05 (v3) call no
 * helper at -O1, -O2, -O3 or -Os, and at -O0 call one for every 128-bit
 * shift, constant counts included (__multi3 never).  GCC synthesises a
 * double-word shift from word shifts only for a constant count or where
 * optimize_insn_for_speed_p () (gcc/optabs.cc:1821-1830), which is
 * false at -Os and, from -O1, in a cold function and in a block GCC
 * predicts cold (gcc/predict.cc:352-366); the proof-of-concept back end
 * before it expanded variable 128-bit shifts itself (v0.5-17) called
 * the helpers in cold_*, path_prob and path_coldcall at -O1, -O2 and
 * -O3, in every function with a variable count at -Os, and nowhere at
 * -O0.  path_expect (plain __builtin_expect) stayed inline.
 */

typedef unsigned __int128 u128;
typedef __int128 s128;
typedef unsigned long u64;

#define mcst_unlikely(x) __builtin_expect_with_probability (!!(x), 1, 0.0001)

extern void warn_once (void) __attribute__ ((cold));

u128 hot_shl (u128 a, int n) { return a << n; }
u128 hot_shr (u128 a, int n) { return a >> n; }
s128 hot_sar (s128 a, int n) { return a >> n; }
u128 hot_const (u128 a) { return a >> 37; }

__attribute__ ((cold)) u128 cold_shl (u128 a, int n) { return a << n; }
__attribute__ ((cold)) u128 cold_shr (u128 a, int n) { return a >> n; }
__attribute__ ((cold)) s128 cold_sar (s128 a, int n) { return a >> n; }
__attribute__ ((cold)) u128 cold_const (u128 a) { return a >> 37; }

/* The kernel's unlikely() under CONFIG_MCST.  */
u64 path_prob (u64 a, unsigned mul, unsigned shift, int c)
{
	if (mcst_unlikely (c))
		return (u64) (((u128) a * mul) >> shift);
	return a;
}

/* unlikely() without CONFIG_MCST.  */
u64 path_expect (u64 a, unsigned mul, unsigned shift, int c)
{
	if (__builtin_expect (c, 0))
		return (u64) (((u128) a * mul) >> shift);
	return a;
}

/* A block that calls a cold function.  */
u128 path_coldcall (u128 a, int n, int c)
{
	if (c) {
		warn_once ();
		return a >> n;
	}
	return a;
}

/* A 128-bit multiply in cold code.  */
__attribute__ ((cold)) u128 cold_mul (u128 a, u128 b) { return a * b; }
