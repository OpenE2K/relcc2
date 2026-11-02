/* SPDX-License-Identifier: GPL-3.0-or-later */
/* gfortran's store of a polymorphic array element as
   VIEW_CONVERT_EXPR <struct t2> (*p), P a pointer to the declared type
   t1 that t2 extends (gfc_trans_scalar_assign's BT_CLASS branch,
   gcc/fortran/trans-expr.cc), written in GCC's GIMPLE front end: is a
   later load of t2's float member ordered after it?
   (docs/spec-poc-findings.md P125; doc/gcc-port-plan.md 7.6.)  A target-
   independent test, run on the build host:

     gcc -O2 -fgimple tools/abi/probes/foldp125_vce.c -o vce && ./vce
     gcc -O2 -fgimple -fno-strict-aliasing ... (control)
     gcc -O2 -fgimple -fdump-tree-fre1-details -S ... (c_5's value number)
     gcc -O2 -fgimple -fno-tree-fre -fdump-rtl-expand -S ... (the MEMs'
         alias sets in f)

   tree.def calls a VIEW_CONVERT_EXPR whose operand and result differ in
   size undefined; reference_alias_ptr_type_1 (gcc/alias.cc) takes the
   operand's alias set for one, t1's here, which has no float in it.

   x86-64, 2026-11-03: host GCC 16.2.1, an x86 build of GCC master
   ca33fb40100 (the PoC's base) and an x86 build of the PoC's tree
   (with its alias.cc change) all print "2 (expect 3)": FRE gives c_5
   a_4's value number, the load from before the store.  With
   -fno-strict-aliasing it prints 3.  In RTL the store's MEM has alias
   set 3 (t1's) at ca33fb40100 and 0 with the PoC's change, the load's 4
   (float's): the PoC's change fixes the RTL alias set only, and
   GIMPLE's oracle still separates the two accesses.  */

struct t1 { int i; };
struct t2 { struct t1 b; float r; };

float __GIMPLE (ssa,startwith("fre")) __attribute__((noipa))
f (struct t1 *p, struct t2 *v)
{
  struct t2 *q;
  struct t2 tmp;
  float a;
  float c;

  __BB(2):
  q_3 = (struct t2 *) p_1(D);
  a_4 = q_3->r;
  tmp = *v_2(D);
  __VIEW_CONVERT <struct t2> (__MEM <struct t1> (p_1(D))) = tmp;
  c_5 = q_3->r;
  c_6 = a_4 + c_5;
  return c_6;
}

int
main ()
{
  struct t2 x = { { 1 }, 1.0f };
  struct t2 y = { { 2 }, 2.0f };
  float s = f ((struct t1 *) &x, &y);
  __builtin_printf ("%g (expect 3)\n", s);
  return s != 3.0f;
}
