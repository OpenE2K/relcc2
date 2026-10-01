/* SPDX-License-Identifier: GPL-3.0-or-later */
/* isets: 6-7
   Control operations added in v6.  */

	.text
cv6:
{
	ibranchd,0 %dr1, %empty
}
{
	icalld,0 wbs = 0x4, %dr1, %empty
}
{
	fillr
	fillc
}
{
	hcall 0x5, wbs = 0x4
}
{
	icall cv6, wbs = 0x8
}
{
	iret
}
{
	hret
}
{
	glaunch
}
