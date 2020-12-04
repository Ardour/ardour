/*
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015-2016 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016-2017 Nick Mainsbridge <mainsbridge@gmail.com>
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

#include <cstdlib>
#include <cmath>
#include <string>

#include <gtkmm/treeview.h>

#include "pbd/error.h"

#include "ardour/session.h"

#include "editor.h"
#include "region_view.h"
#include "selection.h"
#include "time_axis_view.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Editing;

void
Editor::keyboard_selection_finish (bool /*add*/, Editing::EditIgnoreOption ign)
{
	if (!_session) {
		return;
	}

	timepos_t start = selection->time.start_time();
	timepos_t end;
	if ((_edit_point == EditAtPlayhead) && _session->transport_rolling()) {
		end = timepos_t (_session->audible_sample());
	} else {
		end = get_preferred_edit_position(ign);
	}

	//snap the selection start/end
	snap_to (start);

	//if no tracks are selected and we're working from the keyboard, enable all tracks (_something_ has to be selected for any range selection)
	if ( (_edit_point == EditAtPlayhead) && selection->tracks.empty() )
		select_all_tracks();

	selection->set (start, end);

	//if session is playing a range, cancel that
	if (_session->get_play_range()) {
		_session->request_cancel_play_range();
	}

}

void
Editor::keyboard_selection_begin (Editing::EditIgnoreOption ign)
{
	if (_session) {

		timepos_t start;
		timepos_t end (selection->time.end_time());
		if ((_edit_point == EditAtPlayhead) && _session->transport_rolling()) {
			start = timepos_t (_session->audible_sample());
		} else {
			start = get_preferred_edit_position(ign);
		}

		//if there's not already a sensible selection endpoint, go "forever"
		if (start > end ) {
#ifdef MIXBUS
			// 4hours at most.
			// This works around a visual glitch in red-bordered selection rect.
			end  = start + timepos_t (_session->nominal_sample_rate() * 60 * 60 * 4);
#else
			end = timepos_t::max (end.time_domain());
#endif
		}

		//if no tracks are selected and we're working from the keyboard, enable all tracks (_something_ has to be selected for any range selection)
		if ( selection->tracks.empty() )
			select_all_visible_lanes();

		selection->set (start, end);

		//if session is playing a range, cancel that
		if (_session->get_play_range())
			_session->request_cancel_play_range();
	}
}

void
Editor::keyboard_paste ()
{
	paste (1, false);
}
