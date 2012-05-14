#include "tests/utils.h"

#include "audiographer/type_utils.h"

using namespace AudioGrapher;

class TypeUtilsTest : public CppUnit::TestFixture
{
  CPPUNIT_TEST_SUITE (TypeUtilsTest);
  CPPUNIT_TEST (testZeroFillPod);
  CPPUNIT_TEST (testZeroFillNonPod);
  CPPUNIT_TEST (testCopy);
  CPPUNIT_TEST (testMoveBackward);
  CPPUNIT_TEST (testMoveForward);
  CPPUNIT_TEST_SUITE_END ();

  public:
	void setUp()
	{
		
	}

	void tearDown()
	{
		
	}

	void testZeroFillPod()
	{
		unsigned frames = 10;
		float buf[frames];
		TypeUtils<float>::zero_fill (buf, frames);
		float zero = 0.0;
		for (unsigned i = 0; i < frames; ++i) {
			CPPUNIT_ASSERT_EQUAL (zero, buf[i]);
		}
	}
	
	void testZeroFillNonPod()
	{
                /* does not compile on OS X Lion
		unsigned frames = 10;
		NonPodType buf[frames];
		TypeUtils<NonPodType>::zero_fill (buf, frames);
		NonPodType zero;
		for (unsigned i = 0; i < frames; ++i) {
			CPPUNIT_ASSERT (zero == buf[i]);
		}
                */
	}
	
	void testMoveBackward()
	{
		int seq[8] = { 0, 1, 2, 3,
		               4, 5, 6, 7 };
		
		TypeUtils<int>::move (&seq[4], &seq[2], 4);
		
		for (int i = 2; i < 2 + 4; ++i) {
			CPPUNIT_ASSERT_EQUAL (i + 2, seq[i]);
		}
	}
	
	void testMoveForward()
	{
		int seq[8] = { 0, 1, 2, 3,
		               4, 5, 6, 7 };
		
		TypeUtils<int>::move (&seq[2], &seq[4], 4);
		
		for (int i = 4; i < 4 + 4; ++i) {
			CPPUNIT_ASSERT_EQUAL (i - 2, seq[i]);
		}
	}

	void testCopy()
	{
		int const seq1[4] = { 1, 2, 3, 4 };
		int const seq2[4] = { 5, 6, 7, 8 };
		int seq3[8] = { 0, 0, 0, 0,
		                  0, 0, 0, 0 };
		
		TypeUtils<int>::copy (seq1, seq3, 4);
		for (int i = 0; i < 4; ++i) {
			CPPUNIT_ASSERT_EQUAL (seq1[i], seq3[i]);
		}
		
		for (int i = 4; i < 8; ++i) {
			CPPUNIT_ASSERT_EQUAL (0, seq3[i]);
		}
		
		TypeUtils<int>::copy (seq2, &seq3[4], 4);
		for (int i = 0; i < 4; ++i) {
			CPPUNIT_ASSERT_EQUAL (seq1[i], seq3[i]);
		}
		for (int i = 0; i < 4; ++i) {
			CPPUNIT_ASSERT_EQUAL (seq2[i], seq3[4 + i]);
		}
	}

  private:
	
	struct NonPodType {
		NonPodType() : data (42) {}
		bool operator== (NonPodType const & other) const
			{ return data == other.data; }
		int data;
	};
	

};

CPPUNIT_TEST_SUITE_REGISTRATION (TypeUtilsTest);

