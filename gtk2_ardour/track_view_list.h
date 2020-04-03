/*
 * Copyright (C) 2010-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2013-2016 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_gtk_track_view_list_h__
#define __ardour_gtk_track_view_list_h__

#include "ardour/types.h" /* XXX is this here because of some Cocoa nonsense ? */

#include <list>
#include <set>

#include "route_ui.h"
#include "audio_time_axis.h"
#include "midi_time_axis.h"

class TimeAxisView;

class TrackViewList : public std::list<TimeAxisView*>
{
public:
	TrackViewList () {}
	TrackViewList (std::list<TimeAxisView*> const &);

	virtual ~TrackViewList ();

	virtual TrackViewList add (TrackViewList const &);
	bool contains (TimeAxisView const *) const;

	TrackViewList filter_to_unique_playlists ();
	ARDOUR::RouteList routelist () const;

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
	void foreach_stripable_time_axis (Function f) {
		for (iterator i = begin(); i != end(); ) {
			iterator tmp = i;
			++tmp;
			StripableTimeAxisView* t = dynamic_cast<StripableTimeAxisView*> (*i);
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
};

#endif

