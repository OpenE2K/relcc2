/* SPDX-License-Identifier: GPL-3.0-or-later */
/* A one-function library built both as a 32-bit (-mptr32) and a 64-bit
   object, to see whether the two modes mix (spec/abi/mode32.md): run.sh
   links one mode's object into the other mode's program and dlopen()s
   each mode's shared library from the other mode's dlmix.  */

int mix_fn (int x) { return x + 1; }
