/*
 * Copyright (C) 2011-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2021 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2024 Robin Gareus <robin@gareus.org>
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

#include "pbd/compose.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"

#include "ardour/session.h"

#include "audio_region_editor.h"
#include "audio_region_view.h"
#include "editor.h"
#include "region_view.h"
#include "route_properties_box.h"
#include "selection_properties_box.h"
#include "time_info_box.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using std::min;
using std::max;

SelectionPropertiesBox::SelectionPropertiesBox ()
{
	init ();

	_time_info_box  = new TimeInfoBox ("EditorTimeInfo", true);
	_route_prop_box = new RoutePropertiesBox ();

	pack_start(*_time_info_box, false, false, 0);
	pack_start(*_route_prop_box, true, true, 0);

	_time_info_box->set_no_show_all ();
	_route_prop_box->set_no_show_all ();

	_time_info_box->hide ();
	_route_prop_box->hide ();
}

SelectionPropertiesBox::~SelectionPropertiesBox ()
{
	delete _time_info_box;
	delete _route_prop_box;
}

void
SelectionPropertiesBox::init ()
{
	Selection& selection (Editor::instance().get_selection());

	/* watch for any change in our selection, so we can show an appropriate property editor */
	selection.TracksChanged.connect (sigc::mem_fun (*this, &SelectionPropertiesBox::selection_changed));
	selection.RegionsChanged.connect (sigc::mem_fun (*this, &SelectionPropertiesBox::selection_changed));
	selection.TimeChanged.connect (sigc::mem_fun (*this, &SelectionPropertiesBox::selection_changed));
	selection.LinesChanged.connect (sigc::mem_fun (*this, &SelectionPropertiesBox::selection_changed));
	selection.PlaylistsChanged.connect (sigc::mem_fun (*this, &SelectionPropertiesBox::selection_changed));
	selection.PointsChanged.connect (sigc::mem_fun (*this, &SelectionPropertiesBox::selection_changed));
	selection.MarkersChanged.connect (sigc::mem_fun (*this, &SelectionPropertiesBox::selection_changed));
	selection.MidiNotesChanged.connect (sigc::mem_fun (*this, &SelectionPropertiesBox::selection_changed));
	selection.TriggersChanged.connect (sigc::mem_fun (*this, &SelectionPropertiesBox::selection_changed));

	/* maybe we care about mouse mode?? */
	Editor::instance().MouseModeChanged.connect (_editor_connection, invalidator(*this), std::bind (&SelectionPropertiesBox::track_mouse_mode, this), gui_context());
}

void
SelectionPropertiesBox::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	if (!s) {
		return;
	}

	_time_info_box->set_session(s);
	_route_prop_box->set_session(s);

	selection_changed();
}

void
SelectionPropertiesBox::track_mouse_mode ()
{
	/* maybe do something here? */
}

void
SelectionPropertiesBox::selection_changed ()
{
	if (!_session || _session->inital_connect_or_deletion_in_progress ()) {
		_time_info_box->hide ();
		_route_prop_box->hide ();
		return;
	}

	Selection& selection (Editor::instance().get_selection());

	if (!selection.time.empty ()) {
		_time_info_box->show ();
	} else {
		_time_info_box->hide ();
	}

	if (!selection.tracks.empty ()) {
		TimeAxisView *tav = selection.tracks.front ();
		RouteTimeAxisView *rtav = dynamic_cast<RouteTimeAxisView *>(tav);
		_route_prop_box->set_route (rtav->route());
		_route_prop_box->show();
	} else {
		_route_prop_box->hide();
	}

	}
}
