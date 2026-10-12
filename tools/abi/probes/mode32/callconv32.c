/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Calling-convention probes for 32-bit mode (lcc -mptr32,
   spec/abi/mode32.md). Compile to assembly and read where each value is
   passed or returned, as with ../callconv.c:

     L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $L -mptr32 -O1 -fno-optimize-sibling-calls -S callconv32.c
     $L -mptr32 -O1 -fno-optimize-sibling-calls -fPIC -S callconv32.c \
         -o callconv32-pic.s

   Functions named c_* are callers, d_* are callees (definitions), r_* use
   a returned value.  */

#include <stdarg.h>

struct s4 { short a, b; };
struct s8p { void *a, *b; };		/* two pointers: 8 bytes */
struct s12 { int a, b, c; };
struct s16 { long long a, b; };
struct s64 { int a[16]; };
struct s68 { int a[17]; };
struct s72 { long long a[9]; };

int g;
extern int ext_var;
extern void ext_fn (void);
static int stat_fn (int x) { return x + g; }

/* Slots: each argument still takes an 8-byte slot; slots 8 and 9 on the
   stack.  */
int d_stack_args (int a1, int a2, int a3, int a4, int a5, int a6,
		  int a7, int a8, int a9, int a10)
{
	return a1 + a9 * 3 + a10 * 5;
}

/* Pointers and long: one slot each, zero- or sign-extended?  */
long d_ptr_long (char *p, long l, unsigned long ul, void *q)
{
	return (long) (p - (char *) q) + l + (long) ul;
}

/* What callers put above a pointer and a long.  */
extern long e_ptr_long (char *p, long l, unsigned long ul, void *q);
long c_ptr_long (char *p, long l)
{
	return e_ptr_long (p + 1, l - 1, (unsigned long) l, &g) + 1;
}

/* long long and double take whole slots; 9-16-byte aggregates even ones.  */
extern long long e_mixed (char c, long long ll, struct s12 s, int i,
			  struct s16 t, double d, struct s4 u);
long long c_mixed (void)
{
	struct s12 s = { 1, 2, 3 };
	struct s16 t = { 4, 5 };
	struct s4 u = { 6, 7 };
	return e_mixed (1, 2, s, 3, t, 4.0, u) + 1;
}

long long d_mixed (char c, long long ll, struct s12 s, int i,
		   struct s16 t, double d, struct s4 u)
{
	return c + ll + s.c + i + t.b + (long long) d + u.b;
}

/* An 8-byte struct of two pointers: one slot or two?  */
void *d_s8p (int x, struct s8p s) { return x ? s.a : s.b; }

/* long double: two slots, even.  */
long double d_ldbl (int x, long double y) { return y + x; }

/* Results: up to 64 bytes in registers?  */
struct s64 d_ret64 (int x) { struct s64 r = { { x } }; r.a[15] = x; return r; }
struct s68 d_ret68 (int x) { struct s68 r = { { x } }; r.a[16] = x; return r; }
struct s12 d_ret12 (int x) { struct s12 r = { x, x + 1, x + 2 }; return r; }
struct s8p d_ret8p (void *p) { struct s8p r = { p, 0 }; return r; }
char *d_retptr (char *p) { return p + 1; }
long d_retlong (long x) { return x * 3; }
unsigned d_retuns (unsigned x) { return x * 3; }

extern struct s68 e_ret68 (int);
int r_ret68 (void) { return e_ret68 (1).a[16]; }

/* Narrow integers: does the caller extend them (64-bit mode: yes, to 64
   bits), does the callee, and who extends narrow results?  */
extern int e_ext (char, unsigned char, short, unsigned short, int,
		  unsigned, _Bool);
int c_ext (int x) { return e_ext (x, x, x, x, x, x, x) + 1; }
long long d_ext (char a, unsigned char b, short c, unsigned short d, int e,
		 unsigned f, _Bool g)
{
	return (long long) a + b + c + d + e + f + g;
}
char r_char (int x) { return x * 3; }
unsigned short r_ushort (int x) { return x * 3; }
extern char e_rchar (void);
extern unsigned short e_rushort (void);
extern _Bool e_rbool (void);
long long u_rchar (void) { return e_rchar () + 1LL; }
long long u_rushort (void) { return e_rushort () + 1LL; }
long long u_rbool (void) { return e_rbool () + 1LL; }

/* Variadic.  */
long long d_va (int n, ...)
{
	va_list ap;
	long long s = 0;
	va_start (ap, n);
	for (int i = 0; i < n; i++)
		s += va_arg (ap, int);
	s += va_arg (ap, long long);
	s += (long long) va_arg (ap, double);
	s += (long long) va_arg (ap, long double);
	s += (long) va_arg (ap, char *);
	va_end (ap);
	return s;
}

extern long long e_va (int n, ...);
long long c_va (void)
{
	return e_va (2, 10, 20, 30LL, 4.0, (long double) 5.0, "x") + 1;
}

/* va_list is a pointer: its size and how va_arg steps.  */
int d_vasize (void) { return sizeof (va_list); }

/* Unprototyped call.  */
extern long long e_noproto ();
long long c_noproto (void) { return e_noproto (1, 2LL, 3.0) + 1; }

/* Indirect call and function addresses.  */
int c_indirect (int (*f) (int)) { return f (3) + 1; }
void *a_fn (void) { return (void *) ext_fn; }
void *a_static_fn (void) { return (void *) stat_fn; }
void *a_ext_var (void) { return &ext_var; }
int l_ext_var (void) { return ext_var; }

/* Memory accesses of each width.  */
long long mem (char *p, short *h, long long *d, int i)
{
	return p[i] + h[i] + d[i];
}
void memst (char *p, short *h, long long *d, int i, long long v)
{
	p[i] = v; h[i] = v; d[i] = v;
}

/* Pointer arithmetic and comparisons wrap at 32 bits.  */
char *padd (char *p, int i) { return p + i; }
int pcmp (char *p, char *q) { return p < q; }

/* A local whose address escapes: on the stack, addressed with getsp.  */
extern void use (int *);
int local (void) { int a[4] = { 1, 2, 3, 4 }; use (a); return a[1]; }

/* alloca.  */
int c_alloca (int n) { int *p = __builtin_alloca (n * sizeof (int)); use (p); return p[0]; }

/* TLS.  */
__thread int tls_var;
extern __thread int ext_tls;
int l_tls (void) { return tls_var + ext_tls; }
int *a_tls (void) { return &tls_var; }

/* Atomics.  */
int at_add (int *p, int v) { return __atomic_fetch_add (p, v, __ATOMIC_SEQ_CST); }
long long at_add64 (long long *p, long long v) { return __atomic_fetch_add (p, v, __ATOMIC_SEQ_CST); }
int at_cas (int *p, int o, int n) { return __atomic_compare_exchange_n (p, &o, n, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); }
