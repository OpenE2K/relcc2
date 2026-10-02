/* SPDX-License-Identifier: GPL-3.0-or-later */
/* An ifunc whose resolver picks impl_b; see run.sh.  */
static int impl_a(int x) { return x + 1; }
static int impl_b(int x) { return x + 100; }
static int (*resolve_f(void))(int) { return impl_b; }
int f(int) __attribute__((ifunc("resolve_f")));
int (*f_addr(void))(int) { return f; }
