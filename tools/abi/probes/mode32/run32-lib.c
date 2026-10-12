/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The callee side of run32.c (32-bit mode, spec/abi/mode32.md): built as
   a shared library for the dynamic programs (calls through the PLT, TLS
   through __tls_get_addr) and linked in for the static one. See run.sh:
     lcc -mptr32 -O2 -fPIC -shared run32-lib.c -o librun32.so  */

#include <stdarg.h>
#include <string.h>
#include "run32.h"

__thread int lib_tls = 7;
int lib_data = 11;

int *lib_tls_addr (void) { return &lib_tls; }
int *lib_data_addr (void) { return &lib_data; }

static int lib_static_fn (int x) { return x * 5; }
int lib_fn (int x) { return x * 3; }
int (*lib_fp (void)) (int) { return lib_static_fn; }
int (*lib_fn_addr (void)) (int) { return lib_fn; }

long long lib_mixed (char c, long long ll, struct s12 s, int i, struct s16 t,
		     double d, struct s4 u, float f, long double ld, void *p,
		     unsigned char uc, short sh)
{
	return MIXED_SUM (c, ll, s, i, t, d, u, f, ld, p, uc, sh);
}

long long lib_va (int n, ...)
{
	va_list ap;
	long long s = 0;
	va_start (ap, n);
	for (int i = 0; i < n; i++)
		s = s * 3 + va_arg (ap, int);
	s = s * 3 + va_arg (ap, long long);
	s = s * 3 + (long long) va_arg (ap, double);
	s = s * 3 + (long long) va_arg (ap, long double);
	s = s * 3 + (long long) strlen (va_arg (ap, char *));
	struct s12 t = va_arg (ap, struct s12);
	s = s * 3 + t.a + t.b * 5 + t.c * 7;
	va_end (ap);
	return s;
}

int lib_vasize (va_list ap) { return va_arg (ap, int) + va_arg (ap, int); }

struct s68 lib_ret68 (int x)
{
	struct s68 r;
	for (int i = 0; i < 17; i++)
		r.a[i] = x + i;
	return r;
}

struct s12 lib_ret12 (int x) { struct s12 r = { x, x + 1, x + 2 }; return r; }

char lib_rc (int x) { return x * 3; }
unsigned short lib_rus (int x) { return x * 3; }
long lib_rl (long x) { return x * 3; }
long long lib_rll (long long x) { return x * 3; }
