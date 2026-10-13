/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Protected-mode (e2k128) user shared library for pmlibmain.c: data,
   pointers, function pointers, a callback, TLS and register reads from
   inside a library.  Built on the machine (its ld has the new protected
   PLT, protected-mode.md "ELF and linking") by run.sh --left:

     lcc -mptr128 -O1 -fPIC -shared pmlib.c -o libpmlib.so  */

#define NOINLINE __attribute__ ((noinline))

int lib_var = 5;
int lib_arr[4] = { 1, 2, 3, 4 };
int *lib_ptr = &lib_arr[1];
const char lib_str[] = "library string";
__thread int lib_tls = 7;
extern int main_var;			/* defined in the executable */

int lib_fn (int x) { return x + lib_var; }
int (*lib_get_fn (void)) (int) { return lib_fn; }
int *lib_get_arr (void) { return lib_arr; }
int *lib_get_var (void) { return &lib_var; }
int lib_call (int (*cb) (int), int x) { return cb (x) * 2; }
int lib_sum (const int *a, int n)
{
	int s = 0;
	for (int i = 0; i < n; i++)
		s += a[i];
	return s;
}
NOINLINE int lib_read (const int *a, int i) { return a[i]; }
int *lib_tls_addr (void) { return &lib_tls; }
int lib_main_var (void) { return main_var; }

/* %cud, %gd and %usd as this module sees them.  */
void lib_regs (unsigned long out[6])
{
	unsigned long cl, ch, gl, gh, ul, uh;
	__asm__ __volatile__ ("rrd %%cud.lo, %0\n\trrd %%cud.hi, %1\n\t"
			      "rrd %%gd.lo, %2\n\trrd %%gd.hi, %3\n\t"
			      "rrd %%usd.lo, %4\n\trrd %%usd.hi, %5"
			      : "=r" (cl), "=r" (ch), "=r" (gl), "=r" (gh),
				"=r" (ul), "=r" (uh));
	out[0] = cl;
	out[1] = ch;
	out[2] = gl;
	out[3] = gh;
	out[4] = ul;
	out[5] = uh;
}
