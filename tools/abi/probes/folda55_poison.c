/* SPDX-License-Identifier: GPL-3.0-or-later */
/* #pragma GCC poison and GCC's other preprocessor pragmas under lcc
   (spec/abi/source-compat.md, "Pragmas"; docs/spec-poc-findings.md
   A55).  MCST's qemu 7.2.0 tests #ifdef TARGET_E2K in hw/boards.h and
   #ifdef HOST_WORDS_BIGENDIAN in its E2K chipset header, names qemu
   poisons in target-independent code (exec/poison.h).  Compile only:

     L=/opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1/bin/lcc  (any target; l++)
     $L -O2 -Wall -Wunknown-pragmas -Werror -c folda55_poison.c -o p.o; nm p.o
     $L -E folda55_poison.c | grep pragma
     $L -Wall -c -DWARN folda55_poison.c -o /dev/null   # also -DERROR, -DDEP
     gcc -c folda55_poison.c                              # for comparison

   lcc 1.29.16 (v3, v5.8c2; lcc and l++) and 1.31.05 (v3), 2026-11-03:
   - poison: no message at all, also under -Wall -Wunknown-pragmas
     -Werror.  BEFORE (a macro defined before the pragma) stays defined
     (before_defined is in nm), AFTER can still be defined and used
     (AFTER_use is 2), and poisonedvar, a variable declared before the
     pragma, is still read in f.  #ifdef and defined () are plain tests.
     lcc -E passes the pragma through unchanged.
   - -DWARN, -DERROR, -DDEP (#pragma GCC warning, error, dependency):
     warning #1675 "unrecognized GCC pragma [-Wunknown-pragmas]", also
     without -Wall, and nothing else; GCC error doesn't stop the compile
     (exit 0 without -Werror).
   - GCC (x86-64 16.2.1 and the proof-of-concept 0.6+33): "attempt to
     use poisoned" at every use after the pragma, #ifdef and defined ()
     included, plus "poisoning existing macro 'BEFORE'" (a warning).  */

#define BEFORE 1
int poisonedvar;
#pragma GCC poison BEFORE AFTER poisonedvar

#ifdef BEFORE
int before_defined = 1;
#endif
#if defined (AFTER)
int after_defined = 1;
#endif
#define AFTER 2
int AFTER_use = AFTER;
int f (void) { return poisonedvar; }

#ifdef WARN
#pragma GCC warning "a GCC warning pragma"
#endif
#ifdef ERROR
#pragma GCC error "a GCC error pragma"
#endif
#ifdef DEP
#pragma GCC dependency "folda55_no_such_file.h"
#endif
