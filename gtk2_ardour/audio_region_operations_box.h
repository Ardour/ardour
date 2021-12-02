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

#ifndef __audio_region_operations_box_h__
#define __audio_region_operations_box_h__

#include <map>

#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/table.h>

#include "ardour/ardour.h"
#include "ardour/session_handle.h"

#include "widgets/ardour_button.h"

#include "gtkmm2ext/cairo_packer.h"

namespace ARDOUR {
	class Session;
	class Location;
}

class RegionOperationsBox : public Gtk::VBox, public ARDOUR::SessionHandlePtr
{
public:
	RegionOperationsBox () {}
	~RegionOperationsBox () {}
};

class AudioRegionOperationsBox : public RegionOperationsBox
{
public:
	AudioRegionOperationsBox ();
	~AudioRegionOperationsBox ();

	PBD::ScopedConnectionList editor_connections;
	PBD::ScopedConnectionList region_property_connections;

private:
	Gtk::Table table;

	Gtk::Label _header_label;

	Gtk::Label                     mute_regions_label;
	
	ArdourWidgets::ArdourButton    reverse_button;
	ArdourWidgets::ArdourButton    shift_button;
	ArdourWidgets::ArdourButton    normalize_button;

	void selection_changed ();

	void reverse_button_clicked();
	void shift_button_clicked();
	void normalize_button_clicked();
};

#endif /* __audio_region_operations_box_h__ */
