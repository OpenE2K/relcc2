/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Protected-mode (e2k128) shared library that pmlibmain.c loads with
   dlopen.  Built on the machine by run.sh --left:

     lcc -mptr128 -O1 -fPIC -shared pmdl.c -o libpmdl.so  */

int dl_var = 3;
int dl_fn (int x) { return x * dl_var; }
int *dl_ptr (void) { return &dl_var; }
