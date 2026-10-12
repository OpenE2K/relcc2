/* SPDX-License-Identifier: GPL-3.0-or-later */
/* isets: 5-7
   wait fields added in v5: sas (bit 7) and sal (bit 8).  */

	.text
	.global	wt5
wt5:
/* gas sets st_c with sas and ld_c with sal.  */
{
	wait sas = 1
}
{
	wait sal = 1
}
{
	wait st_c = 1, sas = 0
}
{
	wait ld_c = 1, sal = 0
}
{
	wait st_c = 1, ld_c = 1, sas = 1, sal = 1
}
{
	wait all_c = 1, all_e = 1, st_c = 1, ld_c = 1, fl_c = 1, ma_c = 1, trap = 1, sas = 1, sal = 1
}
/* A modifier without its st_c or ld_c, which gas cannot write: raw
   wide instructions (a header word announcing only CS1, then CS1), as
   MCST's kernel writes its waits (__E2K_WAIT in asm/e2k_api.h). objdump
   does not print sas or sal here.  */
	.word	0x00008001
	.word	0x30000080	/* sas */
	.word	0x00008001
	.word	0x30000100	/* sal */
	.word	0x00008001
	.word	0x30000180	/* sas, sal */
