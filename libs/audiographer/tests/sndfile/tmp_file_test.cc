#include "tests/utils.h"
#include "audiographer/sndfile/tmp_file.h"

using namespace AudioGrapher;

class TmpFileTest : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE (TmpFileTest);
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
		uint32_t channels = 2;
		file.reset (new TmpFile<float>(SF_FORMAT_WAV | SF_FORMAT_FLOAT, channels, 44100));
		AllocatingProcessContext<float> c (random_data, frames, channels);
		c.set_flag (ProcessContext<float>::EndOfInput);
		file->process (c);
		
		TypeUtils<float>::zero_fill (c.data (), c.frames());
		
		file->seek (0, SEEK_SET);
		file->read (c);
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, c.data(), c.frames()));
	}

  private:
	boost::shared_ptr<TmpFile<float> > file;

	float * random_data;
	framecnt_t frames;
};

CPPUNIT_TEST_SUITE_REGISTRATION (TmpFileTest);

