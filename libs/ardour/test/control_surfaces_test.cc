#include "control_surfaces_test.h"
#include "ardour/control_protocol_manager.h"
#include "ardour/session.h"

CPPUNIT_TEST_SUITE_REGISTRATION (ControlSurfacesTest);

using namespace std;
using namespace ARDOUR;

/** Instantiate and then immediately tear down all our control surfaces.
 *  This is to check that there are no crashes when doing this ...
 */
void
ControlSurfacesTest::instantiateAndTeardownTest ()
{
	_session->new_audio_track (1, 2, Normal, 0, 1, "Test");
	
	ControlProtocolManager& m = ControlProtocolManager::instance ();
	for (list<ControlProtocolInfo*>::iterator i = m.control_protocol_info.begin(); i != m.control_protocol_info.end(); ++i) {
		m.instantiate (**i);
		m.teardown (**i);
	}
}
