/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2021 Ben Loftis <ben@harrisonconsoles.com>
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

#ifndef __selection_properties_box_h__
#define __selection_properties_box_h__

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

class TimeInfoBox;

class MultiRegionPropertiesBox;

class SlotPropertiesBox;

class AudioRegionPropertiesBox;
class MidiRegionPropertiesBox;

class AudioRegionOperationsBox;
class MidiRegionOperationsBox;

class SelectionPropertiesBox : public Gtk::VBox, public ARDOUR::SessionHandlePtr
{
public:
	SelectionPropertiesBox ();
	~SelectionPropertiesBox ();

	void set_session (ARDOUR::Session*);

	PBD::ScopedConnectionList editor_connections;

private:
	Gtk::Table table;

	Gtk::Label _header_label;

	TimeInfoBox* _time_info_box;

	MultiRegionPropertiesBox* _mregions_prop_box;

	AudioRegionPropertiesBox* _audio_prop_box;
	MidiRegionPropertiesBox* _midi_prop_box;

	AudioRegionOperationsBox* _audio_ops_box;
	MidiRegionOperationsBox* _midi_ops_box;

	SlotPropertiesBox* _slot_prop_box;

	void selection_changed ();

	void track_mouse_mode ();
};

#endif /* __selection_properties_box_h__ */
