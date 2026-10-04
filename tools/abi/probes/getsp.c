/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Which operands of getsp the kernel can decode when the stack must grow
   (spec/abi/stack-and-unwinding.md).  On v3 a getsp that moves the stack
   pointer past the mapped part of the data stack traps, and the kernel
   (arch/e2k/kernel/getsp.c) reads the instruction to learn how far to
   grow it.

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $LCC -O1 tools/abi/probes/getsp.c tools/abi/probes/getsp.s -o getsp
     ./getsp

   Each case runs in a fresh child process (a grown stack stays grown).  */

#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

long gs_win (long), gs_g8 (long), gs_g16 (long), gs_g24 (long);

static void
run (const char *name, long (*f) (long), long n)
{
  pid_t pid = fork ();
  if (pid == 0)
    _exit (f (n) == 2 ? 0 : 1);
  int st;
  waitpid (pid, &st, 0);
  printf ("%-8s %8ld KiB: %s\n", name, n >> 10,
          WIFSIGNALED (st) ? (WTERMSIG (st) == 11 ? "SIGSEGV" : "signal")
          : WEXITSTATUS (st) ? "wrong value" : "ok");
}

int
main (void)
{
  static const struct { const char *name; long (*f) (long); } c[] = {
    { "window", gs_win }, { "%g8", gs_g8 }, { "%g16", gs_g16 },
    { "%g24", gs_g24 } };
  for (unsigned i = 0; i < sizeof c / sizeof c[0]; i++)
    for (long n = 16 << 10; n <= 4 << 20; n *= 16)
      run (c[i].name, c[i].f, n);
  return 0;
}
