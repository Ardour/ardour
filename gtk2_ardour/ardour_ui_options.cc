/*
    Copyright (C) 2005 Paul Davis 

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

    $Id$
*/

#include <ardour/configuration.h>
#include <ardour/session.h>
#include <ardour/audioengine.h>

#include "ardour_ui.h"
#include "actions.h"

using namespace Gtk;
using namespace ARDOUR;

void
ARDOUR_UI::toggle_time_master ()
{
	bool yn = time_master_button.get_active();

	Config->set_jack_time_master (yn);

	if (session) {
		session->engine().reset_timebase ();
	}
}

void
ARDOUR_UI::toggle_session_state (const char* group, const char* action, void (Session::*set)(bool))
{
	if (session) {
		Glib::RefPtr<Action> act = ActionManager::get_action (group, action);
		if (act) {
			Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
			(session->*set) (tact->get_active());
		}
	}
}

void
ARDOUR_UI::toggle_send_mtc ()
{
	toggle_session_state ("options", "SendMTC", &Session::set_send_mtc);
}

void
ARDOUR_UI::toggle_send_mmc ()
{
	toggle_session_state ("options", "SendMMC", &Session::set_send_mmc);
}

void
ARDOUR_UI::toggle_use_mmc ()
{
	toggle_session_state ("options", "UseMMC", &Session::set_mmc_control);
}

void
ARDOUR_UI::toggle_use_midi_control ()
{
	toggle_session_state ("options", "UseMIDIcontrol", &Session::set_midi_control);
}

void
ARDOUR_UI::toggle_send_midi_feedback ()
{
	toggle_session_state ("options", "SendMIDIfeedback", &Session::set_midi_feedback);
}

void
ARDOUR_UI::toggle_AutoConnectNewTrackInputsToHardware()
{
}
void
ARDOUR_UI::toggle_AutoConnectNewTrackOutputsToHardware()
{
}
void
ARDOUR_UI::toggle_AutoConnectNewTrackOutputsToMaster()
{
}
void
ARDOUR_UI::toggle_ManuallyConnectNewTrackOutputs()
{
}
void
ARDOUR_UI::toggle_UseHardwareMonitoring()
{
}
void
ARDOUR_UI::toggle_UseSoftwareMonitoring()
{
}
void
ARDOUR_UI::toggle_UseExternalMonitoring()
{
}
void
ARDOUR_UI::toggle_StopPluginsWithTransport()
{
}
void
ARDOUR_UI::toggle_RunPluginsWhileRecording()
{
}
void
ARDOUR_UI::toggle_VerifyRemoveLastCapture()
{
}
void
ARDOUR_UI::toggle_StopRecordingOnXrun()
{
}
void
ARDOUR_UI::toggle_StopTransportAtEndOfSession()
{
}
void
ARDOUR_UI::toggle_GainReduceFastTransport()
{
}
void
ARDOUR_UI::toggle_LatchedSolo()
{
}
void
ARDOUR_UI::toggle_SoloViaBus()
{
}
void
ARDOUR_UI::toggle_AutomaticallyCreateCrossfades()
{
}
void
ARDOUR_UI::toggle_UnmuteNewFullCrossfades()
{
}
