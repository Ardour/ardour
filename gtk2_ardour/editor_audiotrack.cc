/*
    Copyright (C) 2000-2007 Paul Davis 

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

#include <ardour/location.h>
#include <ardour/audio_diskstream.h>

#include "editor.h"
#include "editing.h"
#include "audio_time_axis.h"
#include "audio_region_view.h"
#include "selection.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

void
Editor::set_loop_from_selection (bool play)
{
	if (session == 0 || selection->time.empty()) {
		return;
	}

	nframes_t start = selection->time[clicked_selection].start;
	nframes_t end = selection->time[clicked_selection].end;
	
	set_loop_range (start, end,  _("set loop range from selection"));

	if (play) {
		session->request_play_loop (true);
		session->request_locate (start, true);
	}
}

void
Editor::set_punch_from_selection ()
{
	if (session == 0 || selection->time.empty()) {
		return;
	}

	nframes_t start = selection->time[clicked_selection].start;
	nframes_t end = selection->time[clicked_selection].end;
	
	set_punch_range (start, end,  _("set punch range from selection"));
}

void
Editor::set_show_waveforms (bool yn)
{
	AudioTimeAxisView* atv;

	if (_show_waveforms != yn) {
		_show_waveforms = yn;
		for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
			if ((atv = dynamic_cast<AudioTimeAxisView*>(*i)) != 0) {
				atv->set_show_waveforms (yn);
			}
		}
	}
}

void
Editor::set_show_waveforms_recording (bool yn)
{
	AudioTimeAxisView* atv;

	if (_show_waveforms_recording != yn) {
		_show_waveforms_recording = yn;
		for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
			if ((atv = dynamic_cast<AudioTimeAxisView*>(*i)) != 0) {
				atv->set_show_waveforms_recording (yn);
			}
		}
	}
}
