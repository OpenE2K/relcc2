! SPDX-License-Identifier: GPL-3.0-or-later
	.text
	.global	f_ext
	.type	f_ext, #function
f_ext:
	{
	  return	%ctpr3
	}
	{
	  ct	%ctpr3
	}
