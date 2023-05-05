#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class TempoMapCutBufferTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(TempoMapCutBufferTest);
	CPPUNIT_TEST(createTest);
	CPPUNIT_TEST(cutTest);
	CPPUNIT_TEST(copyTest);
	CPPUNIT_TEST(pasteTest);
	CPPUNIT_TEST_SUITE_END();

public:
	void createTest();
	void cutTest();
	void copyTest();
	void pasteTest();
};
