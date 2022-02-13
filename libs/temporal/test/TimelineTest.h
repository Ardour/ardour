#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class TimelineTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(TimelineTest);
	CPPUNIT_TEST(createTest);
	CPPUNIT_TEST(addTest);
	CPPUNIT_TEST(subtractTest);
	CPPUNIT_TEST(multiplyTest);
	CPPUNIT_TEST(convertTest);
	CPPUNIT_TEST(roundTest);
	CPPUNIT_TEST_SUITE_END();

public:
	void createTest();
	void addTest();
	void subtractTest();
	void multiplyTest();
	void convertTest();
	void roundTest();
};
