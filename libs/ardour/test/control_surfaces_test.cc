#include "control_surfaces_test.h"
#include "ardour/control_protocol_manager.h"
#include "ardour/session.h"

CPPUNIT_TEST_SUITE_REGISTRATION (ControlSurfacesTest);

using namespace std;
using namespace ARDOUR;

void
ControlSurfacesTest::instantiateAndTeardownTest ()
{
	cout << "HELLO!\n";
	_session->new_audio_track (1, 2, Normal, 0, 1, "Test");
	
	ControlProtocolManager& m = ControlProtocolManager::instance ();
	cout << "CST: Test " << m.control_protocol_info.size() << "\n";
	for (list<ControlProtocolInfo*>::iterator i = m.control_protocol_info.begin(); i != m.control_protocol_info.end(); ++i) {
		cout << "CST: Test " << (*i)->name << "\n";
		m.instantiate (**i);
		m.teardown (**i);
	}
}
