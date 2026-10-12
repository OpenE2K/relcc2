// SPDX-License-Identifier: GPL-3.0-or-later
// Protected-mode (e2k128) C++ probe: exceptions across frames, virtual
// calls, new/delete, a standard container, a pointer to member function.
// Built and run by run.sh, or by hand on a machine with /lib128:
//
//   l++ -mptr128 -O1 pmcxx.cc -o pmcxx && ./pmcxx
//
// Prints one line per check; the last line is "done".

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

struct Base {
	virtual ~Base () {}
	virtual int f (int x) const { return x; }
};
struct Derived : Base {
	int k;
	Derived (int k) : k (k) {}
	int f (int x) const override { return x * k; }
};

static int __attribute__ ((noinline)) thrower (int depth)
{
	if (depth == 0)
		throw std::runtime_error ("deep");
	return thrower (depth - 1) + 1;
}

struct Guard {
	int *p;
	Guard (int *p) : p (p) {}
	~Guard () { ++*p; }
};

static int __attribute__ ((noinline)) unwinds (int *count)
{
	Guard g (count);
	return thrower (5);
}

int main ()
{
	setvbuf (stdout, nullptr, _IOLBF, 0);
	Base *b = new Derived (3);
	std::printf ("virtual: %d (sizeof (Base) %zu, sizeof (Derived) %zu)\n",
		     b->f (7), sizeof (Base), sizeof (Derived));
	delete b;

	int dtors = 0;
	try {
		unwinds (&dtors);
		std::printf ("exception: not thrown\n");
	} catch (const std::exception &e) {
		std::printf ("exception: caught \"%s\", destructors run %d\n",
			     e.what (), dtors);
	}

	std::vector<std::string> v;
	for (int i = 0; i < 100; i++)
		v.push_back (std::to_string (i));
	std::printf ("vector: %zu strings, v[99] = %s\n", v.size (), v[99].c_str ());

	int (Base::*pmf) (int) const = &Base::f;
	Derived d (5);
	std::printf ("member pointer: %d (sizeof %zu)\n", (d.*pmf) (2), sizeof pmf);

	try {
		std::vector<int> w (3);
		std::printf ("at(3): %d\n", w.at (3));
	} catch (const std::out_of_range &) {
		std::printf ("at(3): out_of_range\n");
	}
	std::printf ("done\n");
	return 0;
}
