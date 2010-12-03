#include "tests/utils.h"

#include "audiographer/general/interleaver.h"
#include "audiographer/general/deinterleaver.h"

using namespace AudioGrapher;

class InterleaverDeInterleaverTest : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE (InterleaverDeInterleaverTest);
  CPPUNIT_TEST (testInterleavedInput);
  CPPUNIT_TEST (testDeInterleavedInput);
  CPPUNIT_TEST_SUITE_END ();

  public:
	void setUp()
	{
		channels = 3;
		frames_per_channel = 128;
		total_frames = channels * frames_per_channel;
		
		random_data_a = TestUtils::init_random_data (total_frames, 1.0);
		random_data_b = TestUtils::init_random_data (frames_per_channel, 1.0);
		random_data_c = TestUtils::init_random_data (frames_per_channel, 1.0);

		deinterleaver.reset (new DeInterleaver<float>());
		interleaver.reset (new Interleaver<float>());
		
		sink_a.reset (new VectorSink<float>());
		sink_b.reset (new VectorSink<float>());
		sink_c.reset (new VectorSink<float>());
	}

	void tearDown()
	{
		delete [] random_data_a;
		delete [] random_data_b;
		delete [] random_data_c;
	}

	void testInterleavedInput()
	{
		deinterleaver->init (channels, frames_per_channel);
		interleaver->init (channels, frames_per_channel);
		
		deinterleaver->output (0)->add_output (interleaver->input (0));
		deinterleaver->output (1)->add_output (interleaver->input (1));
		deinterleaver->output (2)->add_output (interleaver->input (2));
		
		interleaver->add_output (sink_a);
		
		// Process and assert
		ProcessContext<float> c (random_data_a, total_frames, channels);
		deinterleaver->process (c);
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data_a, sink_a->get_array(), total_frames));
		
		// And a second round...
		framecnt_t less_frames = (frames_per_channel / 10) * channels;
		deinterleaver->process (c.beginning (less_frames));
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data_a, sink_a->get_array(), less_frames));
	}
	
	void testDeInterleavedInput()
	{
		deinterleaver->init (channels, frames_per_channel);
		interleaver->init (channels, frames_per_channel);
		
		interleaver->add_output (deinterleaver);
		
		deinterleaver->output (0)->add_output (sink_a);
		deinterleaver->output (1)->add_output (sink_b);
		deinterleaver->output (2)->add_output (sink_c);
		
		ProcessContext<float> c_a (random_data_a, frames_per_channel, 1);
		ProcessContext<float> c_b (random_data_b, frames_per_channel, 1);
		ProcessContext<float> c_c (random_data_c, frames_per_channel, 1);
		
		// Process and assert
		interleaver->input (0)->process (c_a);
		interleaver->input (1)->process (c_b);
		interleaver->input (2)->process (c_c);
		
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data_a, sink_a->get_array(), frames_per_channel));
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data_b, sink_b->get_array(), frames_per_channel));
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data_c, sink_c->get_array(), frames_per_channel));
		
		// And a second round...
		framecnt_t less_frames = frames_per_channel / 5;
		interleaver->input (0)->process (c_a.beginning (less_frames));
		interleaver->input (1)->process (c_b.beginning (less_frames));
		interleaver->input (2)->process (c_c.beginning (less_frames));
		
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data_a, sink_a->get_array(), less_frames));
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data_b, sink_b->get_array(), less_frames));
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data_c, sink_c->get_array(), less_frames));
		
	}

  private:
	boost::shared_ptr<Interleaver<float> > interleaver;
	boost::shared_ptr<DeInterleaver<float> > deinterleaver;
	
	boost::shared_ptr<VectorSink<float> > sink_a;
	boost::shared_ptr<VectorSink<float> > sink_b;
	boost::shared_ptr<VectorSink<float> > sink_c;

	float * random_data_a;
	float * random_data_b;
	float * random_data_c;
	
	framecnt_t frames_per_channel;
	framecnt_t total_frames;
	unsigned int channels;
};

CPPUNIT_TEST_SUITE_REGISTRATION (InterleaverDeInterleaverTest);

