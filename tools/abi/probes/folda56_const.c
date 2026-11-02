/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Integer constant expressions lcc takes in C and GCC doesn't: a
   statement expression of constants, and a const variable with a
   constant initializer (spec/abi/source-compat.md; docs/spec-poc-
   findings.md A56).  MCST's qemu 7.2.0 sizes initialized buffers in
   target/e2k/boot with qemu's MAX, a statement expression since 7.2
   (BOOT_STRING_SIZE); MCST's kexec-e2k 1.0.1 writes `const size_t
   alignment = 4096;' and __attribute__ ((aligned (alignment))).
   Compile only:

     L=/opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1/bin/lcc    (or e2k-v3)
     $L -O2 -Wall -S folda56_const.c -o - | grep -E '^[a-z_]+:|addd|cmpe|scld|\.align|\.size'
     $L -O2 -c -DSE_FILE_SCOPE folda56_const.c     # also -DVLA_INIT
     gcc -O2 -c folda56_const.c                    # for comparison

   lcc 1.29.16 (v3, v5.8c2) and 1.31.05 (v3), 2026-11-03, no
   diagnostic (also with -std=c11 -pedantic-errors or -std=c2x):
   - se_array returns 0xaa (sizeof 62 + 'l': the array is not a VLA, so
     it may be initialized), se_case compares with 2, se_static's s is 4
     bytes in .bss, se_assert compiles.
   - cv_case compares with 0x1000, aligned_obj is `.align 4096', buf
     (file scope) is 0x1000 bytes, cv_block returns 5 (a[n] initialized:
     not a VLA), cv_enum returns 4096.
   - -DSE_FILE_SCOPE: error #1166 "statement expressions are only
     allowed in block scope"; -DVLA_INIT (a real VLA): error #145
     "variable "a" may not be initialized".
   GCC (x86-64 16.2.1 and the proof-of-concept 0.6+33, also -std=gnu17)
   rejects every case but cv_case: "variable-sized object may not be
   initialized except with an empty initializer" (se_array, cv_block),
   "case label does not reduce to an integer constant", "storage size
   of 's' isn't constant", "expression in static assertion is not
   constant", "requested alignment is not an integer constant",
   "variably modified 'buf' at file scope", "enumerator value for 'E'
   is not an integer constant".  cv_case is a GCC extension (-pedantic:
   "case label is not an integer constant expression").  */

#include <stddef.h>

/* qemu 7.2's MAX (include/qemu/osdep.h).  */
#define MAX(a, b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); \
                     _a > _b ? _a : _b; })

int se_array (void)
{
  char label[MAX (30, 20) + 32] = "label=";
  return sizeof (label) + label[0];
}

int se_case (int x)
{
  switch (x)
    {
    case MAX (1, 2): return 10;
    default: return 0;
    }
}

char *se_static (void)
{
  static char s[MAX (3, 4)];
  return s;
}

int se_assert (void)
{
  _Static_assert (MAX (3, 4) == 4, "max");
  return 0;
}

#ifdef SE_FILE_SCOPE
char g[MAX (3, 4)];
#endif

#ifdef VLA_INIT
int vla (int n) { char a[n] = "abc"; return a[0]; }
#endif

/* kexec-e2k's constant.  */
const size_t alignment = 4096;
struct big { char c[100]; } aligned_obj __attribute__ ((aligned (alignment)));
static char buf[alignment];
char *get_buf (void) { return buf; }

int cv_case (int x)
{
  switch (x)
    {
    case alignment: return 1;
    default: return 0;
    }
}

int cv_block (void)
{
  const int n = 5;
  char a[n] = "abc";
  return sizeof (a) + 0 * a[0];
}

enum { E = alignment };
int cv_enum (void) { return E; }
