/* Calling-convention probes. Compile to assembly with the reference
   compilers and read where each value is passed or returned:

     toolchains/lcc-1.29.16-e2k-v5/lcc -O1 -fno-optimize-sibling-calls -S
     toolchains/unipro/bin/clang --target=e2k64-linux -march=elbrus-v5 -O1 \
         -fno-optimize-sibling-calls -S

   Functions named c_* are callers, d_* are callees (definitions), r_* use
   a returned value.  */

#include <stdarg.h>
#include <stdbool.h>

struct s3 { char a, b, c; };
struct s16 { long a, b; };
struct s24 { long a, b, c; };
struct s64 { long a[8]; };
struct s72 { long a[9]; };
struct ff { float a, b; };
struct fd { float f; double d; };

/* Argument slots: the 9th and 10th long arrive on the stack.  */
long d_stack_args(long a1, long a2, long a3, long a4, long a5, long a6,
		  long a7, long a8, long a9, long a10)
{
	return a1 + a9 * 3 + a10 * 5;
}

/* A 16-byte argument that no longer fits in registers.  */
long d_split(long a1, long a2, long a3, long a4, long a5, long a6, long a7,
	     struct s16 s)
{
	return a1 + s.a + s.b;
}

/* Variadic callee.  */
long d_va(int n, ...)
{
	va_list ap;
	long s = 0;
	va_start(ap, n);
	for (int i = 0; i < n; i++)
		s += va_arg(ap, long);
	va_end(ap);
	return s;
}

double d_va_double(int n, ...)
{
	va_list ap;
	double s = 0;
	va_start(ap, n);
	while (n--)
		s += va_arg(ap, double);
	va_end(ap);
	return s;
}

/* Results of various kinds.  */
struct s64 d_s64(long x)
{
	struct s64 r;
	for (int i = 0; i < 8; i++)
		r.a[i] = x + i;
	return r;
}

struct ff d_ff(float a, float b) { struct ff r = { a, b }; return r; }
struct s3 d_s3(char a) { struct s3 r = { a, a + 1, a + 2 }; return r; }
__int128 d_i128(long a, long b) { return (__int128)a * b; }
_Complex double d_cplx(double a, double b) { return a + b * 1.0i; }
_Complex float d_cplxf(float a, float b) { return a + b * 1.0fi; }
__float128 d_f128(__float128 a, __float128 b) { return a * b; }
bool d_bool(int a) { return a > 3; }
unsigned short d_ushort(unsigned short a) { return a + 1; }
float d_float(float a, double b) { return a * (float)b; }

/* Callers.  */
extern void use_ff(struct ff, float);
extern void use_cplx(_Complex double, _Complex float);
extern void use_f128(int, __float128);
extern void use_bool(bool, signed char, unsigned char);
extern long noproto();

void c_ff(struct ff x) { use_ff(x, 3.0f); }
void c_cplx(_Complex double a, _Complex float b) { use_cplx(a, b); }
void c_f128(__float128 x) { use_f128(1, x); }
void c_bool(void) { use_bool(true, -2, 200); }
long c_noproto(void) { return noproto(1, 2.0, 3L); }
long r_s64(struct s64 (*f)(long)) { struct s64 r = f(1); return r.a[0] + r.a[7]; }
