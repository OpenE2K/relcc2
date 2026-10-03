/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Kernels for validate.sh, compiled once per optimisation level with
   SUF naming the copy (k_sum_O2 ...).  The data fits in L1.  */

#define CAT2(a, b) a##_##b
#define CAT(a, b) CAT2 (a, b)
#define K(name) CAT (name, SUF)

/* Nothing but the call and the return.  */
long K (k_empty) (const long *a, long n)
{
  return n;
}

long K (k_sum) (const long *a, long n)
{
  long s = 0;
  for (long i = 0; i < n; i++)
    s += a[i];
  return s;
}

double K (k_dot) (const double *a, const double *b, long n)
{
  double s = 0;
  for (long i = 0; i < n; i++)
    s += a[i] * b[i];
  return s;
}

void K (k_saxpy) (double *restrict y, const double *restrict x, double a,
                  long n)
{
  for (long i = 0; i < n; i++)
    y[i] = a * x[i] + y[i];
}

unsigned K (k_hash) (const unsigned char *s, long n)
{
  unsigned h = 5381;
  for (long i = 0; i < n; i++)
    h = h * 33 + s[i];
  return h;
}

long K (k_branchy) (const int *a, long n)
{
  long s = 0;
  for (long i = 0; i < n; i++)
    {
      if (a[i] & 1)
        s += a[i] * 3;
      else
        s -= a[i] >> 1;
    }
  return s;
}

double K (k_poly) (const double *x, long n)
{
  double s = 0;
  for (long i = 0; i < n; i++)
    {
      double v = x[i];
      s += ((((v * 0.5 + 1.5) * v - 2.0) * v + 0.25) * v) - 1.0;
    }
  return s;
}

struct node { struct node *next; long v; };

long K (k_list) (const struct node *p)
{
  long s = 0;
  for (; p; p = p->next)
    s += p->v;
  return s;
}

void K (k_copy) (int *restrict d, const int *restrict s, long n)
{
  for (long i = 0; i < n; i++)
    d[i] = s[i] + 1;
}
