#include "tests/utils.h"

#include "audiographer/general/interleaver.h"

using namespace AudioGrapher;

class InterleaverTest : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE (InterleaverTest);
  CPPUNIT_TEST (testUninitialized);
  CPPUNIT_TEST (testInvalidInputIndex);
  CPPUNIT_TEST (testInvalidInputSize);
  CPPUNIT_TEST (testOutputSize);
  CPPUNIT_TEST (testZeroInput);
  CPPUNIT_TEST (testChannelSync);
  CPPUNIT_TEST_SUITE_END ();

  public:
	void setUp()
	{
		channels = 3;
		samples = 128;
		random_data = TestUtils::init_random_data (samples, 1.0);

		interleaver.reset (new Interleaver<float>());
		sink.reset (new VectorSink<float>());

		interleaver->init (channels, samples);
	}

	void tearDown()
	{
		delete [] random_data;
	}

	void testUninitialized()
	{
		interleaver.reset (new Interleaver<float>());
		ProcessContext<float> c (random_data, samples, 1);
		CPPUNIT_ASSERT_THROW (interleaver->input(0)->process (c), Exception);
	}

	void testInvalidInputIndex()
	{
		ProcessContext<float> c (random_data, samples, 1);
		CPPUNIT_ASSERT_THROW (interleaver->input (3)->process (c), Exception);
	}

	void testInvalidInputSize()
	{
		ProcessContext<float> c (random_data, samples + 1, 1);
		CPPUNIT_ASSERT_THROW (interleaver->input (0)->process (c), Exception);

		interleaver->input (0)->process (c.beginning (samples));
		interleaver->input (1)->process (c.beginning (samples));
		CPPUNIT_ASSERT_THROW (interleaver->input (2)->process (c.beginning (samples - 1)), Exception);

		interleaver->input (0)->process (c.beginning (samples - 1));
		interleaver->input (1)->process (c.beginning (samples - 1));
		CPPUNIT_ASSERT_THROW (interleaver->input (2)->process (c.beginning (samples)), Exception);
	}

	void testOutputSize()
	{
		interleaver->add_output (sink);

		ProcessContext<float> c (random_data, samples, 1);
		interleaver->input (0)->process (c);
		interleaver->input (1)->process (c);
		interleaver->input (2)->process (c);

		samplecnt_t expected_samples = samples * channels;
		samplecnt_t generated_samples = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (expected_samples, generated_samples);

		samplecnt_t less_samples = samples / 2;
		interleaver->input (0)->process (c.beginning (less_samples));
		interleaver->input (1)->process (c.beginning (less_samples));
		interleaver->input (2)->process (c.beginning (less_samples));

		expected_samples = less_samples * channels;
		generated_samples = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (expected_samples, generated_samples);
	}

	void testZeroInput()
	{
		interleaver->add_output (sink);

		// input zero samples to all inputs
		ProcessContext<float> c (random_data, samples, 1);
		interleaver->input (0)->process (c.beginning (0));
		interleaver->input (1)->process (c.beginning (0));
		interleaver->input (2)->process (c.beginning (0));

		// NOTE zero input is allowed to be a NOP

		// ...now test regular input
		interleaver->input (0)->process (c);
		interleaver->input (1)->process (c);
		interleaver->input (2)->process (c);

		samplecnt_t expected_samples = samples * channels;
		samplecnt_t generated_samples = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (expected_samples, generated_samples);
	}

	void testChannelSync()
	{
		interleaver->add_output (sink);
		ProcessContext<float> c (random_data, samples, 1);
		interleaver->input (0)->process (c);
		CPPUNIT_ASSERT_THROW (interleaver->input (0)->process (c), Exception);
	}


  private:
	boost::shared_ptr<Interleaver<float> > interleaver;

	boost::shared_ptr<VectorSink<float> > sink;

	samplecnt_t channels;
	float * random_data;
	samplecnt_t samples;
};

CPPUNIT_TEST_SUITE_REGISTRATION (InterleaverTest);

