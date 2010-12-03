#include "tests/utils.h"

#include "audiographer/general/sr_converter.h"

using namespace AudioGrapher;

class SampleRateConverterTest : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE (SampleRateConverterTest);
  CPPUNIT_TEST (testNoConversion);
  CPPUNIT_TEST (testUpsampleLength);
  CPPUNIT_TEST (testDownsampleLength);
  CPPUNIT_TEST (testRespectsEndOfInput);
  CPPUNIT_TEST_SUITE_END ();

  public:
	void setUp()
	{
		frames = 128;
		random_data = TestUtils::init_random_data(frames);
		sink.reset (new AppendingVectorSink<float>());
		grabber.reset (new ProcessContextGrabber<float>());
		converter.reset (new SampleRateConverter (1));
	}

	void tearDown()
	{
		delete [] random_data;
	}

	void testNoConversion()
	{
		assert (frames % 2 == 0);
		framecnt_t const half_frames = frames / 2;
		framecnt_t frames_output = 0;
		
		converter->init (44100, 44100);
		converter->add_output (sink);
		
		ProcessContext<float> c (random_data, half_frames, 1);
		converter->process (c);
		ProcessContext<float> c2 (&random_data[half_frames], half_frames, 1);
		c2.set_flag (ProcessContext<float>::EndOfInput);
		converter->process (c2);
		
		frames_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (frames, frames_output);
		
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, sink->get_array(), frames));
	}

	void testUpsampleLength()
	{
		assert (frames % 2 == 0);
		framecnt_t const half_frames = frames / 2;
		framecnt_t frames_output = 0;
		
		converter->init (44100, 88200);
		converter->allocate_buffers (half_frames);
		converter->add_output (sink);
		
		ProcessContext<float> c (random_data, half_frames, 1);
		converter->process (c);
		ProcessContext<float> c2 (&random_data[half_frames], half_frames, 1);
		c2.set_flag (ProcessContext<float>::EndOfInput);
		converter->process (c2);

		frames_output = sink->get_data().size();
		framecnt_t tolerance = 3;
		CPPUNIT_ASSERT (2 * frames - tolerance < frames_output && frames_output < 2 * frames + tolerance);
	}

	void testDownsampleLength()
	{
		assert (frames % 2 == 0);
		framecnt_t const half_frames = frames / 2;
		framecnt_t frames_output = 0;
		
		converter->init (88200, 44100);
		converter->allocate_buffers (half_frames);
		converter->add_output (sink);
		
		ProcessContext<float> c (random_data, half_frames, 1);
		converter->process (c);
		ProcessContext<float> c2 (&random_data[half_frames], half_frames, 1);
		c2.set_flag (ProcessContext<float>::EndOfInput);
		converter->process (c2);
		
		frames_output = sink->get_data().size();
		framecnt_t tolerance = 3;
		CPPUNIT_ASSERT (half_frames - tolerance < frames_output && frames_output < half_frames + tolerance);
	}
	
	void testRespectsEndOfInput()
	{
		assert (frames % 2 == 0);
		framecnt_t const half_frames = frames / 2;
		
		converter->init (44100, 48000);
		converter->allocate_buffers (half_frames);
		converter->add_output (grabber);
		
		ProcessContext<float> c (random_data, half_frames, 1);
		converter->process (c);
		ProcessContext<float> c2 (&random_data[half_frames], half_frames / 2, 1);
		c2.set_flag (ProcessContext<float>::EndOfInput);
		converter->process (c2);
		
		for (std::list<ProcessContext<float> >::iterator it = grabber->contexts.begin(); it != grabber->contexts.end(); ++it) {
			std::list<ProcessContext<float> >::iterator next = it; ++next;
			if (next == grabber->contexts.end()) {
				CPPUNIT_ASSERT (it->has_flag (ProcessContext<float>::EndOfInput));
			} else {
				CPPUNIT_ASSERT (!it->has_flag (ProcessContext<float>::EndOfInput));
			}
		}
	}
	

  private:
	boost::shared_ptr<SampleRateConverter > converter;
	boost::shared_ptr<AppendingVectorSink<float> > sink;
	boost::shared_ptr<ProcessContextGrabber<float> > grabber;

	float * random_data;
	framecnt_t frames;
};

CPPUNIT_TEST_SUITE_REGISTRATION (SampleRateConverterTest);

