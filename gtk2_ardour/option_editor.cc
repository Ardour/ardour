/*
    Copyright (C) 2001-2006 Paul Davis 

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
#include <pango/pangoft2.h> // for fontmap resolution control for GnomeCanvas
#include <pango/pangocairo.h> // for fontmap resolution control for GnomeCanvas

#include <pbd/whitespace.h>

#include <ardour/ardour.h>
#include <ardour/session.h>
#include <ardour/audioengine.h>
#include <ardour/configuration.h>
#include <ardour/auditioner.h>
#include <ardour/sndfilesource.h>
#include <ardour/crossfade.h>
#include <midi++/manager.h>
#include <midi++/factory.h>
#include <gtkmm2ext/stop_signal.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/window_title.h>

#include "public_editor.h"
#include "keyboard.h"
#include "mixer_ui.h"
#include "ardour_ui.h"
#include "io_selector.h"
#include "gain_meter.h"
#include "sfdb_ui.h"
#include "utils.h"
#include "editing.h"
#include "option_editor.h"
#include "midi_port_dialog.h"
#include "gui_thread.h"
#include "utils.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Editing;
using namespace Gtkmm2ext;
using namespace std;

static vector<string> positional_sync_strings;

OptionEditor::OptionEditor (ARDOUR_UI& uip, PublicEditor& ed, Mixer_UI& mixui)
	: ArdourDialog ("options editor", false),
	  ui (uip),
	  editor (ed),
	  mixer (mixui),

	  /* Paths */
	  path_table (11, 2),

	  /* misc */

	  short_xfade_adjustment (0, 1.0, 500.0, 5.0, 100.0),
	  short_xfade_slider (short_xfade_adjustment),
	  destructo_xfade_adjustment (1.0, 1.0, 500.0, 1.0, 100.0),
	  destructo_xfade_slider (destructo_xfade_adjustment),
	  history_depth (20, -1, 100, 1.0, 10.0),
	  saved_history_depth (20, 0, 100, 1.0, 10.0),
	  history_depth_spinner (history_depth),
	  saved_history_depth_spinner (saved_history_depth),
	  limit_history_button (_("Limit undo history")),
	  save_history_button (_("Save undo history")),

	  /* Sync */

	  smpte_offset_clock (X_("smpteoffset"), false, X_("SMPTEOffsetClock"), true, true),
	  smpte_offset_negative_button (_("SMPTE offset is negative")),
	  synced_timecode_button (_("Timecode source is sample-clock synced")),

	  /* MIDI */

	  midi_port_table (4, 11),
	  mmc_receive_device_id_adjustment (0.0, 0.0, (double) 0x7f, 1.0, 16.0),
	  mmc_receive_device_id_spinner (mmc_receive_device_id_adjustment),
	  mmc_send_device_id_adjustment (0.0, 0.0, (double) 0x7f, 1.0, 16.0),
	  mmc_send_device_id_spinner (mmc_send_device_id_adjustment),
	  add_midi_port_button (_("Add new MIDI port")),

	  /* Click */

	  click_table (2, 3),
	  click_browse_button (_("Browse")),
	  click_emphasis_browse_button (_("Browse")),

	  /* kbd/mouse */

	  keyboard_mouse_table (4, 4),
	  delete_button_adjustment (3, 1, 5),
	  delete_button_spin (delete_button_adjustment),
	  edit_button_adjustment (3, 1, 5),
	  edit_button_spin (edit_button_adjustment)
	  
{
	using namespace Notebook_Helpers;

	click_io_selector = 0;
	auditioner_io_selector = 0;
	session = 0;
	
	WindowTitle title(Glib::get_application_name());
	title += _("Preferences");
	set_title(title.get_string());

	set_default_size (300, 300);
	set_wmclass (X_("ardour_preferences"), "Ardour");

	set_name ("Preferences");
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);
	
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
	setup_misc_options ();
	setup_keyboard_options ();
	setup_auditioner_editor ();

	notebook.pages().push_back (TabElem (sync_packer, _("Sync")));
	notebook.pages().push_back (TabElem (path_table, _("Paths/Files")));
	notebook.pages().push_back (TabElem (keyboard_mouse_table, _("Kbd/Mouse")));
	notebook.pages().push_back (TabElem (click_packer, _("Click")));
	notebook.pages().push_back (TabElem (audition_packer, _("Audition")));
	notebook.pages().push_back (TabElem (misc_packer, _("Misc")));

	setup_midi_options ();
	notebook.pages().push_back (TabElem (midi_packer, _("MIDI")));

	set_session (0);
	show_all_children();

	Config->map_parameters (mem_fun (*this, &OptionEditor::parameter_changed));
	Config->ParameterChanged.connect (mem_fun (*this, &OptionEditor::parameter_changed));
}

void
OptionEditor::set_session (Session *s)
{
	clear_click_editor ();
	clear_auditioner_editor ();

	click_path_entry.set_text ("");
	click_emphasis_path_entry.set_text ("");
	session_raid_entry.set_text ("");

	click_path_entry.set_sensitive (false);
	click_emphasis_path_entry.set_sensitive (false);
	session_raid_entry.set_sensitive (false);

	short_xfade_slider.set_sensitive (false);
	smpte_offset_negative_button.set_sensitive (false);

	smpte_offset_clock.set_session (s);

	if ((session = s) == 0) {
		return;
	}

	click_path_entry.set_sensitive (true);
	click_emphasis_path_entry.set_sensitive (true);
	session_raid_entry.set_sensitive (true);
	short_xfade_slider.set_sensitive (true);
	smpte_offset_negative_button.set_sensitive (true);

	smpte_offset_clock.set_session (s);
	smpte_offset_clock.set (s->smpte_offset (), true);

	smpte_offset_negative_button.set_active (session->smpte_offset_negative());

	redisplay_midi_ports ();

	setup_click_editor ();
	connect_audition_editor ();

	short_xfade_adjustment.set_value ((Crossfade::short_xfade_length() / (float) session->frame_rate()) * 1000.0);

	add_session_paths ();
}

OptionEditor::~OptionEditor ()
{
}

void
OptionEditor::setup_path_options()
{
	Gtk::Label* label;

	path_table.set_homogeneous (false);
	path_table.set_border_width (12);
	path_table.set_row_spacings (5);

	session_raid_entry.set_name ("OptionsEntry");

	session_raid_entry.signal_activate().connect (mem_fun(*this, &OptionEditor::raid_path_changed));

	label = manage(new Label(_("session RAID path")));
	label->set_name ("OptionsLabel");
	path_table.attach (*label, 0, 1, 0, 1, FILL|EXPAND, FILL);
	path_table.attach (session_raid_entry, 1, 3, 0, 1, Gtk::FILL|Gtk::EXPAND, FILL);

	path_table.show_all();
}

void
OptionEditor::add_session_paths ()
{
	click_path_entry.set_sensitive (true);
	click_emphasis_path_entry.set_sensitive (true);
	session_raid_entry.set_sensitive (true);

	if (Config->get_click_sound().empty()) {
		click_path_entry.set_text (_("internal"));
	} else {
		click_path_entry.set_text (Config->get_click_sound());
	}

	if (Config->get_click_emphasis_sound().empty()) {
		click_emphasis_path_entry.set_text (_("internal"));
	} else {
		click_emphasis_path_entry.set_text (Config->get_click_emphasis_sound());
	}

	session_raid_entry.set_text(session->raid_path());
}

static void
font_scale_changed (Gtk::Adjustment* adj)
{
	Config->set_font_scale((long)floor (adj->get_value() * 1024));
	reset_dpi();
}

void
OptionEditor::setup_misc_options ()
{
	Gtk::HBox* hbox;
	Label* label;

#ifndef GTKOSX
	/* font scaling does nothing with GDK/Quartz */

	Gtk::Adjustment* dpi_adj = new Gtk::Adjustment ((double)Config->get_font_scale() / 1024, 50, 250, 1, 10);
	Gtk::HScale * dpi_range = new Gtk::HScale (*dpi_adj);

	label = manage (new Label (_("Font Scaling")));
	label->set_name ("OptionsLabel");

	dpi_range->set_update_policy (Gtk::UPDATE_DISCONTINUOUS);
	dpi_adj->signal_value_changed().connect (bind (sigc::ptr_fun (font_scale_changed), dpi_adj));

	hbox = manage (new HBox);
	hbox->set_border_width (5);
	hbox->set_spacing (10);
	hbox->pack_start (*label, false, false);
	hbox->pack_start (*dpi_range, true, true);
	misc_packer.pack_start (*hbox, false, false);
#endif

	label = manage (new Label (_("Short crossfade length (msecs)")));
	label->set_name ("OptionsLabel");
	
	hbox = manage (new HBox);
	hbox->set_border_width (5);
	hbox->set_spacing (10);
	hbox->pack_start (*label, false, false);
	hbox->pack_start (short_xfade_slider, true, true);
	misc_packer.pack_start (*hbox, false, false);

	short_xfade_adjustment.signal_value_changed().connect (mem_fun(*this, &OptionEditor::short_xfade_adjustment_changed));

	label = manage (new Label (_("Destructive crossfade length (msecs)")));
	label->set_name ("OptionsLabel");
	
	hbox = manage (new HBox);
	hbox->set_border_width (5);
	hbox->set_spacing (10);
	hbox->pack_start (*label, false, false);
	hbox->pack_start (destructo_xfade_slider, true, true);
	misc_packer.pack_start (*hbox, false, false);
	

	destructo_xfade_adjustment.signal_value_changed().connect (mem_fun(*this, &OptionEditor::destructo_xfade_adjustment_changed));

	hbox = manage (new HBox);
	hbox->set_border_width (5);
	hbox->set_spacing (10);
	hbox->pack_start (limit_history_button, false, false);
	misc_packer.pack_start (*hbox, false, false);

	label = manage (new Label (_("History depth (commands)")));
	label->set_name ("OptionsLabel");

	hbox = manage (new HBox);
	hbox->set_border_width (5);
	hbox->set_spacing (10);
	hbox->pack_start (*label, false, false);
	hbox->pack_start (history_depth_spinner, false, false);
	misc_packer.pack_start (*hbox, false, false);

	history_depth.signal_value_changed().connect (mem_fun (*this, &OptionEditor::history_depth_changed));
	saved_history_depth.signal_value_changed().connect (mem_fun (*this, &OptionEditor::saved_history_depth_changed));
	save_history_button.signal_toggled().connect (mem_fun (*this, &OptionEditor::save_history_toggled));
	limit_history_button.signal_toggled().connect (mem_fun (*this, &OptionEditor::limit_history_toggled));

	hbox = manage (new HBox);
	hbox->set_border_width (5);
	hbox->set_spacing (10);
	hbox->pack_start (save_history_button, false, false);
	misc_packer.pack_start (*hbox, false, false);

	label = manage (new Label (_("Saved history depth (commands)")));
	label->set_name ("OptionsLabel");

	hbox = manage (new HBox);
	hbox->set_border_width (5);
	hbox->set_spacing (10);
	hbox->pack_start (*label, false, false);
	hbox->pack_start (saved_history_depth_spinner, false, false);
	misc_packer.pack_start (*hbox, false, false);
	
	short_xfade_slider.set_update_policy (UPDATE_DISCONTINUOUS);
	destructo_xfade_slider.set_update_policy (UPDATE_DISCONTINUOUS);

	destructo_xfade_adjustment.set_value (Config->get_destructive_xfade_msecs());

	misc_packer.show_all ();
}

void
OptionEditor::limit_history_toggled ()
{
	bool x = limit_history_button.get_active();
	
	if (!x) {
		Config->set_history_depth (0);
		history_depth_spinner.set_sensitive (false);
	} else {
		if (Config->get_history_depth() == 0) {
			/* get back to a sane default */
			Config->set_history_depth (20);
		}
		history_depth_spinner.set_sensitive (true);
	}
}

void
OptionEditor::save_history_toggled ()
{
	bool x = save_history_button.get_active();

	if (x != Config->get_save_history()) {
		Config->set_save_history (x);
		saved_history_depth_spinner.set_sensitive (x);
	}
}

void
OptionEditor::history_depth_changed()
{
	Config->set_history_depth ((int32_t) floor (history_depth.get_value()));
}

void
OptionEditor::saved_history_depth_changed()
{
	Config->set_saved_history_depth ((int32_t) floor (saved_history_depth.get_value()));
}

void
OptionEditor::short_xfade_adjustment_changed ()
{
	if (session) {
		float val = short_xfade_adjustment.get_value();
		
		/* val is in msecs */
		
		Crossfade::set_short_xfade_length ((nframes_t) floor (session->frame_rate() * (val / 1000.0)));
	}
}

void
OptionEditor::destructo_xfade_adjustment_changed ()
{
	float val = destructo_xfade_adjustment.get_value();

	/* val is in msecs */

	
	Config->set_destructive_xfade_msecs ((uint32_t) floor (val));

	if (session) {
		SndFileSource::setup_standard_crossfades (session->frame_rate());
	} 
}

void
OptionEditor::setup_sync_options ()
{
	HBox* hbox;
	vector<string> dumb;

	smpte_offset_clock.set_mode (AudioClock::SMPTE);
	smpte_offset_clock.ValueChanged.connect (mem_fun(*this, &OptionEditor::smpte_offset_chosen));
	
	smpte_offset_negative_button.set_name ("OptionEditorToggleButton");

	smpte_offset_negative_button.unset_flags (Gtk::CAN_FOCUS);

	Label *smpte_offset_label = manage (new Label (_("SMPTE Offset")));
	smpte_offset_label->set_name("OptionsLabel");
	
	hbox = manage (new HBox);
	hbox->set_border_width (5);
	hbox->set_spacing (10);
	hbox->pack_start (*smpte_offset_label, false, false);
	hbox->pack_start (smpte_offset_clock, false, false);
	hbox->pack_start (smpte_offset_negative_button, false, false);

	sync_packer.pack_start (*hbox, false, false);
	sync_packer.pack_start (synced_timecode_button, false, false);

	smpte_offset_negative_button.signal_clicked().connect (mem_fun(*this, &OptionEditor::smpte_offset_negative_clicked));
	synced_timecode_button.signal_toggled().connect (mem_fun(*this, &OptionEditor::synced_timecode_toggled));
}

void
OptionEditor::smpte_offset_negative_clicked ()
{
	if (session) {
		session->set_smpte_offset_negative (smpte_offset_negative_button.get_active());
	}
}

void
OptionEditor::synced_timecode_toggled ()
{
	bool x;

	if ((x = synced_timecode_button.get_active()) != Config->get_timecode_source_is_synced()) {
		Config->set_timecode_source_is_synced (x);
		Config->save_state();
	}
}

void
OptionEditor::smpte_offset_chosen()
{
	if (session) {
		nframes_t frames = smpte_offset_clock.current_duration();
		session->set_smpte_offset (frames);
	}
}


void
OptionEditor::setup_midi_options ()
{
	HBox* hbox;
	Label* label;

	midi_port_table.set_row_spacings (6);
	midi_port_table.set_col_spacings (10);

	redisplay_midi_ports ();

	mmc_receive_device_id_adjustment.set_value (Config->get_mmc_receive_device_id());
	mmc_send_device_id_adjustment.set_value (Config->get_mmc_send_device_id());

	mmc_receive_device_id_adjustment.signal_value_changed().connect (mem_fun (*this, &OptionEditor::mmc_receive_device_id_adjusted));
	mmc_send_device_id_adjustment.signal_value_changed().connect (mem_fun (*this, &OptionEditor::mmc_send_device_id_adjusted));

	hbox = manage (new HBox);
	hbox->set_border_width (6);
	hbox->pack_start (midi_port_table, true, false);

	midi_packer.pack_start (*hbox, false, false);
	midi_packer.pack_start (add_midi_port_button, false, false);

	hbox = manage (new HBox);
	hbox->set_border_width (6);
	hbox->set_spacing (6);
	label = (manage (new Label (_("Inbound MMC Device ID")))); 
	hbox->pack_start (mmc_receive_device_id_spinner, false, false);
	hbox->pack_start (*label, false, false);
	midi_packer.pack_start (*hbox, false, false); 

	mmc_receive_device_id_spinner.set_value(Config->get_mmc_receive_device_id ());

	hbox = manage (new HBox);
	hbox->set_border_width (6);
	hbox->set_spacing (6);
	label = (manage (new Label (_("Outbound MMC Device ID")))); 
	hbox->pack_start (mmc_send_device_id_spinner, false, false);
	hbox->pack_start (*label, false, false);
	midi_packer.pack_start (*hbox, false, false);

	mmc_send_device_id_spinner.set_value(Config->get_mmc_send_device_id ());

	add_midi_port_button.signal_clicked().connect (mem_fun (*this, &OptionEditor::add_midi_port));
}

void
OptionEditor::redisplay_midi_ports ()
{
	MIDI::Manager::PortMap::const_iterator i;
	const MIDI::Manager::PortMap& ports = MIDI::Manager::instance()->get_midi_ports();
	int n;

	/* remove all existing widgets */

	// XXX broken in gtkmm 2.10
	// midi_port_table.clear ();

	for (vector<Widget*>::iterator w = midi_port_table_widgets.begin(); w != midi_port_table_widgets.end(); ++w) {
		midi_port_table.remove (**w);
	}

	midi_port_table_widgets.clear ();

	midi_port_table.resize (ports.size() + 4, 11);

	Gtk::Label* label;

	label = (manage (new Label (_("Port")))); 
	label->show ();
	midi_port_table_widgets.push_back (label);
	midi_port_table.attach (*label, 0, 1, 0, 1);
	label = (manage (new Label (_("Offline")))); 
	label->show ();
	midi_port_table_widgets.push_back (label);
	midi_port_table.attach (*label, 1, 2, 0, 1);
	label = (manage (new Label (_("Trace\nInput")))); 
	label->show ();
	midi_port_table_widgets.push_back (label);
	midi_port_table.attach (*label, 2, 3, 0, 1);
	label = (manage (new Label (_("Trace\nOutput")))); 
	label->show ();
	midi_port_table_widgets.push_back (label);
	midi_port_table.attach (*label, 3, 4, 0, 1);
	label = (manage (new Label (_("MTC")))); 
	label->show ();
	midi_port_table_widgets.push_back (label);
	midi_port_table.attach (*label, 4, 5, 0, 1);
	label = (manage (new Label (_("MMC")))); 
	label->show ();
	midi_port_table_widgets.push_back (label);
	midi_port_table.attach (*label, 6, 7, 0, 1);
	label = (manage (new Label (_("MIDI Parameter\nControl")))); 
	label->show ();
	midi_port_table_widgets.push_back (label);
	midi_port_table.attach (*label, 8, 9, 0, 1);

	Gtk::HSeparator* hsep = (manage (new HSeparator())); 
	hsep->show ();
	midi_port_table_widgets.push_back (hsep);
	midi_port_table.attach (*hsep, 0, 9, 1, 2);
	Gtk::VSeparator* vsep = (manage (new VSeparator())); 
	vsep->show ();
	midi_port_table_widgets.push_back (vsep);
	midi_port_table.attach (*vsep, 5, 6, 0, 8);
	vsep = (manage (new VSeparator())); 
	vsep->show ();
	midi_port_table_widgets.push_back (vsep);
	midi_port_table.attach (*vsep, 7, 8, 0, 8);
	
	for (n = 0, i = ports.begin(); i != ports.end(); ++n, ++i) {

		ToggleButton* tb;
		RadioButton* rb;
		Button* bb;

		/* the remove button. create early so we can pass it to various callbacks */
		
		bb = manage (new Button (Stock::REMOVE));
		bb->set_name ("OptionEditorToggleButton");
		bb->show ();
		midi_port_table_widgets.push_back (bb);
		midi_port_table.attach (*bb, 9, 10, n+2, n+3, FILL|EXPAND, FILL);
		bb->signal_clicked().connect (bind (mem_fun(*this, &OptionEditor::remove_midi_port), i->second));
		bb->set_sensitive (port_removable (i->second));

		label = (manage (new Label (i->first))); 
		label->show ();
		midi_port_table_widgets.push_back (label);
		midi_port_table.attach (*label, 0, 1, n+2, n+3,FILL|EXPAND, FILL );
		
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

		if (i->second->input()) {
			tb->set_active (!i->second->input()->offline());
			tb->signal_toggled().connect (bind (mem_fun(*this, &OptionEditor::port_online_toggled), i->second, tb));
			i->second->input()->OfflineStatusChanged.connect (bind (mem_fun(*this, &OptionEditor::map_port_online), (*i).second, tb));
		}
		tb->show ();
		midi_port_table_widgets.push_back (tb);
		midi_port_table.attach (*tb, 1, 2, n+2, n+3, FILL|EXPAND, FILL);

		tb = manage (new ToggleButton ());
		tb->set_name ("OptionEditorToggleButton");
		tb->signal_toggled().connect (bind (mem_fun(*this, &OptionEditor::port_trace_in_toggled), (*i).second, tb));
		tb->set_size_request (10, 10);
		tb->show ();
		midi_port_table_widgets.push_back (tb);
		midi_port_table.attach (*tb, 2, 3, n+2, n+3, FILL|EXPAND, FILL);

		tb = manage (new ToggleButton ());
		tb->set_name ("OptionEditorToggleButton");
		tb->signal_toggled().connect (bind (mem_fun(*this, &OptionEditor::port_trace_out_toggled), (*i).second, tb));
		tb->set_size_request (10, 10);
		tb->show ();
		midi_port_table_widgets.push_back (tb);
		midi_port_table.attach (*tb, 3, 4, n+2, n+3, FILL|EXPAND, FILL);

		rb = manage (new RadioButton ());
		rb->set_name ("OptionEditorToggleButton");
		if (n == 0) {
			mtc_button_group = rb->get_group();
		} else {
			rb->set_group (mtc_button_group);

		}
		rb->show ();
		midi_port_table_widgets.push_back (rb);
		midi_port_table.attach (*rb, 4, 5, n+2, n+3, FILL|EXPAND, FILL);
		rb->signal_toggled().connect (bind (mem_fun(*this, &OptionEditor::mtc_port_chosen), (*i).second, rb, bb));

		if (session && i->second == session->mtc_port()) {
			rb->set_active (true);
		}
		
		rb = manage (new RadioButton ());
		rb->set_name ("OptionEditorToggleButton");
		if (n == 0) {
			mmc_button_group = rb->get_group();
		} else {
			rb->set_group (mmc_button_group);
		}
		rb->show ();
		midi_port_table_widgets.push_back (rb);
		midi_port_table.attach (*rb, 6, 7, n+2, n+3, FILL|EXPAND, FILL);
		rb->signal_toggled().connect (bind (mem_fun(*this, &OptionEditor::mmc_port_chosen), (*i).second, rb, bb));

		if (session && i->second == session->mmc_port()) {
			rb->set_active (true);
		}

		rb = manage (new RadioButton ());
		rb->set_name ("OptionEditorToggleButton");
		if (n == 0) {
			midi_button_group = rb->get_group();
		} else {
			rb->set_group (midi_button_group);
		}
		rb->show ();
		midi_port_table_widgets.push_back (rb);
		midi_port_table.attach (*rb, 8, 9, n+2, n+3, FILL|EXPAND, FILL);
		rb->signal_toggled().connect (bind (mem_fun(*this, &OptionEditor::midi_port_chosen), (*i).second, rb, bb));

		if (session && i->second == session->midi_port()) {
			rb->set_active (true);
		}

	}

	midi_port_table.show();
}

void
OptionEditor::remove_midi_port (MIDI::Port* port)
{
	MIDI::Manager::instance()->remove_port (port);
	redisplay_midi_ports ();
}

void
OptionEditor::add_midi_port ()
{
	MidiPortDialog dialog;

	dialog.set_position (WIN_POS_MOUSE);
	dialog.set_transient_for (*this);

	dialog.show ();

	int ret = dialog.run ();

	switch (ret) {
	case RESPONSE_ACCEPT:
		break;
	default:
		return;
		break;
	}

	Glib::ustring mode = dialog.port_mode_combo.get_active_text();
	std::string smod;

	if (mode == _("input")) {
		smod = X_("input");
	} else if (mode == (_("output"))) {
		smod = X_("output");
	} else {
		smod = "duplex";
	}


	XMLNode node (X_("MIDI-port"));

	node.add_property ("tag", dialog.port_name.get_text());
	node.add_property ("device", X_("ardour")); // XXX this can't be right for all types
	node.add_property ("type", MIDI::PortFactory::default_port_type());
	node.add_property ("mode", smod);

	if (MIDI::Manager::instance()->add_port (node) != 0) {
		redisplay_midi_ports ();
	}
}

bool
OptionEditor::port_removable (MIDI::Port *port)
{
	if (!session) {
		return true;
	}

	if (port == session->mtc_port() ||
	    port == session->mmc_port() ||
	    port == session->midi_port()) {
		return false;
	}
	return true;
}

void
OptionEditor::mtc_port_chosen (MIDI::Port *port, Gtk::RadioButton* rb, Gtk::Button* bb) 
{
	if (session) {
		if (rb->get_active()) {
			session->set_mtc_port (port->name());
			Config->set_mtc_port_name (port->name());
		} else {
			session->set_mtc_port ("");
		}
		bb->set_sensitive (port_removable (port));
	}
}

void
OptionEditor::mmc_port_chosen (MIDI::Port* port, Gtk::RadioButton* rb, Gtk::Button* bb)
{
	if (session) {
		if (rb->get_active()) {
			session->set_mmc_port (port->name());
			Config->set_mtc_port_name (port->name());
		} else {
			session->set_mmc_port ("");
		}
		bb->set_sensitive (port_removable (port));
	}
}

void
OptionEditor::midi_port_chosen (MIDI::Port* port, Gtk::RadioButton* rb, Gtk::Button* bb)
{
	if (session) {
		if (rb->get_active()) {
			session->set_midi_port (port->name());
			Config->set_midi_port_name (port->name());
		} else {
			session->set_midi_port ("");
		}
		bb->set_sensitive (port_removable (port));
	}
}

void
OptionEditor::port_online_toggled (MIDI::Port* port, ToggleButton* tb)
{
	bool wanted = tb->get_active();

	if (port->input()) {
		if (wanted != port->input()->offline()) {
			port->input()->set_offline (wanted);
		} 
	}
}

void
OptionEditor::map_port_online (MIDI::Port* port, ToggleButton* tb)
{
	bool bstate = tb->get_active ();
	
	if (port->input()) {
		if (bstate != port->input()->offline()) {
			if (port->input()->offline()) {
				tb->set_label (_("offline"));
				tb->set_active (false);
			} else {
				tb->set_label (_("online"));
				tb->set_active (true);
			}
		}
	}
}

void
OptionEditor::mmc_receive_device_id_adjusted ()
{
	uint8_t id = (uint8_t) mmc_receive_device_id_spinner.get_value();
	Config->set_mmc_receive_device_id (id);
}

void
OptionEditor::mmc_send_device_id_adjusted ()
{
	uint8_t id = (uint8_t) mmc_send_device_id_spinner.get_value();
	Config->set_mmc_send_device_id (id);
}

void
OptionEditor::port_trace_in_toggled (MIDI::Port* port, ToggleButton* tb)
{
	bool trace = tb->get_active();

	if (port->input()) {
		if (port->input()->tracing() != trace) {
			port->input()->trace (trace, &cerr, string (port->name()) + string (" input: "));
		}
	}
}

void
OptionEditor::port_trace_out_toggled (MIDI::Port* port, ToggleButton* tb)
{
	bool trace = tb->get_active();

	if (port->output()) {
		if (port->output()->tracing() != trace) {
			port->output()->trace (trace, &cerr, string (port->name()) + string (" output: "));
		}
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
	hide ();
	return TRUE;
}

void
OptionEditor::raid_path_changed ()
{
	if (session) {
		Config->set_raid_path (session_raid_entry.get_text());
	}
}

void
OptionEditor::click_browse_clicked ()
{
	SoundFileChooser sfdb (*this, _("Choose Click"), session);
	
	sfdb.show_all ();
	sfdb.present ();

	int result = sfdb.run ();
 
	if (result == Gtk::RESPONSE_OK) {
		click_chosen(sfdb.get_filename());
	}
}

void
OptionEditor::click_chosen (const string & path)
{
	click_path_entry.set_text (path);
	click_sound_changed ();
}

void
OptionEditor::click_emphasis_browse_clicked ()
{
	SoundFileChooser sfdb (*this, _("Choose Click Emphasis"), session);

	sfdb.show_all ();
	sfdb.present ();

	int result = sfdb.run ();

	if (result == Gtk::RESPONSE_OK) {
		click_emphasis_chosen (sfdb.get_filename());
	}
}

void
OptionEditor::click_emphasis_chosen (const string & path)
{	
	click_emphasis_path_entry.set_text (path);
	click_emphasis_sound_changed ();
}

void
OptionEditor::click_sound_changed ()
{
	if (session) {
		string path = click_path_entry.get_text();

		if (path == Config->get_click_sound()) {
			return;
		}

		strip_whitespace_edges (path);

		if (path == _("internal")) {
			Config->set_click_sound ("");
		} else {
			Config->set_click_sound (path);
		}
	}
}

void
OptionEditor::click_emphasis_sound_changed ()
{
	if (session) {
		string path = click_emphasis_path_entry.get_text();

		if (path == Config->get_click_emphasis_sound()) {
			return;
		}

		strip_whitespace_edges (path);

		if (path == _("internal")) {
			Config->set_click_emphasis_sound ("");
		} else {
			Config->set_click_emphasis_sound (path);
		}
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

static const struct {
    const char *name;
    guint   modifier;
} modifiers[] = {

#ifdef GTKOSX 

	/* Command = Meta
	   Option/Alt = Mod1
	*/

	{ "Shift", GDK_SHIFT_MASK },
	{ "Command", GDK_META_MASK },
	{ "Control", GDK_CONTROL_MASK },
	{ "Option", GDK_MOD1_MASK },
	{ "Command-Shift", GDK_MOD1_MASK|GDK_SHIFT_MASK },
	{ "Command-Option", GDK_MOD1_MASK|GDK_MOD5_MASK },
	{ "Shift-Option", GDK_SHIFT_MASK|GDK_MOD5_MASK },
	{ "Shift-Command-Option", GDK_MOD5_MASK|GDK_SHIFT_MASK|GDK_MOD1_MASK },

#else
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
#endif
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

	vector<string> strs;
	
	for (std::map<std::string,std::string>::iterator bf = Keyboard::binding_files.begin(); bf != Keyboard::binding_files.end(); ++bf) {
		strs.push_back (bf->first);
	}
	
	set_popdown_strings (keyboard_layout_selector, strs);
	keyboard_layout_selector.set_active_text (Keyboard::current_binding_name());
	keyboard_layout_selector.signal_changed().connect (mem_fun (*this, &OptionEditor::bindings_changed));

	label = manage (new Label (_("Keyboard layout")));
	label->set_name ("OptionsLabel");
	label->set_alignment (1.0, 0.5);

	keyboard_mouse_table.attach (*label, 0, 1, 3, 4, Gtk::FILL|Gtk::EXPAND, FILL);
	keyboard_mouse_table.attach (keyboard_layout_selector, 1, 2, 3, 4, Gtk::FILL|Gtk::EXPAND, FILL);
}

void
OptionEditor::bindings_changed ()
{
	string txt;
	
	txt = keyboard_layout_selector.get_active_text();

	for (std::map<string,string>::iterator i = Keyboard::binding_files.begin(); i != Keyboard::binding_files.end(); ++i) {
		if (txt == i->first) {
			if (Keyboard::load_keybindings (i->second)) {
				Keyboard::save_keybindings ();
			}
		}
	}
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
OptionEditor::parameter_changed (const char* parameter_name)
{
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &OptionEditor::parameter_changed), parameter_name));

#define PARAM_IS(x) (!strcmp (parameter_name, (x)))
	
	if (PARAM_IS ("timecode-source-is-synced")) {
		synced_timecode_button.set_active (Config->get_timecode_source_is_synced());
	} else if (PARAM_IS ("history-depth")) {
		int32_t depth = Config->get_history_depth();
		
		history_depth.set_value (depth);
		history_depth_spinner.set_sensitive (depth != 0);
		limit_history_button.set_active (depth != 0);

	} else if (PARAM_IS ("saved-history-depth")) {

		saved_history_depth.set_value (Config->get_saved_history_depth());

	} else if (PARAM_IS ("save-history")) {

		bool x = Config->get_save_history();

		save_history_button.set_active (x);
		saved_history_depth_spinner.set_sensitive (x);
	} else if (PARAM_IS ("font-scale")) {
		reset_dpi();
	}
}
