/* SPDX-License-Identifier: GPL-3.0-or-later */
/* lcc's -mptr128-64 ("protected mode, linkable with -mptr64",
   experimental): read the code and the object's e_flags:

     /opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1/bin/lcc -mptr128-64 -O1 \
         -S pm6464.c
     /opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1/bin/lcc -mptr128-64 -O1 \
         -c pm6464.c && readelf -h pm6464.o

   pmcc.c does not build this way: gas rejects its initialised pointers
   (.ptr sym@AP, sym@PL) outside protected mode.  */

int pm_g = 5;

int pm_sum (int *p, int n)
{
	int s = 0;
	for (int i = 0; i < n; i++)
		s += p[i];
	return s;
}

int *pm_gp (void) { return &pm_g; }
