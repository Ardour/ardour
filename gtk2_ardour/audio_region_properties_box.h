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

#ifndef __audio_region_properties_box_h__
#define __audio_region_properties_box_h__

#include <map>

#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/table.h>

#include "ardour/ardour.h"
#include "ardour/session_handle.h"

#include "widgets/ardour_button.h"

#include "gtkmm2ext/cairo_packer.h"

#include "audio_clock.h"

namespace ARDOUR
{
	class Session;
	class Location;
}

class RegionPropertiesBox : public Gtk::VBox, public ARDOUR::SessionHandlePtr
{
public:
	RegionPropertiesBox ();
	~RegionPropertiesBox ();

	virtual void set_region (boost::shared_ptr<ARDOUR::Region>);

	void set_session (ARDOUR::Session* s);

protected:
	boost::shared_ptr<ARDOUR::Region> _region;

	Gtk::Label _header_label;

private:
	void region_changed (const PBD::PropertyChange& what_changed);

	Gtk::Table table;

	AudioClock length_clock;
	AudioClock start_clock;

	ArdourWidgets::ArdourButton bpm_button;
	ArdourWidgets::ArdourButton metrum_button;

	ArdourWidgets::ArdourButton bbt_toggle;

	PBD::ScopedConnection state_connection;
};

class AudioRegionPropertiesBox : public RegionPropertiesBox
{
public:
	AudioRegionPropertiesBox ();
	~AudioRegionPropertiesBox ();

	virtual void set_region (boost::shared_ptr<ARDOUR::Region>);

private:
	ArdourWidgets::ArdourButton fade_in_enable_button;
	ArdourWidgets::ArdourButton fade_out_enable_button;

	ArdourWidgets::ArdourButton gain_control;
	ArdourWidgets::ArdourButton stretch_selector;
};

#endif /* __audio_region_properties_box_h__ */
