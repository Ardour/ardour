/*
 * Copyright (C) 2017-2019 Robin Gareus <robin@gareus.org>
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

#include "ardour/location.h"
#include "ardour/session.h"

#include "actions.h"
#include "ardour_ui.h"
#include "transport_control.h"

#include "pbd/i18n.h"

using namespace Gtk;

TransportControlProvider::TransportControlProvider ()
	: roll_controllable (new TransportControllable ("transport roll", TransportControllable::Roll))
	, stop_controllable (new TransportControllable ("transport stop", TransportControllable::Stop))
	, goto_start_controllable (new TransportControllable ("transport goto start", TransportControllable::GotoStart))
	, goto_end_controllable (new TransportControllable ("transport goto end", TransportControllable::GotoEnd))
	, auto_loop_controllable (new TransportControllable ("transport auto loop", TransportControllable::AutoLoop))
	, play_selection_controllable (new TransportControllable ("transport play selection", TransportControllable::PlaySelection))
	, rec_controllable (new TransportControllable ("transport rec-enable", TransportControllable::RecordEnable))
{
}

TransportControlProvider::TransportControllable::TransportControllable (std::string name, ToggleType tp)
	: Controllable (name), type(tp)
{
}

void
TransportControlProvider::TransportControllable::set_value (double val, PBD::Controllable::GroupControlDisposition /*group_override*/)
{
	if (val == 0.0) {
		/* do nothing: these are radio-style actions */
		return;
	}

	const char *action = 0;

	switch (type) {
	case Roll:
		action = X_("Roll");
		break;
	case Stop:
		action = X_("Stop");
		break;
	case GotoStart:
		action = X_("GotoStart");
		break;
	case GotoEnd:
		action = X_("GotoEnd");
		break;
	case AutoLoop:
		action = X_("Loop");
		break;
	case PlaySelection:
		action = X_("PlaySelection");
		break;
	case RecordEnable:
		action = X_("Record");
		break;
	default:
		break;
	}

	if (action == 0) {
		return;
	}

	Glib::RefPtr<Action> act = ActionManager::get_action ("Transport", action);

	if (act) {
		act->activate ();
	}
}

double
TransportControlProvider::TransportControllable::get_value () const
{
	if (!_session) {
		return 0.0;
	}

	ARDOUR::Location* rloc;

	switch (type) {
	case Roll:
		return (_session->transport_rolling() ? 1.0 : 0.0);
	case Stop:
		return (!_session->transport_rolling() ? 1.0 : 0.0);
	case GotoStart:
		if ((rloc = _session->locations()->session_range_location()) != 0) {
			return (_session->transport_sample() == rloc->start_sample() ? 1.0 : 0.0);
		}
		return 0.0;
	case GotoEnd:
		if ((rloc = _session->locations()->session_range_location()) != 0) {
			return (_session->transport_sample() == rloc->end_sample() ? 1.0 : 0.0);
		}
		return 0.0;
	case AutoLoop:
		return ((_session->get_play_loop() && _session->transport_rolling())? 1.0 : 0.0);
	case PlaySelection:
		return ((_session->transport_rolling() && _session->get_play_range()) ? 1.0 : 0.0);
	case RecordEnable:
		return (_session->actively_recording() ? 1.0 : 0.0);
	default:
		break;
	}

	return 0.0;
}
