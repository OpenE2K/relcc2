! SPDX-License-Identifier: GPL-3.0-or-later
	.ignore	ld_st_style
	.text
	.global	f_glob
	.type	f_glob, #function
f_glob:
	{
	  return	%ctpr3
	}
	{
	  ct	%ctpr3
	}
	.type	f_loc, #function
f_loc:
	{
	  return	%ctpr3
	}
	{
	  ct	%ctpr3
	}
	.global	f_hid
	.hidden	f_hid
	.type	f_hid, #function
f_hid:
	{
	  return	%ctpr3
	}
	{
	  ct	%ctpr3
	}
	.weak	w_undef
	.global	probe_islocal
	.type	probe_islocal, #function
probe_islocal:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x1
	  return	%ctpr3
	  addd,0 0x0, 0x0, %dr0
	}
	{
	  nop 7
	}
	{
	  ct	%ctpr3
	}
	.global	probe_islocal32
	.type	probe_islocal32, #function
probe_islocal32:
	{
	  setwd	wsz = 0x8, nfx = 0x1, dbl = 0x1
	  return	%ctpr3
	  addd,0 0x0, 0x0, %dr0
	}
	{
	  addd,0 0x0, [ _f32s,_lts0 f_loc@ISLOCAL32 ], %dr2
	}
	{
	  addd,0 0x0, [ _f32s,_lts0 f_glob@ISLOCAL32 ], %dr3
	}
	{
	  addd,0 0x0, [ _f32s,_lts0 f_hid@ISLOCAL32 ], %dr4
	}
	{
	  addd,0 0x0, [ _f32s,_lts0 f_other@ISLOCAL32 ], %dr5
	}
	{
	  addd,0 0x0, [ _f32s,_lts0 f_ext@ISLOCAL32 ], %dr6
	}
	{
	  addd,0 0x0, [ _f32s,_lts0 w_undef@ISLOCAL32 ], %dr7
	}
	{
	  nop 7
	}
	{
	  shld,0 %dr2, 0x0, %dr2
	}
	{
	  ord,0 %dr0, %dr2, %dr0
	}
	{
	  shld,0 %dr3, 0x1, %dr3
	}
	{
	  ord,0 %dr0, %dr3, %dr0
	}
	{
	  shld,0 %dr4, 0x2, %dr4
	}
	{
	  ord,0 %dr0, %dr4, %dr0
	}
	{
	  shld,0 %dr5, 0x3, %dr5
	}
	{
	  ord,0 %dr0, %dr5, %dr0
	}
	{
	  shld,0 %dr6, 0x4, %dr6
	}
	{
	  ord,0 %dr0, %dr6, %dr0
	}
	{
	  shld,0 %dr7, 0x5, %dr7
	}
	{
	  ord,0 %dr0, %dr7, %dr0
	}
	{
	  ct	%ctpr3
	}
	.data
	.global	dyn_tab
	.align	8
dyn_tab:
	.dword	f_loc@DYNOPT
	.dword	f_glob@DYNOPT
	.dword	f_hid@DYNOPT
	.dword	f_other@DYNOPT
	.dword	f_ext@DYNOPT
	.dword	w_undef@DYNOPT
	.global	sym_addr
	.align	8
sym_addr:
	.dword	f_loc
	.dword	f_glob
	.dword	f_hid
	.dword	f_other
	.dword	f_ext
	.dword	w_undef
