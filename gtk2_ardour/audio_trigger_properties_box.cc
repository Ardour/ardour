/*
 * Copyright (C) 2011-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include "pbd/compose.h"
#include <algorithm>

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"

#include "ardour/location.h"
#include "ardour/profile.h"
#include "ardour/session.h"

#include "audio_clock.h"
#include "automation_line.h"
#include "control_point.h"
#include "editor.h"
#include "region_view.h"

#include "audio_trigger_properties_box.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using namespace ArdourWidgets;
using std::max;
using std::min;

AudioTriggerPropertiesBox::AudioTriggerPropertiesBox ()
	: _length_clock (X_("regionlength"), true, "", true, false, true)
	, _start_clock (X_("regionstart"), true, "", false, false)
	, _stretch_toggle (ArdourButton::led_default_elements)
{
	_header_label.set_text (_("AUDIO Trigger Properties:"));

	Gtk::Label* label;
	int         row = 0;

	_header_label.set_alignment (0.0, 0.5);
	pack_start (_header_label, false, false, 6);

	Gtk::Table* bpm_table = manage (new Gtk::Table ());
	bpm_table->set_homogeneous (false);
	bpm_table->set_spacings (4);
	bpm_table->set_border_width (2);
	label = manage (new Gtk::Label (_("BPM:")));
	label->set_alignment (1.0, 0.5);
	bpm_table->attach (*label,      0, 1, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	bpm_table->attach (_bpm_button, 1, 2, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	bpm_table->attach (_abpm_label, 2, 3, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	row++;

	pack_start (*bpm_table, false, false);

	Gtk::Table* metrum_table = manage (new Gtk::Table ());
	metrum_table->set_homogeneous (false);
	metrum_table->set_spacings (4);
	metrum_table->set_border_width (2);
	label = manage (new Gtk::Label (_("Time Sig:")));
	label->set_alignment (1.0, 0.5);
	bpm_table->attach (*label,         0, 1, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	bpm_table->attach (_metrum_button, 1, 2, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	row++;

	pack_start (*metrum_table, false, false);

	row = 0;

	_stretch_toggle.set_text (_("Stretch"));
	_table.attach (_stretch_toggle, 0, 1, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	row++;

	label = manage (new Gtk::Label (_("Start:")));
	label->set_alignment (1.0, 0.5);
	_table.attach (*label,       0, 1, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	_table.attach (_start_clock, 1, 2, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	row++;

	label = manage (new Gtk::Label (_("Length:")));
	label->set_alignment (1.0, 0.5);
	_table.attach (*label,        0, 1, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	_table.attach (_length_clock, 1, 2, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	row++;

	_table.set_homogeneous (false);
	_table.set_spacings (4);
	_table.set_border_width (2);
	pack_start (_table, false, false);

	Gtk::Table* audio_t = manage (new Gtk::Table ());
	audio_t->set_homogeneous (true);
	audio_t->set_spacings (4);

	row = 0;

	label = manage (new Gtk::Label (_("Stretch Mode:")));
	label->set_alignment (1.0, 0.5);
	audio_t->attach (*label, 0, 1, row, row + 1, Gtk::FILL, Gtk::SHRINK);

	_stretch_selector.set_text ("Mixed");
	_stretch_selector.set_name ("generic button");
	audio_t->attach (_stretch_selector, 1, 3, row, row + 1, Gtk::FILL, Gtk::SHRINK);

	row++;

	label = manage (new Gtk::Label (_("Fades:")));
	label->set_alignment (1.0, 0.5);
	_fade_in_enable_button.set_text (_("In"));
	_fade_in_enable_button.set_name ("generic button");
	_fade_out_enable_button.set_text (_("Out"));
	_fade_out_enable_button.set_name ("generic button");
	audio_t->attach (*label,                  0, 1, row, row + 1, Gtk::FILL, Gtk::SHRINK);
	audio_t->attach (_fade_in_enable_button,  1, 2, row, row + 1, Gtk::FILL, Gtk::SHRINK);
	audio_t->attach (_fade_out_enable_button, 2, 3, row, row + 1, Gtk::FILL, Gtk::SHRINK);

	row++;

	label = manage (new Gtk::Label (_("Gain:")));
	label->set_alignment (1.0, 0.5);
	audio_t->attach (*label, 0, 1, row, row + 1, Gtk::FILL, Gtk::SHRINK);

	_gain_control.set_text (_("+6dB"));
	_gain_control.set_name ("generic button");
	audio_t->attach (_gain_control, 1, 3, row, row + 1, Gtk::FILL, Gtk::SHRINK);

	row++;

	pack_start (*audio_t);

	_stretch_toggle.signal_clicked.connect (sigc::mem_fun (*this, &AudioTriggerPropertiesBox::toggle_stretch));
}

AudioTriggerPropertiesBox::~AudioTriggerPropertiesBox ()
{
}

void
AudioTriggerPropertiesBox::toggle_stretch ()
{
	_trigger->set_stretchable (!_trigger->stretchable ());
}

void
AudioTriggerPropertiesBox::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	_length_clock.set_session (s);
	_start_clock.set_session (s);
}

void
AudioTriggerPropertiesBox::set_trigger (ARDOUR::TriggerPtr t)
{
	boost::shared_ptr<ARDOUR::AudioTrigger> audio_trigger = boost::dynamic_pointer_cast<ARDOUR::AudioTrigger> (t);

	if (!audio_trigger) {
		return;
	}

	_trigger = audio_trigger;
	_trigger->PropertyChanged.connect (_state_connection, invalidator (*this), boost::bind (&AudioTriggerPropertiesBox::trigger_changed, this, _1), gui_context ());

	PBD::PropertyChange changed;
	changed.add (ARDOUR::Properties::name);
	changed.add (ARDOUR::Properties::running);
	trigger_changed (changed);
}

void
AudioTriggerPropertiesBox::trigger_changed (const PBD::PropertyChange& what_changed)
{
	AudioClock::Mode mode = _trigger->box ().data_type () == ARDOUR::DataType::AUDIO ? AudioClock::Samples : AudioClock::BBT;

	_start_clock.set_mode (mode);
	_length_clock.set_mode (mode);

	_start_clock.set (_trigger->start_offset ());
	_length_clock.set (_trigger->current_length ()); // set_duration() ?

	_bpm_button.set_text (string_compose ("%1", _trigger->apparent_tempo ()));
	_abpm_label.set_text (string_compose ("%1", _trigger->apparent_tempo ()));
	_metrum_button.set_text ("4/4");

	_stretch_toggle.set_active (_trigger->stretchable () ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
}
