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

#ifndef _gtk_ardour_audio_trigger_properties_box_h_
#define _gtk_ardour_audio_trigger_properties_box_h_

#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/table.h>

#include "ardour/ardour.h"
#include "ardour/session_handle.h"
#include "ardour/triggerbox.h"

#include "widgets/ardour_button.h"

#include "audio_clock.h"
#include "trigger_ui.h"

class TriggerPropertiesBox : public Gtk::VBox, public ARDOUR::SessionHandlePtr, public TriggerUI
{
public:
	TriggerPropertiesBox () {}
	~TriggerPropertiesBox () {}

protected:
	Gtk::Label _header_label;

	PBD::ScopedConnection _state_connection;
};

class AudioTriggerPropertiesBox : public TriggerPropertiesBox
{
public:
	AudioTriggerPropertiesBox ();
	~AudioTriggerPropertiesBox ();

	void set_session (ARDOUR::Session*);

protected:
	virtual void on_trigger_changed (const PBD::PropertyChange& what_changed);

	void toggle_stretch ();

	void start_clock_changed();
	void length_clock_changed();

	void follow_clock_changed();
	void gain_changed();

private:

	Gtk::Table _table;
	Gtk::Label _abpm_label;
	AudioClock _length_clock;
	AudioClock _start_clock;

	Gtk::Adjustment  _follow_length_adjustment;
	Gtk::SpinButton  _follow_length_spinner;

	Gtk::Adjustment  _gain_adjustment;
	Gtk::SpinButton  _gain_spinner;

	ArdourWidgets::ArdourButton _bpm_button;
	ArdourWidgets::ArdourButton _metrum_button;

	ArdourWidgets::ArdourButton _stretch_toggle;

	ArdourWidgets::ArdourButton _stretch_selector;

};

#endif
