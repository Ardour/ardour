#include <cstdio> // for snprintf, grrr 

#include <gtkmm2ext/utils.h>

#include "tempo_dialog.h"
#include "utils.h"

#include "i18n.h"

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;

TempoDialog::TempoDialog (TempoMap& map, jack_nframes_t frame, string action)
	: ArdourDialog ("tempo dialog"),
	  bpm_frame (_("Beats per minute")),
	  ok_button (action),
	  cancel_button (_("Cancel")),
	  when_bar_label (_("Bar")),
	  when_beat_label (_("Beat")),
	  when_table (2, 2),
	  when_frame (_("Location"))
{
	BBT_Time when;
	Tempo tempo (map.tempo_at (frame));
	map.bbt_time (frame, when);

	init (when, tempo.beats_per_minute(), true);
}

TempoDialog::TempoDialog (TempoSection& section, string action)
	: ArdourDialog ("tempo dialog"),
	  bpm_frame (_("Beats per minute")),
	  ok_button (action),
	  cancel_button (_("Cancel")),
	  when_bar_label (_("Bar")),
	  when_beat_label (_("Beat")),
	  when_table (2, 2),
	  when_frame (_("Location"))
{
	init (section.start(), section.beats_per_minute(), section.movable());
}

void
TempoDialog::init (const BBT_Time& when, double bpm, bool movable)
{
	snprintf (buf, sizeof (buf), "%.2f", bpm);
	bpm_entry.set_text (buf);
	bpm_entry.select_region (0, -1);
	
	hspacer1.set_border_width (5);
	hspacer1.pack_start (bpm_entry, false, false);
	vspacer1.set_border_width (5);
	vspacer1.pack_start (hspacer1, false, false);

	bpm_frame.add (vspacer1);

	button_box.set_border_width (10);
	button_box.set_spacing (5);
	button_box.set_homogeneous (true);
	button_box.pack_start (ok_button); 
	button_box.pack_start (cancel_button); 

	vpacker.set_border_width (10);
	vpacker.set_spacing (5);

	if (movable) {
		snprintf (buf, sizeof (buf), "%" PRIu32, when.bars);
		when_bar_entry.set_text (buf);
		snprintf (buf, sizeof (buf), "%" PRIu32, when.beats);
		when_beat_entry.set_text (buf);
		
		when_bar_entry.set_name ("MetricEntry");
		when_beat_entry.set_name ("MetricEntry");
		
		when_bar_label.set_name ("MetricLabel");
		when_beat_label.set_name ("MetricLabel");
		
		Gtkmm2ext::set_size_request_to_display_given_text (when_bar_entry, "999g", 5, 7);
		Gtkmm2ext::set_size_request_to_display_given_text (when_beat_entry, "999g", 5, 7);
		
		when_table.set_homogeneous (true);
		when_table.set_row_spacings (2);
		when_table.set_col_spacings (2);
		when_table.set_border_width (5);
		
		when_table.attach (when_bar_label, 0, 1, 0, 1, Gtk::AttachOptions(0), Gtk::FILL|Gtk::EXPAND);
		when_table.attach (when_bar_entry, 0, 1, 1, 2, Gtk::AttachOptions(0), Gtk::FILL|Gtk::EXPAND);
		
		when_table.attach (when_beat_label, 1, 2, 0, 1, Gtk::AttachOptions(0), Gtk::AttachOptions(0));
		when_table.attach (when_beat_entry, 1, 2, 1, 2, Gtk::AttachOptions(0), Gtk::AttachOptions(0));
		
		when_frame.set_name ("MetricDialogFrame");
		when_frame.add (when_table);

		vpacker.pack_start (when_frame, false, false);
	}

	vpacker.pack_start (bpm_frame, false, false);
	vpacker.pack_start (button_box, false, false);

	bpm_frame.set_name ("MetricDialogFrame");
	bpm_entry.set_name ("MetricEntry");
	ok_button.set_name ("MetricButton");
	cancel_button.set_name ("MetricButton");

	add (vpacker);
	set_name ("MetricDialog");

	set_keyboard_input(true);
}

double 
TempoDialog::get_bpm ()
{
	double bpm;
	
	if (sscanf (bpm_entry.get_text().c_str(), "%lf", &bpm) != 1) {
		return 0;
	}

	return bpm;
}	

bool
TempoDialog::get_bbt_time (BBT_Time& requested)
{
	if (sscanf (when_bar_entry.get_text().c_str(), "%" PRIu32, &requested.bars) != 1) {
		return false;
	}
	
	if (sscanf (when_beat_entry.get_text().c_str(), "%" PRIu32, &requested.beats) != 1) {
		return false;
	}

	return true;
}


MeterDialog::MeterDialog (TempoMap& map, jack_nframes_t frame, string action)
	: ArdourDialog ("meter dialog"),
	  note_frame (_("Meter denominator")),
	  bpb_frame (_("Beats per bar")),
	  ok_button (action),
	  cancel_button (_("Cancel")),
	  when_bar_label (_("Bar")),
	  when_beat_label (_("Beat")),
	  when_frame (_("Location"))
{
	BBT_Time when;
	frame = map.round_to_bar(frame,0); 
	Meter meter (map.meter_at(frame));

	map.bbt_time (frame, when);
	init (when, meter.beats_per_bar(), meter.note_divisor(), true);
}

MeterDialog::MeterDialog (MeterSection& section, string action)
	: ArdourDialog ("meter dialog"),
	  note_frame (_("Meter denominator")),
	  bpb_frame (_("Beats per bar")),
	  ok_button (action),
	  cancel_button (_("Cancel")),
	  when_bar_label (_("Bar")),
	  when_beat_label (_("Beat")),
	  when_frame (_("Location"))
{
	init (section.start(), section.beats_per_bar(), section.note_divisor(), section.movable());
}

void
MeterDialog::init (const BBT_Time& when, double bpb, double note_type, bool movable)
{
	snprintf (buf, sizeof (buf), "%.2f", bpb);
	bpb_entry.set_text (buf);
	bpb_entry.select_region (0, -1);
	Gtkmm2ext::set_size_request_to_display_given_text (bpb_entry, "999999g", 5, 5);

	vector<string> strings;
	
	strings.push_back (_("whole (1)"));
	strings.push_back (_("second (2)"));
	strings.push_back (_("third (3)"));
	strings.push_back (_("quarter (4)"));
	strings.push_back (_("eighth (8)"));
	strings.push_back (_("sixteenth (16)"));
	strings.push_back (_("thirty-second (32)"));
	
	set_popdown_strings (note_types, strings);

	if (note_type==1.0f)
		note_types.set_active_text (_("whole (1)"));
	else if (note_type==2.0f)
		note_types.set_active_text (_("second (2)"));
	else if (note_type==3.0f)
		note_types.set_active_text (_("third (3)"));
	else if (note_type==4.0f)
		note_types.set_active_text (_("quarter (4)"));
	else if (note_type==8.0f)
		note_types.set_active_text (_("eighth (8)"));
	else if (note_type==16.0f)
		note_types.set_active_text (_("sixteenth (16)"));
	else if (note_type==32.0f)
		note_types.set_active_text (_("thirty-second (32)"));
	else
		note_types.set_active_text (_("quarter (4)"));
		
	/* strings.back() just happens to be the longest one to display */
	// GTK2FIX
	// Gtkmm2ext::set_size_request_to_display_given_text (*(note_types.get_entry()), strings.back(), 7, 7);

	hspacer1.set_border_width (5);
	hspacer1.pack_start (note_types, false, false);
	vspacer1.set_border_width (5);
	vspacer1.pack_start (hspacer1, false, false);

	hspacer2.set_border_width (5);
	hspacer2.pack_start (bpb_entry, false, false);
	vspacer2.set_border_width (5);
	vspacer2.pack_start (hspacer2, false, false);

	note_frame.add (vspacer1);
	bpb_frame.add (vspacer2);

	button_box.set_border_width (10);
	button_box.set_spacing (5);
	button_box.set_homogeneous (true);
	button_box.pack_start (ok_button); 
	button_box.pack_start (cancel_button); 

	vpacker.set_border_width (10);
	vpacker.set_spacing (5);
	
	if (movable) {
		snprintf (buf, sizeof (buf), "%" PRIu32, when.bars);
		when_bar_entry.set_text (buf);
		snprintf (buf, sizeof (buf), "%" PRIu32, when.beats);
		when_beat_entry.set_text (buf);
		
		when_bar_entry.set_name ("MetricEntry");
		when_beat_entry.set_name ("MetricEntry");
		
		when_bar_label.set_name ("MetricLabel");
		when_beat_label.set_name ("MetricLabel");
		
		Gtkmm2ext::set_size_request_to_display_given_text (when_bar_entry, "999g", 5, 7);
		Gtkmm2ext::set_size_request_to_display_given_text (when_beat_entry, "999g", 5, 7);
		
		when_table.set_homogeneous (true);
		when_table.set_row_spacings (2);
		when_table.set_col_spacings (2);
		when_table.set_border_width (5);
		
		when_table.attach (when_bar_label, 0, 1, 0, 1, Gtk::AttachOptions(0), Gtk::FILL|Gtk::EXPAND);
		when_table.attach (when_bar_entry, 0, 1, 1, 2, Gtk::AttachOptions(0), Gtk::FILL|Gtk::EXPAND);
		
		when_table.attach (when_beat_label, 1, 2, 0, 1, Gtk::AttachOptions(0), Gtk::AttachOptions(0));
		when_table.attach (when_beat_entry, 1, 2, 1, 2, Gtk::AttachOptions(0), Gtk::AttachOptions(0));
		
		when_frame.set_name ("MetricDialogFrame");
		when_frame.add (when_table);
		
		vpacker.pack_start (when_frame, false, false);
	}

	vpacker.pack_start (bpb_frame, false, false);
	vpacker.pack_start (note_frame, false, false);
	vpacker.pack_start (button_box, false, false);
	
	bpb_frame.set_name ("MetricDialogFrame");
	note_frame.set_name ("MetricDialogFrame");
	bpb_entry.set_name ("MetricEntry");
	ok_button.set_name ("MetricButton");
	cancel_button.set_name ("MetricButton");

	add (vpacker);
	set_name ("MetricDialog");

	set_keyboard_input(true);
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
	double note_type = 0;
	vector<const gchar *>::iterator i;
	string text = note_types.get_active_text();
	
	for (i = strings.begin(); i != strings.end(); ++i) {
		if (text == *i) {
			if (sscanf (text.c_str(), "%*[^0-9]%lf", &note_type) != 1) {
				error << string_compose(_("garbaged note type entry (%1)"), text) << endmsg;
				return 0;
			} else {
				break;
			}
		}
	} 
	
	if (i == strings.end()) {
		if (sscanf (text.c_str(), "%lf", &note_type) != 1) {
			error << string_compose(_("incomprehensible note type entry (%1)"), text) << endmsg;
			return 0;
		}
	}

	return note_type;
}

bool
MeterDialog::get_bbt_time (BBT_Time& requested)
{
	requested.ticks = 0;

	if (sscanf (when_bar_entry.get_text().c_str(), "%" PRIu32, &requested.bars) != 1) {
		return false;
	}
	
	if (sscanf (when_beat_entry.get_text().c_str(), "%" PRIu32, &requested.beats) != 1) {
		return false;
	}

	return true;
}
