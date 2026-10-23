/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The names lcc's __builtin_cpu_is takes and the word of __cpu_info
   each one compares (spec/abi/source-compat.md, "CPU detection at run
   time"; docs/spec-poc-findings.md A33).  Read with -S, nothing runs:

     L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $L -O2 -S folda33_cpuis.c -o - | grep -E '^[a-z_0-9]+:|__cpu_info|cmpesb'
     $L -O2 -S -DBAD='"elbrus-2c"' folda33_cpuis.c -o /dev/null

   Each function is one "ldw [ _f64,_lts1 __cpu_info +K ]" and one
   "cmpesb %g16, CODE": K 0 for the ISA names (elbrus-vN, CODE N), K 4
   for the CPU names (CODE lcc's CPU code).  -DBAD=NAME adds one more
   call with NAME; lcc then stops with 'unknown argument NAME for
   "__builtin_cpu_is"'.

   lcc 1.29.16 (e2k-v3) and 1.31.05 (e2k-v3), 2026-10-24, the same:
   "elbrus" is +4 == 1 and "elbrus-v1" +0 == 1 (both in the manual,
   builtin_mcst.html; "elbrus" is the v1 CPU), the rest as in the table
   in source-compat.md.  Rejected: "elbrus-2c", "elbrus-3m1",
   "elbrus-1c", "elbrus-3s", "elbrus-e2s", "elbrus-v8", "Elbrus" and
   glibc's "elbrus-maket32c" (ldsodefs.h asks for it only under
   __MCST_SUPPORT_ELBRUS_MAKET32C__, which neither lcc predefines:
   lcc -dM -E -x c /dev/null; with it defined by hand the name is
   rejected too).  */

#define IS(fn, name) int fn (void) { return __builtin_cpu_is (name); }

IS (is_elbrus, "elbrus")
IS (is_s, "elbrus-s")
IS (is_2cp, "elbrus-2c+")
IS (is_4c, "elbrus-4c")
IS (is_2cm, "elbrus-2cm")
IS (is_8c, "elbrus-8c")
IS (is_1cp, "elbrus-1c+")
IS (is_8c2, "elbrus-8c2")
IS (is_12c, "elbrus-12c")
IS (is_16c, "elbrus-16c")
IS (is_2c3, "elbrus-2c3")
IS (is_48c, "elbrus-48c")
IS (is_8v7, "elbrus-8v7")

IS (is_v1, "elbrus-v1")
IS (is_v2, "elbrus-v2")
IS (is_v3, "elbrus-v3")
IS (is_v4, "elbrus-v4")
IS (is_v5, "elbrus-v5")
IS (is_v6, "elbrus-v6")
IS (is_v7, "elbrus-v7")

#ifdef BAD
IS (is_bad, BAD)
#endif
