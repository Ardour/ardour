#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class BBTTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(BBTTest);
	CPPUNIT_TEST(createTest);
	CPPUNIT_TEST(addTest);
	CPPUNIT_TEST(subtractTest);
	CPPUNIT_TEST(multiplyTest);
	CPPUNIT_TEST(convertTest);
	CPPUNIT_TEST(roundTest);
	CPPUNIT_TEST(deltaTest);
	CPPUNIT_TEST_SUITE_END();

public:
	void createTest();
	void addTest();
	void subtractTest();
	void multiplyTest();
	void convertTest();
	void roundTest();
	void deltaTest();
};
