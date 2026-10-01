/* Data layout probes for e2k64 (64-bit mode). Every assertion encodes a
   rule from spec/abi/data-layout.md; a reference compiler that disagrees
   fails to compile this file.  */

#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#define LAYOUT(T, size, align) \
	_Static_assert(sizeof(T) == (size), "sizeof(" #T ")"); \
	_Static_assert(_Alignof(T) == (align), "alignof(" #T ")")

LAYOUT(_Bool, 1, 1);
LAYOUT(char, 1, 1);
LAYOUT(short, 2, 2);
LAYOUT(int, 4, 4);
LAYOUT(long, 8, 8);
LAYOUT(long long, 8, 8);
LAYOUT(__int128, 16, 16);
LAYOUT(void *, 8, 8);
LAYOUT(float, 4, 4);
LAYOUT(double, 8, 8);
LAYOUT(long double, 16, 16);
LAYOUT(__float128, 16, 16);
LAYOUT(_Complex float, 8, 4);
LAYOUT(_Complex double, 16, 8);
LAYOUT(size_t, 8, 8);
LAYOUT(wchar_t, 4, 4);
LAYOUT(max_align_t, 32, 16);

/* char is signed; wchar_t is signed int.  */
_Static_assert((char)-1 < 0, "char is signed");
_Static_assert((wchar_t)-1 < 0, "wchar_t is signed");

/* Aggregates: guide figures D-3..D-11.  */
struct d6 { char c; double d; short s; };
LAYOUT(struct d6, 24, 8);
_Static_assert(offsetof(struct d6, s) == 16, "d6.s");

union d8 { char c[3]; short s; };
LAYOUT(union d8, 4, 2);

/* Bit-fields fill containers from the least significant bit.  */
struct bf1 { unsigned a : 3, b : 5, c : 24; };
LAYOUT(struct bf1, 4, 4);

struct bf2 { short s : 9; int j : 9; char c; short t : 9; short u : 9; char d; };
LAYOUT(struct bf2, 12, 4);

struct bf3 { char c; int : 0; char d; short : 9; char e; char : 0; };
LAYOUT(struct bf3, 9, 1);
_Static_assert(offsetof(struct bf3, d) == 4, "bf3.d");
_Static_assert(offsetof(struct bf3, e) == 8, "bf3.e");
