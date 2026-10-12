/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Conservative root scanning on E2K (spec/abi/stack-and-unwinding.md,
   "Scanning for roots"; kernel-interface.md, "Access"): can a garbage
   collector find a pointer that lives only in a register of an outer
   frame?  The registers of every frame below the current one are in the
   procedure stack, which only access_hw_stacks reads.  This prints

     1. the layout of a frame's registers in the procedure stack (an
        assembly frame with known values in %r0..%r15, an 80-bit value
        and a diagnostic value), nfx = 1 and nfx = 0;
     2. whether the words copied out keep their tags (hs_peek reads them
        with the fill MAS, the one load that returns memory tags), and
        whether a plain C loop over the copy (what a collector's mark
        loop does) traps;
     3. which frames the copy ends with: the function that makes the
        system call (glibc's syscall (), or an assembly function that
        calls entry 3 itself) and its caller;
     4. where a heap pointer kept only in a register of a C function
        (lcc -O2) is found, with 4 and with 41 frames between that
        function and the scanner (outer (3), outer (40));
     5. a thread stopped by a signal (as Boehm GC stops threads): where
        its registers are, from inside the handler: its window in the
        procedure stack, %g20 (one of %g16..%g31, saved in the signal
        frame) and %g4 (one of %g0..%g15, not saved there);
     6. with -DHWSCAN_LIBGC (the system's Boehm GC, where installed):
        whether GC_gcollect keeps an object referenced only from a
        register of an outer frame, and survives a diagnostic value;
     7. on v5+ (lcc's __iset__ >= 5), where the upper half of a 128-bit
        register (qppackdl) is in the copy: the slot after the value,
        which section 1 calls the extension.

     L=/opt/mcst/lcc-1.29.16.e2k-v3.4c.linux-6.1/bin/lcc   # v5.8c2, v6.2c3
     $L -O2 -rdynamic hwscan.c hwscan.s -o hwscan -lpthread
     ./hwscan                        # on hardware
   On a machine with libgc (natively, /usr/include/gc.h):
     lcc -O2 -rdynamic -DHWSCAN_LIBGC hwscan.c hwscan.s -o hwscan-gc \
         -lgc -lpthread
     ./hwscan-gc gc
     strace -e trace=access_hw_stacks ./hwscan-gc gc   */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <ucontext.h>
#include <unistd.h>
#include <asm/e2k_syswork.h>
#ifdef HWSCAN_LIBGC
#define GC_THREADS   /* pthread_create becomes GC_pthread_create */
#include <gc.h>
#endif

void hs_marks_fx1 (void (*) (void *), void *, unsigned long, long *);
void hs_marks_fx0 (void (*) (void *), void *, unsigned long, long *);
long hs_direct (long, unsigned long long *, void *, unsigned long, void *,
                unsigned long);
long hs_peek (const void *, unsigned long *);
long hs_spin (volatile long *, long);
long hs_tagtest (long *, long *);
long hs_spin_r1 (volatile long *, long, long);
long hs_spin_r4 (volatile long *, long, long);
long hs_spin_r7 (volatile long *, long, long);
long hs_spin_g20 (volatile long *, long, long);
long hs_spin_r1r4 (volatile long *, long, long);
long hs_spin_g20p1 (volatile long *, long, long);
long hs_spin_g4 (volatile long *, long, long);

extern void *__libc_stack_end;

#define BASE 0x4d41524b00000000UL   /* "MARK" */
#define BASE_D (BASE + 0x100)       /* hs_direct's */
#define MARK_C 0x4c4f43414c000000UL /* "LOCAL" */
#define MARK_SPIN 0x5350494e00000000UL /* "SPIN" */
#define HIDE 0x5a5a5a5a5a5a5a5aUL

volatile long sink;

static long
hw (unsigned long mode, unsigned long long *off, void *buf,
    unsigned long size, void *real)
{
  return syscall (__NR_access_hw_stacks, mode, off, buf, size, real);
}

/* A copy of both hardware stacks.  */
struct snap
{
  unsigned long long ps, cs;
  unsigned char *pbuf;
  unsigned long long *cbuf;
  int direct;
};

static unsigned char pbuf_main[8 << 20], pbuf_sig[8 << 20];
static unsigned long long cbuf_main[1 << 18], cbuf_sig[1 << 18];

/* Sizes and reads through glibc's syscall (), all from this frame.  */
int __attribute__ ((noinline))
take_snap (struct snap *s)
{
  unsigned long long off = 0;
  long r = 0;
  r |= hw (E2K_GET_PROCEDURE_STACK_SIZE, 0, 0, 0, &s->ps);
  r |= hw (E2K_GET_CHAIN_STACK_SIZE, 0, 0, 0, &s->cs);
  r |= hw (E2K_READ_PROCEDURE_STACK_EX, &off, s->pbuf, s->ps, 0);
  off = 0;
  r |= hw (E2K_READ_CHAIN_STACK_EX, &off, s->cbuf, s->cs, 0);
  s->direct = 0;
  sink++;
  return r;
}

/* The same through hs_direct, which calls the kernel itself.  */
int __attribute__ ((noinline))
take_snap_direct (struct snap *s)
{
  unsigned long long off = 0;
  long r = 0;
  r |= hs_direct (E2K_GET_PROCEDURE_STACK_SIZE, 0, 0, 0, &s->ps, BASE_D);
  r |= hs_direct (E2K_GET_CHAIN_STACK_SIZE, 0, 0, 0, &s->cs, BASE_D);
  r |= hs_direct (E2K_READ_PROCEDURE_STACK_EX, &off, s->pbuf, s->ps, 0,
                  BASE_D);
  off = 0;
  r |= hs_direct (E2K_READ_CHAIN_STACK_EX, &off, s->cbuf, s->cs, 0, BASE_D);
  s->direct = 1;
  sink++;
  return r;
}

/* Chain frame I (0 = bottom): its return IP, wbs, wfx, pm, and the
   procedure stack bytes [lo, hi) of the function it returns to.  The
   top chain frame is the function that made the system call; walking
   down, each frame's area is wbs * 32 bytes below the previous one
   (as MCST's unwinder does it).  */
struct frame
{
  unsigned long ip;
  unsigned wbs, wfx, pm;
  unsigned long long lo, hi;
};

static struct frame frames[1 << 16];
static int nframes;

static void
map_frames (const struct snap *s)
{
  int n = s->cs / 32;
  unsigned long long top = s->ps;
  nframes = 0;
  for (int i = n - 1; i >= 1; i--)
    {
      const unsigned long long *c = &s->cbuf[4 * i];
      struct frame *f = &frames[nframes++];
      f->ip = c[1] & 0xfffffffffff8UL;
      f->wbs = (c[2] >> 33) & 0x7f;
      f->wfx = (c[2] >> 25) & 1;
      f->pm = (c[2] >> 57) & 1;
      f->hi = top;
      f->lo = top >= f->wbs * 32ULL ? top - f->wbs * 32ULL : 0;
      top = f->lo;
    }
}

static const char *
symname (unsigned long ip, char *buf, size_t len)
{
  Dl_info di;
  if (ip && dladdr ((void *) ip, &di) && di.dli_sname)
    snprintf (buf, len, "%s+0x%lx", di.dli_sname,
              ip - (unsigned long) di.dli_saddr);
  else if (ip && dladdr ((void *) ip, &di) && di.dli_fname)
    snprintf (buf, len, "%s+0x%lx", strrchr (di.dli_fname, '/')
              ? strrchr (di.dli_fname, '/') + 1 : di.dli_fname,
              ip - (unsigned long) di.dli_fbase);
  else
    snprintf (buf, len, "0x%lx", ip);
  return buf;
}

static int
frame_of (unsigned long long off)
{
  for (int k = 0; k < nframes; k++)
    if (off >= frames[k].lo && off < frames[k].hi)
      return k;
  return -1;
}

static int new_layout = -1;   /* 1: 16 bytes per register (v5+) */

static const char *
reg_at (unsigned long long within, char *buf, size_t len)
{
  if (new_layout < 0)
    snprintf (buf, len, "?");
  else if (new_layout)
    snprintf (buf, len, "%%r%llu%s", within / 16,
              within % 16 ? " (extension)" : "");
  else
    snprintf (buf, len, "%%r%llu%s", within / 32 * 2 + (within % 16) / 8,
              within % 32 >= 16 ? " (extension)" : "");
  return buf;
}

/* Every place in the procedure stack copy holding VALUE.  */
static int
find_value (const struct snap *s, unsigned long value, const char *what)
{
  int hits = 0;
  for (unsigned long long off = 0; off + 8 <= s->ps; off += 8)
    {
      unsigned long v;
      long tag = hs_peek (s->pbuf + off, &v);
      if (v != value)
        continue;
      int k = frame_of (off);
      char sb[128], rb[64];
      if (k < 0)
        printf ("    %s at procedure stack offset %llu (of %llu): below the "
                "frames\n", what, off, s->ps);
      else
        printf ("    %s at procedure stack offset %llu (of %llu), tag %ld: "
                "frame %d from the top, returning to %s, wbs %u: byte %llu "
                "of its %llu = %s\n", what, off, s->ps, tag, k,
                symname (frames[k].ip, sb, sizeof sb), frames[k].wbs,
                off - frames[k].lo, frames[k].hi - frames[k].lo,
                reg_at (off - frames[k].lo, rb, sizeof rb));
      hits++;
    }
  if (!hits)
    printf ("    %s: not in the procedure stack\n", what);
  return hits;
}

static void
print_top_frames (int n)
{
  char sb[128];
  for (int k = 0; k < n && k < nframes; k++)
    printf ("    frame %d: returns to %s, wbs %u (%u bytes at %llu), "
            "wfx %u, pm %u\n", k, symname (frames[k].ip, sb, sizeof sb),
            frames[k].wbs, frames[k].wbs * 32, frames[k].lo, frames[k].wfx,
            frames[k].pm);
}

/* What a collector's mark loop does with each word: compare it with the
   heap bounds.  */
long __attribute__ ((noinline))
plain_scan (const unsigned long *p, long n, unsigned long lo,
            unsigned long hi)
{
  long c = 0;
  for (long i = 0; i < n; i++)
    if (p[i] >= lo && p[i] < hi)
      c++;
  return c;
}

/* 1-3: the marker frame.  */
static struct snap snap1;

void __attribute__ ((noinline))
marks_cb (void *arg)
{
  unsigned long m = MARK_C;
  asm volatile ("" : "+r" (m));
  snap1.pbuf = pbuf_main;
  snap1.cbuf = cbuf_main;
  if (*(int *) arg)
    {
      if (take_snap_direct (&snap1))
        printf ("  take_snap_direct failed\n");
    }
  else if (take_snap (&snap1))
    printf ("  take_snap failed: errno %d\n", errno);
  asm volatile ("" : : "r" (m));
}

static void
dump_marks (const char *name, void (*marks) (void (*) (void *), void *,
                                             unsigned long, long *),
            int direct)
{
  char sb[128];
  printf ("%s%s:\n", name, direct ? ", stacks read by hs_direct" : "");
  marks (marks_cb, &direct, BASE, (long *) 8);
  struct snap *s = &snap1;
  map_frames (s);
  printf ("  procedure stack %llu bytes, chain stack %llu bytes "
          "(%llu frames)\n", s->ps, s->cs, s->cs / 32);
  print_top_frames (5);
  int k;
  for (k = 0; k < nframes; k++)
    if (!strncmp (symname (frames[k].ip, sb, sizeof sb), name, strlen (name)))
      break;
  if (k == nframes)
    {
      printf ("  no frame returns into %s\n", name);
      return;
    }
  printf ("  frame %d (%s): bytes %llu..%llu\n", k, sb, frames[k].lo,
          frames[k].hi);
  for (unsigned long long off = frames[k].lo; off < frames[k].hi; off += 8)
    {
      unsigned long v;
      long tag = hs_peek (s->pbuf + off, &v);
      printf ("    +%3llu tag %ld  %016lx", off - frames[k].lo, tag, v);
      if (v - BASE < 16)
        printf ("  = %%r%lu", v - BASE);
      printf ("\n");
      if (v == BASE + 1 && new_layout < 0)
        new_layout = off - frames[k].lo == 16;
    }
  find_value (s, MARK_C, "marks_cb's local (live across the system calls)");
  if (direct)
    for (int i = 6; i <= 15; i += 9)
      {
        char what[64];
        snprintf (what, sizeof what, "hs_direct's %%r%d", i);
        find_value (s, BASE_D + i, what);
      }
}

static long tagged_word[2];

static void
tag_scan (void)
{
  unsigned long v;
  long rt = hs_tagtest (&tagged_word[0], (long *) 8);
  long mt = hs_peek (&tagged_word[0], &v);
  printf ("A diagnostic register stored with std,sm: register tag %ld; "
          "hs_peek of the memory word: tag %ld, %016lx\n", rt, mt, v);
  fflush (stdout);
  pid_t cp = fork ();
  if (cp == 0)
    {
      long c = plain_scan ((const unsigned long *) tagged_word, 1, 0, ~0UL);
      printf ("  plain_scan of that word: %ld\n", c);
      fflush (stdout);
      _exit (0);
    }
  int cst;
  waitpid (cp, &cst, 0);
  if (WIFSIGNALED (cst))
    printf ("  plain_scan of that word: killed by signal %d (%s)\n",
            WTERMSIG (cst), strsignal (WTERMSIG (cst)));

  /* The diagnostic word is in the marker frame's area of snap1.  */
  printf ("A plain C loop (plain_scan, lcc -O2) over the copy of the "
          "procedure stack:\n");
  fflush (stdout);
  pid_t pid = fork ();
  if (pid == 0)
    {
      long c = plain_scan ((const unsigned long *) snap1.pbuf, snap1.ps / 8,
                           BASE, BASE + 16);
      printf ("  child: %ld words in [BASE, BASE + 16)\n", c);
      fflush (stdout);
      _exit (0);
    }
  int st;
  waitpid (pid, &st, 0);
  if (WIFSIGNALED (st))
    printf ("  child killed by signal %d (%s)\n", WTERMSIG (st),
            strsignal (WTERMSIG (st)));
  fflush (stdout);
}

/* 4: a heap pointer only in a register of outer.  */
static volatile unsigned long hidden;
static struct snap snap4;
static int depth4;

void __attribute__ ((noinline))
scan_here (void)
{
  unsigned long target = hidden ^ HIDE;
  snap4.pbuf = pbuf_main;
  snap4.cbuf = cbuf_main;
  if (take_snap (&snap4))
    printf ("  take_snap failed\n");
  map_frames (&snap4);
  printf ("  depth %d: procedure stack %llu bytes, %llu chain frames\n",
          depth4, snap4.ps, snap4.cs / 32);
  int hits = find_value (&snap4, target, "the pointer");
  /* The data stack, from here to the top of the main stack.  */
  unsigned long *sp = (unsigned long *) &target, *end = __libc_stack_end;
  int dhits = 0;
  for (unsigned long *p = sp; p < end; p++)
    if (*p == target && p != (unsigned long *) &target)
      dhits++;
  printf ("    in the data stack (%ld bytes above this frame): %d times\n",
          (long) ((char *) end - (char *) sp), dhits);
  sink += hits;
}

void __attribute__ ((noinline))
down (int d)
{
  if (d <= 0)
    scan_here ();
  else
    down (d - 1);
  sink++;
}

void __attribute__ ((noinline))
outer (int depth)
{
  void *p = malloc (48);
  hidden = (unsigned long) p ^ HIDE;
  asm volatile ("" : "+r" (p));
  depth4 = depth;
  down (depth);
  asm volatile ("" : : "r" (p));
  /* Not freed: the next run gets another address, so a stale copy of
     this one can't be taken for it.  */
}

/* 5: a thread stopped by a signal.  */
static volatile long spin_flag;
static struct snap snap5;

static void
on_usr1 (int sig, siginfo_t *si, void *ucv)
{
  /* %g0..%g15 are neither saved nor cleared around a handler
     (registers.md): read %g4 before anything else.  */
  unsigned long g4;
  asm volatile ("addd 0x0, %%dg4, %0" : "=r" (g4));
  ucontext_t *uc = ucv;
  (void) sig;
  snap5.pbuf = pbuf_sig;
  snap5.cbuf = cbuf_sig;
  if (take_snap (&snap5))
    printf ("  take_snap failed in the handler\n");
  map_frames (&snap5);
  printf ("  in the handler: procedure stack %llu bytes, chain stack %llu; "
          "uc_extra.proc_stack_offset %llu, chain_stack_offset %llu\n",
          snap5.ps, snap5.cs, uc->uc_extra.proc_stack_offset,
          uc->uc_extra.chain_stack_offset);
  print_top_frames (8);
  find_value (&snap5, MARK_SPIN, "hs_spin's %r4");
  find_value (&snap5, MARK_SPIN + 1, "hs_spin's %g20");
  find_value (&snap5, MARK_SPIN + 2, "hs_spin's %g4");
  for (int i = 0; i < 16; i++)
    if (uc->uc_extra.g[i] == MARK_SPIN + 1 || uc->uc_extra.g[i] == MARK_SPIN + 2)
      printf ("    uc_extra.g[%d] (%%g%d) = %%g%s's value, gtag %u\n", i,
              16 + i, uc->uc_extra.g[i] == MARK_SPIN + 1 ? "20" : "4",
              uc->uc_extra.gtag[i]);
  /* The whole rt_sigframe on the data stack: 64 bytes below siginfo,
     4,272 bytes (kernel-interface.md, "Signals").  */
  const unsigned long *fr = (const unsigned long *) ((char *) si - 64);
  for (int w = 0; w < 4272 / 8; w++)
    if (fr[w] == MARK_SPIN + 1 || fr[w] == MARK_SPIN + 2)
      printf ("    in the signal frame on the data stack: %%g%s's value at "
              "frame + %d (ucontext + %ld)\n",
              fr[w] == MARK_SPIN + 1 ? "20" : "4", w * 8,
              (long) ((char *) &fr[w] - (char *) uc));
  printf ("    the handler's own %%g4 at entry: %s (%016lx)\n",
          g4 == MARK_SPIN + 2 ? "hs_spin's value" : "something else", g4);
  printf ("    the interrupted IP (uc_mcontext.cr0_hi) %s\n",
          symname (uc->uc_mcontext.cr0_hi & 0xfffffffffff8UL,
                   (char[128]) { 0 }, 128));
  fflush (stdout);
  spin_flag = 1;
}

static void *
spinner (void *arg)
{
  (void) arg;
  return (void *) hs_spin (&spin_flag, MARK_SPIN);
}

static void
stopped_thread (void)
{
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = on_usr1;
  sa.sa_flags = SA_SIGINFO;
  sigaction (SIGUSR1, &sa, 0);
  pthread_t t;
  pthread_create (&t, 0, spinner, 0);
  usleep (100000);
  pthread_kill (t, SIGUSR1);
  void *ret;
  pthread_join (t, &ret);
  printf ("  hs_spin returned %s\n", (unsigned long) ret == MARK_SPIN
          ? "its marker: %r4, %g20 and %g4 intact" : "something else");
}

#if __iset__ >= 5
/* 7: hs_qp_frame (fn, hi, lo): %qpr4 = hi:lo (qppackdl: hi the upper
   half), %r1 and %r2 cleared, then fn () with wbs = 8, so %r0..%r15 are
   this frame's part of the procedure stack.  In C (top-level asm) so
   that hwscan.s still assembles for v3.  */
__asm__ (
"	.text\n"
"	.global	hs_qp_frame\n"
"	.type	hs_qp_frame, #function\n"
"	.align	8\n"
"hs_qp_frame:\n"
"	{\n"
"	  setwd	wsz = 0xa, nfx = 0x1, dbl = 0x0\n"
"	  movtd,0	%dr0, %ctpr1\n"
"	  qppackdl,1	%dr1, %dr2, %qpr4\n"
"	}\n"
"	{\n"
"	  addd,0	0x0, 0x0, %dr1\n"
"	  addd,1	0x0, 0x0, %dr2\n"
"	}\n"
"	{\n"
"	  nop 7\n"
"	}\n"
"	{\n"
"	  call	%ctpr1, wbs = 0x8\n"
"	}\n"
"	{\n"
"	  return	%ctpr3\n"
"	}\n"
"	{\n"
"	  nop 5\n"
"	}\n"
"	{\n"
"	  ct	%ctpr3\n"
"	}\n"
"	.size	hs_qp_frame, .-hs_qp_frame\n");
void hs_qp_frame (void (*) (void), unsigned long, unsigned long);

#define QP_HI 0x5150484900000000UL   /* "QPHI" */
#define QP_LO 0x51504c4f00000000UL   /* "QPLO" */
static struct snap snap7;

void __attribute__ ((noinline))
qp_cb (void)
{
  snap7.pbuf = pbuf_main;
  snap7.cbuf = cbuf_main;
  if (take_snap (&snap7))
    printf ("  take_snap failed\n");
}

static void
qp_section (void)
{
  printf ("7. A 128-bit register: %%qpr4 of hs_qp_frame = QP_HI:QP_LO "
          "(qppackdl)\n");
  hs_qp_frame (qp_cb, QP_HI, QP_LO);
  map_frames (&snap7);
  find_value (&snap7, QP_LO, "the low half");
  find_value (&snap7, QP_HI, "the high half");
}
#endif

#ifdef HWSCAN_LIBGC
/* 6: the system's Boehm GC.  */
static volatile int fin_keep[64], fin_drop, fin_marks;

static void
fin (void *obj, void *cd)
{
  (void) obj;
  *(volatile int *) cd = 1;
}

static int gc_scan;

void __attribute__ ((noinline))
gc_bottom (void)
{
  GC_gcollect ();
  GC_invoke_finalizers ();
  if (gc_scan)
    scan_here ();
}

void __attribute__ ((noinline))
gc_down (int d)
{
  if (d <= 0)
    gc_bottom ();
  else
    gc_down (d - 1);
  sink++;
}

void __attribute__ ((noinline))
gc_keep (int depth)
{
  void *p = GC_MALLOC (64);
  GC_REGISTER_FINALIZER (p, fin, (void *) &fin_keep[depth], 0, 0);
  hidden = (unsigned long) p ^ HIDE;
  asm volatile ("" : "+r" (p));
  depth4 = depth;
  gc_down (depth);
  asm volatile ("" : : "r" (p));
}

void __attribute__ ((noinline))
gc_alloc_drop (void)
{
  void *p = GC_MALLOC (64);
  GC_REGISTER_FINALIZER (p, fin, (void *) &fin_drop, 0, 0);
  hidden = (unsigned long) p ^ HIDE;
}

void __attribute__ ((noinline))
gc_drop (int depth)
{
  gc_alloc_drop ();
  depth4 = depth;
  gc_down (depth);
}

void __attribute__ ((noinline))
gc_marks_cb (void *arg)
{
  (void) arg;
  GC_gcollect ();
  sink++;
}

/* A thread spinning in hs_spin with the object only in hs_spin's
   registers (KEEP) or with no reference at all; the main thread collects,
   which stops it with a signal.  */
static volatile int fin_thread[16];   /* one flag per case */
static volatile long gc_spin_flag;
static int gc_case;

void *__attribute__ ((noinline))
gc_thread_alloc (void)
{
  void *o = GC_MALLOC (64);
  GC_REGISTER_FINALIZER (o, fin, (void *) &fin_thread[gc_case], 0, 0);
  return o;
}

/* KEEP 1: the object in hs_spin's %r1 and %r4 (and %g20 = object + 1);
   2..7: only in %r1, %r4, %r7, %g20, %r1 and %r4, %g20 + 1 of
   hs_spin_REG (hidden from every other register); 8, 9: in the frame
   below (gc_holder, gc_holder_mem); 10: only in %g4; 0: dropped
   (control).  */
static long (*const spin_only[]) (volatile long *, long, long) = {
  hs_spin_r1, hs_spin_r4, hs_spin_r7, hs_spin_g20, hs_spin_r1r4,
  hs_spin_g20p1
};
static const char *const spin_name[] = {
  "dropped (control)", "in hs_spin's %r1 and %r4", "only in %r1",
  "only in %r4", "only in %r7", "only in %g20", "only in %r1 and %r4",
  "only in %g20 as object + 1",
  "only in a register of gc_holder, the frame below the interrupted one",
  "only in a local of gc_holder_mem on the data stack",
  "only in %g4 (not saved in the signal frame)"
};

/* The object only in a register of this frame, below the interrupted
   hs_spin (case 8), or only in this frame's data stack (case 9).  */
void __attribute__ ((noinline))
gc_holder (unsigned long h)
{
  void *o = (void *) (h ^ HIDE);
  asm volatile ("" : "+r" (o));
  hs_spin (&gc_spin_flag, 0);
  asm volatile ("" : : "r" (o));
}

void __attribute__ ((noinline))
gc_holder_mem (unsigned long h)
{
  void *volatile o = (void *) (h ^ HIDE);
  hs_spin (&gc_spin_flag, 0);
  o = 0;
}

static void *
gc_spinner (void *keep)
{
  long k = (long) keep;
  if (k == 1)
    return (void *) hs_spin (&gc_spin_flag, (long) gc_thread_alloc ());
  if (k == 10)
    return (void *) hs_spin_g4 (&gc_spin_flag,
                                (long) gc_thread_alloc () ^ HIDE, HIDE);
  if (k == 8)
    gc_holder ((unsigned long) gc_thread_alloc () ^ HIDE);
  else if (k == 9)
    gc_holder_mem ((unsigned long) gc_thread_alloc () ^ HIDE);
  if (k >= 8)
    return 0;
  if (k >= 2)
    return (void *) spin_only[k - 2] (&gc_spin_flag,
                                      (long) gc_thread_alloc () ^ HIDE, HIDE);
  gc_thread_alloc ();
  return (void *) hs_spin (&gc_spin_flag, 0);
}

static void
gc_thread_test (int keep)
{
  pthread_t t;
  /* Finalize what earlier cases left first: each case has its own flag,
     so a late finalizer can't be taken for this case's.  */
  GC_gcollect ();
  GC_invoke_finalizers ();
  gc_case = keep;
  fin_thread[keep] = 0;
  gc_spin_flag = 0;
  pthread_create (&t, 0, gc_spinner, (void *) (long) keep);
  usleep (100000);
  GC_gcollect ();
  GC_invoke_finalizers ();
  int during = fin_thread[keep];
  gc_spin_flag = 1;
  pthread_join (t, 0);
  printf ("  object %s of a thread stopped by GC_gcollect: %s\n",
          spin_name[keep], during ? "finalized" : "kept");
}

static void
libgc_tests (void)
{
  GC_INIT ();
  printf ("6. Boehm GC %u.%u.%u (GC_get_version)\n",
          GC_get_version () >> 16, (GC_get_version () >> 8) & 0xff,
          GC_get_version () & 0xff);
  for (int d = 3; d <= 40; d += 37)
    {
      GC_gcollect ();
      GC_invoke_finalizers ();
      fin_keep[d] = 0;
      gc_scan = 1;
      gc_keep (d);
      int during = fin_keep[d];
      gc_scan = 0;
      GC_gcollect ();
      GC_invoke_finalizers ();
      /* gc_keep -> gc_down (d) .. gc_down (0) -> gc_bottom -> GC_gcollect */
      printf ("  object only in a register of gc_keep, GC_gcollect %d calls "
              "down: %s; after gc_keep returned and another GC_gcollect: "
              "%s\n", d + 3, during ? "FREED while referenced" : "kept",
              fin_keep[d] ? "finalized" : "still kept");
    }
  GC_gcollect ();
  GC_invoke_finalizers ();
  fin_drop = 0;
  gc_scan = 1;
  gc_drop (3);
  printf ("  control, object dropped before GC_gcollect: %s\n",
          fin_drop ? "finalized" : "kept (a stale copy, see above)");
  gc_scan = 0;
  fflush (stdout);
  hs_marks_fx1 (gc_marks_cb, 0, BASE, (long *) 8);
  printf ("  GC_gcollect under a frame holding a diagnostic value: "
          "survived\n");
  for (int k = 1; k <= 10; k++)
    gc_thread_test (k);
  gc_thread_test (0);
}
#endif

int
main (int argc, char **argv)
{
  setvbuf (stdout, 0, _IOLBF, 0);
#ifdef HWSCAN_LIBGC
  if (argc > 1 && !strcmp (argv[1], "gc"))
    {
      libgc_tests ();
      return 0;
    }
#else
  (void) argc;
  (void) argv;
#endif
  printf ("1-3. Register layout in the procedure stack\n");
  dump_marks ("hs_marks_fx1", hs_marks_fx1, 0);
  tag_scan ();
  dump_marks ("hs_marks_fx0", hs_marks_fx0, 0);
  dump_marks ("hs_marks_fx1", hs_marks_fx1, 1);
  printf ("Layout: %s\n", new_layout < 0 ? "unknown"
          : new_layout ? "16 bytes per register (value, extension)"
          : "32 bytes per register pair (two values, two extensions)");

  printf ("4. A heap pointer only in a register of outer ()\n");
  for (int d = 3; d <= 40; d += 37)
    outer (d);

  printf ("5. A thread stopped by SIGUSR1 while spinning in hs_spin\n");
  stopped_thread ();
#if __iset__ >= 5
  qp_section ();
#endif
#ifdef HWSCAN_LIBGC
  libgc_tests ();
#endif
  return 0;
}
