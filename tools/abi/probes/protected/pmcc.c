/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Protected-mode (e2k128) code generation probes: compile to assembly
   and read where lcc passes arguments and results, how it makes and uses
   pointers, globals and function addresses:

     /opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc -mptr128 -O1 \
         -fno-optimize-sibling-calls -S pmcc.c

   (or toolchains/lcc-1.29.16-e2k-v5/lcc where /opt/mcst is absent).
   Functions named c_* are callers, d_* are callees, r_* return
   aggregates, g_* touch globals, p_* are pointer operations, l_* take
   addresses of locals, t_* use thread-local storage, a_* atomics and
   i_* intptr_t (compare lcc 1.31.05, where intptr_t is 16 bytes).
   Nothing here is run; pmrun.c runs.  */

#include <stdarg.h>
#include <stddef.h>
#include <string.h>

struct sp { int *p; int x; };		/* 32 bytes: pointer + int */
struct s8 { int a, b; };		/* 8 bytes, no pointer */
struct s24 { long a, b, c; };
struct s64p { int *p[4]; };		/* 64 bytes of pointers */
struct s80 { long a[10]; };

extern int ext_var;			/* defined elsewhere */
extern int ext_fn (int);
int glob_arr[10];
static int stat_arr[10];
int *glob_ptr = &glob_arr[2];		/* initialised pointer: relocation */
int (*glob_fp) (int) = ext_fn;		/* initialised function pointer */
const char *glob_str = "hello";

/* Argument slots for integers and pointers.  */
long d_ints (int a, long b, char c, short d, unsigned e)
{
	return a + b + c + d + e;
}
long d_ptrs (int *a, int b, int *c, long d)
{
	return *a + b + *c + d;
}
long d_many (int *a1, int *a2, int *a3, int *a4, int *a5, int *a6,
	     int *a7, int *a8)
{
	return *a1 + *a6 + *a7 + *a8;
}
long d_many_ints (long a1, long a2, long a3, long a4, long a5, long a6,
		  long a7, long a8, long a9, long a10)
{
	return a1 + a9 * 3 + a10 * 5;
}
double d_fp (float a, double b, long double c, int *p)
{
	return a + b + (double) c + *p;
}
long c_calls (int *p)
{
	return d_ints (1, 2, 3, 4, 5) + d_ptrs (p, 6, p, 7)
		+ d_many (p, p, p, p, p, p, p, p)
		+ d_many_ints (1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
}

/* Aggregates as arguments and results.  */
long d_sp (struct sp s) { return *s.p + s.x; }
long d_s8 (struct s8 s) { return s.a + s.b; }
long d_s24 (struct s24 s) { return s.a + s.c; }
struct sp r_sp (int *p) { struct sp s = { p, 5 }; return s; }
struct s8 r_s8 (int a) { struct s8 s = { a, a + 1 }; return s; }
struct s64p r_s64p (int *p) { struct s64p s = { { p, p, p, p } }; return s; }
struct s80 r_s80 (long a) { struct s80 s = { { a, 0, 0, 0, 0, 0, 0, 0, 0, a } }; return s; }
long c_aggr (int *p)
{
	struct sp s = r_sp (p);
	struct s8 t = r_s8 (3);
	struct s24 u = { 1, 2, 3 };
	struct s80 v = r_s80 (4);
	return d_sp (s) + d_s8 (t) + d_s24 (u) + r_s64p (p).p[3][0] + v.a[9];
}

/* Variadic functions.  */
long d_va (int n, ...)
{
	va_list ap;
	long s = 0;
	va_start (ap, n);
	for (int i = 0; i < n; i++) {
		int *p = va_arg (ap, int *);
		s += *p + va_arg (ap, int);
	}
	va_end (ap);
	return s;
}
double d_va_fp (int n, ...)
{
	va_list ap;
	double s = 0;
	va_start (ap, n);
	s = va_arg (ap, double) + va_arg (ap, long double);
	va_end (ap);
	return s;
}
long c_va (int *p)
{
	return d_va (2, p, 1, p, 2) + (long) d_va_fp (2, 1.5, (long double) 2.5);
}

/* Unprototyped call.  */
long unproto ();
long c_unproto (int *p) { return unproto (p, 1); }

/* Function pointers: address of a function, call through a pointer.  */
long d_fp_call (int (*f) (int)) { return f (3); }
int (*p_fnaddr (void)) (int) { return ext_fn; }
long c_ext (void) { return ext_fn (4); }

/* Globals: load, store, address, extern, static.  */
int g_load (void) { return glob_arr[3]; }
void g_store (int v) { glob_arr[4] = v; stat_arr[1] = v; }
int *g_addr (void) { return &glob_arr[5]; }
int *g_saddr (void) { return &stat_arr[1]; }
int g_ext (void) { return ext_var; }
int *g_extaddr (void) { return &ext_var; }
int g_ptr_load (void) { return *glob_ptr; }
const char *g_str (void) { return "literal"; }

/* Pointer operations.  */
int *p_add (int *p, long i) { return p + i; }
int *p_index_int (int *p, int i) { return &p[i]; }
long p_diff (int *p, int *q) { return p - q; }
int p_lt (int *p, int *q) { return p < q; }
int p_eq (int *p, int *q) { return p == q; }
int p_null (int *p) { return p == NULL; }
long p_to_long (int *p) { return (long) p; }
int *p_from_long (long x) { return (int *) x; }
int p_load (int *p, long i) { return p[i]; }
void p_store (int *p, long i, int v) { p[i] = v; }
int *p_load_ptr (int **pp) { return *pp; }
void p_store_ptr (int **pp, int *p) { *pp = p; }
long p_load_long (long *p) { return *p; }
char p_load_char (char *p) { return p[1]; }

/* Locals: address of a local, sub-object narrowing, local array.  */
struct two { int a[4]; int b; };
long use (void *);
long l_local (void) { int x = 1; return use (&x); }
long l_sub (void) { struct two t; t.b = 1; return use (t.a); }
long l_member (struct two *t) { return use (&t->b); }
long l_arr (void) { int a[8] = { 0 }; return use (a); }
long l_vla (int n) { int a[n]; a[0] = 1; return use (a); }
long l_alloca (int n) { char *a = __builtin_alloca (n); return use (a); }

/* Library calls the compiler expands.  */
void m_cpy (struct s64p *d, const struct s64p *s) { *d = *s; }
void m_memcpy (void *d, const void *s, size_t n) { memcpy (d, s, n); }

/* Result sizes: where do 112-, 128-, 144- and 256-byte results go?  */
struct s112 { long a[14]; };
struct s128 { long a[16]; };
struct s144 { long a[18]; };
struct s256p { int *p[16]; };
struct s112 r_s112 (long a) { struct s112 s = { { a, [13] = a } }; return s; }
struct s128 r_s128 (long a) { struct s128 s = { { a, [15] = a } }; return s; }
struct s144 r_s144 (long a) { struct s144 s = { { a, [17] = a } }; return s; }
struct s256p r_s256p (int *p) { struct s256p s = { { p, [15] = p } }; return s; }
long c_res (int *p)
{
	return r_s112 (1).a[13] + r_s128 (2).a[15] + r_s144 (3).a[17]
		+ *r_s256p (p).p[15];
}

/* Argument kinds: no arguments, a pointer after an int, a 16-byte
   integer aggregate, a pointer-and-double aggregate, long double,
   __int128, and a pointer that falls past the seventh slot.  */
struct s16 { long a, b; };
struct spd { int *p; double d; };
long d_none (void) { return 1; }
long d_s16 (int a, struct s16 s) { return a + s.a + s.b; }
double d_spd (struct spd s) { return *s.p + s.d; }
long double d_ld (int a, long double x) { return a + x; }
__int128 d_i128 (int a, __int128 x) { return a + x; }
long d_ptr8 (long a1, long a2, long a3, long a4, long a5, long a6,
	     long a7, int *p8) { return a1 + *p8; }
int *d_retptr (int *p) { return p + 1; }
long c_kinds (int *p)
{
	struct s16 s = { 1, 2 };
	struct spd t = { p, 2.5 };
	return d_none () + d_s16 (1, s) + (long) d_spd (t)
		+ (long) d_ld (1, 2.0L) + (long) d_i128 (1, 2)
		+ d_ptr8 (1, 2, 3, 4, 5, 6, 7, p) + *d_retptr (p);
}

/* va_list handling: va_copy, passing a va_list on.  */
long d_vl (int n, va_list ap) { return va_arg (ap, long) + n; }
long d_vcopy (int n, ...)
{
	va_list ap, aq;
	long r;
	va_start (ap, n);
	va_copy (aq, ap);
	r = d_vl (n, aq) + va_arg (ap, long);
	va_end (aq);
	va_end (ap);
	return r;
}

/* Aggregates near the register limit: a 136-byte struct, and a 32-byte
   struct holding a pointer that starts in the last register slot.  */
struct s136 { long a[17]; };
long d_s136 (struct s136 s) { return s.a[0] + s.a[16]; }
long d_sp_last (long a1, long a2, long a3, long a4, long a5, long a6,
		struct sp s) { return a1 + *s.p + s.x; }
long c_big (int *p)
{
	struct s136 s = { { 1, [16] = 2 } };
	struct sp t = { p, 3 };
	return d_s136 (s) + d_sp_last (1, 2, 3, 4, 5, 6, t) + d_none ();
}


/* Thread-local variables: a read and an address.  */
__thread int t_var;
int t_read (void) { return t_var; }
int *t_addr (void) { return &t_var; }

/* Atomics on pointers and on long.  */
void *a_xchg (void **p, void *v) { return __atomic_exchange_n (p, v, __ATOMIC_SEQ_CST); }
int a_cas (void **p, void *e, void *v)
{
	return __atomic_compare_exchange_n (p, &e, v, 0, __ATOMIC_SEQ_CST,
					    __ATOMIC_SEQ_CST);
}
void *a_load (void **p) { return __atomic_load_n (p, __ATOMIC_ACQUIRE); }
long a_add (long *p) { return __atomic_fetch_add (p, 1, __ATOMIC_SEQ_CST); }

/* intptr_t (a 16-byte descriptor type with lcc 1.31.05).  */
#include <stdint.h>
intptr_t i_from_ptr (int *p) { return (intptr_t) p; }
int *i_to_ptr (intptr_t x) { return (int *) x; }
intptr_t i_add (intptr_t x) { return x + 4; }
