#include "tests/utils.h"

#include "audiographer/general/silence_trimmer.h"

using namespace AudioGrapher;

class SilenceTrimmerTest : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE (SilenceTrimmerTest);
  CPPUNIT_TEST (testExceptions);
  CPPUNIT_TEST (testFullBuffers);
  CPPUNIT_TEST (testPartialBuffers);
  CPPUNIT_TEST (testAddSilenceBeginning);
  CPPUNIT_TEST (testAddSilenceEnd);
  CPPUNIT_TEST_SUITE_END ();

  public:
	void setUp()
	{
		samples = 128;

		random_data = TestUtils::init_random_data(samples);
		random_data[0] = 0.5;
		random_data[samples - 1] = 0.5;

		zero_data = new float[samples];
		memset(zero_data, 0, samples * sizeof(float));

		half_random_data = TestUtils::init_random_data(samples);
		memset(half_random_data, 0, (samples / 2) * sizeof(float));

		trimmer.reset (new SilenceTrimmer<float> (samples / 2));
		sink.reset (new AppendingVectorSink<float>());

		trimmer->set_trim_beginning (true);
		trimmer->set_trim_end (true);
	}

	void tearDown()
	{
		delete [] random_data;
		delete [] zero_data;
		delete [] half_random_data;
	}

	void testFullBuffers()
	{
		trimmer->add_output (sink);

		{
		ProcessContext<float> c (zero_data, samples, 1);
		trimmer->process (c);
		samplecnt_t samples_processed = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL ((samplecnt_t) 0, samples_processed);
		}

		{
		ProcessContext<float> c (random_data, samples, 1);
		trimmer->process (c);
		samplecnt_t samples_processed = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (samples, samples_processed);
		CPPUNIT_ASSERT (TestUtils::array_equals (sink->get_array(), random_data, samples));
		}

		{
		ProcessContext<float> c (zero_data, samples, 1);
		trimmer->process (c);
		samplecnt_t samples_processed = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (samples, samples_processed);
		}

		{
		ProcessContext<float> c (random_data, samples, 1);
		trimmer->process (c);
		samplecnt_t samples_processed = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (3 * samples, samples_processed);
		CPPUNIT_ASSERT (TestUtils::array_equals (sink->get_array(), random_data, samples));
		CPPUNIT_ASSERT (TestUtils::array_equals (&sink->get_array()[samples], zero_data, samples));
		CPPUNIT_ASSERT (TestUtils::array_equals (&sink->get_array()[2 * samples], random_data, samples));
		}

		{
		ProcessContext<float> c (zero_data, samples, 1);
		trimmer->process (c);
		samplecnt_t samples_processed = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (3 * samples, samples_processed);
		}
	}

	void testPartialBuffers()
	{
		trimmer->add_output (sink);
		trimmer->reset (samples / 4);
		trimmer->set_trim_beginning (true);
		trimmer->set_trim_end (true);

		{
		ProcessContext<float> c (half_random_data, samples, 1);
		trimmer->process (c);
		samplecnt_t samples_processed = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (samples / 2, samples_processed);
		CPPUNIT_ASSERT (TestUtils::array_equals (sink->get_array(), &half_random_data[samples / 2], samples / 2));
		}

		{
		ProcessContext<float> c (zero_data, samples, 1);
		trimmer->process (c);
		samplecnt_t samples_processed = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (samples / 2, samples_processed);
		}

		{
		ProcessContext<float> c (half_random_data, samples, 1);
		trimmer->process (c);
		samplecnt_t samples_processed = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (2 * samples + samples / 2, samples_processed);
		CPPUNIT_ASSERT (TestUtils::array_equals (&sink->get_array()[samples + samples / 2], half_random_data, samples));
		}
	}

	void testExceptions()
	{
		{
		CPPUNIT_ASSERT_THROW (trimmer->reset (0), Exception);
		}
	}

	void testAddSilenceBeginning()
	{
		trimmer->add_output (sink);

 		samplecnt_t silence = samples / 2;
		trimmer->add_silence_to_beginning (silence);

		{
		ProcessContext<float> c (random_data, samples, 1);
		trimmer->process (c);
		}

		CPPUNIT_ASSERT (TestUtils::array_equals (sink->get_array(), zero_data, silence));
		CPPUNIT_ASSERT (TestUtils::array_equals (&sink->get_array()[silence], random_data, samples));
	}

	void testAddSilenceEnd()
	{
		trimmer->add_output (sink);

		samplecnt_t silence = samples / 3;
		trimmer->add_silence_to_end (silence);

		{
		ProcessContext<float> c (random_data, samples, 1);
		trimmer->process (c);
		}

		{
		ProcessContext<float> c (random_data, samples, 1);
		c.set_flag (ProcessContext<float>::EndOfInput);
		trimmer->process (c);
		}

		samplecnt_t samples_processed = sink->get_data().size();
		samplecnt_t total_samples = 2 * samples + silence;
		CPPUNIT_ASSERT_EQUAL (total_samples, samples_processed);
		CPPUNIT_ASSERT (TestUtils::array_equals (sink->get_array(), random_data, samples));
		CPPUNIT_ASSERT (TestUtils::array_equals (&sink->get_array()[samples], random_data, samples));
		CPPUNIT_ASSERT (TestUtils::array_equals (&sink->get_array()[samples * 2], zero_data, silence));
	}

  private:
	boost::shared_ptr<SilenceTrimmer<float> > trimmer;
	boost::shared_ptr<AppendingVectorSink<float> > sink;

	float * random_data;
	float * zero_data;
	float * half_random_data;
	samplecnt_t samples;
};

CPPUNIT_TEST_SUITE_REGISTRATION (SilenceTrimmerTest);
