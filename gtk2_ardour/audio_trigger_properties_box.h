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

#ifndef __audio_trigger_properties_box_h__
#define __audio_trigger_properties_box_h__

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

class TriggerPropertiesBox : public Gtk::VBox, public ARDOUR::SessionHandlePtr
{
public:
	TriggerPropertiesBox () {}
	~TriggerPropertiesBox () {}

	virtual void set_trigger (ARDOUR::Trigger* t) = 0;
};

class AudioTriggerPropertiesBox : public TriggerPropertiesBox
{
public:
	AudioTriggerPropertiesBox ();
	~AudioTriggerPropertiesBox ();

	void set_trigger (ARDOUR::Trigger* t);

	void set_session (ARDOUR::Session* s);

protected:
	void trigger_changed (const PBD::PropertyChange& what_changed);

	void toggle_stretch ();

	Gtk::Label _header_label;

private:
	ARDOUR::AudioTrigger* _trigger;

	Gtk::Table table;

	Gtk::Label abpm_label;

	AudioClock length_clock;
	AudioClock start_clock;

	ArdourWidgets::ArdourButton bpm_button;
	ArdourWidgets::ArdourButton metrum_button;

	ArdourWidgets::ArdourButton stretch_toggle;

	ArdourWidgets::ArdourButton fade_in_enable_button;
	ArdourWidgets::ArdourButton fade_out_enable_button;

	ArdourWidgets::ArdourButton gain_control;
	ArdourWidgets::ArdourButton stretch_selector;

	PBD::ScopedConnection state_connection;
};

#endif /* __audio_trigger_properties_box_h__ */
