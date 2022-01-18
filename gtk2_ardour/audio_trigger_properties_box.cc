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
	, _bar_adjustment( 1, 0.001, 1000.0, 1.0, 4.0, 0)
	, _bar_spinner (_bar_adjustment)
	, _stretch_toggle (ArdourButton::led_default_elements)
	, _abpm_label  (ArdourButton::Text)
	, _ignore_changes (false)
{
	Gtk::Label* label;
	int         row = 0;

	_abpm_label.set_sizing_text("200.00");

	/* ------- Stretching and Tempo stuff ----------------------------- */
	Gtk::Table* bpm_table = manage (new Gtk::Table ());
	bpm_table->set_homogeneous (false);
	bpm_table->set_spacings (4);
	bpm_table->set_border_width (8);

	_stretch_toggle.set_text (_("Stretch"));
	bpm_table->attach (_stretch_toggle, 0, 1, row, row + 1, Gtk::FILL, Gtk::SHRINK);
	bpm_table->attach (_stretch_selector, 1, 4, row, row + 1, Gtk::FILL, Gtk::SHRINK); row++;

	label = manage (new Gtk::Label (_("BPM:")));
	label->set_alignment (1.0, 0.5);
	bpm_table->attach (*label,      0, 1, row, row + 1, Gtk::FILL, Gtk::SHRINK);
	bpm_table->attach (_abpm_label, 1, 2, row, row + 1, Gtk::FILL, Gtk::SHRINK);

	ArdourButton *half = manage (new ArdourButton (_("/2")));
	half->signal_clicked.connect(sigc::bind (sigc::mem_fun(*this, &AudioTriggerPropertiesBox::MultiplyTempo), 0.5));
	bpm_table->attach (*half, 2, 3, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	ArdourButton *dbl = manage (new ArdourButton (_("x2")));
	dbl->signal_clicked.connect(sigc::bind (sigc::mem_fun(*this, &AudioTriggerPropertiesBox::MultiplyTempo), 2.0));
	bpm_table->attach (*dbl, 3, 4, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);

	row++;

	label = manage (new Gtk::Label (_("Meter:")));
	label->set_alignment (1.0, 0.5);
	bpm_table->attach (*label,         0, 1, row, row + 1, Gtk::FILL, Gtk::SHRINK);
	bpm_table->attach (_meter_selector, 1, 4, row, row + 1, Gtk::FILL, Gtk::SHRINK);

	row++;

	label = manage (new Gtk::Label (_("Clip Length:")));
	label->set_alignment (1.0, 0.5);
	Gtk::Label *bar_label = manage (new Gtk::Label (_("(bars)")));
	bar_label->set_alignment (0.0, 0.5);
	bpm_table->attach (*label,       0, 1, row, row + 1, Gtk::FILL, Gtk::SHRINK);
	bpm_table->attach (_bar_spinner, 1, 2, row, row + 1, Gtk::FILL, Gtk::SHRINK);
	bpm_table->attach (*bar_label,   2, 4, row, row + 1, Gtk::FILL, Gtk::SHRINK);

	ArdourWidgets::Frame* eTempoBox = manage (new ArdourWidgets::Frame);
	eTempoBox->set_label("Stretch Options");
	eTempoBox->set_name("EditorDark");
	eTempoBox->set_edge_color (0x000000ff); // black
	eTempoBox->add (*bpm_table);

	/* -------------- Clip start&length (redundant with the trimmer gui handles?)  ----------*/
	row = 0;

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

	_table.set_homogeneous (false);
	_table.set_spacings (4);
	_table.set_border_width (2);

	attach (*eTempoBox,    0,1, 0,1, Gtk::FILL, Gtk::SHRINK);
#if 0
	attach (_table,        0,1, 1,2, Gtk::FILL, Gtk::SHRINK);
#endif

	_start_clock.ValueChanged.connect (sigc::mem_fun (*this, &AudioTriggerPropertiesBox::start_clock_changed));
	_length_clock.ValueChanged.connect (sigc::mem_fun (*this, &AudioTriggerPropertiesBox::length_clock_changed));

	using namespace Menu_Helpers;

	_meter_selector.set_text ("??");
	_meter_selector.set_name ("generic button");
	_meter_selector.set_sizing_text ("4/4");
	_meter_selector.AddMenuElem (MenuElem ("3/4", sigc::bind (sigc::mem_fun(*this, &AudioTriggerPropertiesBox::meter_changed), Temporal::Meter(3,4))));
	_meter_selector.AddMenuElem (MenuElem ("4/4", sigc::bind (sigc::mem_fun(*this, &AudioTriggerPropertiesBox::meter_changed), Temporal::Meter(4,4))));
	_meter_selector.AddMenuElem (MenuElem ("5/4", sigc::bind (sigc::mem_fun(*this, &AudioTriggerPropertiesBox::meter_changed), Temporal::Meter(5,4))));

	_stretch_selector.set_text ("??");
	_stretch_selector.set_name ("generic button");
	_stretch_selector.set_sizing_text (TriggerUI::longest_stretch_mode);
	_stretch_selector.AddMenuElem (MenuElem (TriggerUI::stretch_mode_to_string(Trigger::Crisp),  sigc::bind (sigc::mem_fun(*this, &AudioTriggerPropertiesBox::set_stretch_mode), Trigger::Crisp)));
	_stretch_selector.AddMenuElem (MenuElem (TriggerUI::stretch_mode_to_string(Trigger::Mixed),  sigc::bind (sigc::mem_fun(*this, &AudioTriggerPropertiesBox::set_stretch_mode), Trigger::Mixed)));
	_stretch_selector.AddMenuElem (MenuElem (TriggerUI::stretch_mode_to_string(Trigger::Smooth), sigc::bind (sigc::mem_fun(*this, &AudioTriggerPropertiesBox::set_stretch_mode), Trigger::Smooth)));

	_stretch_toggle.signal_clicked.connect (sigc::mem_fun (*this, &AudioTriggerPropertiesBox::toggle_stretch));

	_bar_spinner.set_can_focus(false);
	_bar_spinner.signal_changed ().connect (sigc::mem_fun (*this, &AudioTriggerPropertiesBox::bars_changed));
}

AudioTriggerPropertiesBox::~AudioTriggerPropertiesBox ()
{
}

void
AudioTriggerPropertiesBox::MultiplyTempo(float mult)
{
	TriggerPtr trigger (tref.trigger());
	boost::shared_ptr<AudioTrigger> at = boost::dynamic_pointer_cast<AudioTrigger> (trigger);
	if (at) {
		at->set_segment_tempo (at->segment_tempo () * mult);
	}
}

void
AudioTriggerPropertiesBox::toggle_stretch ()
{
	TriggerPtr trigger (tref.trigger());
	boost::shared_ptr<AudioTrigger> at = boost::dynamic_pointer_cast<AudioTrigger> (trigger);
	if (at) {
		at->set_stretchable (!at->stretchable ());
	}
}

void
AudioTriggerPropertiesBox::set_stretch_mode (Trigger::StretchMode sm)
{
	TriggerPtr trigger (tref.trigger());
	boost::shared_ptr<AudioTrigger> at = boost::dynamic_pointer_cast<AudioTrigger> (trigger);
	if (at) {
		at->set_stretch_mode (sm);
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
AudioTriggerPropertiesBox::on_trigger_changed (const PBD::PropertyChange& pc)
{
	TriggerPtr trigger (tref.trigger());
	boost::shared_ptr<AudioTrigger> at = boost::dynamic_pointer_cast<AudioTrigger> (trigger);
	if (!at) {
		return;
	}

	_ignore_changes = true;

	if (pc.contains (Properties::start) || pc.contains (Properties::length)) {  /* NOT REACHED from current code */
		AudioClock::Mode mode = at->box ().data_type () == ARDOUR::DataType::AUDIO ? AudioClock::Samples : AudioClock::BBT;

		_start_clock.set_mode (mode);
		_length_clock.set_mode (mode);

		_start_clock.set (at->start_offset ());
		_length_clock.set (at->current_length ()); // set_duration() ?
	}

	if (pc.contains (Properties::tempo_meter) || pc.contains (Properties::follow_length)) {

		char buf[32];
		sprintf(buf, "%3.2f", at->segment_tempo ());
		_abpm_label.set_text (buf);

		ArdourWidgets::set_tooltip (_abpm_label, string_compose ("Clip Tempo, used for stretching.  Estimated tempo (from file) was: %1", trigger->estimated_tempo ()));

		_meter_selector.set_text (string_compose ("%1/%2", at->meter().divisions_per_bar(), at->meter().note_value()));

		_bar_adjustment.set_value(at->segment_barcnt());
	}

	if (pc.contains (Properties::stretch_mode) || pc.contains (Properties::stretchable)) {
		_stretch_toggle.set_active (at->stretchable () ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);

		_stretch_selector.set_sensitive(at->stretchable ());
		_stretch_selector.set_text(stretch_mode_to_string(at->stretch_mode ()));
	}

	_ignore_changes = false;
}

void
AudioTriggerPropertiesBox::meter_changed (Temporal::Meter m)
{
	TriggerPtr trigger (tref.trigger());
	boost::shared_ptr<AudioTrigger> at = boost::dynamic_pointer_cast<AudioTrigger> (trigger);
	if (at) {
		at->set_segment_meter(m);
	}
}

void
AudioTriggerPropertiesBox::bars_changed ()
{
	if (_ignore_changes) {
		return;
	}

	TriggerPtr trigger (tref.trigger());
	boost::shared_ptr<AudioTrigger> at = boost::dynamic_pointer_cast<AudioTrigger> (trigger);
	if (at) {
		at->set_segment_barcnt (_bar_adjustment.get_value());
	}
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
