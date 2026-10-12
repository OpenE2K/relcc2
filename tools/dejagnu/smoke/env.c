/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Smoke test: the environment. Prints E2K_SMOKE_VAR, which smoke.exp sets
   on the build machine for a local target (qemu passes the environment
   on, as GCC's dg-set-target-env-var needs).  */
#include <stdio.h>
#include <stdlib.h>

int main (void)
{
  const char *v = getenv ("E2K_SMOKE_VAR");
  printf ("E2K_SMOKE_VAR=%s\n", v ? v : "(unset)");
  return 0;
}
