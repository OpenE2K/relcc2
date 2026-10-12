/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Smoke test: an integer division by zero, a hardware exception. On
   hardware it is SIGFPE (signal 8); MCST's qemu stops with "exception
   handling is unimplemented" and kills itself (signal 9).  */
volatile int a = 1, b = 0;

int main (void)
{
  return a / b;
}
