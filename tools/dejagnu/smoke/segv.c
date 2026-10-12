/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Smoke test: a crash at run time, a store through a null pointer. On
   hardware the board must report signal 11 (SIGSEGV); MCST's qemu can't
   deliver the fault and kills itself (signal 9).  */
int *volatile p = 0;

int main (void)
{
  *p = 1;
  return 0;
}
