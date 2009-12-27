#include "utils.h"
#include "audiographer/sndfile_writer.h"

using namespace AudioGrapher;

class SndfileWriterTest : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE (SndfileWriterTest);
  CPPUNIT_TEST (testProcess);
  CPPUNIT_TEST_SUITE_END ();

  public:
	void setUp()
	{
		frames = 128;
		random_data = TestUtils::init_random_data(frames);
	}

	void tearDown()
	{
		delete [] random_data;
	}

	void testProcess()
	{
		uint channels = 2;
		std::string filename ("test.wav");
		writer.reset (new SndfileWriter<float>(channels, 44100, SF_FORMAT_WAV | SF_FORMAT_FLOAT, filename));
		ProcessContext<float> c (random_data, frames, channels);
		c.set_flag (ProcessContext<float>::EndOfInput);
		writer->process (c);
	}

  private:
	boost::shared_ptr<SndfileWriter<float> > writer;

	float * random_data;
	nframes_t frames;
};

CPPUNIT_TEST_SUITE_REGISTRATION (SndfileWriterTest);

