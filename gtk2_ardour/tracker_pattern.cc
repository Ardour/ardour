/*
    Copyright (C) 2016 Nil Geisweiller

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

#include <cmath>
#include <map>

#include <gtkmm/cellrenderercombo.h>

#include "pbd/file_utils.h"

#include "evoral/midi_util.h"
#include "evoral/Note.hpp"

#include "ardour/beats_frames_converter.h"
#include "ardour/midi_model.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/session.h"
#include "ardour/tempo.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/rgb_macros.h"

#include "ui_config.h"
#include "midi_tracker_editor.h"
#include "note_player.h"
#include "tooltips.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Glib;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Editing;
using Timecode::BBT_Time;

////////////////////
// TrackerPattern //
////////////////////

TrackerPattern::TrackerPattern(ARDOUR::Session* session,
                               boost::shared_ptr<ARDOUR::Region> region)
	: _session(session), _region(region),
	  _conv(_session->tempo_map(), _region->position())
{
}

void TrackerPattern::set_rows_per_beat(uint16_t rpb)
{
	rows_per_beat = rpb;
	beats_per_row = Evoral::Beats(1.0 / rows_per_beat);
	_ticks_per_row = BBT_Time::ticks_per_beat/rows_per_beat;
}

Evoral::Beats TrackerPattern::find_first_row_beats()
{	
	return _conv.from (_region->first_frame()).snap_to (beats_per_row);
}

Evoral::Beats TrackerPattern::find_last_row_beats()
{
	return _conv.from (_region->last_frame()).snap_to (beats_per_row);
}

uint32_t TrackerPattern::find_nrows()
{
	return (last_beats - first_beats).to_double() * rows_per_beat;
}

framepos_t TrackerPattern::frame_at_row(uint32_t irow)
{
	return _conv.to (beats_at_row(irow));
}

Evoral::Beats TrackerPattern::beats_at_row(uint32_t irow)
{
	return first_beats + (irow*1.0) / rows_per_beat;
}

uint32_t TrackerPattern::row_at_beats(Evoral::Beats beats)
{
	Evoral::Beats half_row(0.5/rows_per_beat);
	return (beats - first_beats + half_row).to_double() * rows_per_beat;
}

uint32_t TrackerPattern::row_at_beats_min_delay(Evoral::Beats beats)
{
	Evoral::Beats tpr_minus_1 = Evoral::Beats::ticks(_ticks_per_row - 1);
	return (beats - first_beats + tpr_minus_1).to_double() * rows_per_beat;
}

uint32_t TrackerPattern::row_at_beats_max_delay(Evoral::Beats beats)
{
	return (beats - first_beats).to_double() * rows_per_beat;
}
