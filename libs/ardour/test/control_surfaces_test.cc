/*
    Copyright (C) 2012 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "control_surfaces_test.h"
#include "ardour/control_protocol_manager.h"
#include "ardour/session.h"

CPPUNIT_TEST_SUITE_REGISTRATION (ControlSurfacesTest);

using namespace std;
using namespace ARDOUR;

/** Instantiate and then immediately tear down all our control surfaces.
 *  This is to check that there are no crashes when doing this.
 */
void
ControlSurfacesTest::instantiateAndTeardownTest ()
{
	_session->new_audio_track (1, 2, Normal, 0, 1, "Test");
	
	ControlProtocolManager& m = ControlProtocolManager::instance ();
	for (list<ControlProtocolInfo*>::iterator i = m.control_protocol_info.begin(); i != m.control_protocol_info.end(); ++i) {
		m.activate (**i);
		m.deactivate (**i);
	}
}
