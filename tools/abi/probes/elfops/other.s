! SPDX-License-Identifier: GPL-3.0-or-later
	.text
	.global	f_other
	.type	f_other, #function
f_other:
	{
	  return	%ctpr3
	}
	{
	  ct	%ctpr3
	}
