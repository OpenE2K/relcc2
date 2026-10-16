/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The process entry state a C _start relies on (spec/abi/kernel-interface.md,
   "Process entry"): glibc's crt1.o _start and ld.so's _start2 are C
   functions that find argc through `register long *fp asm ("FP")` and
   take rtld_fini from %dr0 (glibc sysdeps/unix/sysv/linux/e2k/start.c,
   sysdeps/e2k/dl-machine.h).  glibcbuild_entry.s runs before _start and
   records the state; main prints it.

     L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $L -O2 glibcbuild_entry.c glibcbuild_entry.s \
        -Wl,-e,glibcbuild_entry -o entry-dyn -lpthread
     $L -O2 -static glibcbuild_entry.c glibcbuild_entry.s \
        -Wl,-e,glibcbuild_entry -o entry-static -lpthread
     ./entry-dyn a b; ./entry-static a b         # on hardware

   Output, one "name value" line each: %dr0-%dr7 with their tags (and,
   for a nonzero %dr0, the /proc/self/maps line that holds it), the
   global registers that are not 0 with tag 0, the predicates that are
   not plain false, the window (%wd, %br, %cr1 of the frame below), the
   stacks (%usd: where argc and argv are relative to it), the FP and
   loop state registers, and glibc's __libc_stack_end and
   pthread_getattr_np's view of the main thread's stack.  MCST's qemu
   stops on rrd of %psp, %pshtp and %sbr (stackregs.c), so run it on
   hardware.

   Results (2026-10-16; v3 build on the 4C, v5.8c2 on the 8C2, v6.2c3 on
   the 2C3).  Static, the kernel's state, all three: %dr0-%dr7 0 with tag
   0; every predicate false; %wd psize 64 (4 pairs), %br 0; %cr0.hi and
   %cr1 0 (no frame below); usd.ptr is the address of argc, stored as a
   64-bit word, 16-byte aligned (not always 32), argv at +8, envp
   after argv's null; %pfpfr 0x1fbf, %fpcr 0x37f, %fpsr 0x3f, %bgr 0xff,
   %rndpr 0x0003ffff0001ffff, %rpr 0; the globals 0 with tag 5 (empty),
   except %g0-%g15 0 with tag 0 on the 4C; %lsr 0xff0000000000 and
   %ilcr 0 (8C2, 2C3) or 0x6000000000 and 9 (4C); __libc_stack_end NULL
   and pthread_getattr_np on the main thread fails with ENOENT.
   Dynamic, ld.so's state: %dr0 is _dl_fini (in ld-linux.so.2),
   %dr1-%dr7 ld.so's leftovers with tag 0; %cr1.lo wbs 4, wpsz 4, and
   %cr0.hi returns into ld.so's _start; %g13 the thread pointer, the
   other globals ld.so's leftovers (four of them diagnostic on the 4C);
   every predicate false; usd.ptr is argc's address as above;
   __libc_stack_end equals usd.ptr.  */

#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

unsigned long glibcbuild_state[176];
extern void *__libc_stack_end;

static const char *const sreg_names[] = {
  "wd", "br", "usd.lo", "usd.hi", "cr0.hi", "cr1.lo", "cr1.hi", "psp.lo",
  "psp.hi", "pshtp", "pcsp.lo", "pcsp.hi", "pcshtp", "sbr", "upsr",
  "pfpfr", "fpcr", "fpsr", "lsr", "ilcr", "bgr", "rndpr", "rpr.lo",
  "rpr.hi", "getsp0",
};

static unsigned long
bits (unsigned long v, int hi, int lo)
{
  return (v >> lo) & ((2UL << (hi - lo)) - 1);
}

static void
print_mapping (unsigned long addr)
{
  char line[512];
  FILE *f = fopen ("/proc/self/maps", "r");

  if (!f)
    return;
  while (fgets (line, sizeof line, f))
    {
      unsigned long lo, hi;
      if (sscanf (line, "%lx-%lx", &lo, &hi) == 2 && lo <= addr && addr < hi)
	{
	  printf ("  in mapping: %s", line);
	  break;
	}
    }
  fclose (f);
}

int
main (int argc, char **argv)
{
  unsigned long *s = glibcbuild_state;
  unsigned long *sr = s + 144;
  unsigned long ptr, v;
  pthread_attr_t attr;
  void *stack_addr;
  size_t stack_size;
  int i, n, e;

  if (s[175] != 1)
    {
      printf ("entry stub did not run (link with -Wl,-e,glibcbuild_entry)\n");
      return 1;
    }

  for (i = 0; i < 8; i++)
    printf ("dr%d 0x%016lx tag %lu\n", i, s[i], s[8 + i]);
  if (s[0])
    print_mapping (s[0]);

  n = 0;
  for (i = 0; i < 32; i++)
    if (s[16 + i] || s[48 + i])
      {
	printf ("g%d 0x%016lx tag %lu\n", i, s[16 + i], s[48 + i]);
	n++;
      }
  printf ("globals not 0 with tag 0: %d of 32\n", n);

  n = 0;
  for (i = 0; i < 32; i++)
    if (s[80 + i] || s[112 + i])
      {
	printf ("pred%d %s\n", i, s[112 + i] ? "diagnostic" : "true");
	n++;
      }
  printf ("predicates not false: %d of 32\n", n);

  for (i = 0; i < (int) (sizeof sreg_names / sizeof sreg_names[0]); i++)
    printf ("%s 0x%016lx\n", sreg_names[i], sr[i]);

  v = sr[0];
  printf ("wd: base %lu size %lu psize %lu fx %lu dbl %lu (bytes; 16 a pair)\n",
	  bits (v, 10, 0), bits (v, 26, 16), bits (v, 42, 32), bits (v, 48, 48),
	  bits (v, 49, 49));
  v = sr[1];
  printf ("br: rbs %lu rsz %lu rcur %lu psz %lu pcur %lu\n", bits (v, 5, 0),
	  bits (v, 11, 6), bits (v, 17, 12), bits (v, 22, 18), bits (v, 27, 23));
  v = sr[5];
  printf ("cr1.lo: wbs %lu wpsz %lu wfx %lu ussz_hi %lu psr 0x%lx\n",
	  bits (v, 39, 33), bits (v, 32, 26), bits (v, 25, 25), bits (v, 15, 0),
	  bits (v, 63, 57));
  v = sr[6];
  printf ("cr1.hi: br 0x%lx wdbl %lu ussz %lu (x16 = 0x%lx)\n",
	  bits (v, 27, 0), bits (v, 35, 35), bits (v, 63, 36),
	  bits (v, 63, 36) * 16);
  printf ("cr0.hi: return address 0x%lx\n", sr[4] & 0xffffffffffffUL);
  if (sr[4] & 0xffffffffffffUL)
    print_mapping (sr[4] & 0xffffffffffffUL);

  ptr = sr[2] & 0xffffffffffffUL;
  printf ("usd: ptr 0x%lx size 0x%lx bottom 0x%lx; getsp 0 %s ptr\n",
	  ptr, bits (sr[3], 63, 32), ptr - bits (sr[3], 63, 32),
	  sr[24] == ptr ? "==" : "!=");
  printf ("usd.ptr mod 16 = %lu, mod 32 = %lu, mod 64 = %lu; sbr - ptr = 0x%lx\n",
	  ptr % 16, ptr % 32, ptr % 64, (sr[13] & 0xffffffffffffUL) - ptr);
  printf ("at usd.ptr: long %ld (argc %d); argv == usd.ptr + 8: %s;"
	  " envp == argv + argc + 1: %s\n",
	  *(long *) ptr, argc, (char **) (ptr + 8) == argv ? "yes" : "no",
	  (char **) (ptr + 8) + argc + 1 == environ ? "yes" : "no");
  printf ("__libc_stack_end %p (usd.ptr 0x%lx)\n", __libc_stack_end, ptr);
  e = pthread_getattr_np (pthread_self (), &attr);
  if (e == 0 && pthread_attr_getstack (&attr, &stack_addr, &stack_size) == 0)
    printf ("pthread_getattr_np: stack %p size 0x%zx, top %p\n", stack_addr,
	    stack_size, (char *) stack_addr + stack_size);
  else
    printf ("pthread_getattr_np failed: %s\n", strerror (e));
  return 0;
}
