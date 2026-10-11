/* SPDX-License-Identifier: GPL-3.0-or-later */
/* What lcc's sanitizers emit (spec/abi/kernel-interface.md,
   "Sanitizers"): read the assembly.

     L=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $L -O2 -fsanitize=address -S sanitize.c -o asan.s
     $L -O2 -fsanitize=memory -S sanitize.c -o msan.s
     $L -O2 -fsanitize=leak -S sanitize.c -o lsan.s
     $L -O2 -fsanitize=undefined -S sanitize.c -o ubsan.s   (no change)
     $L -O2 -fsanitize=thread -S sanitize.c                 (refused)
     $L -fsanitize=address -v sanitize.c                    (the link)  */

int load4 (int *p) { return *p; }
void store8 (long *p, long v) { *p = v; }
int glob[10];
int useglob (int i) { return glob[i]; }
extern void sink (int *);
int stackvar (int i) { int a[8]; sink (a); return a[i]; }
void sink (int *p) { (void) p; }
int main (void) { return 0; }
