#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class RenderTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (RenderTest);
	CPPUNIT_TEST (basics);
	CPPUNIT_TEST_SUITE_END ();

public:
	void basics ();
	void check (std::string const &);
};

