/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Global register rotation: abg and %bgr (spec/isa/loops.md).  The
   functions are in abg.s.  One case per process, so a trap leaves
   nothing behind:

     LCC=/opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1/bin/lcc
     $LCC -O1 -Wa,--defsym,ABG_V5=1 tools/isa-probes/abg.c \
       tools/isa-probes/abg.s -o abg      # v5 (without the -Wa for v3)
     ./abg CASE [ARG]                     # on hardware
     ABG_TOUCH=1 ./abg load_g24 ARG       # the same load without a fault

   Prints out[0] (%bgr at entry), out[1] (%bgr after the case) and, for
   cases that read %g24..%g31, their values; or the signal and si_code.  */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef void fn (unsigned long *, unsigned long);
fn bgr_read, bgr_same, bgr_set, abgi, abgd, abgid, abgi_from, abgd_from,
  abgi_dump, abgd_dump, read_alc0, read_alc1, read_alc2, read_alc3,
  read_alc4, read_alc5, write_g24, abgid_from, read_sm4, quad_read,
  load_g24, load_g24_tags;

static const struct { const char *name; fn *f; int dump; } cases[] = {
  { "bgr_read", bgr_read, 0 }, { "bgr_same", bgr_same, 0 },
  { "bgr_set", bgr_set, 1 }, { "abgi", abgi, 0 }, { "abgd", abgd, 0 },
  { "abgid", abgid, 0 }, { "abgi_from", abgi_from, 0 },
  { "abgd_from", abgd_from, 0 }, { "abgi_dump", abgi_dump, 1 },
  { "abgd_dump", abgd_dump, 1 }, { "read_alc0", read_alc0, 2 },
  { "read_alc1", read_alc1, 2 }, { "read_alc2", read_alc2, 2 },
  { "read_alc3", read_alc3, 2 }, { "read_alc4", read_alc4, 2 },
  { "read_alc5", read_alc5, 2 }, { "write_g24", write_g24, 0 },
  { "abgid_from", abgid_from, 0 }, { "read_sm4", read_sm4, 4 },
  { "quad_read", quad_read, 3 }, { "load_g24", load_g24, 1 },
  { "load_g24_tags", load_g24_tags, 5 },
};

static void
handler (int sig, siginfo_t *si, void *uc)
{
  (void) uc;
  char buf[64];
  int n = snprintf (buf, sizeof buf, "signal %d code %d addr %p\n", sig,
                    si->si_code, si->si_addr);
  write (1, buf, n);
  _exit (0);
}

int
main (int argc, char **argv)
{
  if (argc < 2)
    return 2;
  unsigned long arg = argc > 2 ? strtoul (argv[2], NULL, 16) : 0;
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = handler;
  sa.sa_flags = SA_SIGINFO;
  sigaction (SIGFPE, &sa, NULL);
  sigaction (SIGILL, &sa, NULL);
  sigaction (SIGSEGV, &sa, NULL);
  sigaction (SIGBUS, &sa, NULL);
  setvbuf (stdout, NULL, _IONBF, 0);
  for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++)
    if (strcmp (argv[1], cases[i].name) == 0)
      {
        static unsigned long out[32];
        /* A page the case's load faults on (load_g24).  */
        out[10] = (unsigned long) mmap (NULL, 4096, PROT_READ | PROT_WRITE,
                                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (getenv ("ABG_TOUCH"))       /* no fault: the load just loads */
          *(volatile unsigned long *) out[10] = 0x77;
        printf ("%s %lx: ", argv[1], arg);
        cases[i].f (out, arg);
        printf ("bgr %03x -> %03x", (unsigned) out[0], (unsigned) out[1]);
        if (cases[i].dump == 1)
          for (int k = 2; k < 10; k++)
            printf (" %lx", out[k]);
        else if (cases[i].dump == 2)
          printf (" g24 %lx", out[2]);
        else if (cases[i].dump == 5)
          for (int k = 0; k < 8; k++)
            printf (" %lx/%lx", out[2 + k], out[16 + k] & 0xff);
        else if (cases[i].dump == 4)
          printf (" tag %lx g24 %lx", out[2], out[3]);
        else if (cases[i].dump == 3)
          for (int k = 2; k < 8; k += 2)
            printf (" %lx:%lx", out[k + 1], out[k]);
        printf ("\n");
        return 0;
      }
  return 2;
}
