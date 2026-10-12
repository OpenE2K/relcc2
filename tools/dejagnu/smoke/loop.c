/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Smoke test: a program that never ends. The board must kill it at the
   time limit, report fail and a timeout, and leave no process behind.  */
volatile unsigned long n;

int main (void)
{
  for (;;)
    n++;
}
