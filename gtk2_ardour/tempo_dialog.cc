/*
    Copyright (C) 2000-2007 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <cstdio> // for snprintf, grrr

#include <gtkmm/stock.h>

#include "gtkmm2ext/utils.h"

#include "tempo_dialog.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;
using namespace PBD;

TempoDialog::TempoDialog (TempoMap& map, framepos_t frame, const string&)
	: ArdourDialog (_("New Tempo"))
	, _map (&map)
	, _section (0)
	, bpm_adjustment (60.0, 1.0, 999.9, 0.1, 1.0)
	, bpm_spinner (bpm_adjustment)
	, when_bar_label (_("bar:"), ALIGN_RIGHT, ALIGN_CENTER)
	, when_beat_label (_("beat:"), ALIGN_RIGHT, ALIGN_CENTER)
	, pulse_selector_label (_("Pulse:"), ALIGN_RIGHT, ALIGN_CENTER)
	, tap_tempo_button (_("Tap tempo"))
{
	Tempo tempo (map.tempo_at_frame (frame));
	Timecode::BBT_Time when (map.bbt_at_frame (frame));

	init (when, tempo.note_types_per_minute(), tempo.note_type(), TempoSection::Constant, true, MusicTime);
}

TempoDialog::TempoDialog (TempoMap& map, TempoSection& section, const string&)
	: ArdourDialog (_("Edit Tempo"))
	, _map (&map)
	, _section (&section)
	, bpm_adjustment (60.0, 1.0, 999.9, 0.1, 1.0)
	, bpm_spinner (bpm_adjustment)
	, when_bar_label (_("bar:"), ALIGN_RIGHT, ALIGN_CENTER)
	, when_beat_label (_("beat:"), ALIGN_RIGHT, ALIGN_CENTER)
	, pulse_selector_label (_("Pulse:"), ALIGN_RIGHT, ALIGN_CENTER)
	, tap_tempo_button (_("Tap tempo"))
{
	Timecode::BBT_Time when (map.bbt_at_frame (section.frame()));
	init (when, section.note_types_per_minute(), section.note_type(), section.type()
	      , section.initial() || section.locked_to_meter(), section.position_lock_style());
}

void
TempoDialog::init (const Timecode::BBT_Time& when, double bpm, double note_type, TempoSection::Type type, bool initial, PositionLockStyle style)
{
	vector<string> strings;
	NoteTypes::iterator x;

	bpm_spinner.set_numeric (true);
	bpm_spinner.set_digits (3);
	bpm_spinner.set_wrap (true);
	bpm_spinner.set_value (bpm);
	bpm_spinner.set_alignment (1.0);

	note_types.insert (make_pair (_("whole"), 1.0));
	strings.push_back (_("whole"));
	note_types.insert (make_pair (_("second"), 2.0));
	strings.push_back (_("second"));
	note_types.insert (make_pair (_("third"), 3.0));
	strings.push_back (_("third"));
	note_types.insert (make_pair (_("quarter"), 4.0));
	strings.push_back (_("quarter"));
	note_types.insert (make_pair (_("eighth"), 8.0));
	strings.push_back (_("eighth"));
	note_types.insert (make_pair (_("sixteenth"), 16.0));
	strings.push_back (_("sixteenth"));
	note_types.insert (make_pair (_("thirty-second"), 32.0));
	strings.push_back (_("thirty-second"));
	note_types.insert (make_pair (_("sixty-fourth"), 64.0));
	strings.push_back (_("sixty-fourth"));
	note_types.insert (make_pair (_("one-hundred-twenty-eighth"), 128.0));
	strings.push_back (_("one-hundred-twenty-eighth"));

	set_popdown_strings (pulse_selector, strings);

	for (x = note_types.begin(); x != note_types.end(); ++x) {
		if (x->second == note_type) {
			pulse_selector.set_active_text (x->first);
			break;
		}
	}

	if (x == note_types.end()) {
		pulse_selector.set_active_text (strings[3]); // "quarter"
	}

	strings.clear();

	tempo_types.insert (make_pair (_("ramped"), TempoSection::Ramp));
	strings.push_back (_("ramped"));
	tempo_types.insert (make_pair (_("constant"), TempoSection::Constant));
	strings.push_back (_("constant"));
	set_popdown_strings (tempo_type, strings);
	TempoTypes::iterator tt;
	for (tt = tempo_types.begin(); tt != tempo_types.end(); ++tt) {
		if (tt->second == type) {
			tempo_type.set_active_text (tt->first);
			break;
		}
	}
	if (tt == tempo_types.end()) {
		tempo_type.set_active_text (strings[1]); // "constant"
	}

	strings.clear();

	lock_styles.insert (make_pair (_("music"), MusicTime));
	strings.push_back (_("music"));
	lock_styles.insert (make_pair (_("audio"), AudioTime));
	strings.push_back (_("audio"));
	set_popdown_strings (lock_style, strings);
	LockStyles::iterator ls;
	for (ls = lock_styles.begin(); ls != lock_styles.end(); ++ls) {
		if (ls->second == style) {
			lock_style.set_active_text (ls->first);
			break;
		}
	}
	if (ls == lock_styles.end()) {
		lock_style.set_active_text (strings[0]); // "music"
	}

	Table* table;

	if (UIConfiguration::instance().get_allow_non_quarter_pulse()) {
		table = manage (new Table (5, 7));
	} else {
		table = manage (new Table (5, 6));
	}

	table->set_spacings (6);
	table->set_homogeneous (false);

	int row;

	if (UIConfiguration::instance().get_allow_non_quarter_pulse()) {
		table->attach (pulse_selector_label, 0, 1, 0, 1);
		table->attach (pulse_selector, 1, 5, 0, 1);

		row = 1;
	} else {
		row = 0;
	}

	Label* bpm_label = manage (new Label(_("Beats per Minute:"), ALIGN_LEFT, ALIGN_CENTER));
	table->attach (*bpm_label, 0, 1, row, row + 1);
	table->attach (bpm_spinner, 1, 5, row, row + 1);
	++row;

	char buf[64];

	snprintf (buf, sizeof (buf), "%" PRIu32, when.bars);
	when_bar_entry.set_text (buf);
	snprintf (buf, sizeof (buf), "%" PRIu32, when.beats);
	when_beat_entry.set_text (buf);

	if (!initial) {
		when_bar_entry.set_width_chars(4);
		when_beat_entry.set_width_chars (4);
		when_bar_entry.set_alignment (1.0);
		when_beat_entry.set_alignment (1.0);

		when_bar_label.set_name ("MetricLabel");
		when_beat_label.set_name ("MetricLabel");

		table->attach (when_bar_label, 1, 2, row, row+1, Gtk::AttachOptions(0), Gtk::AttachOptions(0));
		table->attach (when_bar_entry, 2, 3, row, row+1, Gtk::AttachOptions(0), Gtk::AttachOptions(0));

		table->attach (when_beat_label, 3, 4, row, row+1, Gtk::AttachOptions(0), Gtk::AttachOptions(0));
		table->attach (when_beat_entry, 4, 5, row, row+1, Gtk::AttachOptions(0), Gtk::AttachOptions(0));

		Label* when_label = manage (new Label(_("Tempo begins at"), ALIGN_LEFT, ALIGN_CENTER));
		table->attach (*when_label, 0, 1, row, row+1);

		++row;
		++row;

		Label* lock_style_label = manage (new Label(_("Lock Style:"), ALIGN_RIGHT, ALIGN_CENTER));
		table->attach (*lock_style_label, 0, 1, row, row + 1);
		table->attach (lock_style, 1, 5, row, row + 1);

		--row;
	}


	Label* tempo_type_label = manage (new Label(_("Tempo Type:"), ALIGN_RIGHT, ALIGN_CENTER));
	table->attach (*tempo_type_label, 0, 1, row, row + 1);
	table->attach (tempo_type, 1, 5, row, row + 1);

	++row;

	get_vbox()->set_border_width (12);
	get_vbox()->pack_end (*table);

	table->show_all ();

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::APPLY, RESPONSE_ACCEPT);
	set_response_sensitive (RESPONSE_ACCEPT, true);
	set_default_response (RESPONSE_ACCEPT);

	bpm_spinner.show ();
	tap_tempo_button.show ();
	get_vbox()->set_spacing (6);
	get_vbox()->pack_end (tap_tempo_button);
	bpm_spinner.grab_focus ();

	set_name ("MetricDialog");

	bpm_spinner.signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &TempoDialog::response), RESPONSE_ACCEPT));
	bpm_spinner.signal_button_press_event().connect (sigc::mem_fun (*this, &TempoDialog::bpm_button_press), false);
	bpm_spinner.signal_button_release_event().connect (sigc::mem_fun (*this, &TempoDialog::bpm_button_release), false);
	bpm_spinner.signal_changed().connect (sigc::mem_fun (*this, &TempoDialog::bpm_changed));
	when_bar_entry.signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &TempoDialog::response), RESPONSE_ACCEPT));
	when_bar_entry.signal_key_release_event().connect (sigc::mem_fun (*this, &TempoDialog::entry_key_release), false);
	when_beat_entry.signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &TempoDialog::response), RESPONSE_ACCEPT));
	when_beat_entry.signal_key_release_event().connect (sigc::mem_fun (*this, &TempoDialog::entry_key_release), false);
	pulse_selector.signal_changed().connect (sigc::mem_fun (*this, &TempoDialog::pulse_change));
	tempo_type.signal_changed().connect (sigc::mem_fun (*this, &TempoDialog::tempo_type_change));
	lock_style.signal_changed().connect (sigc::mem_fun (*this, &TempoDialog::lock_style_change));
	tap_tempo_button.signal_button_press_event().connect (sigc::mem_fun (*this, &TempoDialog::tap_tempo_button_press), false);
	tap_tempo_button.signal_focus_out_event().connect (sigc::mem_fun (*this, &TempoDialog::tap_tempo_focus_out));

	tapped = false;
}

bool
TempoDialog::is_user_input_valid() const
{
	return (when_beat_entry.get_text() != "")
		&& (when_bar_entry.get_text() != "")
		&& (when_bar_entry.get_text() != "0");
}

void
TempoDialog::bpm_changed ()
{
	set_response_sensitive (RESPONSE_ACCEPT, is_user_input_valid());
}

bool
TempoDialog::bpm_button_press (GdkEventButton*)
{
	return false;
}

bool
TempoDialog::bpm_button_release (GdkEventButton*)
{
	/* the value has been modified, accept should work now */

	set_response_sensitive (RESPONSE_ACCEPT, is_user_input_valid());
	return false;
}

bool
TempoDialog::entry_key_release (GdkEventKey*)
{
	Timecode::BBT_Time bbt;
	get_bbt_time (bbt);

	if (_section && is_user_input_valid()) {
		set_response_sensitive (RESPONSE_ACCEPT, _map->can_solve_bbt (_section, bbt));
	} else {
		set_response_sensitive (RESPONSE_ACCEPT, is_user_input_valid());
	}

	return false;
}

double
TempoDialog::get_bpm ()
{
	return bpm_spinner.get_value ();
}

bool
TempoDialog::get_bbt_time (Timecode::BBT_Time& requested)
{
	if (sscanf (when_bar_entry.get_text().c_str(), "%" PRIu32, &requested.bars) != 1) {
		return false;
	}

	if (sscanf (when_beat_entry.get_text().c_str(), "%" PRIu32, &requested.beats) != 1) {
		return false;
	}

	requested.ticks = 0;

	return true;
}

double
TempoDialog::get_note_type ()
{
	NoteTypes::iterator x = note_types.find (pulse_selector.get_active_text());

	if (x == note_types.end()) {
		error << string_compose(_("incomprehensible pulse note type (%1)"), pulse_selector.get_active_text()) << endmsg;
		return 0;
	}

	return x->second;
}

TempoSection::Type
TempoDialog::get_tempo_type ()
{
	TempoTypes::iterator x = tempo_types.find (tempo_type.get_active_text());

	if (x == tempo_types.end()) {
		error << string_compose(_("incomprehensible tempo type (%1)"), tempo_type.get_active_text()) << endmsg;
		return TempoSection::Constant;
	}

	return x->second;
}

PositionLockStyle
TempoDialog::get_lock_style ()
{
	LockStyles::iterator x = lock_styles.find (lock_style.get_active_text());

	if (x == lock_styles.end()) {
		error << string_compose(_("incomprehensible lock style (%1)"), lock_style.get_active_text()) << endmsg;
		return MusicTime;
	}

	return x->second;
}

void
TempoDialog::pulse_change ()
{
	set_response_sensitive (RESPONSE_ACCEPT, is_user_input_valid());
}

void
TempoDialog::tempo_type_change ()
{
	set_response_sensitive (RESPONSE_ACCEPT, is_user_input_valid());
}

void
TempoDialog::lock_style_change ()
{
	set_response_sensitive (RESPONSE_ACCEPT, is_user_input_valid());
}

bool
TempoDialog::tap_tempo_button_press (GdkEventButton *ev)
{
	double t;

	// Linear least-squares regression
	if (tapped) {
		t = 1e-6 * (g_get_monotonic_time () - first_t); // Subtract first_t to avoid precision problems

		double n = tap_count;
		sum_y += t;
		sum_x += n;
		sum_xy += n * t;
		sum_xx += n * n;
		double T = (sum_xy/n - sum_x/n * sum_y/n) / (sum_xx/n - sum_x/n * sum_x/n);

		if (t - last_t < T / 1.2 || t - last_t > T * 1.2) {
			tapped = false;
		} else {
			bpm_spinner.set_value (60.0 / T);
		}
	}
	if (!tapped) {
		first_t = g_get_monotonic_time ();
		t = 0.0;
		sum_y = 0.0;
		sum_x = 1.0;
		sum_xy = 0.0;
		sum_xx = 1.0;
		tap_count = 1.0;

		tapped = true;
	}
	tap_count++;
	last_t = t;

	return true;
}

bool
TempoDialog::tap_tempo_focus_out (GdkEventFocus* )
{
	tapped = false;
	return false;
}

MeterDialog::MeterDialog (TempoMap& map, framepos_t frame, const string&)
	: ArdourDialog (_("New Meter"))
{
	frame = map.round_to_bar(frame, RoundNearest).frame;
	Timecode::BBT_Time when (map.bbt_at_frame (frame));
	Meter meter (map.meter_at_frame (frame));

	init (when, meter.divisions_per_bar(), meter.note_divisor(), false, MusicTime);
}

MeterDialog::MeterDialog (TempoMap& map, MeterSection& section, const string&)
	: ArdourDialog (_("Edit Meter"))
{
	Timecode::BBT_Time when (map.bbt_at_frame (section.frame()));

	init (when, section.divisions_per_bar(), section.note_divisor(), section.initial(), section.position_lock_style());
}

void
MeterDialog::init (const Timecode::BBT_Time& when, double bpb, double divisor, bool initial, PositionLockStyle style)
{
	char buf[64];
	vector<string> strings;
	NoteTypes::iterator x;

	snprintf (buf, sizeof (buf), "%.2f", bpb);
	bpb_entry.set_text (buf);
	bpb_entry.select_region (0, -1);
	bpb_entry.set_alignment (1.0);

	note_types.insert (make_pair (_("whole"), 1.0));
	strings.push_back (_("whole"));
	note_types.insert (make_pair (_("second"), 2.0));
	strings.push_back (_("second"));
	note_types.insert (make_pair (_("third"), 3.0));
	strings.push_back (_("third"));
	note_types.insert (make_pair (_("quarter"), 4.0));
	strings.push_back (_("quarter"));
	note_types.insert (make_pair (_("eighth"), 8.0));
	strings.push_back (_("eighth"));
	note_types.insert (make_pair (_("sixteenth"), 16.0));
	strings.push_back (_("sixteenth"));
	note_types.insert (make_pair (_("thirty-second"), 32.0));
	strings.push_back (_("thirty-second"));
	note_types.insert (make_pair (_("sixty-fourth"), 64.0));
	strings.push_back (_("sixty-fourth"));
	note_types.insert (make_pair (_("one-hundred-twenty-eighth"), 128.0));
	strings.push_back (_("one-hundred-twenty-eighth"));

	set_popdown_strings (note_type, strings);

	for (x = note_types.begin(); x != note_types.end(); ++x) {
		if (x->second == divisor) {
			note_type.set_active_text (x->first);
			break;
		}
	}

	if (x == note_types.end()) {
		note_type.set_active_text (strings[3]); // "quarter"
	}

	strings.clear();

	lock_styles.insert (make_pair (_("music"), MusicTime));
	strings.push_back (_("music"));
	lock_styles.insert (make_pair (_("audio"), AudioTime));
	strings.push_back (_("audio"));
	set_popdown_strings (lock_style, strings);
	LockStyles::iterator ls;
	for (ls = lock_styles.begin(); ls != lock_styles.end(); ++ls) {
		if (ls->second == style) {
			lock_style.set_active_text (ls->first);
			break;
		}
	}
	if (ls == lock_styles.end()) {
		lock_style.set_active_text (strings[0]); // "music"
	}

	Label* note_label = manage (new Label (_("Note value:"), ALIGN_RIGHT, ALIGN_CENTER));
	Label* lock_label = manage (new Label (_("Lock style:"), ALIGN_RIGHT, ALIGN_CENTER));
	Label* bpb_label = manage (new Label (_("Beats per bar:"), ALIGN_RIGHT, ALIGN_CENTER));
	Table* table = manage (new Table (3, 3));
	table->set_spacings (6);

	table->attach (*bpb_label, 0, 1, 0, 1, FILL|EXPAND, FILL|EXPAND);
	table->attach (bpb_entry, 1, 2, 0, 1, FILL|EXPAND, FILL|EXPAND);
	table->attach (*note_label, 0, 1, 1, 2, FILL|EXPAND, FILL|EXPAND);
	table->attach (note_type, 1, 2, 1, 2, FILL|EXPAND, FILL|EXPAND);

	snprintf (buf, sizeof (buf), "%" PRIu32, when.bars);
	when_bar_entry.set_text (buf);
	when_bar_entry.set_alignment (1.0);

	if (!initial) {
		Label* when_label = manage (new Label(_("Meter begins at bar:"), ALIGN_LEFT, ALIGN_CENTER));

		table->attach (*when_label, 0, 1, 2, 3, FILL | EXPAND, FILL | EXPAND);
		table->attach (when_bar_entry, 1, 2, 2, 3, FILL | EXPAND, FILL | EXPAND);

		table->attach (*lock_label, 0, 1, 3, 4, FILL|EXPAND, FILL|EXPAND);
		table->attach (lock_style, 1, 2, 3, 4, FILL|EXPAND, SHRINK);
	}

	get_vbox()->set_border_width (12);
	get_vbox()->pack_start (*table, false, false);

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::APPLY, RESPONSE_ACCEPT);
	set_response_sensitive (RESPONSE_ACCEPT, true);
	set_default_response (RESPONSE_ACCEPT);

	get_vbox()->show_all ();

	set_name ("MetricDialog");
	bpb_entry.signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &MeterDialog::response), RESPONSE_ACCEPT));
	bpb_entry.signal_key_press_event().connect (sigc::mem_fun (*this, &MeterDialog::entry_key_press), false);
	bpb_entry.signal_key_release_event().connect (sigc::mem_fun (*this, &MeterDialog::entry_key_release));
	when_bar_entry.signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &MeterDialog::response), RESPONSE_ACCEPT));
	when_bar_entry.signal_key_press_event().connect (sigc::mem_fun (*this, &MeterDialog::entry_key_press), false);
	when_bar_entry.signal_key_release_event().connect (sigc::mem_fun (*this, &MeterDialog::entry_key_release));
	note_type.signal_changed().connect (sigc::mem_fun (*this, &MeterDialog::note_type_change));
	lock_style.signal_changed().connect (sigc::mem_fun (*this, &MeterDialog::lock_style_change));

}

bool
MeterDialog::is_user_input_valid() const
{
	return (when_bar_entry.get_text() != "")
		&& (when_bar_entry.get_text() != "0")
		&& (bpb_entry.get_text() != "");
}

bool
MeterDialog::entry_key_press (GdkEventKey* ev)
{

	switch (ev->keyval) {

	case GDK_0:
	case GDK_1:
	case GDK_2:
	case GDK_3:
	case GDK_4:
	case GDK_5:
	case GDK_6:
	case GDK_7:
	case GDK_8:
	case GDK_9:
	case GDK_KP_0:
	case GDK_KP_1:
	case GDK_KP_2:
	case GDK_KP_3:
	case GDK_KP_4:
	case GDK_KP_5:
	case GDK_KP_6:
	case GDK_KP_7:
	case GDK_KP_8:
	case GDK_KP_9:
	case GDK_period:
	case GDK_comma:
	case  GDK_KP_Delete:
	case  GDK_KP_Enter:
	case  GDK_Delete:
	case  GDK_BackSpace:
	case  GDK_Escape:
	case  GDK_Return:
	case  GDK_Home:
	case  GDK_End:
	case  GDK_Left:
	case  GDK_Right:
	case  GDK_Num_Lock:
	case  GDK_Tab:
		return FALSE;
	default:
		break;
	}

	return TRUE;
}

bool
MeterDialog::entry_key_release (GdkEventKey*)
{
	set_response_sensitive (RESPONSE_ACCEPT, is_user_input_valid());
	return false;
}

void
MeterDialog::note_type_change ()
{
        set_response_sensitive (RESPONSE_ACCEPT, is_user_input_valid());
}

void
MeterDialog::lock_style_change ()
{
        set_response_sensitive (RESPONSE_ACCEPT, is_user_input_valid());
}

double
MeterDialog::get_bpb ()
{
	double bpb = 0;

	if (sscanf (bpb_entry.get_text().c_str(), "%lf", &bpb) != 1) {
		return 0;
	}

	return bpb;
}

double
MeterDialog::get_note_type ()
{
	NoteTypes::iterator x = note_types.find (note_type.get_active_text());

	if (x == note_types.end()) {
		error << string_compose(_("incomprehensible meter note type (%1)"), note_type.get_active_text()) << endmsg;
		return 0;
	}

	return x->second;
}

PositionLockStyle
MeterDialog::get_lock_style ()
{
	LockStyles::iterator x = lock_styles.find (lock_style.get_active_text());

	if (x == lock_styles.end()) {
		error << string_compose(_("incomprehensible meter lock style (%1)"), lock_style.get_active_text()) << endmsg;
		return MusicTime;
	}

	return x->second;
}

bool
MeterDialog::get_bbt_time (Timecode::BBT_Time& requested)
{
	if (sscanf (when_bar_entry.get_text().c_str(), "%" PRIu32, &requested.bars) != 1) {
		return false;
	}

	requested.beats = 1;
	requested.ticks = 0;

	return true;
}
