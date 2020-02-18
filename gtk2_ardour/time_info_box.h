/*
 * Copyright (C) 2011-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __time_info_box_h__
#define __time_info_box_h__

#include <map>

#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/table.h>

#include "ardour/ardour.h"
#include "ardour/session_handle.h"

#include "gtkmm2ext/cairo_packer.h"

namespace ARDOUR {
	class Session;
	class Location;
}

class AudioClock;

class TimeInfoBox : public CairoHPacker, public ARDOUR::SessionHandlePtr
{
public:
	TimeInfoBox (std::string state_node_name, bool with_punch);
	~TimeInfoBox ();

	void set_session (ARDOUR::Session*);

private:
	Gtk::Table table;

	AudioClock* selection_start;
	AudioClock* selection_end;
	AudioClock* selection_length;

	AudioClock* punch_start;
	AudioClock* punch_end;

	Gtk::Label selection_title;
	Gtk::Label punch_title;
	bool syncing_selection;
	bool syncing_punch;
	bool with_punch_clock;

	void punch_changed (ARDOUR::Location*);
	void punch_location_changed (ARDOUR::Location*);
	void watch_punch (ARDOUR::Location*);
	PBD::ScopedConnectionList punch_connections;
	PBD::ScopedConnectionList editor_connections;
	PBD::ScopedConnectionList region_property_connections;

	void selection_changed ();
	void region_selection_changed ();

	void sync_selection_mode (AudioClock*);
	void sync_punch_mode (AudioClock*);

	bool clock_button_release_event (GdkEventButton* ev, AudioClock* src);
	void track_mouse_mode ();
};

#endif /* __time_info_box_h__ */
