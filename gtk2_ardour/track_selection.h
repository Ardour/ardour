/*
    Copyright (C) 2000-2009 Paul Davis

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

#ifndef __ardour_gtk_track_selection_h__
#define __ardour_gtk_track_selection_h__

#include "track_view_list.h"
#include "route_ui.h"
#include "audio_time_axis.h"
#include "midi_time_axis.h"

class PublicEditor;

class TrackSelection : public TrackViewList
{
public:
	TrackSelection (PublicEditor const * e) : _editor (e) {}
	TrackSelection (PublicEditor const *, TrackViewList const &);

	virtual ~TrackSelection ();

	TrackViewList add (TrackViewList const &);

	template <typename Function>
	void foreach_time_axis (Function f) {
		for (iterator i = begin(); i != end(); ++i) {
			f (*i);
		}
	}

	template <typename Function>
	void foreach_route_ui (Function f) {
		for (iterator i = begin(); i != end(); ) {
			iterator tmp = i;
			++tmp;

			RouteUI* t = dynamic_cast<RouteUI*> (*i);
			if (t) {
				f (t);
			}
			i = tmp;
		}
	}

	template <typename Function>
	void foreach_route_time_axis (Function f) {
		for (iterator i = begin(); i != end(); ) {
			iterator tmp = i;
			++tmp;
			RouteTimeAxisView* t = dynamic_cast<RouteTimeAxisView*> (*i);
			if (t) {
				f (t);
			}
			i = tmp;
		}
	}

	template <typename Function>
	void foreach_audio_time_axis (Function f) {
		for (iterator i = begin(); i != end(); ) {
			iterator tmp = i;
			++tmp;
			AudioTimeAxisView* t = dynamic_cast<AudioTimeAxisView*> (*i);
			if (t) {
				f (t);
			}
			i = tmp;
		}
	}

	template <typename Function>
	void foreach_midi_time_axis (Function f) {
		for (iterator i = begin(); i != end(); ) {
			iterator tmp = i;
			++tmp;
			MidiTimeAxisView* t = dynamic_cast<MidiTimeAxisView*> (*i);
			if (t) {
				f (t);
			}
			i = tmp;
		}
	}

private:
	PublicEditor const * _editor;
};

#endif /* __ardour_gtk_track_selection_h__ */
