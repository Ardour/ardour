#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class TestNeedingSession : public CppUnit::TestFixture
{
public:
	virtual void setUp ();
	virtual void tearDown ();

protected:
	ARDOUR::Session* _session;
};
