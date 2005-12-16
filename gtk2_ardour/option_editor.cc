/*
    Copyright (C) 2001 Paul Davis 

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

    $Id$
*/

#include <ardour/session.h>
#include <ardour/audioengine.h>
#include <ardour/configuration.h>
#include <ardour/auditioner.h>
#include <ardour/crossfade.h>
#include <midi++/manager.h>
#include <gtkmm2ext/stop_signal.h>
#include <gtkmm2ext/utils.h>

#include "public_editor.h"
#include "mixer_ui.h"
#include "ardour_ui.h"
#include "io_selector.h"
#include "gain_meter.h"
#include "sfdb_ui.h"
#include "utils.h"
#include "editing.h"
#include "option_editor.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace Gtk;
using namespace Editing;
using namespace Gtkmm2ext;
using namespace std;

static const gchar *psync_strings[] = {
	N_("Internal"),
	N_("Slave to MTC"),
	N_("Sync with JACK"),
	N_("never used but stops crashes"),
	0
};

static const gchar *lmode_strings[] = {
	N_("Later regions are higher"),
	N_("Most recently added/moved/trimmed regions are higher"),
	N_("Most recently added regions are higher"),
	0
};

static const gchar *xfl_strings[] = {
	N_("Span entire region overlap"),
	N_("Short fades at the start of the overlap"),
	0
};

static vector<string> positional_sync_strings;
static vector<string> layer_mode_strings;
static vector<string> xfade_model_strings;

OptionEditor::OptionEditor (ARDOUR_UI& uip, PublicEditor& ed, Mixer_UI& mixui)
	: Dialog ("option editor"),
	  ui (uip),
	  editor (ed),
	  mixer (mixui),

	  /* Paths */
	  path_table (11, 2),
	  sfdb_paths(ListStore::create(sfdb_path_columns)),
	  sfdb_path_view(sfdb_paths),

	  /* Fades */

	  auto_xfade_button (_("Automatically create crossfades")),
	  xfade_active_button (_("New full-overlap crossfades are unmuted")),
	  layer_mode_label (_("Region layering mode")),
	  xfade_model_label (_("Crossfade model")),
	  short_xfade_adjustment (0, 1.0, 500.0, 5.0, 100.0),
	  short_xfade_slider (short_xfade_adjustment),

	  /* solo */
	  solo_latched_button (_("Latched solo")),
	  solo_via_bus_button (_("Solo via bus")),

	  /* display */

	  show_waveforms_button (_("Show waveforms")),
	  show_waveforms_recording_button (_("Show waveforms while recording")),
	  mixer_strip_width_button (_("Narrow mixer strips")),
	  show_measures_button (_("Show measure lines")),
	  follow_playhead_button (_("Follow playhead")),
	  
	  /* Sync */

	  send_mtc_button (_("Send MTC")),
	  send_mmc_button (_("Send MMC")),
	  jack_time_master_button (_("JACK time master")),
	  smpte_offset_clock (X_("SMPTEOffsetClock"), true, true),
	  smpte_offset_negative_button (_("SMPTE offset is negative")),

	  /* MIDI */

	  midi_feedback_button (_("Send MIDI parameter feedback")),
	  midi_control_button (_("MIDI parameter control")),
	  mmc_control_button (_("MMC control")),
	  
	  /* Click */

	  click_table (2, 3),
	  click_browse_button (_("Browse")),
	  click_emphasis_browse_button (_("Browse")),

	  /* kbd/mouse */

	  keyboard_mouse_table (3, 4),
	  delete_button_adjustment (3, 1, 5),
	  delete_button_spin (delete_button_adjustment),
	  edit_button_adjustment (3, 1, 5),
	  edit_button_spin (edit_button_adjustment),

	  /* Misc */

	  auto_connect_inputs_button (_("Auto-connect new track inputs to hardware")),
	  auto_connect_output_physical_button (_("Auto-connect new track outputs to hardware")),
	  auto_connect_output_master_button (_("Auto-connect new track outputs to master bus")),
	  auto_connect_output_manual_button (_("Manually connect new track outputs")),
	  hw_monitor_button(_("Use Hardware Monitoring")),
	  sw_monitor_button(_("Use Software Monitoring")),
	  plugins_stop_button (_("Stop plugins with transport")),
	  plugins_on_rec_button (_("Run plugins while recording")),
	  verify_remove_last_capture_button (_("Verify remove last capture")),
	  stop_rec_on_xrun_button (_("Stop recording on xrun")),
	  stop_at_end_button (_("Stop transport at end of session")),
	  debug_keyboard_button (_("Debug keyboard events")),
	  speed_quieten_button (_("-12dB gain reduction for ffwd/rew"))
	  
{
	using namespace Notebook_Helpers;

	click_io_selector = 0;
	auditioner_io_selector = 0;

	set_default_size (300, 300);
	set_title (_("ardour: options editor"));
	set_wmclass (_("ardour_option_editor"), "Ardour");

	set_name ("OptionsWindow");
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);
	
	layer_mode_label.set_name ("OptionsLabel");
	xfade_model_label.set_name ("OptionsLabel");
	
	VBox *vbox = get_vbox();
	set_border_width (3);

	vbox->set_spacing (4);
	vbox->pack_start(notebook);

	signal_delete_event().connect (mem_fun(*this, &OptionEditor::wm_close));

	notebook.set_show_tabs (true);
	notebook.set_show_border (true);
	notebook.set_name ("OptionsNotebook");

	setup_sync_options();
	setup_path_options();
	setup_fade_options ();
	setup_solo_options ();
	setup_display_options ();
	setup_misc_options ();
	setup_keyboard_options ();
	setup_auditioner_editor ();

	notebook.pages().push_back (TabElem (misc_packer, _("Misc")));
	notebook.pages().push_back (TabElem (sync_packer, _("Sync")));
	notebook.pages().push_back (TabElem (path_table, _("Paths/Files")));
	notebook.pages().push_back (TabElem (display_packer, _("Display")));
	notebook.pages().push_back (TabElem (keyboard_mouse_table, _("Kbd/Mouse")));
	notebook.pages().push_back (TabElem (click_packer, _("Click")));
	notebook.pages().push_back (TabElem (audition_packer, _("Audition")));
	notebook.pages().push_back (TabElem (fade_packer, _("Layers & Fades")));
	notebook.pages().push_back (TabElem (solo_packer, _("Solo")));

	if (!MIDI::Manager::instance()->get_midi_ports().empty()) {
		setup_midi_options ();
		notebook.pages().push_back (TabElem (midi_packer, _("MIDI")));
	}

	set_session (0);
	show_all_children();
}

void
OptionEditor::set_session (Session *s)
{
	clear_click_editor ();
	clear_auditioner_editor ();

	click_path_entry.set_text ("");
	click_emphasis_path_entry.set_text ("");
	session_raid_entry.set_text ("");

	send_mtc_button.set_sensitive (false);
	send_mmc_button.set_sensitive (false);
	midi_feedback_button.set_sensitive (false);
	midi_control_button.set_sensitive (false);
	mmc_control_button.set_sensitive (false);
	click_path_entry.set_sensitive (false);
	click_emphasis_path_entry.set_sensitive (false);
	session_raid_entry.set_sensitive (false);
	plugins_on_rec_button.set_sensitive (false);
	verify_remove_last_capture_button.set_sensitive (false);
	slave_type_combo.set_sensitive (false);
	solo_latched_button.set_sensitive (false);
	solo_via_bus_button.set_sensitive (false);
	smpte_fps_combo.set_sensitive (false);
	meter_hold_combo.set_sensitive (false);
	meter_falloff_combo.set_sensitive (false);
	auto_connect_inputs_button.set_sensitive (false);
	auto_connect_output_physical_button.set_sensitive (false);
	auto_connect_output_master_button.set_sensitive (false);
	auto_connect_output_manual_button.set_sensitive (false);
	layer_mode_combo.set_sensitive (false);
	short_xfade_slider.set_sensitive (false);
	smpte_offset_negative_button.set_sensitive (false);

	smpte_offset_clock.set_session (s);

	if ((session = s) == 0) {
		return;
	}

	send_mtc_button.set_sensitive (true);
	send_mmc_button.set_sensitive (true);
	midi_feedback_button.set_sensitive (true);
	midi_control_button.set_sensitive (true);
	mmc_control_button.set_sensitive (true);
	click_path_entry.set_sensitive (true);
	click_emphasis_path_entry.set_sensitive (true);
	session_raid_entry.set_sensitive (true);
	plugins_on_rec_button.set_sensitive (true);
	verify_remove_last_capture_button.set_sensitive (true);
	slave_type_combo.set_sensitive (true);
	solo_latched_button.set_sensitive (true);
	solo_via_bus_button.set_sensitive (true);
	smpte_fps_combo.set_sensitive (true);
	meter_hold_combo.set_sensitive (true);
	meter_falloff_combo.set_sensitive (true);
	auto_connect_inputs_button.set_sensitive (true);
	auto_connect_output_physical_button.set_sensitive (true);
	auto_connect_output_master_button.set_sensitive (true);
	auto_connect_output_manual_button.set_sensitive (true);
	layer_mode_combo.set_sensitive (true);
	short_xfade_slider.set_sensitive (true);
	smpte_offset_negative_button.set_sensitive (true);

	if (!s->smpte_drop_frames) {
		// non-drop frames
		if (s->smpte_frames_per_second == 24.0)
			smpte_fps_combo.set_active_text (_("24 FPS"));
		else if (s->smpte_frames_per_second == 25.0)
			smpte_fps_combo.set_active_text (_("25 FPS"));
		else if (s->smpte_frames_per_second == 30.0)
			smpte_fps_combo.set_active_text (_("30 FPS"));
		else
			smpte_fps_combo.set_active_text (_("???"));
	} else {
		// drop frames
		if (floor(s->smpte_frames_per_second) == 29.0)
			smpte_fps_combo.set_active_text (_("30 FPS drop"));
		else
			smpte_fps_combo.set_active_text (_("???"));
	}
	
	smpte_offset_clock.set_session (s);
	smpte_offset_clock.set (s->smpte_offset (), true);

	smpte_offset_negative_button.set_active (session->smpte_offset_negative());
	send_mtc_button.set_active (session->get_send_mtc());

	/* MIDI I/O */

	send_mmc_button.set_active (session->get_send_mmc());
	midi_control_button.set_active (session->get_midi_control());
	midi_feedback_button.set_active (session->get_midi_feedback());
	mmc_control_button.set_active (session->get_mmc_control());

	/* set up port assignments */

	std::map<MIDI::Port*,vector<RadioButton*> >::iterator res;

	if (session->mtc_port()) {
		if ((res = port_toggle_buttons.find (session->mtc_port())) != port_toggle_buttons.end()) {
			(*res).second[MtcIndex]->set_active (true);
		}
	} 

	if (session->mmc_port ()) {
		if ((res = port_toggle_buttons.find (session->mmc_port())) != port_toggle_buttons.end()) {
			(*res).second[MmcIndex]->set_active (true);
		} 
	}

	if (session->midi_port()) {
		if ((res = port_toggle_buttons.find (session->midi_port())) != port_toggle_buttons.end()) {
			(*res).second[MidiIndex]->set_active (true);
		}
	}

	auto_connect_inputs_button.set_active (session->get_input_auto_connect());

	Session::AutoConnectOption oac = session->get_output_auto_connect();
	if (oac & Session::AutoConnectPhysical) {
		auto_connect_output_physical_button.set_active (true);
	} else if (oac & Session::AutoConnectMaster) {
		auto_connect_output_master_button.set_active (true);
	} else {
		auto_connect_output_manual_button.set_active (true);
	}

	setup_click_editor ();
	connect_audition_editor ();

	plugins_on_rec_button.set_active (session->get_recording_plugins ());
	verify_remove_last_capture_button.set_active (Config->get_verify_remove_last_capture());

	layer_mode_combo.set_active_text (layer_mode_strings[session->get_layer_model()]);
	xfade_model_combo.set_active_text (xfade_model_strings[session->get_xfade_model()]);

	short_xfade_adjustment.set_value ((Crossfade::short_xfade_length() / (float) session->frame_rate()) * 1000.0);

	xfade_active_button.set_active (session->get_crossfades_active());
	solo_latched_button.set_active (session->solo_latched());
	solo_via_bus_button.set_active (session->solo_model() == Session::SoloBus);
	
	add_session_paths ();

	vector<string> dumb;
	dumb.push_back (positional_sync_strings[Session::None]);
	dumb.push_back (positional_sync_strings[Session::JACK]);
	if (session->mtc_port()) {
		dumb.push_back (positional_sync_strings[Session::MTC]);
	} 
	set_popdown_strings (slave_type_combo, dumb);

	// meter stuff
	if (session->meter_falloff() == 0.0f) {
		meter_falloff_combo.set_active_text (_("Off"));
	} else if (session->meter_falloff() <= 0.3f) {
		meter_falloff_combo.set_active_text (_("Slowest"));
	} else if (session->meter_falloff() <= 0.4f) {
		meter_falloff_combo.set_active_text (_("Slow"));
	} else if (session->meter_falloff() <= 0.8f) {
		meter_falloff_combo.set_active_text (_("Medium"));
	} else if (session->meter_falloff() <= 1.4f) {
		meter_falloff_combo.set_active_text (_("Fast"));
	} else if (session->meter_falloff() <= 2.0f) {
		meter_falloff_combo.set_active_text (_("Faster"));
	} else {
		meter_falloff_combo.set_active_text (_("Fastest"));
	}

	switch ((int) floor (session->meter_hold())) {
	case 0:
		meter_hold_combo.set_active_text (_("Off"));
		break;
	case 40:
		meter_hold_combo.set_active_text (_("Short"));
		break;
	case 100:
		meter_hold_combo.set_active_text (_("Medium"));
		break;
	case 200:
		meter_hold_combo.set_active_text (_("Long"));
		break;
	}
	
	session_control_changed (Session::SlaveType);
	session_control_changed (Session::AlignChoice);
	session->ControlChanged.connect (mem_fun(*this, &OptionEditor::queue_session_control_changed));
}

OptionEditor::~OptionEditor ()
{
}

static const gchar *native_format_strings[] = {
	N_("Broadcast WAVE/floating point"),
	N_("WAVE/floating point"),
	0
};

void
OptionEditor::setup_path_options()
{
	Gtk::Label* label;

	path_table.set_homogeneous (true);
	path_table.set_border_width (12);
	path_table.set_row_spacings (5);

	session_raid_entry.set_name ("OptionsEntry");

	session_raid_entry.signal_activate().connect (mem_fun(*this, &OptionEditor::raid_path_changed));

	label = manage(new Label(_("session RAID path")));
	label->set_name ("OptionsLabel");
	path_table.attach (*label, 0, 1, 0, 1, FILL|EXPAND, FILL);
	path_table.attach (session_raid_entry, 1, 3, 0, 1, Gtk::FILL|Gtk::EXPAND, FILL);

	label = manage(new Label(_("Native Format")));
	label->set_name ("OptionsLabel");
	path_table.attach (*label, 0, 1, 1, 2, FILL|EXPAND, FILL);
	path_table.attach (native_format_combo, 1, 3, 1, 2, Gtk::FILL|Gtk::EXPAND, FILL);

	label = manage(new Label(_("Soundfile Search Paths")));
	label->set_name("OptionsLabel");
	path_table.attach(*label, 0, 1, 2, 3, FILL|EXPAND, FILL);
	path_table.attach(sfdb_path_view, 1, 3, 2, 3, Gtk::FILL|Gtk::EXPAND, FILL);

	sfdb_path_view.append_column(_("Paths"), sfdb_path_columns.paths);

	vector<string> nfstrings = internationalize (native_format_strings);

	set_popdown_strings (native_format_combo, nfstrings);
	native_format_combo.signal_changed().connect (mem_fun(*this, &OptionEditor::native_format_chosen));

	fixup_combo_size (native_format_combo, nfstrings);

	if (Config->get_native_format_is_bwf()) {
		native_format_combo.set_active_text (native_format_strings[0]);
	} else {
		native_format_combo.set_active_text (native_format_strings[1]);
	}
	
	path_table.show_all();
}

void
OptionEditor::add_session_paths ()
{
	click_path_entry.set_sensitive (true);
	click_emphasis_path_entry.set_sensitive (true);
	session_raid_entry.set_sensitive (true);

	if (session->click_sound.length() == 0) {
		click_path_entry.set_text (_("internal"));
	} else {
		click_path_entry.set_text (session->click_sound);
	}

	if (session->click_emphasis_sound.length() == 0) {
		click_emphasis_path_entry.set_text (_("internal"));
	} else {
		click_emphasis_path_entry.set_text (session->click_emphasis_sound);
	}

	session_raid_entry.set_text(session->raid_path());
}

void
OptionEditor::setup_fade_options ()
{
	Gtk::HBox* hbox;
	vector<string> dumb;
	
	auto_xfade_button.set_name ("OptionEditorToggleButton");
	xfade_active_button.set_name ("OptionEditorToggleButton");

	hbox = manage (new HBox);
	hbox->set_border_width (12);
	hbox->pack_start (auto_xfade_button, false, false);
	fade_packer.pack_start (*hbox, false, false);

	hbox = manage (new HBox);
	hbox->set_border_width (12);
	hbox->pack_start (xfade_active_button, false, false);
	fade_packer.pack_start (*hbox, false, false);

	layer_mode_strings = internationalize (lmode_strings);

	dumb.push_back (lmode_strings[Session::LaterHigher]);
	dumb.push_back (lmode_strings[Session::MoveAddHigher]);
	dumb.push_back (lmode_strings[Session::AddHigher]);
	set_popdown_strings (layer_mode_combo, dumb);

	layer_mode_combo.signal_changed ().connect (mem_fun(*this, &OptionEditor::layer_mode_chosen));

	fixup_combo_size (layer_mode_combo, layer_mode_strings);

	hbox = manage (new HBox);
	hbox->set_border_width (5);
	hbox->set_spacing (10);
	hbox->pack_start (layer_mode_label, false, false);
	hbox->pack_start (layer_mode_combo, false, false);
	fade_packer.pack_start (*hbox, false, false);

	xfade_model_strings = internationalize (xfl_strings);

	dumb.clear ();
	dumb.push_back (xfade_model_strings[FullCrossfade]);
	dumb.push_back (xfade_model_strings[ShortCrossfade]);
	set_popdown_strings (xfade_model_combo, dumb);

	xfade_model_combo.signal_changed().connect (mem_fun(*this, &OptionEditor::xfade_model_chosen));

	fixup_combo_size (xfade_model_combo, xfade_model_strings);

	hbox = manage (new HBox);
	hbox->set_border_width (5);
	hbox->set_spacing (10);
	hbox->pack_start (xfade_model_label, false, false);
	hbox->pack_start (xfade_model_combo, false, false);
	fade_packer.pack_start (*hbox, false, false);

	auto_xfade_button.set_active (Config->get_auto_xfade());
	/* xfade and layer mode active requires session */

	auto_xfade_button.signal_clicked().connect (mem_fun(*this, &OptionEditor::auto_xfade_clicked));
	xfade_active_button.signal_clicked().connect (mem_fun(*this, &OptionEditor::xfade_active_clicked));
	
	Label* short_xfade_label = manage (new Label (_("Short crossfade length (msecs)")));
	short_xfade_label->set_name ("OptionsLabel");
	
	hbox = manage (new HBox);
	hbox->set_border_width (5);
	hbox->set_spacing (10);
	hbox->pack_start (*short_xfade_label, false, false);
	hbox->pack_start (short_xfade_slider, true, true);
	fade_packer.pack_start (*hbox, false, false);

	short_xfade_adjustment.signal_value_changed().connect (mem_fun(*this, &OptionEditor::short_xfade_adjustment_changed));

	fade_packer.show_all ();
}

void
OptionEditor::short_xfade_adjustment_changed ()
{
	if (session) {
		float val = short_xfade_adjustment.get_value();
		
		/* val is in msecs */
		
		Crossfade::set_short_xfade_length ((jack_nframes_t) floor (session->frame_rate() * (val / 1000.0)));
	}
}

void
OptionEditor::layer_mode_chosen ()
{
	if (!session) {
		return;
	}

	string which = layer_mode_combo.get_active_text ();

	if (which == layer_mode_strings[Session::LaterHigher]) {
		session->set_layer_model (Session::LaterHigher);
	} else if (which == layer_mode_strings[Session::MoveAddHigher]) {
		session->set_layer_model (Session::MoveAddHigher);
	} else if (which == layer_mode_strings[Session::AddHigher]) {
		session->set_layer_model (Session::AddHigher);
	}
}

void
OptionEditor::xfade_model_chosen ()
{
	if (!session) {
		return;
	}

	string which = xfade_model_combo.get_active_text ();

	if (which == xfade_model_strings[FullCrossfade]) {
		session->set_xfade_model (FullCrossfade);
	} else if (which == xfade_model_strings[ShortCrossfade]) {
		session->set_xfade_model (ShortCrossfade);
	}
}

void
OptionEditor::auto_xfade_clicked ()
{
	Config->set_auto_xfade (auto_xfade_button.get_active());
}

void
OptionEditor::xfade_active_clicked ()
{
	if (session) {
		session->set_crossfades_active (xfade_active_button.get_active());
	}
}

void
OptionEditor::setup_solo_options ()
{
	Gtk::HBox* hbox;

	solo_via_bus_button.set_name ("OptionEditorToggleButton");
	solo_latched_button.set_name ("OptionEditorToggleButton");

	hbox = manage (new HBox);
	hbox->set_border_width (12);
	hbox->pack_start (solo_via_bus_button, false, false);
	solo_packer.pack_start (*hbox, false, false);

	hbox = manage (new HBox);
	hbox->set_border_width (12);
	hbox->pack_start (solo_latched_button, false, false);
	solo_packer.pack_start (*hbox, false, false);

	solo_via_bus_button.signal_clicked().connect 
		(mem_fun(*this, &OptionEditor::solo_via_bus_clicked));
	solo_latched_button.signal_clicked().connect 
		(mem_fun(*this, &OptionEditor::solo_latched_clicked));

	solo_packer.show_all ();
}

void
OptionEditor::solo_via_bus_clicked ()
{
	if (!session) {
		return;
	}

	if (solo_via_bus_button.get_active()) {
		session->set_solo_model (Session::SoloBus);
	} else {
		session->set_solo_model (Session::InverseMute);
	}
}

void
OptionEditor::solo_latched_clicked ()
{
	if (!session) {
		return;
	}

	bool x = solo_latched_button.get_active();

	if (x != session->solo_latched()) {
		session->set_solo_latched (x);
	}
}

void
OptionEditor::setup_display_options ()
{
	HBox* hbox;
	vector<string> dumb;

	display_packer.set_border_width (12);
	display_packer.set_spacing (5);

	show_waveforms_button.set_name ("OptionEditorToggleButton");
	show_waveforms_recording_button.set_name ("OptionEditorToggleButton");
	show_measures_button.set_name ("OptionEditorToggleButton");
	follow_playhead_button.set_name ("OptionEditorToggleButton");
	mixer_strip_width_button.set_name ("OptionEditorToggleButton");

	mixer_strip_width_button.set_active (mixer.get_strip_width() == Narrow);

	hbox = manage (new HBox);
	hbox->set_border_width (8);
	hbox->pack_start (show_waveforms_button, false, false);
	display_packer.pack_start (*hbox, false, false);

	hbox = manage (new HBox);
	hbox->set_border_width (8);
	hbox->pack_start (show_waveforms_recording_button, false, false);
	display_packer.pack_start (*hbox, false, false);
	
	hbox = manage (new HBox);
	hbox->set_border_width (8);
	hbox->pack_start (show_measures_button, false, false);
	display_packer.pack_start (*hbox, false, false);

	hbox = manage (new HBox);
	hbox->set_border_width (8);
	hbox->pack_start (mixer_strip_width_button, false, false);
	display_packer.pack_start (*hbox, false, false);

	hbox = manage (new HBox);
	hbox->set_border_width (8);
	hbox->pack_start (follow_playhead_button, false, false);
	display_packer.pack_start (*hbox, false, false);

	Label *meter_hold_label = manage (new Label (_("Meter Peak Hold")));
	meter_hold_label->set_name ("OptionsLabel");
	dumb.clear ();
	dumb.push_back (_("Off"));
	dumb.push_back (_("Short"));
	dumb.push_back (_("Medium"));
	dumb.push_back (_("Long"));
	set_popdown_strings (meter_hold_combo, dumb);
	meter_hold_combo.signal_changed().connect (mem_fun(*this, &OptionEditor::meter_hold_chosen));
	hbox = manage (new HBox);
	hbox->set_border_width (8);
	hbox->set_spacing (8);
	hbox->pack_start (*meter_hold_label, false, false);
	hbox->pack_start (meter_hold_combo, false, false);
	display_packer.pack_start (*hbox, false, false);

	Label *meter_falloff_label = manage (new Label (_("Meter Falloff")));
	meter_falloff_label->set_name ("OptionsLabel");
	dumb.clear ();
	dumb.push_back (_("Off"));
	dumb.push_back (_("Slowest"));
	dumb.push_back (_("Slow"));
	dumb.push_back (_("Medium"));
	dumb.push_back (_("Fast"));
	dumb.push_back (_("Faster"));
	dumb.push_back (_("Fastest"));
	set_popdown_strings (meter_falloff_combo, dumb);
	meter_falloff_combo.signal_changed().connect (mem_fun(*this, &OptionEditor::meter_falloff_chosen));
	hbox = manage (new HBox);
	hbox->set_border_width (8);
	hbox->set_spacing (8);
	hbox->pack_start (*meter_falloff_label, false, false);
	hbox->pack_start (meter_falloff_combo, false, false);
	display_packer.pack_start (*hbox, false, false);
	
	
	show_waveforms_button.signal_clicked().connect (mem_fun(*this, &OptionEditor::show_waveforms_clicked));
	show_waveforms_recording_button.signal_clicked().connect (mem_fun(*this, &OptionEditor::show_waveforms_recording_clicked));
	show_measures_button.signal_clicked().connect (mem_fun(*this, &OptionEditor::show_measures_clicked));
	mixer_strip_width_button.signal_clicked().connect (mem_fun(*this, &OptionEditor::strip_width_clicked));
	follow_playhead_button.signal_clicked().connect (mem_fun(*this, &OptionEditor::follow_playhead_clicked));

	editor.DisplayControlChanged.connect (mem_fun(*this, &OptionEditor::display_control_changed));

	show_measures_button.set_active (editor.show_measures());
	show_waveforms_button.set_active (editor.show_waveforms());
	show_waveforms_recording_button.set_active (editor.show_waveforms_recording());
	follow_playhead_button.set_active (editor.follow_playhead());
}

void
OptionEditor::meter_hold_chosen ()
{
	if (session) {
		string str = meter_hold_combo.get_active_text();
		
		if (str == _("Off")) {
			session->set_meter_hold (0);
		} else if (str == _("Short")) {
			session->set_meter_hold (40);
		} else if (str == _("Medium")) {
			session->set_meter_hold (100);
		} else if (str == _("Long")) {
			session->set_meter_hold (200);
		}
	}
}

void
OptionEditor::meter_falloff_chosen ()
{
	if (session) {
		string str = meter_falloff_combo.get_active_text();
		
		if (str == _("Off")) {
			session->set_meter_falloff (0.0f);
		} else if (str == _("Slowest")) {
			session->set_meter_falloff (0.266f); // 6.6 dB/sec falloff at update rate of 40 ms
		} else if (str == _("Slow")) {
			session->set_meter_falloff (0.342f); // 8.6 dB/sec falloff at update rate of 40 ms
		} else if (str == _("Medium")) {
			session->set_meter_falloff  (0.7f);
		} else if (str == _("Fast")) {
			session->set_meter_falloff (1.1f);
		} else if (str == _("Faster")) {
			session->set_meter_falloff (1.5f);
		} else if (str == _("Fastest")) {
			session->set_meter_falloff (2.5f);
		}
	}
}

void
OptionEditor::display_control_changed (Editing::DisplayControl dc)
{
	ToggleButton* button = 0;
	bool val = true;

	switch (dc) {
	case ShowMeasures:
		val = editor.show_measures ();
		button = &show_measures_button;
		break;
	case ShowWaveforms:
		val = editor.show_waveforms ();
		button = &show_waveforms_button;
		break;
	case ShowWaveformsRecording:
		val = editor.show_waveforms_recording ();
		button = &show_waveforms_recording_button;
		break;
	case FollowPlayhead:
		val = editor.follow_playhead ();
		button = &follow_playhead_button;
		break;
	}

	if (button->get_active() != val) {
		button->set_active (val);
	}
}

void
OptionEditor::setup_sync_options ()
{
	Label *slave_type_label = manage (new Label (_("Positional Sync")));
	HBox* hbox;
	vector<string> dumb;

	slave_type_label->set_name("OptionsLabel");
	positional_sync_strings = internationalize (psync_strings);

	slave_type_combo.set_name ("OptionsEntry");
	slave_type_combo.signal_changed().connect (mem_fun(*this, &OptionEditor::slave_type_chosen));

	dumb.clear ();
	dumb.push_back (X_("24 FPS"));
	dumb.push_back (X_("25 FPS"));
	dumb.push_back (X_("30 FPS drop"));
	dumb.push_back (X_("30 FPS non-drop"));
	
	set_popdown_strings (smpte_fps_combo, dumb);
	smpte_fps_combo.signal_changed().connect (mem_fun(*this, &OptionEditor::smpte_fps_chosen));
	
	smpte_offset_clock.set_mode (AudioClock::SMPTE);
	smpte_offset_clock.ValueChanged.connect (mem_fun(*this, &OptionEditor::smpte_offset_chosen));
	
	send_mtc_button.set_name ("OptionEditorToggleButton");
	jack_time_master_button.set_name ("OptionEditorToggleButton");
	smpte_offset_negative_button.set_name ("OptionEditorToggleButton");

	send_mtc_button.unset_flags (Gtk::CAN_FOCUS);
	jack_time_master_button.unset_flags (Gtk::CAN_FOCUS);
	smpte_offset_negative_button.unset_flags (Gtk::CAN_FOCUS);

	hbox = manage (new HBox);
	hbox->set_border_width (5);
	hbox->set_spacing (10);
	hbox->pack_start (*slave_type_label, false, false);
	hbox->pack_start (slave_type_combo, false, false);

	sync_packer.pack_start (*hbox, false, false);
	
	hbox = manage (new HBox);
	hbox->set_border_width (5);
	hbox->pack_start (send_mtc_button, false, false);
	sync_packer.pack_start (*hbox, false, false);

	hbox = manage (new HBox);
	hbox->set_border_width (5);
	hbox->pack_start (jack_time_master_button, false, false);
	sync_packer.pack_start (*hbox, false, false);

	Label *smpte_fps_label = manage (new Label (_("SMPTE Frames/second")));
	Label *smpte_offset_label = manage (new Label (_("SMPTE Offset")));
	smpte_fps_label->set_name("OptionsLabel");
	smpte_offset_label->set_name("OptionsLabel");
	
	hbox = manage (new HBox);
	hbox->set_border_width (5);
	hbox->set_spacing (10);
	hbox->pack_start (*smpte_fps_label, false, false);
	hbox->pack_start (smpte_fps_combo, false, false);

	sync_packer.pack_start (*hbox, false, false);

	hbox = manage (new HBox);
	hbox->set_border_width (5);
	hbox->set_spacing (10);
	hbox->pack_start (*smpte_offset_label, false, false);
	hbox->pack_start (smpte_offset_clock, false, false);
	hbox->pack_start (smpte_offset_negative_button, false, false);

	sync_packer.pack_start (*hbox, false, false);

	jack_time_master_button.set_active (Config->get_jack_time_master());

	send_mtc_button.signal_button_press_event().connect (bind (mem_fun(*this, &OptionEditor::send_mtc_toggled), &send_mtc_button));
	jack_time_master_button.signal_clicked().connect (mem_fun(*this, &OptionEditor::jack_time_master_clicked));
	smpte_offset_negative_button.signal_clicked().connect (mem_fun(*this, &OptionEditor::smpte_offset_negative_clicked));
}

void
OptionEditor::smpte_offset_negative_clicked ()
{
	if (session) {
		session->set_smpte_offset_negative (smpte_offset_negative_button.get_active());
	}
}

void
OptionEditor::smpte_fps_chosen ()
{
	if (session) {
		string str = smpte_fps_combo.get_active_text();
		
		if (str == X_("24 FPS")) {
			session->set_smpte_type (24.0, false);
		} else if (str == X_("25 FPS")) {
			session->set_smpte_type (25.0, false);
		} else if (str == X_("30 FPS drop")) {
			session->set_smpte_type (29.97, true);
		} else if (str == X_("30 FPS non-drop")) {
			session->set_smpte_type (30.0, false);
		}
	}
}

void
OptionEditor::smpte_offset_chosen()
{
	if (session) {
		jack_nframes_t frames = smpte_offset_clock.current_duration();
		session->set_smpte_offset (frames);
	}
}


void
OptionEditor::setup_midi_options ()
{
	HBox* hbox;
	MIDI::Manager::PortMap::const_iterator i;
	const MIDI::Manager::PortMap& ports = MIDI::Manager::instance()->get_midi_ports();
	int n;
	ToggleButton* tb;
	RadioButton* rb;

	Gtk::Table* table = manage (new Table (ports.size() + 4, 9));

	table->set_row_spacings (6);
	table->set_col_spacings (10);

	table->attach (*(manage (new Label (X_("Port")))), 0, 1, 0, 1);
	table->attach (*(manage (new Label (X_("Offline")))), 1, 2, 0, 1);
	table->attach (*(manage (new Label (X_("Trace\nInput")))), 2, 3, 0, 1);
	table->attach (*(manage (new Label (X_("Trace\nOutput")))), 3, 4, 0, 1);
	table->attach (*(manage (new Label (X_("MTC")))), 4, 5, 0, 1);
	table->attach (*(manage (new Label (X_("MMC")))), 6, 7, 0, 1);
	table->attach (*(manage (new Label (X_("MIDI Parameter\nControl")))), 8, 9, 0, 1);

	table->attach (*(manage (new HSeparator())), 0, 9, 1, 2);
	table->attach (*(manage (new VSeparator())), 5, 6, 0, 8);
	table->attach (*(manage (new VSeparator())), 7, 8, 0, 8);
	
	for (n = 0, i = ports.begin(); i != ports.end(); ++n, ++i) {

		pair<MIDI::Port*,vector<RadioButton*> > newpair;

		newpair.first = i->second;

		table->attach (*(manage (new Label (i->first))), 0, 1, n+2, n+3,FILL|EXPAND, FILL );
		tb = manage (new ToggleButton (_("online")));
		tb->set_name ("OptionEditorToggleButton");

		/* remember, we have to handle the i18n case where the relative
		   lengths of the strings in language N is different than in english.
		*/

		if (strlen (_("offline")) > strlen (_("online"))) {
			set_size_request_to_display_given_text (*tb, _("offline"), 15, 12);
		} else {
			set_size_request_to_display_given_text (*tb, _("online"), 15, 12);
		}

		tb->set_active (!(*i).second->input()->offline());
		tb->signal_button_press_event().connect (bind (mem_fun(*this, &OptionEditor::port_online_toggled), (*i).second, tb));
		(*i).second->input()->OfflineStatusChanged.connect (bind (mem_fun(*this, &OptionEditor::map_port_online), (*i).second, tb));
		table->attach (*tb, 1, 2, n+2, n+3, FILL|EXPAND, FILL);

		tb = manage (new ToggleButton ());
		tb->set_name ("OptionEditorToggleButton");
		tb->signal_button_press_event().connect (bind (mem_fun(*this, &OptionEditor::port_trace_in_toggled), (*i).second, tb));
		tb->set_size_request (10, 10);
		table->attach (*tb, 2, 3, n+2, n+3, FILL|EXPAND, FILL);

		tb = manage (new ToggleButton ());
		tb->set_name ("OptionEditorToggleButton");
		tb->signal_button_press_event().connect (bind (mem_fun(*this, &OptionEditor::port_trace_out_toggled), (*i).second, tb));
		tb->set_size_request (10, 10);
		table->attach (*tb, 3, 4, n+2, n+3, FILL|EXPAND, FILL);

		rb = manage (new RadioButton ());
		newpair.second.push_back (rb);
		rb->set_name ("OptionEditorToggleButton");
		if (n == 0) {
			mtc_button_group = rb->get_group();
		} else {
			rb->set_group (mtc_button_group);

		}
		table->attach (*rb, 4, 5, n+2, n+3, FILL|EXPAND, FILL);
		rb->signal_button_press_event().connect (bind (mem_fun(*this, &OptionEditor::mtc_port_chosen), (*i).second, rb));

		if (Config->get_mtc_port_name() == i->first) {
			rb->set_active (true);
		}
		
		rb = manage (new RadioButton ());
		newpair.second.push_back (rb);
		rb->set_name ("OptionEditorToggleButton");
		if (n == 0) {
			mmc_button_group = rb->get_group();
		} else {
			rb->set_group (mmc_button_group);
		}
		table->attach (*rb, 6, 7, n+2, n+3, FILL|EXPAND, FILL);
		rb->signal_button_press_event().connect (bind (mem_fun(*this, &OptionEditor::mmc_port_chosen), (*i).second, rb));

		if (Config->get_mmc_port_name() == i->first) {
			rb->set_active (true);
		}

		rb = manage (new RadioButton ());
		newpair.second.push_back (rb);
		rb->set_name ("OptionEditorToggleButton");
		if (n == 0) {
			midi_button_group = rb->get_group();
		} else {
			rb->set_group (midi_button_group);
		}
		table->attach (*rb, 8, 9, n+2, n+3, FILL|EXPAND, FILL);
		rb->signal_button_press_event().connect (bind (mem_fun(*this, &OptionEditor::midi_port_chosen), (*i).second, rb));

		if (Config->get_midi_port_name() == i->first) {
			rb->set_active (true);
		}
		
		port_toggle_buttons.insert (newpair);
	}

	table->show_all ();

	hbox = manage (new HBox);
	hbox->set_border_width (6);
	hbox->pack_start (*table, true, false);
	midi_packer.pack_start (*hbox, false, false);
	
	VBox* mmcbuttonbox = manage (new VBox);

	mmc_control_button.set_name ("OptionEditorToggleButton");

	hbox = manage (new HBox);
	hbox->set_border_width (6);
	hbox->pack_start (mmc_control_button, false, false, 36);
	mmcbuttonbox->pack_start (*hbox, false, false);

	midi_control_button.set_name ("OptionEditorToggleButton");

	hbox = manage (new HBox);
	hbox->set_border_width (6);
	hbox->pack_start (midi_control_button, false, false, 36);
	mmcbuttonbox->pack_start (*hbox, false, false);

	send_mmc_button.set_name ("OptionEditorToggleButton");

	hbox = manage (new HBox);
	hbox->set_border_width (6);
	hbox->pack_start (send_mmc_button, false, false, 36);
	mmcbuttonbox->pack_start (*hbox, false, false);
	
	midi_feedback_button.set_name ("OptionEditorToggleButton");

	hbox = manage (new HBox);
	hbox->set_border_width (6);
	hbox->pack_start (midi_feedback_button, false, false, 36);
	mmcbuttonbox->pack_start (*hbox, false, false);

	midi_packer.pack_start (*mmcbuttonbox, false, false);

	mmc_control_button.signal_toggled().connect (bind (mem_fun(*this, &OptionEditor::mmc_control_toggled), &mmc_control_button));
	midi_control_button.signal_toggled().connect (bind (mem_fun(*this, &OptionEditor::midi_control_toggled), &midi_control_button));
	send_mmc_button.signal_toggled().connect (bind (mem_fun(*this, &OptionEditor::send_mmc_toggled), &send_mmc_button));
	midi_feedback_button.signal_toggled().connect (bind (mem_fun(*this, &OptionEditor::midi_feedback_toggled), &midi_feedback_button));
}

gint
OptionEditor::mtc_port_chosen (GdkEventButton* ev, MIDI::Port *port, Gtk::RadioButton* rb) 
{
	if (session) {
		if (!rb->get_active()) {
			if (port) {
				session->set_mtc_port (port->name());
				Config->set_mtc_port_name (port->name());
			} else {
				session->set_mtc_port ("");
			}

			/* update sync options to reflect MTC port availability */

			vector<string> dumb;
			dumb.push_back (positional_sync_strings[Session::None]);
			dumb.push_back (positional_sync_strings[Session::JACK]);

			if (session->mtc_port()) {
				dumb.push_back (positional_sync_strings[Session::MTC]);
			}
			set_popdown_strings (slave_type_combo, dumb);

			rb->set_active (true);
		}
	}

	return stop_signal (*rb, "button_press_event");
}

gint
OptionEditor::mmc_port_chosen (GdkEventButton* ev, MIDI::Port* port, Gtk::RadioButton* rb)
{
	if (session) {
		if (!rb->get_active()) {
			if (port) {
				session->set_mmc_port (port->name());
				Config->set_mtc_port_name (port->name());
			} else {
				session->set_mmc_port ("");
			}
			rb->set_active (true);
		}
	}
	return stop_signal (*rb, "button_press_event");
}

gint
OptionEditor::midi_port_chosen (GdkEventButton* ev, MIDI::Port* port, Gtk::RadioButton* rb)
{
	if (session) {
		if (!rb->get_active()) {
			if (port) {
				session->set_midi_port (port->name());
				Config->set_midi_port_name (port->name());
			} else {
				session->set_midi_port ("");
			}
			rb->set_active (true);
		}
	}
	return stop_signal (*rb, "button_press_event");
}

gint
OptionEditor::port_online_toggled (GdkEventButton* ev, MIDI::Port* port, ToggleButton* tb)
{
	bool wanted = tb->get_active(); /* it hasn't changed at this point */

	if (wanted != port->input()->offline()) {
		port->input()->set_offline (wanted);
	} 
	return stop_signal (*tb, "button_press_event");
}

void
OptionEditor::map_port_online (MIDI::Port* port, ToggleButton* tb)
{
	if (port->input()->offline()) {
		static_cast<Label*>(tb->get_child())->set_text (_("offline"));
		tb->set_active (false);
	} else {
		static_cast<Label*>(tb->get_child())->set_text (_("online"));
		tb->set_active (true);
	}
}

gint
OptionEditor::port_trace_in_toggled (GdkEventButton* ev, MIDI::Port* port, ToggleButton* tb)
{
	/* XXX not very good MVC style here */

	port->input()->trace (!tb->get_active(), &cerr, string (port->name()) + string (" input: "));
	tb->set_active (!tb->get_active());
	return stop_signal (*tb, "button_press_event");
}

gint
OptionEditor::port_trace_out_toggled (GdkEventButton* ev,MIDI::Port* port, ToggleButton* tb)
{
	/* XXX not very good MVC style here */

	port->output()->trace (!tb->get_active(), &cerr, string (port->name()) + string (" output: "));
	tb->set_active (!tb->get_active());
	return stop_signal (*tb, "button_press_event");
}

gint
OptionEditor::send_mtc_toggled (GdkEventButton *ev, CheckButton *button)
{
	if (session) {
		session->set_send_mtc (!button->get_active());
	}
	return stop_signal (*button, "button_press_event");
}

void
OptionEditor::send_mmc_toggled (CheckButton *button)
{
	if (session) {
		session->set_send_mmc (button->get_active());
	}
}

void
OptionEditor::mmc_control_toggled (CheckButton *button)
{
	if (session) {
		session->set_mmc_control (button->get_active());
	}
}

void
OptionEditor::midi_control_toggled (CheckButton *button)
{
	if (session) {
		session->set_midi_control (button->get_active());
	}
}

void
OptionEditor::midi_feedback_toggled (CheckButton *button)
{
	if (session) {
		session->set_midi_feedback (button->get_active());
	}
}

void
OptionEditor::save ()
{
	/* XXX a bit odd that we save the entire session state here */

	ui.save_state ("");
}

gint
OptionEditor::wm_close (GdkEventAny *ev)
{
	save ();
	just_close_win();
	return TRUE;
}

void
OptionEditor::jack_time_master_clicked ()
{
	bool yn = jack_time_master_button.get_active();

	Config->set_jack_time_master (yn);

	if (session) {
		session->engine().reset_timebase ();
	}
}

void
OptionEditor::raid_path_changed ()
{
	if (session) {
		session->set_raid_path (session_raid_entry.get_text());
	}
}

void
OptionEditor::click_browse_clicked ()
{
	SoundFileChooser sfdb (_("Choose Click"));
	
	int result = sfdb.run ();

	if (result == Gtk::RESPONSE_OK) {
		click_chosen(sfdb.get_filename());
	}
}

void
OptionEditor::click_chosen (stringcr_t path)
{
	click_path_entry.set_text (path);
	click_sound_changed ();
}

void
OptionEditor::click_emphasis_browse_clicked ()
{
	SoundFileChooser sfdb (_("Choose Click Emphasis"));

	int result = sfdb.run ();

	if (result == Gtk::RESPONSE_OK) {
		click_emphasis_chosen (sfdb.get_filename());
	}
}

void
OptionEditor::click_emphasis_chosen (stringcr_t path)
{	
	click_emphasis_path_entry.set_text (path);
	click_emphasis_sound_changed ();
}

void
OptionEditor::click_sound_changed ()
{
	if (session) {
		string path = click_path_entry.get_text();

		if (path == session->click_sound) {
			return;
		}

		if (path.length() == 0) {

			session->set_click_sound ("");

		} else {

			strip_whitespace_edges (path);
			
			if (path == _("internal")) {
				session->set_click_sound ("");
			} else {
				session->set_click_sound (path);
			}
		}
	}
}

void
OptionEditor::click_emphasis_sound_changed ()
{
	if (session) {
		string path = click_emphasis_path_entry.get_text();

		if (path == session->click_emphasis_sound) {
			return;
		}

		if (path.length() == 0) {

			session->set_click_emphasis_sound ("");

		} else {

			strip_whitespace_edges (path);

			if (path == _("internal")) {
				session->set_click_emphasis_sound ("");
			} else {
				session->set_click_emphasis_sound (path);
			}
		}
	}
}

void
OptionEditor::show_waveforms_clicked ()
{
	editor.set_show_waveforms (show_waveforms_button.get_active());
}

void
OptionEditor::show_waveforms_recording_clicked ()
{
	editor.set_show_waveforms_recording (show_waveforms_recording_button.get_active());
}

void
OptionEditor::show_measures_clicked ()
{
	editor.set_show_measures (show_measures_button.get_active());
}

void
OptionEditor::follow_playhead_clicked ()
{
	editor.set_follow_playhead (follow_playhead_button.get_active());
}

void
OptionEditor::strip_width_clicked ()
{
	mixer.set_strip_width (mixer_strip_width_button.get_active() ? Narrow : Wide);
}


void
OptionEditor::just_close_win()
{
	hide();
}

void
OptionEditor::queue_session_control_changed (Session::ControlType t)
{
	ui.call_slot (bind (mem_fun(*this, &OptionEditor::session_control_changed), t));
}

void
OptionEditor::session_control_changed (Session::ControlType t)
{
	switch (t) {
	case Session::SlaveType:
		switch (session->slave_source()) {
		case Session::None:
			slave_type_combo.set_active_text (positional_sync_strings[Session::None]);
			break;
		case Session::MTC:
			slave_type_combo.set_active_text (positional_sync_strings[Session::MTC]);
			break;
		case Session::JACK:
			slave_type_combo.set_active_text (positional_sync_strings[Session::JACK]);
			break;
		default:
			slave_type_combo.set_active_text (_("--unknown--"));
			break;
		}
		
		break;

	case Session::SendMTC:
		map_some_session_state (send_mtc_button, &Session::get_send_mtc);
		break;

	case Session::SendMMC:
		map_some_session_state (send_mmc_button, &Session::get_send_mmc);
		break;

	case Session::MMCControl:       
		map_some_session_state (mmc_control_button, &Session::get_mmc_control);
		break;

	case Session::MidiFeedback:       
		map_some_session_state (midi_feedback_button, &Session::get_midi_feedback);
		break;

	case Session::MidiControl:       
		 map_some_session_state (midi_control_button, &Session::get_midi_control);
		break;
	
	default:
		break;
	}
}

void
OptionEditor::native_format_chosen ()
{
	string which;

	if (session == 0) {
		return;
	}

	bool use_bwf = (native_format_combo.get_active_text() == native_format_strings[0]);

	if (use_bwf != Config->get_native_format_is_bwf()) {
		Config->set_native_format_is_bwf (use_bwf);
		session->reset_native_file_format ();
	}
}

void
OptionEditor::slave_type_chosen ()
{
	string which;

	if (session == 0) {
		return;
	}

	which = slave_type_combo.get_active_text();

	if (which == positional_sync_strings[Session::None]) {
		session->request_slave_source (Session::None);
	} else if (which == positional_sync_strings[Session::MTC]) {
		session->request_slave_source (Session::MTC);
	} else if (which == positional_sync_strings[Session::JACK]) {
		session->request_slave_source (Session::JACK);
	} 
}

void
OptionEditor::clear_click_editor ()
{
	if (click_io_selector) {
		click_packer.remove (*click_io_selector);
		click_packer.remove (*click_gpm);
		delete click_io_selector;
		delete click_gpm;
		click_io_selector = 0;
		click_gpm = 0;
	}
}

void
OptionEditor::setup_click_editor ()
{
	Label* label;
	HBox* hpacker = manage (new HBox);

	click_path_entry.set_sensitive (true);
	click_emphasis_path_entry.set_sensitive (true);

	click_path_entry.set_name ("OptionsEntry");
	click_emphasis_path_entry.set_name ("OptionsEntry");
	
	click_path_entry.signal_activate().connect (mem_fun(*this, &OptionEditor::click_sound_changed));
	click_emphasis_path_entry.signal_activate().connect (mem_fun(*this, &OptionEditor::click_emphasis_sound_changed));

	click_path_entry.signal_focus_out_event().connect (bind (mem_fun(*this, &OptionEditor::focus_out_event_handler), &OptionEditor::click_sound_changed));
	click_emphasis_path_entry.signal_focus_out_event().connect (bind (mem_fun(*this, &OptionEditor::focus_out_event_handler), &OptionEditor::click_emphasis_sound_changed));

	click_browse_button.set_name ("EditorGTKButton");
	click_emphasis_browse_button.set_name ("EditorGTKButton");
	click_browse_button.signal_clicked().connect (mem_fun(*this, &OptionEditor::click_browse_clicked));
	click_emphasis_browse_button.signal_clicked().connect (mem_fun(*this, &OptionEditor::click_emphasis_browse_clicked));

	click_packer.set_border_width (12);
	click_packer.set_spacing (5);

	click_io_selector = new IOSelector (*session, session->click_io(), false);
	click_gpm = new GainMeter (session->click_io(), *session);

	click_table.set_col_spacings (10);
	
	label = manage(new Label(_("Click audio file")));
	label->set_name ("OptionsLabel");
	click_table.attach (*label, 0, 1, 0, 1, FILL|EXPAND, FILL);
	click_table.attach (click_path_entry, 1, 2, 0, 1, Gtk::FILL|Gtk::EXPAND, FILL);
	click_table.attach (click_browse_button, 2, 3, 0, 1, FILL|EXPAND, FILL);
	
	label = manage(new Label(_("Click emphasis audiofile")));
	label->set_name ("OptionsLabel");
	click_table.attach (*label, 0, 1, 1, 2, FILL|EXPAND, FILL);
	click_table.attach (click_emphasis_path_entry, 1, 2, 1, 2, Gtk::FILL|Gtk::EXPAND, FILL);
	click_table.attach (click_emphasis_browse_button, 2, 3, 1, 2, FILL|EXPAND, FILL);

	hpacker->set_spacing (10);
	hpacker->pack_start (*click_io_selector, false, false);
	hpacker->pack_start (*click_gpm, false, false);

	click_packer.pack_start (click_table, false, false);
	click_packer.pack_start (*hpacker, false, false);

	click_packer.show_all ();
}

void
OptionEditor::clear_auditioner_editor ()
{
	if (auditioner_io_selector) {
		audition_hpacker.remove (*auditioner_io_selector);
		audition_hpacker.remove (*auditioner_gpm);
		delete auditioner_io_selector;
		delete auditioner_gpm;
		auditioner_io_selector = 0;
		auditioner_gpm = 0;
	}
}

void
OptionEditor::setup_auditioner_editor ()
{
	audition_packer.set_border_width (12);
	audition_packer.set_spacing (5);
	audition_hpacker.set_spacing (10);

	audition_label.set_name ("OptionEditorAuditionerLabel");
	audition_label.set_text (_("The auditioner is a dedicated mixer strip used\n"
				   "for listening to specific regions outside the context\n"
				   "of the overall mix. It can be connected just like any\n"
				   "other mixer strip."));
	
	audition_packer.pack_start (audition_label, false, false, 10);
	audition_packer.pack_start (audition_hpacker, false, false);
}

void
OptionEditor::connect_audition_editor ()
{
	auditioner_io_selector = new IOSelector (*session, session->the_auditioner(), false);
	auditioner_gpm = new GainMeter (session->the_auditioner(), *session);

	audition_hpacker.pack_start (*auditioner_io_selector, false, false);
	audition_hpacker.pack_start (*auditioner_gpm, false, false);

	auditioner_io_selector->show_all ();
	auditioner_gpm->show_all ();
}

bool
OptionEditor::focus_out_event_handler (GdkEventFocus* ev, void (OptionEditor::*pmf)()) 
{
	(this->*pmf)();
	return false;
}

void
OptionEditor::setup_misc_options()
{
	Gtk::Table* table = manage (new Table (4, 2));	
	table->set_homogeneous (true);

	misc_packer.set_border_width (8);
	misc_packer.set_spacing (3);
	misc_packer.pack_start (*table, true, true);

	table->attach (hw_monitor_button, 0, 1, 0, 1, Gtk::FILL, FILL, 8, 0);
	table->attach (sw_monitor_button, 0, 1, 1, 2, Gtk::FILL, FILL, 8, 0);
	table->attach (plugins_stop_button, 0, 1, 2, 3, Gtk::FILL, FILL, 8, 0);
	table->attach (plugins_on_rec_button, 0, 1, 3, 4, Gtk::FILL, FILL, 8, 0);
	table->attach (verify_remove_last_capture_button, 0, 1, 4, 5, Gtk::FILL, FILL, 8, 0);

	table->attach (stop_rec_on_xrun_button, 1, 2, 0, 1, Gtk::FILL, FILL, 8, 0);
	table->attach (stop_at_end_button, 1, 2, 1, 2, Gtk::FILL, FILL, 8, 0);
	table->attach (debug_keyboard_button, 1, 2, 2, 3, Gtk::FILL, FILL, 8, 0);
	table->attach (speed_quieten_button, 1, 2, 3, 4, Gtk::FILL, FILL, 8, 0);

	Gtk::VBox* connect_box = manage (new VBox);
	connect_box->set_spacing (3);
	connect_box->set_border_width (8);

	auto_connect_output_button_group = auto_connect_output_master_button.get_group();
	auto_connect_output_manual_button.set_group (auto_connect_output_button_group);
	auto_connect_output_physical_button.set_group (auto_connect_output_button_group);

	Gtk::HBox* useless_box = manage (new HBox);
	useless_box->pack_start (auto_connect_inputs_button, false, false);
	connect_box->pack_start (*useless_box, false, false);
	connect_box->pack_start (auto_connect_output_master_button, false, false);
	connect_box->pack_start (auto_connect_output_physical_button, false, false);
	connect_box->pack_start (auto_connect_output_manual_button, false, false);

	misc_packer.pack_start (*connect_box, false, false);
	
	hw_monitor_button.set_name ("OptionEditorToggleButton");
	sw_monitor_button.set_name ("OptionEditorToggleButton");
	plugins_stop_button.set_name ("OptionEditorToggleButton");
	plugins_on_rec_button.set_name ("OptionEditorToggleButton");
	verify_remove_last_capture_button.set_name ("OptionEditorToggleButton");
	auto_connect_inputs_button.set_name ("OptionEditorToggleButton");
	auto_connect_output_physical_button.set_name ("OptionEditorToggleButton");
	auto_connect_output_master_button.set_name ("OptionEditorToggleButton");
	auto_connect_output_manual_button.set_name ("OptionEditorToggleButton");
	stop_rec_on_xrun_button.set_name ("OptionEditorToggleButton");
	stop_at_end_button.set_name ("OptionEditorToggleButton");
	debug_keyboard_button.set_name ("OptionEditorToggleButton");
	speed_quieten_button.set_name ("OptionEditorToggleButton");

	hw_monitor_button.set_active (Config->get_use_hardware_monitoring());
	sw_monitor_button.set_active (!Config->get_no_sw_monitoring());
	plugins_stop_button.set_active (Config->get_plugins_stop_with_transport());
	stop_rec_on_xrun_button.set_active (Config->get_stop_recording_on_xrun());
	stop_at_end_button.set_active (Config->get_stop_at_session_end());
	debug_keyboard_button.set_active (false);
	speed_quieten_button.set_active (Config->get_quieten_at_speed() != 1.0f);

	hw_monitor_button.signal_clicked().connect (mem_fun(*this, &OptionEditor::hw_monitor_clicked));
	sw_monitor_button.signal_clicked().connect (mem_fun(*this, &OptionEditor::sw_monitor_clicked));
	plugins_stop_button.signal_clicked().connect (mem_fun(*this, &OptionEditor::plugins_stop_with_transport_clicked));
	plugins_on_rec_button.signal_clicked().connect (mem_fun(*this, &OptionEditor::plugins_on_while_recording_clicked));
	verify_remove_last_capture_button.signal_clicked().connect (mem_fun(*this, &OptionEditor::verify_remove_last_capture_clicked));
	auto_connect_inputs_button.signal_clicked().connect (mem_fun(*this, &OptionEditor::auto_connect_inputs_clicked));
	auto_connect_output_physical_button.signal_clicked().connect (mem_fun(*this, &OptionEditor::auto_connect_output_physical_clicked));
	auto_connect_output_master_button.signal_clicked().connect (mem_fun(*this, &OptionEditor::auto_connect_output_master_clicked));
	auto_connect_output_manual_button.signal_clicked().connect (mem_fun(*this, &OptionEditor::auto_connect_output_manual_clicked));
	stop_rec_on_xrun_button.signal_clicked().connect (mem_fun(*this, &OptionEditor::stop_rec_on_xrun_clicked));
	stop_at_end_button.signal_clicked().connect (mem_fun(*this, &OptionEditor::stop_at_end_clicked));
	debug_keyboard_button.signal_clicked().connect (mem_fun(*this, &OptionEditor::debug_keyboard_clicked));
	speed_quieten_button.signal_clicked().connect (mem_fun(*this, &OptionEditor::speed_quieten_clicked));
}

void
OptionEditor::speed_quieten_clicked ()
{
	if (speed_quieten_button.get_active()) {
		Config->set_quieten_at_speed (0.251189); // -12dB reduction for ffwd or rewind
	} else {
		Config->set_quieten_at_speed (1.0); /* no change */
	}
}

void
OptionEditor::debug_keyboard_clicked ()
{
	extern bool debug_keyboard;
	debug_keyboard = debug_keyboard_button.get_active ();
}

void
OptionEditor::auto_connect_inputs_clicked ()
{
	if (session) {
		session->set_input_auto_connect (auto_connect_inputs_button.get_active());
	}
}

void
OptionEditor::auto_connect_output_master_clicked ()
{
	if (session) {
		if (auto_connect_output_master_button.get_active()) {
			session->set_output_auto_connect (Session::AutoConnectMaster);
		} 
	}
}

void
OptionEditor::auto_connect_output_physical_clicked ()
{
	if (session) {
		if (auto_connect_output_physical_button.get_active()) {
			session->set_output_auto_connect (Session::AutoConnectPhysical);
		} 
	}
}

void
OptionEditor::auto_connect_output_manual_clicked ()
{
	if (session) {
		if (auto_connect_output_manual_button.get_active()) {
			session->set_output_auto_connect (Session::AutoConnectOption (0));
		} 
	}
}

void
OptionEditor::hw_monitor_clicked ()
{
	Config->set_use_hardware_monitoring (hw_monitor_button.get_active());
}

void
OptionEditor::sw_monitor_clicked ()
{
	Config->set_no_sw_monitoring (!sw_monitor_button.get_active());
}

void
OptionEditor::plugins_stop_with_transport_clicked ()
{
	Config->set_plugins_stop_with_transport (plugins_stop_button.get_active());
}

void
OptionEditor::plugins_on_while_recording_clicked ()
{
	if (session) {
		session->set_recording_plugins (plugins_on_rec_button.get_active());
	}
}

void
OptionEditor::verify_remove_last_capture_clicked ()
{
	Config->set_verify_remove_last_capture(verify_remove_last_capture_button.get_active());
}

void
OptionEditor::stop_rec_on_xrun_clicked ()
{
	Config->set_stop_recording_on_xrun (stop_rec_on_xrun_button.get_active());
}

void
OptionEditor::stop_at_end_clicked ()
{
	Config->set_stop_at_session_end (stop_at_end_button.get_active());
}
						  
static const struct {
    const char *name;
    guint   modifier;
} modifiers[] = {
	{ "Shift", GDK_SHIFT_MASK },
	{ "Control", GDK_CONTROL_MASK },
	{ "Alt (Mod1)", GDK_MOD1_MASK },
	{ "Control-Shift", GDK_CONTROL_MASK|GDK_SHIFT_MASK },
	{ "Control-Alt", GDK_CONTROL_MASK|GDK_MOD1_MASK },
	{ "Shift-Alt", GDK_SHIFT_MASK|GDK_MOD1_MASK },
	{ "Control-Shift-Alt", GDK_CONTROL_MASK|GDK_SHIFT_MASK|GDK_MOD1_MASK },
	{ "Mod2", GDK_MOD2_MASK },
	{ "Mod3", GDK_MOD3_MASK },
	{ "Mod4", GDK_MOD4_MASK },
	{ "Mod5", GDK_MOD5_MASK },
	{ 0, 0 }
};

void
OptionEditor::setup_keyboard_options ()
{
	vector<string> dumb;
	Label* label;

	keyboard_mouse_table.set_border_width (12);
	keyboard_mouse_table.set_row_spacings (5);
	keyboard_mouse_table.set_col_spacings (5);

	/* internationalize and prepare for use with combos */

	for (int i = 0; modifiers[i].name; ++i) {
		dumb.push_back (_(modifiers[i].name));
	}

	set_popdown_strings (edit_modifier_combo, dumb);
	edit_modifier_combo.signal_changed().connect (mem_fun(*this, &OptionEditor::edit_modifier_chosen));

	for (int x = 0; modifiers[x].name; ++x) {
		if (modifiers[x].modifier == Keyboard::edit_modifier ()) {
			edit_modifier_combo.set_active_text (_(modifiers[x].name));
			break;
		}
	}

	label = manage (new Label (_("Edit using")));
	label->set_name ("OptionsLabel");
	label->set_alignment (1.0, 0.5);
		
	keyboard_mouse_table.attach (*label, 0, 1, 0, 1, Gtk::FILL|Gtk::EXPAND, FILL);
	keyboard_mouse_table.attach (edit_modifier_combo, 1, 2, 0, 1, Gtk::FILL|Gtk::EXPAND, FILL);

	label = manage (new Label (_("+ button")));
	label->set_name ("OptionsLabel");
	
	keyboard_mouse_table.attach (*label, 3, 4, 0, 1, Gtk::FILL|Gtk::EXPAND, FILL);
	keyboard_mouse_table.attach (edit_button_spin, 4, 5, 0, 1, Gtk::FILL|Gtk::EXPAND, FILL);

	edit_button_spin.set_name ("OptionsEntry");
	edit_button_adjustment.set_value (Keyboard::edit_button());
	edit_button_adjustment.signal_value_changed().connect (mem_fun(*this, &OptionEditor::edit_button_changed));

	set_popdown_strings (delete_modifier_combo, dumb);
	delete_modifier_combo.signal_changed().connect (mem_fun(*this, &OptionEditor::delete_modifier_chosen));

	for (int x = 0; modifiers[x].name; ++x) {
		if (modifiers[x].modifier == Keyboard::delete_modifier ()) {
			delete_modifier_combo.set_active_text (_(modifiers[x].name));
			break;
		}
	}

	label = manage (new Label (_("Delete using")));
	label->set_name ("OptionsLabel");
	label->set_alignment (1.0, 0.5);
		
	keyboard_mouse_table.attach (*label, 0, 1, 1, 2, Gtk::FILL|Gtk::EXPAND, FILL);
	keyboard_mouse_table.attach (delete_modifier_combo, 1, 2, 1, 2, Gtk::FILL|Gtk::EXPAND, FILL);

	label = manage (new Label (_("+ button")));
	label->set_name ("OptionsLabel");

	keyboard_mouse_table.attach (*label, 3, 4, 1, 2, Gtk::FILL|Gtk::EXPAND, FILL);
	keyboard_mouse_table.attach (delete_button_spin, 4, 5, 1, 2, Gtk::FILL|Gtk::EXPAND, FILL);

	delete_button_spin.set_name ("OptionsEntry");
	delete_button_adjustment.set_value (Keyboard::delete_button());
	delete_button_adjustment.signal_value_changed().connect (mem_fun(*this, &OptionEditor::delete_button_changed));

	set_popdown_strings (snap_modifier_combo, dumb);
	snap_modifier_combo.signal_changed().connect (mem_fun(*this, &OptionEditor::snap_modifier_chosen));
	
	for (int x = 0; modifiers[x].name; ++x) {
		if (modifiers[x].modifier == (guint) Keyboard::snap_modifier ()) {
			snap_modifier_combo.set_active_text (_(modifiers[x].name));
			break;
		}
	}

	label = manage (new Label (_("Ignore snap using")));
	label->set_name ("OptionsLabel");
	label->set_alignment (1.0, 0.5);
	
	keyboard_mouse_table.attach (*label, 0, 1, 2, 3, Gtk::FILL|Gtk::EXPAND, FILL);
	keyboard_mouse_table.attach (snap_modifier_combo, 1, 2, 2, 3, Gtk::FILL|Gtk::EXPAND, FILL);
}

void
OptionEditor::edit_modifier_chosen ()
{
	string txt;
	
	txt = edit_modifier_combo.get_active_text();

	for (int i = 0; modifiers[i].name; ++i) {
		if (txt == _(modifiers[i].name)) {
			Keyboard::set_edit_modifier (modifiers[i].modifier);
			break;
		}
	}
}

void
OptionEditor::delete_modifier_chosen ()
{
	string txt;
	
	txt = delete_modifier_combo.get_active_text();

	for (int i = 0; modifiers[i].name; ++i) {
		if (txt == _(modifiers[i].name)) {
			Keyboard::set_delete_modifier (modifiers[i].modifier);
			break;
		}
	}
}

void
OptionEditor::snap_modifier_chosen ()
{
	string txt;
	
	txt = snap_modifier_combo.get_active_text();

	for (int i = 0; modifiers[i].name; ++i) {
		if (txt == _(modifiers[i].name)) {
			Keyboard::set_snap_modifier (modifiers[i].modifier);
			break;
		}
	}
}

void
OptionEditor::delete_button_changed ()
{
	Keyboard::set_delete_button ((guint) delete_button_adjustment.get_value());
}

void
OptionEditor::edit_button_changed ()
{
	Keyboard::set_edit_button ((guint) edit_button_adjustment.get_value());
}

void
OptionEditor::fixup_combo_size (Gtk::ComboBoxText& combo, vector<string>& strings)
{
	/* find the widest string */

	string::size_type maxlen = 0;
	string maxstring;

	for (vector<string>::iterator i = strings.begin(); i != strings.end(); ++i) {
		string::size_type l;

		if ((l = (*i).length()) > maxlen) {
			maxlen = l;
			maxstring = *i;
		}
	}

	/* try to include ascenders and descenders */

	if (maxstring.length() > 2) {
		maxstring[0] = 'g';
		maxstring[1] = 'l';
	}

	const guint32 FUDGE = 10; // Combo's are stupid - they steal space from the entry for the button

	set_size_request_to_display_given_text (combo, maxstring.c_str(), 10 + FUDGE, 10);
}

void
OptionEditor::map_some_session_state (CheckButton& button, bool (Session::*get)() const)
{
	if (session) {
		button.set_active ((session->*get)());
	}
}

