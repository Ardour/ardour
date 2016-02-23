#include <string.h>
#include <stdlib.h>
#include "reallocpool_test.h"
#include "pbd/reallocpool.h"

CPPUNIT_TEST_SUITE_REGISTRATION (ReallocPoolTest);

using namespace std;

ReallocPoolTest::ReallocPoolTest ()
{
}

void
ReallocPoolTest::testBasic ()
{
	::srand (0);
	PBD::ReallocPool *m = new PBD::ReallocPool("TestPool", 256 * 1024);

	for (int l = 0; l < 2 * 1024 * 1024; ++l) {
		void *x[32];
		size_t s[32];
		int cnt = ::rand() % 32;
		for (int i = 0; i < cnt; ++i) {
			s[i] = ::rand() % 1024;
			x[i] = m->malloc (s[i]);
		}
		for (int i = 0; i < cnt; ++i) {
			if (x[i]) {
				memset (x[i], 0xa5, s[i]);
			}
		}
		for (int i = 0; i < cnt; ++i) {
			m->free (x[i]);
		}
	}
#ifdef RAP_WITH_CALL_STATS
	CPPUNIT_ASSERT (m->mem_used() == 0);
#endif
	delete (m);
}
