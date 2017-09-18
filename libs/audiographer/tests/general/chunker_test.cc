#include "tests/utils.h"

#include "audiographer/general/chunker.h"

#include <cassert>

using namespace AudioGrapher;

class ChunkerTest : public CppUnit::TestFixture
{
	// TODO: Test EndOfInput handling

  CPPUNIT_TEST_SUITE (ChunkerTest);
  CPPUNIT_TEST (testSynchronousProcess);
  CPPUNIT_TEST (testAsynchronousProcess);
  CPPUNIT_TEST (testChoppingProcess);
  CPPUNIT_TEST (testEndOfInputFlagHandling);
  CPPUNIT_TEST_SUITE_END ();

  public:
	void setUp()
	{
		samples = 128;
		random_data = TestUtils::init_random_data(samples);
		sink.reset (new VectorSink<float>());
		chunker.reset (new Chunker<float>(samples * 2));
	}

	void tearDown()
	{
		delete [] random_data;
	}

	void testSynchronousProcess()
	{
		chunker->add_output (sink);
		samplecnt_t samples_output = 0;

		ProcessContext<float> const context (random_data, samples, 1);

		chunker->process (context);
		samples_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL ((samplecnt_t) 0, samples_output);

		chunker->process (context);
		samples_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (2 * samples, samples_output);
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, sink->get_array(), samples));
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, &sink->get_array()[samples], samples));

		sink->reset();

		chunker->process (context);
		samples_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL ((samplecnt_t) 0, samples_output);

		chunker->process (context);
		samples_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (2 * samples, samples_output);
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, sink->get_array(), samples));
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, &sink->get_array()[samples], samples));
	}

	void testAsynchronousProcess()
	{
		assert (samples % 2 == 0);

		chunker->add_output (sink);
		samplecnt_t samples_output = 0;

		ProcessContext<float> const half_context (random_data, samples / 2, 1);
		ProcessContext<float> const context (random_data, samples, 1);

		// 0.5
		chunker->process (half_context);
		samples_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL ((samplecnt_t) 0, samples_output);

		// 1.5
		chunker->process (context);
		samples_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL ((samplecnt_t) 0, samples_output);

		// 2.5
		chunker->process (context);
		samples_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (2 * samples, samples_output);
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, sink->get_array(), samples / 2));
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, &sink->get_array()[samples / 2], samples));
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, &sink->get_array()[ 3 * samples / 2], samples / 2));

		sink->reset();

		// 3.5
		chunker->process (context);
		samples_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL ((samplecnt_t) 0, samples_output);

		// 4.0
		chunker->process (half_context);
		samples_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (2 * samples, samples_output);
		CPPUNIT_ASSERT (TestUtils::array_equals (&random_data[samples / 2], sink->get_array(), samples / 2));
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, &sink->get_array()[samples / 2], samples));
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, &sink->get_array()[ 3 * samples / 2], samples / 2));
	}

	void testChoppingProcess()
	{
		sink.reset (new AppendingVectorSink<float>());

		assert (samples % 2 == 0);
		chunker.reset (new Chunker<float>(samples / 4));

		chunker->add_output (sink);
		samplecnt_t samples_output = 0;

		ProcessContext<float> const half_context (random_data, samples / 2, 1);
		ProcessContext<float> const context (random_data, samples, 1);

		// 0.5
		chunker->process (half_context);
		samples_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL ((samplecnt_t) samples / 2, samples_output);

		// 1.5
		chunker->process (context);
		samples_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL ((samplecnt_t) samples / 2 * 3, samples_output);

		// 2.5
		chunker->process (context);
		samples_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (samples / 2 * 5, samples_output);
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, sink->get_array(), samples / 2));
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, &sink->get_array()[samples / 2], samples));
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, &sink->get_array()[ 3 * samples / 2], samples / 2));
	}

	void testEndOfInputFlagHandling()
	{
		boost::shared_ptr<ProcessContextGrabber<float> > grabber(new ProcessContextGrabber<float>());

		assert (samples % 2 == 0);
		chunker.reset (new Chunker<float>(samples));
		chunker->add_output (grabber);

		ProcessContext<float> const half_context (random_data, samples / 2, 1);
		ProcessContext<float> const context (random_data, samples, 1);
		context.set_flag(ProcessContext<>::EndOfInput);

		// Process 0.5 then 1.0
		chunker->process (half_context);
		chunker->process (context);

		// Should output two contexts
		CPPUNIT_ASSERT_EQUAL((int)grabber->contexts.size(), 2);
		ProcessContextGrabber<float>::ContextList::iterator it = grabber->contexts.begin();

		// first 1.0 not end of input
		CPPUNIT_ASSERT_EQUAL(it->samples(), samples);
		CPPUNIT_ASSERT(!it->has_flag(ProcessContext<>::EndOfInput));

		// Then 0.5 with end of input
		++it;
		CPPUNIT_ASSERT_EQUAL(it->samples(), samples / 2);
		CPPUNIT_ASSERT(it->has_flag(ProcessContext<>::EndOfInput));
	}

  private:
	boost::shared_ptr<Chunker<float> > chunker;
	boost::shared_ptr<VectorSink<float> > sink;

	float * random_data;
	samplecnt_t samples;
};

CPPUNIT_TEST_SUITE_REGISTRATION (ChunkerTest);

