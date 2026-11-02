/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * folda40_zerobss.c: where a compiler puts an object explicitly
 * initialized to zero (spec/abi/source-compat.md; kernel-build.md).
 * MCST's kernel writes some such variables in p2v's boot_startup
 * (boot_parse_param, boot_setup_serial_console: serial_dump_console_num,
 * arch/l/kernel/console.c:47; boot_cpucount, p2v/boot_head.c:49) before
 * boot_init -> boot_setup clears .bss (p2v/boot_head.c:452, 644, 678-679,
 * 710), which works only if they are in .data (the proof-of-concept back
 * end's finding A40).
 *
 * Compile only and list the symbols (D/d .data, B/b .bss, R .rodata,
 * C common):
 *
 *   L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc   (also v5.8c2,
 *                                 v6.2c3, toolchains/lcc-1.31.05-e2k-v3/lcc)
 *   $L -O2 -c tools/abi/probes/folda40_zerobss.c -o z.o && nm z.o
 *   (also -O0, -O3 -fkernel, -fno-zero-initialized-in-bss; l++ -x c++;
 *   -DTLS adds __thread objects: readelf -s, .tdata or .tbss)
 *   gcc -O2 -c ...; e2k-linux-gnu-gcc -O2 -c ...
 *
 * 2026-11-03: lcc 1.29.16 (v3, v5.8c2, v6.2c3 at -O0, -O2 and -O3
 * -fkernel; l++ -O2) and 1.31.05 (v3, v5.8c2 at -O3) put every `= 0`,
 * `= { 0 }`, `= { }`, `= 0.0`, null pointer and zero _Bool object in
 * .data (D, d; function-scope statics too), and `__thread ... = 0` in
 * .tdata; uninitialized ones in .bss (C with -fcommon), const ones in
 * .rodata.  -fzero-initialized-in-bss and -fno-zero-initialized-in-bss
 * are accepted silently and change nothing.  GCC (host 16.2.1 x86-64, the
 * proof-of-concept E2K back end) puts the zero ones in .bss and .tbss,
 * and with -fno-zero-initialized-in-bss exactly where lcc does.  -0.0
 * and 1 are .data for both.
 */

#ifdef __cplusplus
# define _Bool bool
#endif

int g_zero = 0;
static int s_zero = 0;
int g_un;
static int s_un;
int g_one = 1;
long g_arr[64] = { 0 };
struct st { int a, b; } g_st = { 0 };
struct st g_st2 = { };
char *g_ptr = 0;
double g_d = 0.0;
double g_dm = -0.0;
_Bool g_b = 0;
const int c_zero = 0;
#ifdef TLS
__thread int t_zero = 0;
__thread int t_un;
#endif

int *
f (void)
{
  static int l_zero = 0;
  static int l_un;
  return &l_zero + (&l_un - &s_zero) + (&s_un - &s_zero);
}
