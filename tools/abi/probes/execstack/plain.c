/* SPDX-License-Identifier: GPL-3.0-or-later */
/* An ordinary program for the GNU-stack note tests: built with lcc, it
   has no trampoline and no assembly (tools/abi/probes/execstack.sh). */
int helper(int x);
int main(void) { return helper(41) - 42; }
