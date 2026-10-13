// SPDX-License-Identifier: GPL-3.0-or-later
// Protected-mode (e2k128) C++ shared library for pmlibxxmain.cc: an
// exception thrown inside the library and caught in the program, a
// class whose virtual functions live in the library, a callback that
// throws from the program through the library.  Built on the machine by
// run.sh --left:
//
//   l++ -mptr128 -O1 -fPIC -shared pmlibxx.cc -o libpmlibxx.so

#include <stdexcept>
#include <string>

struct LibBase {
	virtual int f () { return 1; }
	virtual ~LibBase () {}
};

struct LibDerived : LibBase {
	int v;
	explicit LibDerived (int x) : v (x) {}
	int f () override { return v; }
};

void lib_throw (int x)
{
	if (x > 0)
		throw std::runtime_error ("from the library " + std::to_string (x));
}

LibBase *lib_make (int x) { return new LibDerived (x); }

int lib_via (int (*cb) (int), int x)
{
	struct Guard {
		int *p;
		~Guard () { *p += 1; }
	};
	static int unwound;
	Guard g = { &unwound };
	return cb (x) + unwound;
}
