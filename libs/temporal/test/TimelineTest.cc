#include <iostream>
#include <stdlib.h>

#include "temporal/timeline.h"

#include "TimelineTest.h"

CPPUNIT_TEST_SUITE_REGISTRATION(TimelineTest);

using namespace Temporal;

void
TimelineTest::createTest()
{
	using namespace Temporal;
	using namespace std;

	srandom (time (0));

	vector<timepos_t> times;

	for (int n = 0; n < 32767; ++n) {
		times.push_back (timepos_t (random() % 32120321));
	}

	uint32_t lessthan = 0;
	uint32_t greaterthan = 0;

	for (vector<timepos_t>::iterator t = times.begin(); t != times.end(); ++t) {
		if ((*t) + timepos_t (random() % 20207) < timepos_t (2299307)) {
			lessthan++;
		} else {
			greaterthan++;
		}
	}

	std::cerr << "LT " << lessthan << " GT " << greaterthan << std::endl;

}

void
TimelineTest::addTest()
{
}

void
TimelineTest::subtractTest()
{
}

void
TimelineTest::multiplyTest()
{
}

void
TimelineTest::roundTest()
{
}

void
TimelineTest::convertTest()
{
}

