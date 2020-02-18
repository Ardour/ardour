/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2016 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "control_surfaces_test.h"
#include "control_protocol/control_protocol.h"
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
	_session->new_audio_track (1, 2, NULL, 1, "Test", PresentationInfo::max_order, Normal);

	ControlProtocolManager& m = ControlProtocolManager::instance ();
	for (list<ControlProtocolInfo*>::iterator i = m.control_protocol_info.begin(); i != m.control_protocol_info.end(); ++i) {
#if 1
		/* Push2 needs libcanvas -- which needs pango, which needs a screen
		 * IA__gdk_pango_context_get_for_screen: assertion 'GDK_IS_SCREEN (screen)' failed
		 */
		if ((*i)->name == "Ableton Push 2") {
			continue;
		}
#endif
		std::cout << "ControlSurfacesTest: " << (*i)->name << "\n";
		if ((*i)->protocol && (*i)->protocol->active()) {
			/* may already be active because of user preferences */
			m.deactivate (**i);
		}

		m.activate (**i);
		m.activate (**i); // should be a NO-OP, prints a warning

		m.deactivate (**i);
		m.deactivate (**i); // should be a NO-OP
	}
}
