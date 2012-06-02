#include <cstring>
#include <cmath>
#include "ardour/mtdm.h"
#include "mtdm_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION (MTDMTest);

using namespace std;

void
MTDMTest::basicTest ()
{
	float in[256];
	float out[256];

	memset (in, 0, 256 * sizeof (float));
	MTDM* mtdm = new MTDM;
	mtdm->process (256, in, out);
	memcpy (in, out, 256 * sizeof (float));
	
	for (int i = 0; i < 64; ++i) {
		mtdm->process (256, in, out);
		memcpy (in, out, 256 * sizeof (float));

		CPPUNIT_ASSERT_EQUAL (0, mtdm->resolve ());
		CPPUNIT_ASSERT (mtdm->err() < 1);
		CPPUNIT_ASSERT_EQUAL (256.0, rint (mtdm->del()));
	}
}
