// SPDX-License-Identifier: GPL-3.0-or-later
// Speculative devirtualization and a class whose vtable lives elsewhere
// (doc/gcc-port-plan.md, M8; the proof of concept's finding P134: qpdf
// 10.0.1's test_large_file and its hidden QPDF::CopiedStreamDataProvider).
// Copied's key function (provide) is defined in another object, a shared
// library that may hide it, so this file must not need Copied's vtable.
// Its destructor is inline. GCC's -fdevirtualize-speculatively (on from
// -O2, opts.cc:646) guesses that `delete p` calls Copied's deleting
// destructor: it compares the function pointer loaded from the vtable
// with that destructor's address (gimple_ic, value-prof.cc:1300-1336)
// and inlines the destructor on the guessed path, whose first store puts
// Copied's vtable address into the object; the destructor's out-of-line
// COMDAT copies store it too. Compile only, then count the references:
//
//   L=/opt/mcst/lcc-1.29.16.e2k-v5.8c2.linux-6.1/bin
//   $L/l++ -O3 -c foldp134_devirt.cc -o lcc.o && e2k-linux-nm lcc.o
//   g++ -O2 -S foldp134_devirt.cc -o - | grep -c _ZTV6Copied
//   g++ -O2 -fno-devirtualize-speculatively -S foldp134_devirt.cc -o - |
//     grep -c _ZTV6Copied
//   (the same two with the port's g++)
//
// 2026-11-03: l++ 1.29.16 (v5.8c2) at -O2, -O3 and -O4 defines only
// use() and references no Copied symbol; x86 GCC 16.2.1 and the proof of
// concept 0.6 (v3) at -O2 reference _ZTV6Copied 4 times (cmpq/compare
// against _ZN6CopiedD0Ev, then the vptr store), 0 with
// -fno-devirtualize-speculatively and at -O1. Linked against a library
// that hides Copied, the GCC object's reference is undefined.

struct Provider {
  virtual ~Provider ();
  virtual void provide (int) = 0;
};

class __attribute__ ((visibility ("hidden"))) Copied : public Provider {
public:
  Copied (int);
  ~Copied () override {}
  void provide (int) override;	// the key function, in the library
  int x;
};

void
use (Provider *p)
{
  p->provide (1);
  delete p;
}
