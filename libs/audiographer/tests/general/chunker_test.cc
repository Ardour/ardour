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
		frames = 128;
		random_data = TestUtils::init_random_data(frames);
		sink.reset (new VectorSink<float>());
		chunker.reset (new Chunker<float>(frames * 2));
	}

	void tearDown()
	{
		delete [] random_data;
	}

	void testSynchronousProcess()
	{
		chunker->add_output (sink);
		framecnt_t frames_output = 0;
		
		ProcessContext<float> const context (random_data, frames, 1);
		
		chunker->process (context);
		frames_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL ((framecnt_t) 0, frames_output);
		
		chunker->process (context);
		frames_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (2 * frames, frames_output);
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, sink->get_array(), frames));
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, &sink->get_array()[frames], frames));
		
		sink->reset();
		
		chunker->process (context);
		frames_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL ((framecnt_t) 0, frames_output);
		
		chunker->process (context);
		frames_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (2 * frames, frames_output);
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, sink->get_array(), frames));
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, &sink->get_array()[frames], frames));
	}
	
	void testAsynchronousProcess()
	{
		assert (frames % 2 == 0);
		
		chunker->add_output (sink);
		framecnt_t frames_output = 0;
		
		ProcessContext<float> const half_context (random_data, frames / 2, 1);
		ProcessContext<float> const context (random_data, frames, 1);
		
		// 0.5
		chunker->process (half_context);
		frames_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL ((framecnt_t) 0, frames_output);
		
		// 1.5
		chunker->process (context);
		frames_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL ((framecnt_t) 0, frames_output);
		
		// 2.5
		chunker->process (context);
		frames_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (2 * frames, frames_output);
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, sink->get_array(), frames / 2));
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, &sink->get_array()[frames / 2], frames));
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, &sink->get_array()[ 3 * frames / 2], frames / 2));
		
		sink->reset();
		
		// 3.5
		chunker->process (context);
		frames_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL ((framecnt_t) 0, frames_output);
		
		// 4.0
		chunker->process (half_context);
		frames_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (2 * frames, frames_output);
		CPPUNIT_ASSERT (TestUtils::array_equals (&random_data[frames / 2], sink->get_array(), frames / 2));
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, &sink->get_array()[frames / 2], frames));
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, &sink->get_array()[ 3 * frames / 2], frames / 2));
	}
	
	void testChoppingProcess()
	{
		sink.reset (new AppendingVectorSink<float>());
		
		assert (frames % 2 == 0);
		chunker.reset (new Chunker<float>(frames / 4));
		
		chunker->add_output (sink);
		framecnt_t frames_output = 0;
		
		ProcessContext<float> const half_context (random_data, frames / 2, 1);
		ProcessContext<float> const context (random_data, frames, 1);
		
		// 0.5
		chunker->process (half_context);
		frames_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL ((framecnt_t) frames / 2, frames_output);
		
		// 1.5
		chunker->process (context);
		frames_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL ((framecnt_t) frames / 2 * 3, frames_output);
		
		// 2.5
		chunker->process (context);
		frames_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (frames / 2 * 5, frames_output);
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, sink->get_array(), frames / 2));
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, &sink->get_array()[frames / 2], frames));
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, &sink->get_array()[ 3 * frames / 2], frames / 2));
	}

	void testEndOfInputFlagHandling()
	{
		boost::shared_ptr<ProcessContextGrabber<float> > grabber(new ProcessContextGrabber<float>());
		
		assert (frames % 2 == 0);
		chunker.reset (new Chunker<float>(frames));
		chunker->add_output (grabber);
		
		ProcessContext<float> const half_context (random_data, frames / 2, 1);
		ProcessContext<float> const context (random_data, frames, 1);
		context.set_flag(ProcessContext<>::EndOfInput);
		
		// Process 0.5 then 1.0
		chunker->process (half_context);
		chunker->process (context);

		// Should output two contexts
		CPPUNIT_ASSERT_EQUAL((int)grabber->contexts.size(), 2);
		ProcessContextGrabber<float>::ContextList::iterator it = grabber->contexts.begin();

		// first 1.0 not end of input
		CPPUNIT_ASSERT_EQUAL(it->frames(), frames);
		CPPUNIT_ASSERT(!it->has_flag(ProcessContext<>::EndOfInput));

		// Then 0.5 with end of input
		++it;
		CPPUNIT_ASSERT_EQUAL(it->frames(), frames / 2);
		CPPUNIT_ASSERT(it->has_flag(ProcessContext<>::EndOfInput));
	}

  private:
	boost::shared_ptr<Chunker<float> > chunker;
	boost::shared_ptr<VectorSink<float> > sink;

	float * random_data;
	framecnt_t frames;
};

CPPUNIT_TEST_SUITE_REGISTRATION (ChunkerTest);

