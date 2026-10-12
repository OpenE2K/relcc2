/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Data layout of 32-bit mode (lcc -mptr32, spec/abi/mode32.md). Every
   assertion encodes a rule of that document; a compiler that disagrees
   fails to compile this file:
     /opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc -mptr32 -c layout32.c  */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>
#include <wchar.h>

#define LAYOUT(T, size, align) \
	_Static_assert(sizeof(T) == (size), "sizeof(" #T ")"); \
	_Static_assert(_Alignof(T) == (align), "alignof(" #T ")")

#ifndef __ptr32__
#error "build with -mptr32"
#endif

/* ILP32: int, long and pointers are 4 bytes.  */
LAYOUT(_Bool, 1, 1);
LAYOUT(char, 1, 1);
LAYOUT(short, 2, 2);
LAYOUT(int, 4, 4);
LAYOUT(long, 4, 4);
LAYOUT(void *, 4, 4);
LAYOUT(void (*)(void), 4, 4);
LAYOUT(size_t, 4, 4);
LAYOUT(ptrdiff_t, 4, 4);
LAYOUT(intptr_t, 4, 4);
LAYOUT(va_list, 4, 4);

/* Unlike i386, 8- and 16-byte types keep their natural alignment.  */
LAYOUT(long long, 8, 8);
LAYOUT(double, 8, 8);
LAYOUT(__int128, 16, 16);
LAYOUT(long double, 16, 16);
LAYOUT(__float128, 16, 16);
LAYOUT(_Complex float, 8, 4);
LAYOUT(_Complex double, 16, 8);
LAYOUT(max_align_t, 32, 16);
struct cll { char c; long long ll; };
LAYOUT(struct cll, 16, 8);
_Static_assert(offsetof(struct cll, ll) == 8, "cll.ll");
struct cld { char c; long double ld; };
LAYOUT(struct cld, 32, 16);

LAYOUT(wchar_t, 4, 4);
_Static_assert((char)-1 < 0, "char is signed");
_Static_assert((wchar_t)-1 < 0, "wchar_t is signed");

/* A long bit-field has a 4-byte container.  */
struct bfl { long a : 3; char c; };
LAYOUT(struct bfl, 4, 4);
struct bfp { char c; long a : 30; };
LAYOUT(struct bfp, 8, 4);

/* Pointers in aggregates: 4 bytes each.  */
struct pp { void *a, *b; };
LAYOUT(struct pp, 8, 4);

/* glibc's types: 32-bit time_t and off_t unless _TIME_BITS=64 or
   _FILE_OFFSET_BITS=64 is defined.  */
#if !defined _FILE_OFFSET_BITS && !defined _TIME_BITS
LAYOUT(time_t, 4, 4);
LAYOUT(off_t, 4, 4);
LAYOUT(struct timespec, 8, 4);
#endif
