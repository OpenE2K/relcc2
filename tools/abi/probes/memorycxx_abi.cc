// SPDX-License-Identifier: GPL-3.0-or-later
// The C++ ABI beyond calls (spec/abi/calling-convention.md, "C++ ABI
// beyond calls"): how l++ mangles E2K's types, lays out and calls
// through member pointers, guards function-local statics and writes
// array cookies.  Read with -S and nm; nothing here runs.
//
//   L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/l++   # or 1.31.05's
//   $L -O2 -S tools/abi/probes/memorycxx_abi.cc -o abi.s
//   $L -O2 -c tools/abi/probes/memorycxx_abi.cc -o abi.o
//   e2k-linux-nm abi.o | c++filt         # mangled names
//
// What to read: the names of the m_* functions (types), sizes_*
// (constants it returns), pmf_* and pdm_* (member-pointer constants
// and the call sequence), guard_* (the guard variable's name, size and
// the inline test before __cxa_guard_acquire), cookie_* (new[] and
// delete[]: the cookie's size and where the count is stored), the
// constructors (whether they return this) and the vtable and typeinfo
// symbols emitted.  memorycxx_abi_x.cc runs the same things across
// compilers.
//
// Results of 2026-10-16 (l++ 1.29.16 and 1.31.05 v3: the same names and
// code; the proof-of-concept GCC snap-v04f compared): every mangled
// name agrees (long double and __float80 e, __float128 g, __int128 n,
// vectors Dv<N>_<type>, va_list Pc); l++ makes __float80 the same type
// as long double and rejects _Complex __float128.  Member-function
// pointers are 16 bytes, align 8, Itanium's generic layout (ptr: the
// function or 1 + the vtable offset; adj: the this adjustment in bytes,
// not doubled); data-member pointers 8 bytes, null -1.  Guards are 8
// bytes, byte 0 tested with ldb and a dependent compare, then
// __cxa_guard_acquire/release/abort.  Array cookies are max (8,
// alignof (T)) bytes with the count in the 8 bytes before the array
// (l++ calls __cxa_vec_new and __cxa_vec_delete3, and for a 64-byte
// aligned T operator new[] (size_t, align_val_t)).  Constructors return
// nothing; C2 is an alias of C1.  Static constructors go to .ctors (one
// __sti__ function a file), which ld puts into .init_array.  alignof of
// a 32-byte vector is 32 (the proof-of-concept GCC: 16, from its
// BIGGEST_ALIGNMENT of 128 bits).

#include <stddef.h>
#include <new>
#include <typeinfo>

// ---------------------------------------------------------- mangling
typedef int v2si __attribute__ ((vector_size (8)));
typedef short v4hi __attribute__ ((vector_size (8)));
typedef char v8qi __attribute__ ((vector_size (8)));
typedef float v2sf __attribute__ ((vector_size (8)));
typedef int v4si __attribute__ ((vector_size (16)));
typedef long v2di __attribute__ ((vector_size (16)));
typedef float v4sf __attribute__ ((vector_size (16)));
typedef double v2df __attribute__ ((vector_size (16)));
typedef unsigned char v16qu __attribute__ ((vector_size (16)));
typedef double v4df __attribute__ ((vector_size (32)));

void m_ld (long double) {}
void m_f80 (__float80) {}
void m_f128 (__float128) {}
void m_i128 (__int128) {}
void m_u128 (unsigned __int128) {}
void m_cf (_Complex float) {}
void m_cd (_Complex double) {}
void m_cld (_Complex long double) {}
#ifndef __LCC__          // l++: "_Complex can only be used with
void m_c128 (_Complex __float128) {}   // floating-point types"
#endif
void m_v2si (v2si) {}
void m_v4hi (v4hi) {}
void m_v8qi (v8qi) {}
void m_v2sf (v2sf) {}
void m_v4si (v4si) {}
void m_v2di (v2di) {}
void m_v4sf (v4sf) {}
void m_v2df (v2df) {}
void m_v16qu (v16qu) {}
void m_v4df (v4df) {}
void m_wchar (wchar_t) {}
void m_c16 (char16_t) {}
void m_c32 (char32_t) {}
void m_null (decltype (nullptr)) {}
void m_size (size_t) {}
void m_ptrdiff (ptrdiff_t) {}
void m_va (__builtin_va_list) {}
void m_schar (signed char, unsigned char, char) {}
void m_ldp (long double *, const __float128 &) {}

// ----------------------------------------------------------- sizes
struct A { long a; virtual ~A (); virtual long fa (long); };
struct B { long b; virtual long v1 (long); virtual long v2 (long);
           long nv (long); long x; };
struct D : A, B { long d; long v2 (long) override; long dn (long); };
struct V : virtual B { long w; };

typedef long (B::*pmf_t) (long);
typedef long (D::*pmfd_t) (long);
typedef long B::*pdm_t;

extern "C" long sizes_pmf (void) { return sizeof (pmf_t); }
extern "C" long sizes_pmf_align (void) { return alignof (pmf_t); }
extern "C" long sizes_pdm (void) { return sizeof (pdm_t); }
extern "C" long sizes_ld (void) { return sizeof (long double) * 100
                                  + alignof (long double); }
extern "C" long sizes_f128 (void) { return sizeof (__float128) * 100
                                    + alignof (__float128); }
extern "C" long sizes_v4df (void) { return sizeof (v4df) * 100
                                    + alignof (v4df); }
extern "C" long sizes_maxalign (void)
{ return sizeof (max_align_t) * 100 + alignof (max_align_t); }
extern "C" long sizes_newalign (void)
{ return __STDCPP_DEFAULT_NEW_ALIGNMENT__; }
extern "C" long sizes_wchar (void) { return sizeof (wchar_t) * 100
                                     + ((wchar_t) -1 < 0); }

// ---------------------------------------------------- member pointers
// Constants: a non-virtual, a virtual (second slot), a virtual in a
// second base, null; written to memory so the two words show.
extern "C" void pmf_nv (pmf_t *p) { *p = &B::nv; }
extern "C" void pmf_v1 (pmf_t *p) { *p = &B::v1; }
extern "C" void pmf_v2 (pmf_t *p) { *p = &B::v2; }
extern "C" void pmf_null (pmf_t *p) { *p = nullptr; }
extern "C" void pmf_d_v2 (pmfd_t *p) { *p = &D::v2; }      // D's own slot
extern "C" void pmf_d_bv1 (pmfd_t *p) { *p = &D::v1; }     // B's, adjusted
extern "C" void pmf_d_dn (pmfd_t *p) { *p = &D::dn; }
extern "C" void pmf_d_fa (pmfd_t *p) { *p = &D::fa; }      // A's (first base)
extern "C" void pdm_x (pdm_t *p) { *p = &B::x; }
extern "C" void pdm_null (pdm_t *p) { *p = nullptr; }
// The call sequence: the virtual test and the this adjustment.
extern "C" long pmf_call (B *o, pmf_t f, long a) { return (o->*f) (a); }
extern "C" long pmfd_call (D *o, pmfd_t f, long a) { return (o->*f) (a); }
extern "C" int pmf_isnull (pmf_t f) { return f == nullptr; }
extern "C" int pmf_eq (pmf_t f, pmf_t g) { return f == g; }
extern "C" long pdm_read (B *o, pdm_t m) { return o->*m; }
// Conversion from the derived class's member pointer to the base's.
extern "C" void pmf_conv (pmfd_t *d, pmf_t f) { *d = f; }

// ------------------------------------------------------------ guards
struct G { G (); ~G (); long v; };
long compute (void);
extern "C" G *guard_obj (void) { static G g; return &g; }
extern "C" long guard_long (void) { static long x = compute (); return x; }
inline long &guard_inline (void) { static long y = compute (); return y; }
extern "C" long guard_inline_use (void) { return guard_inline (); }
extern "C" long guard_tls (void)
{ thread_local long t = compute (); return t; }
// A static with a trivial destructor but a dynamic initializer, at
// namespace scope (no guard: the TU's init function) and as a template
// static member (a guard, comdat).
long ns_init = compute ();
template <class T> struct TS { static long s; };
template <class T> long TS<T>::s = compute ();
extern "C" long guard_tmpl (void) { return TS<int>::s; }

// ----------------------------------------------------------- cookies
struct C { long v; ~C (); };
struct C1 { char v; ~C1 (); };
struct alignas (16) C16 { long v; ~C16 (); };
struct alignas (64) C64 { long v; ~C64 (); };
struct T { long v; };                 // trivial: no cookie
extern "C" C *cookie_new (long n) { return new C[n]; }
extern "C" C1 *cookie_new1 (long n) { return new C1[n]; }
extern "C" C16 *cookie_new16 (long n) { return new C16[n]; }
extern "C" C64 *cookie_new64 (long n) { return new C64[n]; }
extern "C" T *cookie_newt (long n) { return new T[n]; }
extern "C" void cookie_del (C *p) { delete[] p; }
extern "C" void cookie_del16 (C16 *p) { delete[] p; }
extern "C" void cookie_del64 (C64 *p) { delete[] p; }
// Placement new[] with a cookie (the count goes before the buffer).
extern "C" C *cookie_place (void *buf, long n) { return new (buf) C[n]; }

// ---------------------------------------- constructors, vtables, RTTI
struct K { K (long); ~K (); virtual long key (long); virtual long inl (long)
           { return 1; } long k; };
K::K (long a) : k (a) {}
K::~K () {}
long K::key (long a) { return a + k; }     // key function: vtable here
extern "C" K *ctor_call (void *p) { return new (p) K (5); }
extern "C" const void *rtti_ld (void) { return &typeid (long double); }
extern "C" const void *rtti_f128 (void) { return &typeid (__float128); }
extern "C" const void *rtti_i128 (void) { return &typeid (__int128); }
extern "C" const void *rtti_v4si (void) { return &typeid (v4si); }
extern "C" int rtti_name_cmp (const std::type_info &a,
                              const std::type_info &b) { return a == b; }

// Static destructors: __cxa_atexit or atexit, and __dso_handle.
struct S2 { ~S2 (); };
S2 s2;

// The key function: K2's first non-inline virtual function is defined
// in another file, so this one must only reference _ZTV2K2.
struct K2 { K2 () {} virtual long k2 (long); virtual long i2 (long)
            { return 2; } };
extern "C" K2 *key_other (void *p) { return new (p) K2; }
// Callers of an out-of-line constructor: whether they take `this' back
// from the callee's result register (ARM's rule) or keep their own.
extern "C" long ctor_result (void) { G g; return g.v; }
extern "C" G *ctor_new (void) { return new G; }
// D's key function is here: its vtable and the thunk for B-in-D.
long D::v2 (long a) { return a + d; }
// A virtual base: the offset read from the vtable.
extern "C" long vbase_read (V *v) { return v->x; }
// thread_local across files: the wrapper (_ZTW) and init (_ZTH) names.
extern thread_local long etl;
extern "C" long tls_wrap (void) { return etl; }
thread_local long dtl = compute ();
