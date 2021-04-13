#include "tests/utils.h"

#include "audiographer/general/normalizer.h"
#include "audiographer/general/peak_reader.h"

using namespace AudioGrapher;

class NormalizerTest : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE (NormalizerTest);
  CPPUNIT_TEST (testConstAmplify);
  CPPUNIT_TEST_SUITE_END ();

  public:
	void setUp()
	{
		samples = 1024;
	}

	void tearDown()
	{
		delete [] random_data;
	}

	void testConstAmplify()
	{
		float target = 0.0;
		random_data = TestUtils::init_random_data(samples, 0.5);

		normalizer.reset (new Normalizer(target, samples));
		peak_reader.reset (new PeakReader());
		sink.reset (new VectorSink<float>());

		ProcessContext<float> const c (random_data, samples, 1);
		peak_reader->process (c);

		float peak = peak_reader->get_peak();
		normalizer->set_peak (peak);
		normalizer->add_output (sink);
		normalizer->process (c);

		peak_reader->reset();
		ConstProcessContext<float> normalized (sink->get_array(), samples, 1);
		peak_reader->process (normalized);

		peak = peak_reader->get_peak();
		CPPUNIT_ASSERT (-FLT_EPSILON <= (peak - 1.0) && (peak - 1.0) <= 0.0);
	}

  private:
	boost::shared_ptr<Normalizer> normalizer;
	boost::shared_ptr<PeakReader> peak_reader;
	boost::shared_ptr<VectorSink<float> > sink;

	float * random_data;
	samplecnt_t samples;
};

CPPUNIT_TEST_SUITE_REGISTRATION (NormalizerTest);
