// SPDX-License-Identifier: GPL-3.0-or-later
// C++ in 32-bit mode (spec/abi/mode32.md): an exception thrown through
// frames, a std::string result and a virtual call, with l++ -mptr32:
//   /opt/mcst/lcc-1.29.16.e2k-v3.linux-6.1/bin/l++ -mptr32 -O2 cxx32.cc
#include <cstdio>
#include <stdexcept>
#include <string>

struct B { virtual ~B () {} virtual int f () const { return 1; } };
struct D : B { int f () const override { return 2; } };

__attribute__ ((noinline)) static std::string mk (int n)
{
	if (n > 3)
		throw std::runtime_error ("deep " + std::to_string (n));
	return std::string (n, 'x') + mk (n + 1);
}

int main ()
{
	D d;
	const B &b = d;
	try {
		mk (0);
		std::printf ("cxx: no exception\n");
	} catch (const std::exception &e) {
		std::printf ("cxx: caught '%s', virtual %d, sizeof(std::string) %d\n",
			     e.what (), b.f (), (int) sizeof (std::string));
	}
	return 0;
}
