#include "tests/utils.h"

#include "audiographer/general/sample_format_converter.h"

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
		samples = 128;
		random_data = TestUtils::init_random_data(samples, 1.0);
	}

	void tearDown()
	{
		delete [] random_data;
	}

	void testInit()
	{
		// Float never uses dithering and should always use full 32 bits of data
		boost::shared_ptr<SampleFormatConverter<float> > f_converter (new SampleFormatConverter<float>(1));
		f_converter->init (samples, D_Tri, 32); // Doesn't throw
		CPPUNIT_ASSERT_THROW (f_converter->init (samples, D_Tri, 24), Exception);
		CPPUNIT_ASSERT_THROW (f_converter->init (samples, D_Tri, 48), Exception);

		/* Test that too large data widths throw.
		   We are fine with unnecessarily narrow data widths */

		boost::shared_ptr<SampleFormatConverter<int32_t> > i_converter (new SampleFormatConverter<int32_t>(1));
		i_converter->init (samples, D_Tri, 32); // Doesn't throw
		i_converter->init (samples, D_Tri, 24); // Doesn't throw
		i_converter->init (samples, D_Tri, 8); // Doesn't throw
		i_converter->init (samples, D_Tri, 16); // Doesn't throw
		CPPUNIT_ASSERT_THROW (i_converter->init (samples, D_Tri, 48), Exception);

		boost::shared_ptr<SampleFormatConverter<int16_t> > i16_converter (new SampleFormatConverter<int16_t>(1));
		i16_converter->init (samples, D_Tri, 16); // Doesn't throw
		i16_converter->init (samples, D_Tri, 8); // Doesn't throw
		CPPUNIT_ASSERT_THROW (i16_converter->init (samples, D_Tri, 32), Exception);
		CPPUNIT_ASSERT_THROW (i16_converter->init (samples, D_Tri, 48), Exception);

		boost::shared_ptr<SampleFormatConverter<uint8_t> > ui_converter (new SampleFormatConverter<uint8_t>(1));
		ui_converter->init (samples, D_Tri, 8); // Doesn't throw
		ui_converter->init (samples, D_Tri, 4); // Doesn't throw
		CPPUNIT_ASSERT_THROW (ui_converter->init (samples, D_Tri, 16), Exception);
	}

	void testFrameCount()
	{
		boost::shared_ptr<SampleFormatConverter<int32_t> > converter (new SampleFormatConverter<int32_t>(1));
		boost::shared_ptr<VectorSink<int32_t> > sink (new VectorSink<int32_t>());

		converter->init (samples, D_Tri, 32);
		converter->add_output (sink);
		samplecnt_t samples_output = 0;

		{
		ProcessContext<float> pc(random_data, samples / 2, 1);
		converter->process (pc);
		samples_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (samples / 2, samples_output);
		}

		{
		ProcessContext<float> pc(random_data, samples, 1);
		converter->process (pc);
		samples_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (samples, samples_output);
		}

		{
		ProcessContext<float> pc(random_data, samples + 1, 1);
		CPPUNIT_ASSERT_THROW(converter->process (pc), Exception);
		}
	}

	void testFloat()
	{
		boost::shared_ptr<SampleFormatConverter<float> > converter (new SampleFormatConverter<float>(1));
		boost::shared_ptr<VectorSink<float> > sink (new VectorSink<float>());
		samplecnt_t samples_output = 0;

		converter->init(samples, D_Tri, 32);
		converter->add_output (sink);

		converter->set_clip_floats (false);
		ProcessContext<float> const pc(random_data, samples, 1);
		converter->process (pc);
		samples_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (samples, samples_output);
		CPPUNIT_ASSERT (TestUtils::array_equals(sink->get_array(), random_data, samples));

		// Make sure a few samples are < -1.0 and > 1.0
		random_data[10] = -1.5;
		random_data[20] = 1.5;

		converter->set_clip_floats (true);
		converter->process (pc);
		samples_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (samples, samples_output);
		CPPUNIT_ASSERT (TestUtils::array_filled(sink->get_array(), samples));

		for (samplecnt_t i = 0; i < samples; ++i) {
			// fp comparison needs a bit of tolerance, 1.01 << 1.5
			CPPUNIT_ASSERT(sink->get_data()[i] < 1.01);
			CPPUNIT_ASSERT(sink->get_data()[i] > -1.01);
		}
	}

	void testInt32()
	{
		boost::shared_ptr<SampleFormatConverter<int32_t> > converter (new SampleFormatConverter<int32_t>(1));
		boost::shared_ptr<VectorSink<int32_t> > sink (new VectorSink<int32_t>());
		samplecnt_t samples_output = 0;

		converter->init(samples, D_Tri, 32);
		converter->add_output (sink);

		ProcessContext<float> pc(random_data, samples, 1);
		converter->process (pc);
		samples_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (samples, samples_output);
		CPPUNIT_ASSERT (TestUtils::array_filled(sink->get_array(), samples));
	}

	void testInt24()
	{
		boost::shared_ptr<SampleFormatConverter<int32_t> > converter (new SampleFormatConverter<int32_t>(1));
		boost::shared_ptr<VectorSink<int32_t> > sink (new VectorSink<int32_t>());
		samplecnt_t samples_output = 0;

		converter->init(samples, D_Tri, 24);
		converter->add_output (sink);

		ProcessContext<float> pc(random_data, samples, 1);
		converter->process (pc);
		samples_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (samples, samples_output);
		CPPUNIT_ASSERT (TestUtils::array_filled(sink->get_array(), samples));
	}

	void testInt16()
	{
		boost::shared_ptr<SampleFormatConverter<int16_t> > converter (new SampleFormatConverter<int16_t>(1));
		boost::shared_ptr<VectorSink<int16_t> > sink (new VectorSink<int16_t>());
		samplecnt_t samples_output = 0;

		converter->init(samples, D_Tri, 16);
		converter->add_output (sink);

		ProcessContext<float> pc(random_data, samples, 1);
		converter->process (pc);
		samples_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (samples, samples_output);
		CPPUNIT_ASSERT (TestUtils::array_filled(sink->get_array(), samples));
	}

	void testUint8()
	{
		boost::shared_ptr<SampleFormatConverter<uint8_t> > converter (new SampleFormatConverter<uint8_t>(1));
		boost::shared_ptr<VectorSink<uint8_t> > sink (new VectorSink<uint8_t>());
		samplecnt_t samples_output = 0;

		converter->init(samples, D_Tri, 8);
		converter->add_output (sink);

		ProcessContext<float> pc(random_data, samples, 1);
		converter->process (pc);
		samples_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (samples, samples_output);
		CPPUNIT_ASSERT (TestUtils::array_filled(sink->get_array(), samples));
	}

	void testChannelCount()
	{
		boost::shared_ptr<SampleFormatConverter<int32_t> > converter (new SampleFormatConverter<int32_t>(3));
		boost::shared_ptr<VectorSink<int32_t> > sink (new VectorSink<int32_t>());
		samplecnt_t samples_output = 0;

		converter->init(samples, D_Tri, 32);
		converter->add_output (sink);

		ProcessContext<float> pc(random_data, 4, 1);
		CPPUNIT_ASSERT_THROW (converter->process (pc), Exception);

		samplecnt_t new_sample_count = samples - (samples % 3);
		converter->process (ProcessContext<float> (pc.data(), new_sample_count, 3));
		samples_output = sink->get_data().size();
		CPPUNIT_ASSERT_EQUAL (new_sample_count, samples_output);
		CPPUNIT_ASSERT (TestUtils::array_filled(sink->get_array(), pc.samples()));
	}

  private:

	float * random_data;
	samplecnt_t samples;
};

CPPUNIT_TEST_SUITE_REGISTRATION (SampleFormatConverterTest);

