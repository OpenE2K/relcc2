/* SPDX-License-Identifier: GPL-3.0-or-later */
/* lcc's default C dialect and the old C that GCC 14 and later reject
   (spec/abi/source-compat.md, "Language standards";
   docs/spec-poc-findings.md A47).  Compile only, one case at a time,
   and read the diagnostics:

     L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc   (or e2k-v5.8c2,
                                       toolchains/lcc-1.31.05-e2k-v3/lcc)
     G=toolchains/poc-cross/cross-0.6+23-v3/gcc-e2k-0.6+23-v3/bin/e2k-linux-gnu-gcc
     for c in IMPLICIT_INT KR_PARM IMPLICIT_DECL INT_CONV INCOMPAT_PTR \
              RETURN_MISMATCH PARM_NO_TYPE BOOL; do
       echo "== $c"
       LC_ALL=C $L -c -D$c tools/abi/probes/folda47_dialect.c -o /dev/null
       $G -c -D$c tools/abi/probes/folda47_dialect.c -o /dev/null
       $G -std=gnu17 -fpermissive -c -D$c tools/abi/probes/folda47_dialect.c \
           -o /dev/null
     done
     $L -dM -E -xc /dev/null | grep -E '__STDC_VERSION__|__STRICT_ANSI__'
     $L -dM -E -xc++ /dev/null | grep -E '__cplusplus|__STRICT_ANSI__'

   lcc 1.29.16 (e2k-v3, e2k-v5.8c2) and 1.31.05 (e2k-v3), 2026-11-03,
   exit status 0 in every case but BOOL:
     IMPLICIT_INT     no diagnostic; with -Wall warning #260 "explicit
                      type is missing ("int" assumed)"
     KR_PARM          warning #1051 "standard requires that parameter
                      "p" be given a type by a subsequent declaration"
     IMPLICIT_DECL    warning #223 "function ... declared implicitly"
                      [-Wimplicit-function-declaration]
     INT_CONV         warning #3311 [-Wint-conversion]
     INCOMPAT_PTR     warning #3309 [-Wincompatible-pointer-types]
     RETURN_MISMATCH  warning #117 "non-void function "f" should return
                      a value"
     PARM_NO_TYPE     warning #92 "identifier-list parameters may only
                      be used in a function definition"
     BOOL             error #20 "identifier "bool" is undefined"
   -dM: __STDC_VERSION__ 201710L, __cplusplus 201703L, no
   __STRICT_ANSI__ (lcc's manual, lang_std.html: -std=gnu18 and
   -std=gnu++17 by default).
   The PoC GCC (17.0.0, 0.6+23; GCC's defaults gnu23 and gnu++20,
   __STDC_VERSION__ 202311L, __cplusplus 202002L): every case but BOOL
   is an error (-Wimplicit-int for IMPLICIT_INT and KR_PARM, the
   others under the option lcc names, PARM_NO_TYPE
   -Wdeclaration-missing-parameter-type); BOOL compiles (a C23
   keyword).  With -std=gnu17 -fpermissive each is a warning and BOOL
   is "unknown type name 'bool'", as with lcc.  */

#if defined IMPLICIT_INT
static x = 1;
f (void) { return x; }
#elif defined KR_PARM
int g (p) { return p; }
#elif defined IMPLICIT_DECL
int f (void) { return undeclared_fn (1); }
#elif defined INT_CONV
int *f (long n) { return n; }
#elif defined INCOMPAT_PTR
int *f (long *p) { return p; }
#elif defined RETURN_MISMATCH
int f (int c) { if (c) return; return 1; }
#elif defined PARM_NO_TYPE
void f (i);
int g (void) { f (1); return 0; }
#elif defined BOOL
bool f (bool c) { return !c; }
#else
int nothing;
#endif
