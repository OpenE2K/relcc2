/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Prints "101 102 1": the resolved implementation is called directly and
   through an address taken in another object, and the addresses agree.  */
#include <stdio.h>
int f(int);
int (*f_addr(void))(int);
int main(void) { printf("%d %d %d\n", f(1), f_addr()(2), f_addr() == f); return 0; }
