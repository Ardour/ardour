#include "ardour/resampled_source.h"
#include "ardour/sndfileimportable.h"
#include "resampled_source.h"

CPPUNIT_TEST_SUITE_REGISTRATION (ResampledSourceTest);

using namespace ARDOUR;

void
ResampledSourceTest::seekTest ()
{
	boost::shared_ptr<SndFileImportableSource> s (new SndFileImportableSource ("../libs/ardour/test/data/test.wav"));
	ResampledImportableSource r (s, 48000, SrcBest);

	/* Make sure that seek (0) has the desired effect, ie that
	   given the same input you get the same output after seek (0)
	   as you got when the Source was newly created.
	*/

	Sample A[64];
	r.read (A, 64);

	r.seek (0);

	Sample B[64];
	r.read (B, 64);

	for (int i = 0; i < 64; ++i) {
		CPPUNIT_ASSERT (A[i] == B[i]);
	}
}
