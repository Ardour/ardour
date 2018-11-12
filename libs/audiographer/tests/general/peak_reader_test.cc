#include "tests/utils.h"

#include "audiographer/general/peak_reader.h"

using namespace AudioGrapher;

class PeakReaderTest : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE (PeakReaderTest);
  CPPUNIT_TEST (testProcess);
  CPPUNIT_TEST_SUITE_END ();

  public:
	void setUp()
	{
		samples = 128;
		random_data = TestUtils::init_random_data(samples);
	}

	void tearDown()
	{
		delete [] random_data;
	}

	void testProcess()
	{
		reader.reset (new PeakReader());
		ProcessContext<float> c (random_data, samples, 1);

		float peak = 1.5;
		random_data[10] = peak;
		reader->process (c);
		CPPUNIT_ASSERT_EQUAL(peak, reader->get_peak());

		peak = 2.0;
		random_data[10] = peak;
		reader->process (c);
		CPPUNIT_ASSERT_EQUAL(peak, reader->get_peak());

		peak = -2.1;
		random_data[10] = peak;
		reader->process (c);
		float expected = fabs(peak);
		CPPUNIT_ASSERT_EQUAL(expected, reader->get_peak());
	}

  private:
	boost::shared_ptr<PeakReader> reader;

	float * random_data;
	samplecnt_t samples;
};

CPPUNIT_TEST_SUITE_REGISTRATION (PeakReaderTest);
