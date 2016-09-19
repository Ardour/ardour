
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class SessionTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE (SessionTest);
	CPPUNIT_TEST (new_session);
	CPPUNIT_TEST (new_session_from_template);
	CPPUNIT_TEST (open_session_utf8_path);
	CPPUNIT_TEST_SUITE_END ();

public:

	void new_session ();
	void new_session_from_template ();
	void open_session_utf8_path ();
};
