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
	.global	g_ext
	.type	g_ext, #function
g_ext:
	{
	  setwd	wsz = 0x4, nfx = 0x1, dbl = 0x0
	  return	%ctpr3
	  addd,0	%dr0, 0x1, %dr0
	}
	{
	  ct	%ctpr3
	}
	.size	g_ext, .-g_ext
