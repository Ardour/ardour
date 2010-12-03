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
		frames = 128;
		
		random_data = TestUtils::init_random_data(frames);
		random_data[0] = 0.5;
		random_data[frames - 1] = 0.5;
		
		zero_data = new float[frames];
		memset(zero_data, 0, frames * sizeof(float));
		
		half_random_data = TestUtils::init_random_data(frames);
		memset(half_random_data, 0, (frames / 2) * sizeof(float));
		
		trimmer.reset (new SilenceTrimmer<float> (frames / 2));
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
		ProcessContext<float> c (zero_data, frames, 1);
		trimmer->process (c);
		framecnt_t frames_processed = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL ((framecnt_t) 0, frames_processed);
		}
		
		{
		ProcessContext<float> c (random_data, frames, 1);
		trimmer->process (c);
		framecnt_t frames_processed = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (frames, frames_processed);
		CPPUNIT_ASSERT (TestUtils::array_equals (sink->get_array(), random_data, frames));
		}
		
		{
		ProcessContext<float> c (zero_data, frames, 1);
		trimmer->process (c);
		framecnt_t frames_processed = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (frames, frames_processed);
		}
		
		{
		ProcessContext<float> c (random_data, frames, 1);
		trimmer->process (c);
		framecnt_t frames_processed = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (3 * frames, frames_processed);
		CPPUNIT_ASSERT (TestUtils::array_equals (sink->get_array(), random_data, frames));
		CPPUNIT_ASSERT (TestUtils::array_equals (&sink->get_array()[frames], zero_data, frames));
		CPPUNIT_ASSERT (TestUtils::array_equals (&sink->get_array()[2 * frames], random_data, frames));
		}
		
		{
		ProcessContext<float> c (zero_data, frames, 1);
		trimmer->process (c);
		framecnt_t frames_processed = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (3 * frames, frames_processed);
		}
	}
	
	void testPartialBuffers()
	{
		trimmer->add_output (sink);
		trimmer->reset (frames / 4);
		trimmer->set_trim_beginning (true);
		trimmer->set_trim_end (true);
		
		{
		ProcessContext<float> c (half_random_data, frames, 1);
		trimmer->process (c);
		framecnt_t frames_processed = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (frames / 2, frames_processed);
		CPPUNIT_ASSERT (TestUtils::array_equals (sink->get_array(), &half_random_data[frames / 2], frames / 2));
		}
		
		{
		ProcessContext<float> c (zero_data, frames, 1);
		trimmer->process (c);
		framecnt_t frames_processed = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (frames / 2, frames_processed);
		}
		
		{
		ProcessContext<float> c (half_random_data, frames, 1);
		trimmer->process (c);
		framecnt_t frames_processed = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (2 * frames + frames / 2, frames_processed);
		CPPUNIT_ASSERT (TestUtils::array_equals (&sink->get_array()[frames + frames / 2], half_random_data, frames));
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
		
 		framecnt_t silence = frames / 2;
		trimmer->add_silence_to_beginning (silence);
		
		{
		ProcessContext<float> c (random_data, frames, 1);
		trimmer->process (c);
		}
		
		CPPUNIT_ASSERT (TestUtils::array_equals (sink->get_array(), zero_data, silence));
		CPPUNIT_ASSERT (TestUtils::array_equals (&sink->get_array()[silence], random_data, frames));
	}
	
	void testAddSilenceEnd()
	{
		trimmer->add_output (sink);
		
		framecnt_t silence = frames / 3;
		trimmer->add_silence_to_end (silence);
		
		{
		ProcessContext<float> c (random_data, frames, 1);
		trimmer->process (c);
		}
		
		{
		ProcessContext<float> c (random_data, frames, 1);
		c.set_flag (ProcessContext<float>::EndOfInput);
		trimmer->process (c);
		}
		
		framecnt_t frames_processed = sink->get_data().size();
		framecnt_t total_frames = 2 * frames + silence;
		CPPUNIT_ASSERT_EQUAL (total_frames, frames_processed);
		CPPUNIT_ASSERT (TestUtils::array_equals (sink->get_array(), random_data, frames));
		CPPUNIT_ASSERT (TestUtils::array_equals (&sink->get_array()[frames], random_data, frames));
		CPPUNIT_ASSERT (TestUtils::array_equals (&sink->get_array()[frames * 2], zero_data, silence));
	}

  private:
	boost::shared_ptr<SilenceTrimmer<float> > trimmer;
	boost::shared_ptr<AppendingVectorSink<float> > sink;

	float * random_data;
	float * zero_data;
	float * half_random_data;
	framecnt_t frames;
};

CPPUNIT_TEST_SUITE_REGISTRATION (SilenceTrimmerTest);
