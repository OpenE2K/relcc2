// SPDX-License-Identifier: GPL-3.0-or-later
/* A member defined, by its qualified name, in a namespace that doesn't
   enclose its class (spec/abi/source-compat.md; docs/spec-poc-findings.md
   A58).  MCST's LibreOffice 6.3 bridge for E2K (cpp2uno.cxx in
   bridges/source/cpp_uno/gcc3_linux_e2k) defines
   bridges::cpp_uno::shared::VtableFactory's member functions inside an
   unnamed namespace.  Compile only, read the diagnostics and symbols:

     X=/opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1/bin/l++   (or 1.31.05)
     $X -O2 -Wall -c folda58_memberns.cc -o m.o     # also -DNAMED, -DNESTED,
                                                    # -DSTATIC_DM, -DFREE
     readelf -sW m.o | awk '$8 ~ /_Z/ {print $5, $6, $8}' | c++filt
     g++ -O2 -c folda58_memberns.cc                 # also -fpermissive

   l++ 1.29.16 (v5.8c2) and 1.31.05 (v3), 2026-11-03, no message (also
   with -std=c++17 or -pedantic-errors): N::S::f defined in the unnamed
   namespace, N::S::g in an unrelated named one (-DNAMED) and N::S::h
   in an unnamed namespace inside N (-DNESTED) are ordinary GLOBAL
   DEFAULT symbols (_ZN1N1S1fEv, ...), as when defined at file scope.
   A static data member (-DSTATIC_DM) or a namespace member function
   (-DFREE) defined the same way is error #551 "... cannot be defined
   in the current scope".
   GCC (x86-64 16.2.1, the proof-of-concept 0.6+33): every member is
   "definition of 'int N::S::f()' is not in namespace enclosing 'N::S'
   [-fpermissive]", static data members included, a permerror
   (cp/decl.cc, check_class_member_definition_namespace); with
   -fpermissive a warning, and the symbol is the same GLOBAL DEFAULT
   one.  -DFREE is an error in GCC too ("declaration of 'int free_fn()'
   not in a namespace surrounding 'N'", also under -fpermissive).  */

namespace N
{
  struct S
  {
    int f ();
    int g ();
    int h ();
    static int sm;
  };
  int free_fn ();
}

#ifndef STATIC_DM
int N::S::sm = 5;
#endif

namespace
{
  int helper () { return 7; }
  int N::S::f () { return helper (); }
#ifdef STATIC_DM
  int N::S::sm = 5;
#endif
#ifdef FREE
  int N::free_fn () { return 3; }
#endif
}

#ifdef NAMED
namespace M
{
  int N::S::g () { return 2; }
}
#endif

#ifdef NESTED
namespace N
{
  namespace
  {
    int N::S::h () { return 4; }
  }
}
#endif

int use (N::S *p) { return p->f () + N::S::sm; }
