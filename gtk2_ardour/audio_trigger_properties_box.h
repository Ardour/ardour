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
#include "widgets/ardour_dropdown.h"

#include "audio_clock.h"
#include "trigger_ui.h"

class TriggerPropertiesBox : public Gtk::Table, public ARDOUR::SessionHandlePtr, public TriggerUI
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
	void set_stretch_mode (ARDOUR::Trigger::StretchMode sm);

	void start_clock_changed();
	void length_clock_changed();

	void meter_changed (Temporal::Meter m);

	void beats_changed();

private:

	void MultiplyTempo(float mult);

	Gtk::Table _table;

	AudioClock _length_clock;
	AudioClock _start_clock;

	Gtk::Label               _bpm_label;
	Gtk::Label               _length_label;
	Gtk::Label               _beat_label;

	Gtk::Label               _bars_label;
	Gtk::Label               _bars_display;

	Gtk::Adjustment               _beat_adjustment;
	Gtk::SpinButton               _beat_spinner;

	ArdourWidgets::ArdourButton _stretch_toggle;

	ArdourWidgets::ArdourDropdown _stretch_selector;

	ArdourWidgets::ArdourButton _abpm_label;

	ArdourWidgets::ArdourButton _half_button;
	ArdourWidgets::ArdourButton _dbl_button;

	bool _ignore_changes;
};

#endif
