/* SPDX-License-Identifier: GPL-3.0-or-later */
/* isets: 7-7
   wait fields added in v7: macp (bit 14) and fl_c_mode (15). objdump
   prints neither.  */

	.text
	.global	wt7
wt7:
{
	wait macp = 1
}
/* gas takes fl_c_mode only with fl_c = 1.  */
{
	wait fl_c = 1, fl_c_mode = 1
}
{
	wait fl_c = 1, fl_c_mode = 0
}
{
	wait all_c = 1, all_e = 1, st_c = 1, ld_c = 1, fl_c = 1, ma_c = 1, trap = 1, sas = 1, sal = 1, las = 1, lal = 1, mt = 1, int = 1, mem_mod = 1, macp = 1, fl_c_mode = 1
}
/* fl_c_mode without fl_c: raw, as in wait_v5plus.s.  */
	.word	0x00008001
	.word	0x30008000	/* fl_c_mode */
