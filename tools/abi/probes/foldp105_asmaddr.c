/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * foldp105_asmaddr.c: how an "m" asm operand prints when MCST's asm text
 * puts it right after an operation's channel digit, with no space
 * (spec/abi/source-compat.md, "Other constraints").  MCST's kernel does
 * that in NATIVE_ATOMIC_FETCH_XCHG_UNLESS_INC
 * ("{st"#mem_size_letter "," mem_model##_ATOMIC_CHANNEL "%[addr], ...",
 * arch/e2k/include/asm/e2k_api.h:4880-4881; the channel is "5" with
 * CONFIG_CPU_ISET_MIN >= 5, else "2"), which asm/sclkr.h:82-83 applies
 * to the global prev_sclkr (arch/e2k/kernel/sclkr.c,
 * arch/e2k/fast_syscalls/sclkr.c), and in USER_ATOMIC_FETCH_OP
 * (e2k_api.h:6096-6097, a user pointer).  gas reads every digit after
 * the comma as the channel, so the text after it must not start with a
 * digit.
 *
 * Run tools/abi/probes/foldp105_asmaddr.sh: it compiles this file with
 * lcc -S, assembles lcc's output with MCST gas, and assembles the
 * literal-first forms glued and spaced.  Nothing runs.
 */

struct prev { long counter; long pad[7]; } prev_sclkr;
long *ptr;

/* A global's address, as asm/sclkr.h passes &prev_sclkr.res.counter.  */
void glob (long v)
{
	__asm__ ("{std,5%[addr], %[v]}"
		 : [addr] "+m" (prev_sclkr.counter) : [v] "r" (v));
}

/* A global plus an offset.  */
void glob_off (long v)
{
	__asm__ ("{std,5%[addr], %[v]}"
		 : [addr] "+m" (prev_sclkr.pad[3]) : [v] "r" (v));
}

/* Through a pointer, as USER_ATOMIC_FETCH_OP.  */
void viaptr (long v)
{
	__asm__ ("{std,5%[addr], %[v]}" : [addr] "+m" (ptr[2]) : [v] "r" (v));
}

/* A load, channel 0.  */
long load (void)
{
	long r;
	__asm__ ("{ldd,0%[addr], %[r]}"
		 : [r] "=r" (r) : [addr] "m" (prev_sclkr.counter));
	return r;
}
