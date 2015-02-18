/*
    Copyright (C) 2000-2015 Waves Audio Ltd.

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

#include <gio/gio.h>
#include <gtk/gtkiconfactory.h>

#include "pbd/memento_command.h"
#include "pbd/file_utils.h"
#include "gtkmm2ext/utils.h"

#include "ardour/filesystem_paths.h"
#include "ardour/profile.h"
#include "ardour/session.h"
#include "ardour/types.h"

#include "canvas/canvas.h"
#include "canvas/pixbuf.h"

#include "actions.h"
#include "ardour_ui.h"
#include "editing.h"
#include "editor.h"
#include "gui_thread.h"
#include "time_axis_view.h"
#include "utils.h"
#include "i18n.h"

void 
Editor::move_markers_command (std::list<Marker*>&markers, const std::list<ARDOUR::Location*>& locations)
{
	const size_t markers_count = markers.size ();
	if (markers_count != locations.size ()) {
		WavesMessageDialog (_("Move Markers"), _("MOVE MARKERS: Invalid argument!")).run ();
		return;
	}

	std::list<Marker*>::iterator mi;
	std::list<ARDOUR::Location*>::const_iterator li;

	for (mi = markers.begin (); mi != markers.end (); ++mi) {
		ARDOUR::Location* location = (*mi)->location ();
		if (location && !location->locked ()) {
			break;
		}
	}

	if (mi == markers.end ()) {
		return;
	}

	begin_reversible_command (_("move marker"));
	XMLNode &before = session()->locations()->get_state();

	for (mi = markers.begin (), li = locations.begin (); mi != markers.end (); ++mi, ++li) {
		ARDOUR::Location* location = (*mi)->location ();
		ARDOUR::Location* copy = (*li);

		if (location && !location->locked()) {
			if (location->is_mark()) {
				location->set_start (copy->start());
			} else {
				location->set (copy->start(), copy->end());
			}
		}
	}

	XMLNode &after = session()->locations()->get_state();
	session()->add_command(new MementoCommand<ARDOUR::Locations>(*(session()->locations()), &before, &after));
	commit_reversible_command ();
}
