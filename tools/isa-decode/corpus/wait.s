/* SPDX-License-Identifier: GPL-3.0-or-later */
/* isets: 3-7
   wait with each field every version has (bundle.md, CS1 opcode 3).  */

	.text
	.global	wt
wt:
{
	wait all_c = 0
}
{
	wait all_c = 1
}
{
	wait all_e = 1
}
{
	wait st_c = 1
}
{
	wait ld_c = 1
}
{
	wait fl_c = 1
}
{
	wait ma_c = 1
}
{
	wait trap = 1
}
{
	wait all_c = 1, all_e = 1, st_c = 1, ld_c = 1, fl_c = 1, ma_c = 1, trap = 1
}
