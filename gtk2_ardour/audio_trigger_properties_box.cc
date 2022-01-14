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

#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/menu_elems.h"

#include "widgets/tooltips.h"

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
	, _follow_length_adjustment (0, 0, 128, 1, 4)
	, _follow_length_spinner (_follow_length_adjustment)
	, _gain_adjustment( 0.0, -20.0, +20.0, 1.0, 3.0, 0)
	, _gain_spinner (_gain_adjustment)
	, _stretch_toggle (ArdourButton::led_default_elements)
	, _abpm_label  (ArdourButton::Text)
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
	bpm_table->attach (_abpm_label, 1, 2, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);

	ArdourButton *half = manage (new ArdourButton (_("/2")));
	half->signal_clicked.connect(sigc::bind (sigc::mem_fun(*this, &AudioTriggerPropertiesBox::MultiplyTempo), 0.5));
	bpm_table->attach (*half, 2, 3, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	ArdourButton *dbl = manage (new ArdourButton (_("x2")));
	dbl->signal_clicked.connect(sigc::bind (sigc::mem_fun(*this, &AudioTriggerPropertiesBox::MultiplyTempo), 2.0));
	bpm_table->attach (*dbl, 3, 4, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);

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
	_table.attach (_stretch_selector, 1, 2, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	row++;

	label = manage (new Gtk::Label (_("Start:")));
	label->set_alignment (1.0, 0.5);
	_table.attach (*label,       0, 1, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	_table.attach (_start_clock, 1, 2, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	row++;

	label = manage (new Gtk::Label (_("Clip Length:")));
	label->set_alignment (1.0, 0.5);
	_table.attach (*label,        0, 1, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	_table.attach (_length_clock, 1, 2, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	row++;

	label = manage (new Gtk::Label (_("Follow Length:")));
	label->set_alignment (1.0, 0.5);
	Gtk::Label *beat_label = manage (new Gtk::Label (_("(beats)")));
	beat_label->set_alignment (0.0, 0.5);
	_table.attach (*label,                 0, 1, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	_table.attach (_follow_length_spinner, 1, 2, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	_table.attach (*beat_label,            2, 3, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	row++;

	_table.set_homogeneous (false);
	_table.set_spacings (4);
	_table.set_border_width (2);
	pack_start (_table, false, false);

	Gtk::Table* audio_t = manage (new Gtk::Table ());
	audio_t->set_homogeneous (true);
	audio_t->set_spacings (4);

	row = 0;

	label = manage (new Gtk::Label (_("Gain:")));
	label->set_alignment (1.0, 0.5);
	Gtk::Label *db_label = manage (new Gtk::Label (_("(dB)")));
	db_label->set_alignment (0.0, 0.5);
	audio_t->attach (*label,        0, 1, row, row + 1, Gtk::FILL, Gtk::SHRINK);
	audio_t->attach (_gain_spinner, 1, 2, row, row + 1, Gtk::FILL, Gtk::SHRINK);
	audio_t->attach (*db_label,     2, 3, row, row + 1, Gtk::FILL, Gtk::SHRINK);

	row++;

	pack_start (*audio_t);

	using namespace Menu_Helpers;

	_stretch_selector.set_text ("??");
	_stretch_selector.set_name ("generic button");
	_stretch_selector.set_sizing_text (TriggerUI::longest_stretch_mode);
	_stretch_selector.AddMenuElem (MenuElem (TriggerUI::stretch_mode_to_string(Trigger::Crisp),  sigc::bind (sigc::mem_fun(*this, &AudioTriggerPropertiesBox::set_stretch_mode), Trigger::Crisp)));
	_stretch_selector.AddMenuElem (MenuElem (TriggerUI::stretch_mode_to_string(Trigger::Mixed),  sigc::bind (sigc::mem_fun(*this, &AudioTriggerPropertiesBox::set_stretch_mode), Trigger::Mixed)));
	_stretch_selector.AddMenuElem (MenuElem (TriggerUI::stretch_mode_to_string(Trigger::Smooth), sigc::bind (sigc::mem_fun(*this, &AudioTriggerPropertiesBox::set_stretch_mode), Trigger::Smooth)));

	_stretch_toggle.signal_clicked.connect (sigc::mem_fun (*this, &AudioTriggerPropertiesBox::toggle_stretch));

	_follow_length_spinner.set_can_focus(false);
	_follow_length_spinner.signal_changed ().connect (sigc::mem_fun (*this, &AudioTriggerPropertiesBox::follow_clock_changed));

	_gain_spinner.set_can_focus(false);
	_gain_spinner.configure(_gain_adjustment, 0.0, 1);
	_gain_spinner.signal_changed ().connect (sigc::mem_fun (*this, &AudioTriggerPropertiesBox::gain_changed));
}

AudioTriggerPropertiesBox::~AudioTriggerPropertiesBox ()
{
}

void
AudioTriggerPropertiesBox::MultiplyTempo(float mult)
{
	TriggerPtr trigger (tref.trigger());
	if (trigger) {
		trigger->set_segment_tempo (trigger->segment_tempo () * mult);
	}
}

void
AudioTriggerPropertiesBox::toggle_stretch ()
{
	TriggerPtr trigger (tref.trigger());
	if (trigger) {
		trigger->set_stretchable (!trigger->stretchable ());
	}
}

void
AudioTriggerPropertiesBox::set_stretch_mode (Trigger::StretchMode sm)
{
	TriggerPtr trigger (tref.trigger());
	if (trigger) {
		trigger->set_stretch_mode (sm);
	}
}

void
AudioTriggerPropertiesBox::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	_length_clock.set_session (s);
	_start_clock.set_session (s);
}

void
AudioTriggerPropertiesBox::on_trigger_changed (const PBD::PropertyChange& what_changed)
{
	TriggerPtr trigger (tref.trigger());
	if (!trigger) {
		return;
	}

	AudioClock::Mode mode = trigger->box ().data_type () == ARDOUR::DataType::AUDIO ? AudioClock::Samples : AudioClock::BBT;

	_start_clock.set_mode (mode);
	_length_clock.set_mode (mode);

	_start_clock.set (trigger->start_offset ());
	_length_clock.set (trigger->current_length ()); // set_duration() ?

	int metrum_numerator = trigger->meter().divisions_per_bar();
	int bar_beats = metrum_numerator * trigger->follow_length().bars;
	int beats = trigger->follow_length().beats;
	_follow_length_adjustment.set_value (bar_beats+beats);  //note: 0 is a special case meaning "use clip length"

	_start_clock.ValueChanged.connect (sigc::mem_fun (*this, &AudioTriggerPropertiesBox::start_clock_changed));
	_length_clock.ValueChanged.connect (sigc::mem_fun (*this, &AudioTriggerPropertiesBox::length_clock_changed));

	_abpm_label.set_text (string_compose ("%1", trigger->segment_tempo ()));
	ArdourWidgets::set_tooltip (_abpm_label, string_compose ("Clip Tempo, used for stretching.  Estimated tempo (from file) was: %1", trigger->estimated_tempo ()));

	_metrum_button.set_text (string_compose ("%1/%2", metrum_numerator, trigger->meter().note_value()));

	_stretch_toggle.set_active (trigger->stretchable () ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);

	_stretch_selector.set_sensitive(trigger->stretchable ());
	_stretch_selector.set_text(stretch_mode_to_string(trigger->stretch_mode ()));

	float gain = accurate_coefficient_to_dB(trigger->gain());
	if (gain != _gain_adjustment.get_value()) {
		_gain_adjustment.set_value (gain);
	}
}

void
AudioTriggerPropertiesBox::gain_changed ()
{
	float coeff = dB_to_coefficient(_gain_adjustment.get_value());

	trigger()->set_gain(coeff);
}


void
AudioTriggerPropertiesBox::start_clock_changed ()
{
	trigger()->set_start(_start_clock.current_time());
}

void
AudioTriggerPropertiesBox::length_clock_changed ()
{
	trigger()->set_length(_length_clock.current_duration());  //?
}

void
AudioTriggerPropertiesBox::follow_clock_changed ()
{
	int beatz = (int) _follow_length_adjustment.get_value();

	int metrum_numerator = trigger()->meter().divisions_per_bar();

	int bars = beatz/metrum_numerator;
	int beats = beatz%metrum_numerator;

	trigger()->set_follow_length(Temporal::BBT_Offset(bars,beats,0));
}
