#include "utils.h"
#include "audiographer/sample_format_converter.h"

using namespace AudioGrapher;

class SampleFormatConverterTest : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE (SampleFormatConverterTest);
  CPPUNIT_TEST (testInit);
  CPPUNIT_TEST (testFrameCount);
  CPPUNIT_TEST (testFloat);
  CPPUNIT_TEST (testInt32);
  CPPUNIT_TEST (testInt24);
  CPPUNIT_TEST (testInt16);
  CPPUNIT_TEST (testUint8);
  CPPUNIT_TEST (testChannelCount);
  CPPUNIT_TEST_SUITE_END ();

  public:
	void setUp()
	{
		frames = 128;
		random_data = TestUtils::init_random_data(frames, 1.0);
	}

	void tearDown()
	{
		delete [] random_data;
	}

	void testInit()
	{
		boost::shared_ptr<SampleFormatConverter<float> > f_converter (new SampleFormatConverter<float>(1));
		f_converter->init (frames, D_Tri, 32); // Doesn't throw
		CPPUNIT_ASSERT_THROW (f_converter->init (frames, D_Tri, 24), Exception);
		CPPUNIT_ASSERT_THROW (f_converter->init (frames, D_Tri, 48), Exception);
		
		boost::shared_ptr<SampleFormatConverter<int32_t> > i_converter (new SampleFormatConverter<int32_t>(1));
		i_converter->init (frames, D_Tri, 32); // Doesn't throw
		i_converter->init (frames, D_Tri, 24); // Doesn't throw
		CPPUNIT_ASSERT_THROW (i_converter->init (frames, D_Tri, 8), Exception);
		CPPUNIT_ASSERT_THROW (i_converter->init (frames, D_Tri, 16), Exception);
		CPPUNIT_ASSERT_THROW (i_converter->init (frames, D_Tri, 48), Exception);
		
		boost::shared_ptr<SampleFormatConverter<int16_t> > i16_converter (new SampleFormatConverter<int16_t>(1));
		i16_converter->init (frames, D_Tri, 16); // Doesn't throw
		CPPUNIT_ASSERT_THROW (i16_converter->init (frames, D_Tri, 8), Exception);
		CPPUNIT_ASSERT_THROW (i16_converter->init (frames, D_Tri, 32), Exception);
		CPPUNIT_ASSERT_THROW (i16_converter->init (frames, D_Tri, 48), Exception);
		
		boost::shared_ptr<SampleFormatConverter<uint8_t> > ui_converter (new SampleFormatConverter<uint8_t>(1));
		ui_converter->init (frames, D_Tri, 8); // Doesn't throw
		CPPUNIT_ASSERT_THROW (ui_converter->init (frames, D_Tri, 4), Exception);
		CPPUNIT_ASSERT_THROW (ui_converter->init (frames, D_Tri, 16), Exception);
	}

	void testFrameCount()
	{
		boost::shared_ptr<SampleFormatConverter<int32_t> > converter (new SampleFormatConverter<int32_t>(1));
		boost::shared_ptr<VectorSink<int32_t> > sink (new VectorSink<int32_t>());
		
		converter->init (frames, D_Tri, 32);
		converter->add_output (sink);
		nframes_t frames_output = 0;
		
		{
		ProcessContext<float> pc(random_data, frames / 2, 1);
		converter->process (pc);
		frames_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (frames / 2, frames_output);
		}
		
		{
		ProcessContext<float> pc(random_data, frames, 1);
		converter->process (pc);
		frames_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (frames, frames_output);
		}
		
		{
		ProcessContext<float> pc(random_data, frames + 1, 1);
		CPPUNIT_ASSERT_THROW(converter->process (pc), Exception);
		}
	}

	void testFloat()
	{
		boost::shared_ptr<SampleFormatConverter<float> > converter (new SampleFormatConverter<float>(1));
		boost::shared_ptr<VectorSink<float> > sink (new VectorSink<float>());
		nframes_t frames_output = 0;
		
		converter->init(frames, D_Tri, 32);
		converter->add_output (sink);
		
		converter->set_clip_floats (false);
		ProcessContext<float> const pc(random_data, frames, 1);
		converter->process (pc);
		frames_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (frames, frames_output);
		CPPUNIT_ASSERT (TestUtils::array_equals(sink->get_array(), random_data, frames));
		
		// Make sure a few samples are < -1.0 and > 1.0
		random_data[10] = -1.5;
		random_data[20] = 1.5;
		
		converter->set_clip_floats (true);
		converter->process (pc);
		frames_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (frames, frames_output);
		CPPUNIT_ASSERT (TestUtils::array_filled(sink->get_array(), frames));
		
		for (nframes_t i = 0; i < frames; ++i) {
			// fp comparison needs a bit of tolerance, 1.01 << 1.5
			CPPUNIT_ASSERT(sink->get_data()[i] < 1.01);
			CPPUNIT_ASSERT(sink->get_data()[i] > -1.01);
		}
	}

	void testInt32()
	{
		boost::shared_ptr<SampleFormatConverter<int32_t> > converter (new SampleFormatConverter<int32_t>(1));
		boost::shared_ptr<VectorSink<int32_t> > sink (new VectorSink<int32_t>());
		nframes_t frames_output = 0;
		
		converter->init(frames, D_Tri, 32);
		converter->add_output (sink);
		
		ProcessContext<float> pc(random_data, frames, 1);
		converter->process (pc);
		frames_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (frames, frames_output);
		CPPUNIT_ASSERT (TestUtils::array_filled(sink->get_array(), frames));
	}
	
	void testInt24()
	{
		boost::shared_ptr<SampleFormatConverter<int32_t> > converter (new SampleFormatConverter<int32_t>(1));
		boost::shared_ptr<VectorSink<int32_t> > sink (new VectorSink<int32_t>());
		nframes_t frames_output = 0;
		
		converter->init(frames, D_Tri, 24);
		converter->add_output (sink);
		
		ProcessContext<float> pc(random_data, frames, 1);
		converter->process (pc);
		frames_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (frames, frames_output);
		CPPUNIT_ASSERT (TestUtils::array_filled(sink->get_array(), frames));
	}
	
	void testInt16()
	{
		boost::shared_ptr<SampleFormatConverter<int16_t> > converter (new SampleFormatConverter<int16_t>(1));
		boost::shared_ptr<VectorSink<int16_t> > sink (new VectorSink<int16_t>());
		nframes_t frames_output = 0;
		
		converter->init(frames, D_Tri, 16);
		converter->add_output (sink);
		
		ProcessContext<float> pc(random_data, frames, 1);
		converter->process (pc);
		frames_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (frames, frames_output);
		CPPUNIT_ASSERT (TestUtils::array_filled(sink->get_array(), frames));
	}
	
	void testUint8()
	{
		boost::shared_ptr<SampleFormatConverter<uint8_t> > converter (new SampleFormatConverter<uint8_t>(1));
		boost::shared_ptr<VectorSink<uint8_t> > sink (new VectorSink<uint8_t>());
		nframes_t frames_output = 0;
		
		converter->init(frames, D_Tri, 8);
		converter->add_output (sink);
		
		ProcessContext<float> pc(random_data, frames, 1);
		converter->process (pc);
		frames_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (frames, frames_output);
		CPPUNIT_ASSERT (TestUtils::array_filled(sink->get_array(), frames));
	}
	
	void testChannelCount()
	{
		boost::shared_ptr<SampleFormatConverter<int32_t> > converter (new SampleFormatConverter<int32_t>(3));
		boost::shared_ptr<VectorSink<int32_t> > sink (new VectorSink<int32_t>());
		nframes_t frames_output = 0;
		
		converter->init(frames, D_Tri, 32);
		converter->add_output (sink);
		
		ProcessContext<float> pc(random_data, 4, 1);
		CPPUNIT_ASSERT_THROW (converter->process (pc), Exception);
		
		pc.frames() = frames - (frames % 3);
		converter->process (pc);
		frames_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (pc.frames(), frames_output);
		CPPUNIT_ASSERT (TestUtils::array_filled(sink->get_array(), pc.frames()));
	}

  private:

	float * random_data;
	nframes_t frames;
};

CPPUNIT_TEST_SUITE_REGISTRATION (SampleFormatConverterTest);

