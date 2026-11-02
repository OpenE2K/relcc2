/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Size and alignment of GNU vectors beyond 256 bytes (spec/abi/
   data-layout.md, "Scalar types", the GNU vectors row;
   docs/spec-poc-findings.md A35).  Read with -S, nothing runs:

     L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc   (or e2k-v5.8c2,
                                                         or lcc 1.31.05)
     $L -O2 -S folda35_vec.c -o - | awk '/^\t\.align/ { a = $2 }
         /^[a-z_0-9]+:/ { n = $1 } /\.uaword/ && n { print n, $2; n = "" }
         /^g_/ { print $1, ".align", a }'
     $L -O2 -S -DTOOBIG folda35_vec.c -o /dev/null

   Each word below is named after what it holds (size, _Alignof,
   __alignof__, the member's offset in struct { char c; v x; } and that
   struct's _Alignof) for a char vector of 1, 32, 64, 512 bytes, 2^28,
   2^29 and 2^31 bytes, and an int vector of 2^31 bytes; g_* are objects
   whose .align shows the alignment lcc gives them.  -DTOOBIG adds a
   vector of 2^32 bytes.

   lcc 1.29.16 (e2k-v3, e2k-v5.8c2) and 1.31.05 (e2k-v3, e2k-v5.8c2),
   2026-10-24, all the same: every word is N, every g_* has .align N,
   up to 2^31 bytes; 2^32 is "error #1684: vector size is too large".
   A non-power-of-two number of elements is error #1685, a size that is
   not a multiple of the element error #1686.  The proof-of-concept GCC
   (build of 2026-10-23, with its 41099d2c29c) gives N up to 2^28 bytes,
   then 2^28 (default_vector_alignment caps the layout at ELF's
   MAX_OFILE_ALIGNMENT, 2^28 bytes), and refuses the 2^31-byte char
   vector ("number of vector components 2147483648 exceeds
   2147483646").  */

#define VEC(name, T, n)							\
  typedef T name __attribute__ ((vector_size (n)));			\
  struct s_##name { char c; name x; };					\
  int size_##name = sizeof (name);					\
  int alignof_##name = _Alignof (name);					\
  int gnualign_##name = __alignof__ (name);				\
  int offset_##name = __builtin_offsetof (struct s_##name, x);		\
  int salign_##name = _Alignof (struct s_##name);			\
  name g_##name;

VEC (v1, char, 1)
VEC (v32, char, 32)
VEC (v64, char, 64)
VEC (v512, char, 512)
VEC (v256m, char, 1 << 28)
VEC (v512m, char, 1 << 29)
VEC (v2g, char, 1UL << 31)
VEC (vi2g, int, 1UL << 31)

#ifdef TOOBIG
VEC (v4g, char, 1UL << 32)
#endif
