/* SPDX-License-Identifier: GPL-3.0-or-later */
/* gdb, DWARF and core dumps of 32-bit programs (spec/abi/mode32.md, "C
   runtime"): a small program to debug, run by dbg32.sh on each machine
   with dbg32.gdb (breakpoints, backtrace, registers, variables of every
   kind, finish, next, step, a watchpoint, gcore) and then made to dump
   core (`abort' or `segv' as the argument) for gdb to read.  dbg32.sh
   builds it for 32-bit and 64-bit mode at -O0 -g:

     L=/opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1/bin/lcc
     $L -mptr32 -O0 -g dbg32.c -o dbg32
     gdb -q -batch -x dbg32.gdb ./dbg32
     (ulimit -c unlimited; ./dbg32 abort); gdb -q -batch ./dbg32 core  */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct pt
{
  int x;
  long long y;
  double d;
  char name[8];
  struct pt *next;
};

struct pt gpt = { 1, 0x1122334455667788LL, 2.5, "glob", &gpt };
long glong = -5;
unsigned long gulong = 0xfffffff0ul;
long long gll = 0x0102030405060708LL;
float gf = 1.25f;
long double gld = 3.0L;
int garr[4] = { 10, 20, 30, 40 };
const char *gstr = "hello";
void *gptr = &garr[2];
int (*gfp) (int);
__thread int tlsv = 77;
volatile int watched;

static const char *mode = "";

__attribute__ ((noinline)) int
leaf (int a, long long b, struct pt *p, char c, double dd)
{
  int loc = a * 3;
  watched = loc;
  if (!strcmp (mode, "abort"))
    abort ();
  if (!strcmp (mode, "segv"))
    *(volatile int *) (unsigned long) (b & 0x10) = 1;	/* address 0 */
  return loc + (int) b + p->x + c + (int) dd;
}

__attribute__ ((noinline)) int
mid (int n)
{
  struct pt local = gpt;
  local.x = n;
  local.next = 0;
  strcpy (local.name, "local");
  return leaf (n, gll + n, &local, 'q', 0.5) + 1;
}

int
rec (int n)
{
  return n ? rec (n - 1) + 1 : mid (7);
}

int
main (int argc, char **argv)
{
  if (argc > 1)
    mode = argv[1];
  gfp = rec;
  int r = gfp (3);
  printf ("dbg32: %d, sizeof (long) %d, sizeof (void *) %d\n", r,
	  (int) sizeof (long), (int) sizeof (void *));
  return 0;
}
