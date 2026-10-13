// SPDX-License-Identifier: GPL-3.0-or-later
// What type va_list is in C++ (docs/spec-poc-findings.md A27): the
// mangled names of functions taking one, read from the symbols:
//   /opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/l++ -O2 -c pocabi1_valist.cc
//   e2k-linux-nm pocabi1_valist.o     (or -S and read the labels)
// Built with -DCALLER it calls the four functions instead of defining
// them, so a caller from one compiler can be linked with definitions from
// another (an undefined symbol means the names differ).

#include <cstdarg>

#ifdef CALLER
long sum (int n, va_list ap);
long sum_ptr (int n, va_list *ap);
long sum_const (int n, const va_list ap);
namespace ns { long sum_std (int n, std::va_list ap); }

static long
call (int n, ...)
{
  va_list ap;
  va_start (ap, n);
  long s = sum (n, ap);
  va_end (ap);
  va_start (ap, n);
  s += sum_ptr (n, &ap);
  va_end (ap);
  va_start (ap, n);
  s += sum_const (n, ap);
  va_end (ap);
  va_start (ap, n);
  s += ns::sum_std (n, ap);
  va_end (ap);
  return s;
}

#include <stdio.h>
int
main (void)
{
  long s = call (3, 1L, 2L, 3L);
  printf ("%s %ld\n", s == 24 ? "ok" : "FAIL", s);
  return s != 24;
}
#else
long
sum (int n, va_list ap)
{
  long s = 0;
  while (n--)
    s += va_arg (ap, long);
  return s;
}

long
sum_ptr (int n, va_list *ap)
{
  long s = 0;
  while (n--)
    s += va_arg (*ap, long);
  return s;
}

long
sum_const (int n, const va_list ap)
{
  va_list c;
  __builtin_memcpy (&c, &ap, sizeof c);   // va_copy rejects a const source in clang
  long s = 0;
  while (n--)
    s += va_arg (c, long);
  return s;
}

namespace ns
{
long
sum_std (int n, std::va_list ap)
{
  long s = 0;
  while (n--)
    s += va_arg (ap, long);
  return s;
}
}

// In C terms: which pointer type __builtin_va_list is compatible with.
extern "C" int is_char_ptr (void)
{
  return __is_same (__builtin_va_list, char *);
}
extern "C" int is_void_ptr (void)
{
  return __is_same (__builtin_va_list, void *);
}
#endif
