// SPDX-License-Identifier: GPL-3.0-or-later
// Protected-mode (e2k128) C++ program using libpmlibxx.so (pmlibxx.cc):
// exceptions and virtual calls across a protected library boundary.
// Built and run on the machine by run.sh --left:
//
//   l++ -mptr128 -O1 -fPIC -shared pmlibxx.cc -o libpmlibxx.so
//   l++ -mptr128 -O1 pmlibxxmain.cc -o pmlibxxmain -L. -lpmlibxx
//   LD_LIBRARY_PATH=. ./pmlibxxmain

#include <cstdio>
#include <stdexcept>

struct LibBase {
	virtual int f () { return 1; }
	virtual ~LibBase () {}
};

void lib_throw (int x);
LibBase *lib_make (int x);
int lib_via (int (*cb) (int), int x);

static int thrower (int x)
{
	if (x > 1)
		throw std::out_of_range ("from the program");
	return x;
}

int main ()
{
	std::setvbuf (stdout, nullptr, _IOLBF, 0);
	try {
		lib_throw (5);
		std::printf ("xx throw: not thrown\n");
	} catch (const std::runtime_error &e) {
		std::printf ("xx throw: caught \"%s\"\n", e.what ());
	}
	LibBase *b = lib_make (42);
	std::printf ("xx virtual: f()=%d\n", b->f ());
	delete b;
	std::printf ("xx via: lib_via(thrower, 1)=%d\n", lib_via (thrower, 1));
	try {
		lib_via (thrower, 2);
		std::printf ("xx via: not thrown\n");
	} catch (const std::out_of_range &e) {
		std::printf ("xx via: caught \"%s\" through the library's frame; "
			     "lib_via(thrower, 1)=%d (its destructor ran)\n",
			     e.what (), lib_via (thrower, 1));
	}
	std::printf ("done\n");
	return 0;
}
