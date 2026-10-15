/* SPDX-License-Identifier: GPL-3.0-or-later */
/* What lcc's -gline writes, and what MCST's gdb makes of it (read
   statically: the program is never run under gdb, because gdb would
   plant its breakpoint trap inside a wide instruction).

     LCC=/opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/lcc
     $LCC -O3 -gline -S tools/abi/probes/debuginfo_gline.c -o gline.s
     $LCC -O3 -gline tools/abi/probes/debuginfo_gline.c -o gline
     readelf --debug-dump=rawline,decodedline gline
     gdb -batch -ex 'info line debuginfo_gline.c:LOOP_LINE' \
         -ex 'break debuginfo_gline.c:LOOP_LINE' -ex 'info breakpoints' ./gline

   (debuginfo.sh runs the gdb part with LOOP_LINE filled in.)

   lcc 1.29.16 and 1.31.05 (lcc output): -gline puts `.label .LM.N` in
   front of each operation inside the braces of a wide instruction and
   writes .debug_line itself (DWARF 2, minimum_instruction_length 4,
   one DW_LNE_set_address row per operation, all is_stmt, in schedule
   order, not address order).  MCST gas gives a .label the address of
   the syllable that encodes the next operation (ALS for an ALU
   operation, CS0 for disp, ...), so each row points inside a wide
   instruction.  -g instead writes minimum_instruction_length 8 and
   rows at wide-instruction starts.  ldis prints "! file : line" after
   each operation of a -gline object, after each wide instruction of a
   -g one.  -gline's .debug_info is a compile unit with DW_AT_stmt_list
   and no address range or functions, so MCST's gdb 15.2 maps none of
   the line table: "Line number N is out of range", "No line number
   information available" (4C, 8C2, 2C3).  */

int g, h;

int
f (int *a, int n, int c, int *b)
{
  int s = 0;
  for (int i = 0; i < n; i++)
    s += a[i] * 3; /* LOOP_LINE */
  if (c > 2)
    g = s;
  else
    h = -s;
  for (int i = 0; i < n; i++)
    b[i] = a[i] + c;
  return s + g;
}

int
main (int argc, char **argv)
{
  int a[100], b[100];
  (void) argv;
  for (int i = 0; i < 100; i++)
    a[i] = i;
  return f (a, 100, argc, b) & 1;
}
