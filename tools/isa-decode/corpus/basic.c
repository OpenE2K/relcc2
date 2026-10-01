/* Freestanding code exercising common compiler output on E2K: loops,
   branches, switches, calls, floating point, atomics and aggregates.  */

typedef unsigned long size_t;
typedef __builtin_va_list va_list;

long dot(const long *a, const long *b, int n)
{
	long s = 0;
	for (int i = 0; i < n; i++)
		s += a[i] * b[i];
	return s;
}

double saxpy(double *y, const double *x, double a, int n)
{
	double sum = 0;
	for (int i = 0; i < n; i++) {
		y[i] = a * x[i] + y[i];
		sum += y[i];
	}
	return sum;
}

float fmix(float a, float b, int op)
{
	switch (op) {
	case 0: return a + b;
	case 1: return a - b;
	case 2: return a * b;
	case 3: return a / b;
	case 4: return __builtin_sqrtf(a);
	case 5: return a < b ? a : b;
	case 6: return (float)(int)a;
	default: return 0;
	}
}

int classify(int c)
{
	switch (c) {
	case 'a': case 'e': case 'i': case 'o': case 'u': return 1;
	case '0' ... '9': return 2;
	case ' ': case '\t': case '\n': return 3;
	case 100: return 4;
	case 1000: return 5;
	default: return 0;
	}
}

unsigned long divmod(unsigned long a, unsigned long b, unsigned long *rem)
{
	*rem = a % b;
	return a / b;
}

int sdiv32(int a, int b) { return a / b + a % b; }

extern int ext_func(int, long, double, void *);

int caller(int x)
{
	int v = ext_func(x, x * 3L, x * 0.5, &x);
	return v + ext_func(v, 0, 1.0, 0);
}

int sum_va(int n, ...)
{
	va_list ap;
	int s = 0;
	__builtin_va_start(ap, n);
	while (n--)
		s += __builtin_va_arg(ap, int);
	__builtin_va_end(ap);
	return s;
}

struct big { long a[9]; char tag; };

struct big copy_big(const struct big *p)
{
	struct big b = *p;
	b.tag++;
	return b;
}

int (*pick(int i))(int)
{
	extern int f0(int), f1(int);
	return i ? f1 : f0;
}

int call_ptr(int (*f)(int), int x) { return f(x) + f(x + 1); }

size_t my_strlen(const char *s)
{
	const char *p = s;
	while (*p)
		p++;
	return p - s;
}

void *my_memcpy(void *d, const void *s, size_t n)
{
	char *dp = d;
	const char *sp = s;
	while (n--)
		*dp++ = *sp++;
	return d;
}

unsigned bits(unsigned x, unsigned long y)
{
	return __builtin_popcount(x) + __builtin_ctzl(y | 1) + __builtin_clz(x | 1)
	       + (x >> 3 | x << 29) + __builtin_bswap32(x);
}

long fib(long n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }

long long_constants(long x)
{
	return x * 0x123456789abcdefL + 0x7fffffffffffffffL - (x & 0xffff0000ffffL);
}

double conv(long a, unsigned b, float c)
{
	return (double)a + (double)b + (double)c + (double)(unsigned long)a;
}

int nested(int *m, int rows, int cols)
{
	int best = 0;
	for (int r = 0; r < rows; r++)
		for (int c = 0; c < cols; c++)
			if (m[r * cols + c] > best && (r ^ c) & 1)
				best = m[r * cols + c];
	return best;
}

static int counter;
int bump(void) { return ++counter; }

__thread int tls_var;
int tls_get(void) { return tls_var++; }

int alloca_user(int n)
{
	char *buf = __builtin_alloca(n);
	for (int i = 0; i < n; i++)
		buf[i] = i;
	return buf[n / 2];
}
