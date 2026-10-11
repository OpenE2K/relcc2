// SPDX-License-Identifier: GPL-3.0-or-later
// Unwinding through signal frames and thread cancellation with lcc's
// runtime (MCST libgcc's unwinder, glibc): spec/abi/kernel-interface.md.
//
//   L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/l++
//   $L -O2 -rdynamic sigeh.cc sigeh-c.c sysabi.s -o sigeh -lpthread
//   $L -O2 -rdynamic -fnon-call-exceptions sigeh.cc sigeh-c.c sysabi.s \
//      -o sigeh-nc -lpthread
//   ./sigeh; ./sigeh-nc                    # on hardware

#include <csignal>
#include <setjmp.h>
#include <cstdio>
#include <cxxabi.h>
#include <execinfo.h>
#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unwind.h>

extern "C" long sa_ldd (char *);

static void
in_child (const char *what, void (*fn) ())
{
  fflush (stdout);
  pid_t pid = fork ();
  if (pid == 0)
    {
      fn ();
      fflush (stdout);
      _exit (0);
    }
  int st;
  waitpid (pid, &st, 0);
  if (WIFSIGNALED (st))
    printf ("  [%s: killed by signal %d]\n", what, WTERMSIG (st));
  else if (WEXITSTATUS (st))
    printf ("  [%s: exit %d]\n", what, WEXITSTATUS (st));
  fflush (stdout);
}

extern "C" volatile int dtors;
volatile int dtors;

struct Guard
{
  ~Guard () { dtors++; }
};

// 1. Throwing from a handler of a synchronous signal (raise).
static void
throwing_handler (int)
{
  throw 42;
}

static void __attribute__ ((noinline))
raiser ()
{
  Guard g;
  raise (SIGUSR1);
  printf ("    (returned from raise)\n");
}

static void
throw_raise ()
{
  signal (SIGUSR1, throwing_handler);
  dtors = 0;
  try
    {
      raiser ();
      printf ("  raise: no exception\n");
    }
  catch (int v)
    {
      printf ("  raise: caught %d in main, %d destructor(s) ran\n", v,
              dtors);
    }
}

// 1b. Throwing and catching inside the handler.
static void
local_handler (int)
{
  Guard g;
  try
    {
      throw 7;
    }
  catch (int v)
    {
      printf ("  inside the handler: caught %d\n", v);
    }
}

static void
throw_local ()
{
  signal (SIGUSR1, local_handler);
  raise (SIGUSR1);
  printf ("  ... and the program went on\n");
}

// 2. Throwing from a SIGSEGV handler of a load fault (needs
// -fnon-call-exceptions in the faulting frame, but sa_ldd is assembly
// without unwind info, so fault in C++ code too).
static volatile char *nullp = (char *) 0x1000;

static void __attribute__ ((noinline))
faulter_asm ()
{
  Guard g;
  sa_ldd ((char *) nullp);
}

static void __attribute__ ((noinline))
faulter_cc ()
{
  Guard g;
  printf ("    (read %d)\n", *nullp);
}

static void
throw_fault_asm ()
{
  signal (SIGSEGV, throwing_handler);
  dtors = 0;
  try
    {
      faulter_asm ();
    }
  catch (int v)
    {
      printf ("  fault in an assembly leaf: caught %d, %d destructor(s) "
              "ran\n", v, dtors);
    }
}

static void
throw_fault_cc ()
{
  signal (SIGSEGV, throwing_handler);
  dtors = 0;
  try
    {
      faulter_cc ();
    }
  catch (int v)
    {
      printf ("  fault in C++ code: caught %d, %d destructor(s) ran\n", v,
              dtors);
    }
}

// 3. Backtraces from inside a handler.
static void *main_lo, *main_hi;

struct Trace
{
  int n, reached_main;
};

static _Unwind_Reason_Code
trace_fn (struct _Unwind_Context *ctx, void *arg)
{
  Trace *t = (Trace *) arg;
  void *ip = (void *) _Unwind_GetIP (ctx);
  if (t->n < 12)
    printf ("    #%d %p\n", t->n, ip);
  if (ip >= main_lo && ip < main_hi)
    t->reached_main = 1;
  t->n++;
  return _URC_NO_REASON;
}

static void
trace_handler (int)
{
  Trace t = { 0, 0 };
  printf ("  _Unwind_Backtrace from a handler:\n");
  _Unwind_Reason_Code r = _Unwind_Backtrace (trace_fn, &t);
  printf ("    %d frames, end code %d, %s main\n", t.n, (int) r,
          t.reached_main ? "reached" : "did not reach");
  void *buf[32];
  int n = backtrace (buf, 32);
  printf ("  glibc backtrace from a handler: %d frames\n", n);
  fflush (stdout);
  backtrace_symbols_fd (buf, n, 1);
}

static void __attribute__ ((noinline))
trace_raiser ()
{
  raise (SIGUSR2);
}

static void
trace_case ()
{
  signal (SIGUSR2, trace_handler);
  trace_raiser ();
}

// 4. Thread cancellation and pthread_exit run destructors.
static volatile int cancel_caught_forced, cancel_caught_all;

static void __attribute__ ((noinline))
cancel_inner ()
{
  Guard g;
  for (;;)
    pause ();
}

static void *
cancel_thread (void *)
{
  Guard g;
  try
    {
      cancel_inner ();
    }
  catch (abi::__forced_unwind &)
    {
      cancel_caught_forced = 1;
      throw;
    }
  return 0;
}

static void __attribute__ ((noinline))
exit_inner ()
{
  Guard g;
  pthread_exit (0);
}

static void *
exit_thread (void *)
{
  Guard g;
  try
    {
      exit_inner ();
    }
  catch (...)
    {
      cancel_caught_all = 1;
      throw;
    }
  return 0;
}

static void __attribute__ ((noinline))
test_inner ()
{
  Guard g;
  for (;;)
    pthread_testcancel ();
}

static void *
test_thread (void *)
{
  Guard g;
  try
    {
      test_inner ();
    }
  catch (abi::__forced_unwind &)
    {
      cancel_caught_forced = 1;
      throw;
    }
  return 0;
}

extern "C" void *sigeh_plain_thread (void *);   // sigeh-c.c: pause() in C

static void
run_cancel (const char *what, void *(*fn) (void *), int cancel)
{
  pthread_t t;
  void *ret;
  dtors = 0;
  cancel_caught_forced = cancel_caught_all = 0;
  pthread_create (&t, 0, fn, 0);
  if (cancel)
    {
      usleep (100000);
      pthread_cancel (t);
    }
  pthread_join (t, &ret);
  printf ("  %s: %s, %d destructor(s) ran, forced unwind %s\n", what,
          ret == PTHREAD_CANCELED ? "canceled" : "exited", dtors,
          cancel_caught_forced || cancel_caught_all ? "caught and rethrown"
          : "not caught");
}

static void
cancel_plain ()
{
  run_cancel ("C thread blocked in pause(), canceled", sigeh_plain_thread, 1);
}

static void
cancel_test ()
{
  run_cancel ("C++ thread in a pthread_testcancel loop, canceled",
              test_thread, 1);
}

static void
cancel_blocked ()
{
  run_cancel ("C++ thread blocked in pause(), canceled", cancel_thread, 1);
}

static void
cancel_exit ()
{
  run_cancel ("C++ thread calling pthread_exit", exit_thread, 0);
}

// 5. siglongjmp out of a handler restores the mask.
static sigjmp_buf jb;

static void
jump_handler (int)
{
  siglongjmp (jb, 1);
}

static void
jump_case ()
{
  signal (SIGUSR1, jump_handler);
  if (!sigsetjmp (jb, 1))
    raise (SIGUSR1);
  sigset_t s;
  sigprocmask (SIG_BLOCK, 0, &s);
  printf ("  siglongjmp out of a handler: SIGUSR1 %s afterwards\n",
          sigismember (&s, SIGUSR1) ? "still blocked" : "unblocked");
}

int
main ()
{
  setvbuf (stdout, 0, _IOLBF, 0);
  main_lo = (void *) main;
  main_hi = (char *) main_lo + 4096;
  printf ("1. throw from a raise()d handler:\n");
  in_child ("raise", throw_raise);
  in_child ("inside", throw_local);
  printf ("2. throw from a SIGSEGV handler:\n");
  in_child ("fault in assembly", throw_fault_asm);
  in_child ("fault in C++", throw_fault_cc);
  printf ("3. backtraces from a handler:\n");
  in_child ("backtrace", trace_case);
  printf ("4. cancellation:\n");
  in_child ("cancel C blocked", cancel_plain);
  in_child ("cancel testcancel", cancel_test);
  in_child ("cancel C++ blocked", cancel_blocked);
  in_child ("pthread_exit", cancel_exit);
  printf ("5. siglongjmp:\n");
  in_child ("siglongjmp", jump_case);
  return 0;
}
