/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Prints, for symbols of each kind, the value @ISLOCAL32 gives (1 if the
   linker considers the symbol to bind locally) and whether @DYNOPT gave
   the symbol's address. See spec/abi/elf.md and run.sh.  */
#include <stdio.h>
long probe_islocal(void), probe_islocal32(void);
extern long dyn_tab[6], sym_addr[6];
static const char *names[] = { "f_loc", "f_glob", "f_hid", "f_other", "f_ext", "w_undef" };
int main(void) {
	long a = probe_islocal(), b = probe_islocal32();
	for (int i = 0; i < 6; i++)
		printf("%-8s ISLOCAL32=%ld DYNOPT=%s\n", names[i], (b >> i) & 1,
		       dyn_tab[i] == 0 ? "0" : dyn_tab[i] == sym_addr[i] ? "addr" : "other");
	return 0;
}
