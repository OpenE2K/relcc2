/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Loops that lcc -O3 compiles with speculative FP operations (,sm), in a
   file of their own so that the compiler doesn't see the caller's
   feclearexcept; specfp.c runs them (spec/isa/speculation.md, "What lcc
   speculates").  */

double
dot (const double *a, const double *b, int n)
{
  double s = 0;
  for (int i = 0; i < n; i++)
    s += a[i] * b[i];
  return s;
}

void
condstore (double *restrict a, const double *restrict b, int n)
{
  for (int i = 0; i < n; i++)
    {
      double t = b[i] / 3.0;
      if (t > 1.0)
        a[i] = t;
    }
}
