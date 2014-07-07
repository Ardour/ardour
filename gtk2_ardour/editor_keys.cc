/*
    Copyright (C) 2000 Paul Davis

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

#include <cstdlib>
#include <cmath>
#include <string>

#include <gtkmm/treeview.h>

#include "pbd/error.h"

#include "ardour/session.h"

#include "ardour_ui.h"
#include "editor.h"
#include "region_view.h"
#include "selection.h"
#include "time_axis_view.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Editing;

void
Editor::keyboard_selection_finish (bool /*add*/)
{
	if (_session) {

		framepos_t start = selection->time.start();
		framepos_t end;
		
		if ((_edit_point == EditAtPlayhead) && _session->transport_rolling()) {
			end = _session->audible_frame();
		} else {
			end = get_preferred_edit_position();
		}

		//snap the selection start/end
		snap_to(start);
		
		//if no tracks are selected and we're working from the keyboard, enable all tracks (_something_ has to be selected for any range selection)
		if ( (_edit_point == EditAtPlayhead) && selection->tracks.empty() )
			select_all_tracks();

		selection->set (start, end);

		//if session is playing a range, cancel that
		if (_session->get_play_range())
			_session->request_cancel_play_range();

	}
}

void
Editor::keyboard_selection_begin ()
{
	if (_session) {

		framepos_t start;
		framepos_t end = selection->time.end_frame();  //0 if no current selection

		if ((_edit_point == EditAtPlayhead) && _session->transport_rolling()) {
			start = _session->audible_frame();
		} else {
			start = get_preferred_edit_position();
		}
		
		//snap the selection start/end
		snap_to(start);
		
		//if there's not already a sensible selection endpoint, go "forever"
		if ( start > end ) {
			end = max_framepos;
		}
		
		//if no tracks are selected and we're working from the keyboard, enable all tracks (_something_ has to be selected for any range selection)
		if ( selection->tracks.empty() )
			select_all_tracks();
					
		selection->set (start, end);

		//if session is playing a range, cancel that
		if (_session->get_play_range())
			_session->request_cancel_play_range();
	}
}

void
Editor::keyboard_paste ()
{
	paste (1);
}
