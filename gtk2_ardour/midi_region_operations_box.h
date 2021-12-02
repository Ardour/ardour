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

#ifndef __midi_region_operations_box_h__
#define __midi_region_operations_box_h__

#include <map>

#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/table.h>

#include "ardour/ardour.h"
#include "ardour/session_handle.h"

#include "widgets/ardour_button.h"

#include "gtkmm2ext/cairo_packer.h"

#include "audio_region_operations_box.h"

namespace ARDOUR {
	class Session;
	class Location;
}

class MidiRegionOperationsBox : public RegionOperationsBox
{
public:
	MidiRegionOperationsBox ();
	~MidiRegionOperationsBox ();

	PBD::ScopedConnectionList editor_connections;
	PBD::ScopedConnectionList region_property_connections;

private:
	Gtk::Table table;

	Gtk::Label _header_label;

	ArdourWidgets::ArdourButton    quantize_button;
	ArdourWidgets::ArdourButton    legatize_button;
	ArdourWidgets::ArdourButton    transform_button;

	void quantize_button_clicked();
	void legatize_button_clicked();
	void transform_button_clicked();
};

#endif /* __midi_region_operations_box_h__ */
