/* SPDX-License-Identifier: GPL-3.0-or-later */
/* What lcc makes of -mtune=native and -march=native (spec/abi/
   source-compat.md, the CPU list under "Predefined macros";
   docs/spec-poc-findings.md A54).  modem-manager-gui's Makefile_h
   passes -mtune=native, mumps's CMake -mtune=native for GNU Fortran.
   Compile only; `which' records __iset__ and the CPU macro, readelf -h
   the e_flags:

     for t in v3 v3.4c v4.8c v5.8c2 v6.2c3; do
       L=/opt/mcst/lcc-1.29.16.e2k-$t.linux-6.1/bin/lcc
       for o in "" -mtune=native -march=native \
                "-march=elbrus-v6 -march=native" "-march=elbrus-v6 -mtune=native"; do
         $L -O3 -c folda54_native.c -o n.o $o
         echo "$t [$o] $(readelf -h n.o | grep Flags) $(strings n.o | grep ^iset)"
       done
     done
     (also toolchains/lcc-1.31.05-e2k-{v3,v4.8c,v5.8c2,v6.2c3}/lcc; on the
     4C and 8C2 the same with the machine's own lcc; -mtune=foo prints
     the valid arguments)

   lcc 1.29.16 and 1.31.05, 2026-11-03:
   - Both options take native, first in the list a wrong argument
     prints ("Valid arguments are: native, elbrus-2c+, elbrus-4c, ...";
     1.31.05 without elbrus-2c+ and elbrus-v2).  -mcpu=native is
     "unrecognized command line option".
   - Cross compilers: native is the compiler's default, as the manual
     says (lcc_options.htm).  Alone it changes nothing (the same
     assembly and e_flags: e2k-v3 0x3000000 "iset 3 cpu none", e2k-v5.8c2
     0x5000010 "iset 5 cpu elbrus-8c2").  It replaces an earlier value:
     "-march=elbrus-v6 -march=native" on e2k-v3 is v3; and -mtune wins
     over -march, so "-march=elbrus-v6 -mtune=native" (either order) is
     the default too (v3 on e2k-v3, elbrus-8c2 on e2k-v5.8c2).
   - Native lcc on the 4C (ALT, e2k-v3-linux): -mtune=native is
     -mtune=elbrus-4c (identical assembly; 0x3000010, __elbrus_4c__; the
     driver passes --mcpu-internal=elbrus-4c and as -mcpu=elbrus-4c),
     -march=native is elbrus-v3 (0x3000000), also after
     -march=elbrus-v5.  The same through ALT's /bin/lcc, a wrapper
     (gcc-11_lcc1.29-alt1) that runs /opt/mcst/bin/lcc -O3
     -march=elbrus-v3 -gdwarf32 -DLCC_BUG_148755 before the user's
     options, and with /opt/mcst/bin/lcc itself.
   - Native lcc on the 8C2 (OS Elbrus, e2k-8c2-linux, whose default is
     elbrus-8c2): -mtune=native is elbrus-8c2 (0x5000010), -march=native
     elbrus-v5 without a CPU (0x5000000, no __elbrus_8c2__; the driver
     passes --mcpu-internal=elbrus-v5): the machine's ISA, not the
     compiler's default.
   - strace on the 4C: -mtune=native opens no file the plain compile
     doesn't (no /proc/cpuinfo).  */

#define S2(x) #x
#define S(x) S2(x)
#if defined __elbrus_4c__
# define CPU "elbrus-4c"
#elif defined __elbrus_8c__
# define CPU "elbrus-8c"
#elif defined __elbrus_1cplus__
# define CPU "elbrus-1c+"
#elif defined __elbrus_8c2__
# define CPU "elbrus-8c2"
#elif defined __elbrus_12c__
# define CPU "elbrus-12c"
#elif defined __elbrus_16c__
# define CPU "elbrus-16c"
#elif defined __elbrus_2c3__
# define CPU "elbrus-2c3"
#elif defined __elbrus_48c__
# define CPU "elbrus-48c"
#elif defined __elbrus_8v7__
# define CPU "elbrus-8v7"
#else
# define CPU "none"
#endif

const char which[] = "iset " S (__iset__) " cpu " CPU;

/* Some code whose schedule depends on the target.  */
double dot (const double *a, const double *b, int n)
{
  double s = 0;
  for (int i = 0; i < n; i++)
    s += a[i] * b[i];
  return s;
}
