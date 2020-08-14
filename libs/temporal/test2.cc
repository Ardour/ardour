/* standalone compile, from within this directory:

   g++ `pkg-config --cflags --libs libxml-2.0` `pkg-config --cflags --libs glibmm-2.4` -I../pbd -I../../build/libs/pbd  -I. -o test2 test2.cc

*/

#include <atomic>
#include <bitset>
#include <iostream>
#include <cstdint>
#include <cstdlib>

#include <sys/time.h>

static int loop_count = 10000000;

using namespace std;

int64_t
get_microseconds ()
{
	struct timespec ts;
	if (clock_gettime (CLOCK_MONOTONIC, &ts) != 0) {
		/* EEEK! */
		return 0;
	}
	return (int64_t)ts.tv_sec * 1000000 + (ts.tv_nsec / 1000);
}

void
single_atomic ()
{
	struct thing {
		std::atomic<int64_t> v;
		thing() : v (0) { }
	};

	thing t;
	int odd = 0;

	cout << "atomic<int64_t> is lock free ? " << t.v.is_lock_free() << endl;

	int64_t before = get_microseconds ();

	for (int n = 0; n < loop_count; ++n) {
		t.v = random ();
		t.v = t.v + 1;
		if (t.v % 2) {
			odd++;
		}
	}
	int64_t after = get_microseconds ();
	std::cout << "odd: " << odd << " usecs = " << after - before << std::endl;
}

void
bitfields ()
{
	struct alignas(16) thing {
		int b : 1;
		int64_t v : 62;
		thing() : b (0), v (0) {}
		thing (int bc, int64_t vc) : b (bc), v (vc) {}

		thing operator+(int n) { return thing (b, v + n); }
		thing& operator=(thing const & other) { b = other.b; v = other.v; return *this; }
	};

	thing t;
	int odd = 0;
	int64_t before = get_microseconds ();

	for (int n = 0; n < loop_count; ++n) {
		t.v = random ();
		t.v = t.v + 1;
		if (t.v % 2) {
			odd++;
		}
	}

	int64_t after = get_microseconds ();
	std::cout << "odd: " << odd << " usecs = " << after - before << std::endl;
}

void
masks ()
{
	struct alignas(16) thing {
	  private:
		int64_t v;

		bool is_beats() const { return v&(1LL<<62); }
		int64_t val() const { return v & ~(1LL<<62); }
		static int64_t build (bool bc, int64_t v) { return (bc ? (1LL<<62) : 0) | v; }

	  public:
		thing() : v (0) {}
		thing (bool bc, int64_t vc) : v (build (bc, vc)) {}

		thing operator+(int n) const { return thing (is_beats(), val() + n); }
		thing& operator=(thing const & other) { v = other.v; return *this; }
		thing& operator+=(int64_t n) { v = build (is_beats(), val() + n); return *this; }
		thing& operator=(int64_t n) { v = build (is_beats(), n); return *this; }
		operator int64_t () const { return val(); }
		bool operator==(int64_t vv) const { return val() == vv; }
	};

	thing t;
	int odd = 0;
	int64_t before = get_microseconds ();

	for (int n = 0; n < loop_count; ++n) {
		t = random ();
		t = t + 1;
		if ((t % 2) == 0) {
			odd++;
		}
	}

	int64_t after = get_microseconds ();
	std::cout << "odd: " << odd << " usecs = " << after - before << std::endl;
}

#include "pbd/int62.h"

void
atomic_masks ()
{

	int62_t t;
	int odd = 0;
	int64_t before = get_microseconds ();

	for (int n = 0; n < loop_count; ++n) {
		t = random ();
		t = t + 1;
		if ((t.val() % 2LL) == 0LL) {
			odd++;
		}
	}

	int64_t after = get_microseconds ();
	std::cout << "odd: " << odd << " usecs = " << after - before << std::endl;

	int62_t x;

	x = 1;
	cerr << "should be 1: " << x.val() << endl;
	x -= 1;
	cerr << "should be 0:  " << x.val() << endl;
	x -= 1;
	cerr << "should be -1: " << x.val() << endl;

	x  = int62_t::build (false, int62_t::min);
	cerr << "should be " << int62_t::min << ' ' << x.val() << endl;

	x  = int62_t::build (true, int62_t::min);
	cerr << "should still be " << int62_t::min << ' ' << x.val() << " and also flag: " << x.flagged() << endl;
	x += -x;
	cerr << "invert+add should be zero: " << x.val() << " and also flag: " << x.flagged() << endl;
}


void
test_ints ()
{
	int62_t i62;
	int64_t i64 (0);
	int64_t arg;
	int64_t old62;
	int64_t old64;
	int skips = 0;

	for (int n = 0; n < loop_count; ++n) {

		arg = random();
		old62 = i62.val();
		old64 = i64;

		char opchar;
		int op = random() % 4;

		switch (op) {
		case 0:
			if (INT64_MAX - arg >= i64) {
				i64 += arg;
				if (i64 <= std::numeric_limits<int62_t>::max().val() && i64 >= std::numeric_limits<int62_t>::min().val()) {
					i62 += arg;
				} else {
					i64 = old64;
					skips++;
				}
			}
			opchar = '+';
			break;
		case 1:
			if (INT64_MIN + arg <= i64) {
				i64 -= arg;
				if (i64 <= std::numeric_limits<int62_t>::max().val() && i64 >= std::numeric_limits<int62_t>::min().val()) {
					i62 -= arg;
				} else {
					i64 = old64;
					skips++;
				}
			}
			opchar = '-';
			break;
		case 2:
			if (arg == 0) {
				i64 = 0;
				i62 = 0;
			} else {
				if (INT64_MAX / arg > i64) {
					i64 *= arg;
					if (i64 <= std::numeric_limits<int62_t>::max().val() && i64 >= std::numeric_limits<int62_t>::min().val()) {
						i62 *= arg;
					} else {
						i64 = old64;
						skips++;
					}
				}
			}
			opchar = '*';
			break;
		case 3:

			if (arg == 0) {
				continue;
			}

			i64 /= arg;
			if (i64 <= std::numeric_limits<int62_t>::max().val() && i64 >= std::numeric_limits<int62_t>::min().val()) {
				i62 /= arg;
			} else {
				i64 = old64;
				skips++;
			}
			opchar = '/';
			break;
		}

		if (i62.val() != i64) {
			cerr << "failure after " << n << " op = " << opchar << " arg " << arg << " old was " << old62 <<  " cur " << i62.val() << " vs. " << i64 <<  " whose old was " << old64 << endl;
			break;
		}

		// cerr << old64 << ' ' << opchar << ' ' << arg << " = " << i64 << endl;
	}

	cerr << "Had to skip " << skips << " of " << loop_count << endl;
}

int
main (int argc, char *argv[])
{
	if (argc > 1) {
		loop_count = atoi (argv[1]);
	}

	srandom (time ((time_t *) 0));

	single_atomic ();
	bitfields ();
	masks ();
	atomic_masks ();
	// test_ints ();

	return 0;
}
