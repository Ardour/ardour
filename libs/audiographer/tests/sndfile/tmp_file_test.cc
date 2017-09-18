#include "tests/utils.h"
#include "audiographer/sndfile/tmp_file_sync.h"

using namespace AudioGrapher;

class TmpFileTest : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE (TmpFileTest);
  CPPUNIT_TEST (testProcess);
  CPPUNIT_TEST_SUITE_END ();

  public:
	void setUp()
	{
		samples = 128;
		random_data = TestUtils::init_random_data(samples);
	}

	void tearDown()
	{
		delete [] random_data;
	}

	void testProcess()
	{
		uint32_t channels = 2;
		file.reset (new TmpFileSync<float>(SF_FORMAT_WAV | SF_FORMAT_FLOAT, channels, 44100));
		AllocatingProcessContext<float> c (random_data, samples, channels);
		c.set_flag (ProcessContext<float>::EndOfInput);
		file->process (c);

		TypeUtils<float>::zero_fill (c.data (), c.samples());

		file->seek (0, SEEK_SET);
		file->read (c);
		CPPUNIT_ASSERT (TestUtils::array_equals (random_data, c.data(), c.samples()));
	}

  private:
	boost::shared_ptr<TmpFileSync<float> > file;

	float * random_data;
	samplecnt_t samples;
};

CPPUNIT_TEST_SUITE_REGISTRATION (TmpFileTest);

