#include <boost/shared_ptr.hpp>
#include "test_needing_session.h"

class LuaScriptTest : public TestNeedingSession
{
	CPPUNIT_TEST_SUITE (LuaScriptTest);
	CPPUNIT_TEST (session_script_test);
	CPPUNIT_TEST (dsp_script_test);
	CPPUNIT_TEST_SUITE_END ();

public:
	void session_script_test ();
	void dsp_script_test ();
};
