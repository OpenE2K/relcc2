/* SPDX-License-Identifier: GPL-3.0-or-later */
/* isets: 6-7
   wait fields added in v6: las (bit 9), lal (10), mt (11), int (12) and
   mem_mod (13). objdump prints none of them.  */

	.text
	.global	wt6
wt6:
/* gas sets st_c with las and ld_c with lal.  */
{
	wait las = 1
}
{
	wait lal = 1
}
{
	wait mt = 1
}
{
	wait int = 1
}
{
	wait mem_mod = 1
}
/* MCST's kernel barriers (asm/e2k_api.h).  */
{
	wait ld_c = 1, lal = 1, sal = 1
}
{
	wait ld_c = 1, lal = 1, mt = 1
}
{
	wait st_c = 1, sas = 1, las = 1
}
{
	wait all_c = 1, all_e = 1, st_c = 1, ld_c = 1, fl_c = 1, ma_c = 1, trap = 1, sas = 1, sal = 1, las = 1, lal = 1, mt = 1, int = 1, mem_mod = 1
}
/* las and lal without st_c and ld_c: raw, as in wait_v5plus.s.  */
	.word	0x00008001
	.word	0x30000200	/* las */
	.word	0x00008001
	.word	0x30000400	/* lal */
