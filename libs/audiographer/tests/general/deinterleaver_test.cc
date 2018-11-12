#include "tests/utils.h"

#include "audiographer/general/deinterleaver.h"

using namespace AudioGrapher;

class DeInterleaverTest : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE (DeInterleaverTest);
  CPPUNIT_TEST (testUninitialized);
  CPPUNIT_TEST (testInvalidOutputIndex);
  CPPUNIT_TEST (testInvalidInputSize);
  CPPUNIT_TEST (testOutputSize);
  CPPUNIT_TEST (testZeroInput);
  CPPUNIT_TEST_SUITE_END ();

  public:
	void setUp()
	{
		channels = 3;
		samples_per_channel = 128;
		total_samples = channels * samples_per_channel;
		random_data = TestUtils::init_random_data (total_samples, 1.0);

		deinterleaver.reset (new DeInterleaver<float>());
		sink_a.reset (new VectorSink<float>());
		sink_b.reset (new VectorSink<float>());
		sink_c.reset (new VectorSink<float>());
	}

	void tearDown()
	{
		delete [] random_data;
	}

	void testUninitialized()
	{
		deinterleaver.reset (new DeInterleaver<float>());
		CPPUNIT_ASSERT_THROW (deinterleaver->output(0)->add_output (sink_a), Exception);
	}

	void testInvalidOutputIndex()
	{
		deinterleaver->init (3, samples_per_channel);
		CPPUNIT_ASSERT_THROW (deinterleaver->output(3)->add_output (sink_a), Exception);
	}

	void testInvalidInputSize()
	{
		deinterleaver->init (channels, samples_per_channel);

		ProcessContext<float> c (random_data, 2 * total_samples, channels);

		// Too many, samples % channels == 0
		CPPUNIT_ASSERT_THROW (deinterleaver->process (c.beginning (total_samples + channels)), Exception);

		// Too many, samples % channels != 0
		CPPUNIT_ASSERT_THROW (deinterleaver->process (c.beginning (total_samples + 1)), Exception);

		// Too few, samples % channels != 0
		CPPUNIT_ASSERT_THROW (deinterleaver->process (c.beginning (total_samples - 1)), Exception);
	}

	void assert_outputs (samplecnt_t expected_samples)
	{
		samplecnt_t generated_samples = 0;

		generated_samples = sink_a->get_data().size();
		CPPUNIT_ASSERT_EQUAL (expected_samples, generated_samples);

		generated_samples = sink_b->get_data().size();
		CPPUNIT_ASSERT_EQUAL (expected_samples, generated_samples);

		generated_samples = sink_c->get_data().size();
		CPPUNIT_ASSERT_EQUAL (expected_samples, generated_samples);
	}

	void testOutputSize()
	{
		deinterleaver->init (channels, samples_per_channel);

		deinterleaver->output (0)->add_output (sink_a);
		deinterleaver->output (1)->add_output (sink_b);
		deinterleaver->output (2)->add_output (sink_c);

		// Test maximum sample input
		ProcessContext<float> c (random_data, total_samples, channels);
		deinterleaver->process (c);
		assert_outputs (samples_per_channel);

		// Now with less samples
		samplecnt_t const less_samples = samples_per_channel / 4;
		deinterleaver->process (c.beginning (less_samples * channels));
		assert_outputs (less_samples);
	}

	void testZeroInput()
	{
		deinterleaver->init (channels, samples_per_channel);

		deinterleaver->output (0)->add_output (sink_a);
		deinterleaver->output (1)->add_output (sink_b);
		deinterleaver->output (2)->add_output (sink_c);

		// Input zero samples
		ProcessContext<float> c (random_data, total_samples, channels);
		deinterleaver->process (c.beginning (0));

		// ...and now test regular input
		deinterleaver->process (c);
		assert_outputs (samples_per_channel);
	}


  private:
	boost::shared_ptr<DeInterleaver<float> > deinterleaver;

	boost::shared_ptr<VectorSink<float> > sink_a;
	boost::shared_ptr<VectorSink<float> > sink_b;
	boost::shared_ptr<VectorSink<float> > sink_c;

	float * random_data;
	samplecnt_t samples_per_channel;
	samplecnt_t total_samples;
	unsigned int channels;
};

CPPUNIT_TEST_SUITE_REGISTRATION (DeInterleaverTest);

