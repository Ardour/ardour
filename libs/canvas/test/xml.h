#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class XMLTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (XMLTest);
	CPPUNIT_TEST (get);
	CPPUNIT_TEST (set);
	CPPUNIT_TEST_SUITE_END ();

public:
	void get ();
	void set ();

private:	
	void check (std::string const &);
};
