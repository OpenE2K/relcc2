! SPDX-License-Identifier: GPL-3.0-or-later
! unsigned long rdclk(void): %clkr
	.text
	.global	rdclk
	.type	rdclk, #function
	.align	8
rdclk:
	{
	  return	%ctpr3
	  rrd,0	%clkr, %dr0
	}
	{
	  nop 5
	}
	{
	  ct	%ctpr3
	}
	.size	rdclk, .-rdclk
