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

#ifndef __gtk_ardour_option_editor_h__
#define __gtk_ardour_option_editor_h__

#include <gtkmm/notebook.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/table.h>
#include <gtkmm/entry.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/scale.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/comboboxtext.h>

#include <ardour/session.h>

#include "ardour_dialog.h"
#include "editing.h"
#include "audio_clock.h"

class ARDOUR_UI;
class PublicEditor;
class Mixer_UI;
class IOSelector;
class GainMeter;
class PannerUI;

class OptionEditor : public Gtk::Dialog
{
  public:
	OptionEditor (ARDOUR_UI&, PublicEditor&, Mixer_UI&);
	~OptionEditor ();

	void set_session (ARDOUR::Session *);
	void save ();

  private:
	ARDOUR::Session *session;
	ARDOUR_UI& ui;
	PublicEditor& editor;
	Mixer_UI& mixer;

	Gtk::Notebook notebook;

	/* Generic */

	void session_control_changed (ARDOUR::Session::ControlType);
	void queue_session_control_changed (ARDOUR::Session::ControlType);
	void map_some_session_state (Gtk::CheckButton& button, bool (ARDOUR::Session::*get)() const);
	gint wm_close (GdkEventAny *);
	void just_close_win();
	bool focus_out_event_handler (GdkEventFocus*, void (OptionEditor::*pmf)());

	/* paths */

	Gtk::Table		path_table;

	Gtk::Entry              session_raid_entry;

	Gtk::ComboBoxText       native_format_combo;

	Glib::RefPtr<Gtk::ListStore> sfdb_paths;
	Gtk::TreeView sfdb_path_view;

	struct SoundFilePathColumns : public Gtk::TreeModel::ColumnRecord
	{
	  public:
	    Gtk::TreeModelColumn<std::string> paths;

		SoundFilePathColumns() { add (paths); }
	};
	SoundFilePathColumns sfdb_path_columns;

	void setup_path_options();
	void add_session_paths ();
	void remove_session_paths ();
	void native_format_chosen ();
	void raid_path_changed ();

	/* fades */

	// Gtk::Table		fade_table;

	Gtk::VBox        fade_packer;
	Gtk::CheckButton auto_xfade_button;
	Gtk::CheckButton xfade_active_button;
	Gtk::Label       layer_mode_label;
	Gtk::ComboBoxText layer_mode_combo;
	Gtk::Label       xfade_model_label;
	Gtk::ComboBoxText xfade_model_combo;
	Gtk::Adjustment  short_xfade_adjustment;
	Gtk::HScale      short_xfade_slider;

	void auto_xfade_clicked ();
	void xfade_active_clicked ();
	void layer_mode_chosen ();
	void xfade_model_chosen ();
	void setup_fade_options();
	void short_xfade_adjustment_changed ();

	/* solo */

	Gtk::VBox        solo_packer;
	Gtk::CheckButton solo_latched_button;
	Gtk::CheckButton solo_via_bus_button;

	void solo_latched_clicked();
	void solo_via_bus_clicked ();
	
	void setup_solo_options();

	/* display */

	Gtk::VBox        display_packer;
	Gtk::CheckButton show_waveforms_button;
	Gtk::CheckButton show_waveforms_recording_button;
	Gtk::CheckButton mixer_strip_width_button;
	Gtk::CheckButton show_measures_button;
	Gtk::CheckButton follow_playhead_button;
	Gtk::ComboBoxText meter_hold_combo;
	Gtk::ComboBoxText meter_falloff_combo;

	void setup_display_options();
	void show_waveforms_clicked ();
	void show_waveforms_recording_clicked ();
	void show_measures_clicked ();
	void strip_width_clicked ();
	void follow_playhead_clicked ();
	void meter_hold_chosen ();
	void meter_falloff_chosen ();
	
	void display_control_changed (Editing::DisplayControl);

	/* Sync */

	Gtk::VBox sync_packer;

	Gtk::CheckButton send_mtc_button;
	Gtk::CheckButton send_mmc_button;
	Gtk::CheckButton jack_time_master_button;
	Gtk::ComboBoxText slave_type_combo;
	Gtk::ComboBoxText smpte_fps_combo;
	AudioClock smpte_offset_clock;
	Gtk::CheckButton smpte_offset_negative_button;

	void setup_sync_options ();
	gint send_mtc_toggled (GdkEventButton*, Gtk::CheckButton*);

	void slave_type_chosen ();
	void jack_time_master_clicked ();
	void jack_transport_master_clicked ();
	void smpte_fps_chosen ();
	void smpte_offset_chosen ();
	void smpte_offset_negative_clicked ();

	/* MIDI */

	Gtk::VBox  midi_packer;
	Gtk::CheckButton midi_feedback_button;
	Gtk::CheckButton midi_control_button;
	Gtk::CheckButton mmc_control_button;

	Gtk::RadioButton::Group mtc_button_group;
	Gtk::RadioButton::Group mmc_button_group;
	Gtk::RadioButton::Group midi_button_group;

	void send_mmc_toggled (Gtk::CheckButton*);
	void mmc_control_toggled (Gtk::CheckButton*);
	void midi_control_toggled (Gtk::CheckButton*);
	void midi_feedback_toggled (Gtk::CheckButton*);

	gint port_online_toggled (GdkEventButton*,MIDI::Port*,Gtk::ToggleButton*);
	gint port_trace_in_toggled (GdkEventButton*,MIDI::Port*,Gtk::ToggleButton*);
	gint port_trace_out_toggled (GdkEventButton*,MIDI::Port*,Gtk::ToggleButton*);
	
	gint mmc_port_chosen (GdkEventButton*,MIDI::Port*,Gtk::RadioButton*);
	gint mtc_port_chosen (GdkEventButton*,MIDI::Port*,Gtk::RadioButton*);
	gint midi_port_chosen (GdkEventButton*,MIDI::Port*,Gtk::RadioButton*);

	void map_port_online (MIDI::Port*, Gtk::ToggleButton*);

	void setup_midi_options();

	enum PortIndex {
		MtcIndex = 0,
		MmcIndex = 1,
		MidiIndex = 2
	};

	std::map<MIDI::Port*,std::vector<Gtk::RadioButton*> > port_toggle_buttons;

	/* Click */

	IOSelector*   click_io_selector;
	GainMeter* click_gpm;
	PannerUI*     click_panner;
	Gtk::VBox     click_packer;
	Gtk::Table    click_table;
	Gtk::Entry    click_path_entry;
	Gtk::Entry    click_emphasis_path_entry;
	Gtk::Button   click_browse_button;
	Gtk::Button   click_emphasis_browse_button;

	void setup_click_editor ();
	void clear_click_editor ();

	void click_chosen (ARDOUR::stringcr_t paths);
	void click_emphasis_chosen (ARDOUR::stringcr_t paths);

	void click_browse_clicked ();
	void click_emphasis_browse_clicked ();
	
	void click_sound_changed ();
	void click_emphasis_sound_changed ();

	/* Auditioner */

	Gtk::VBox     audition_packer;
	Gtk::HBox     audition_hpacker;
	Gtk::Label    audition_label;
	IOSelector*   auditioner_io_selector;
	GainMeter* auditioner_gpm;
	PannerUI* auditioner_panner;

	void setup_auditioner_editor ();
	void clear_auditioner_editor ();
	void connect_audition_editor ();

	/* keyboard/mouse */

	Gtk::Table keyboard_mouse_table;
	Gtk::ComboBoxText edit_modifier_combo;
	Gtk::ComboBoxText delete_modifier_combo;
	Gtk::ComboBoxText snap_modifier_combo;
	Gtk::Adjustment delete_button_adjustment;
	Gtk::SpinButton delete_button_spin;
	Gtk::Adjustment edit_button_adjustment;
	Gtk::SpinButton edit_button_spin;

	void setup_keyboard_options ();
	void delete_modifier_chosen ();
	void edit_modifier_chosen ();
	void snap_modifier_chosen ();
	void edit_button_changed ();
	void delete_button_changed ();

	/* Miscellany */

	Gtk::VBox misc_packer;

	Gtk::CheckButton auto_connect_inputs_button;

	Gtk::RadioButton auto_connect_output_physical_button;
	Gtk::RadioButton auto_connect_output_master_button;
	Gtk::RadioButton auto_connect_output_manual_button;
	Gtk::RadioButton::Group auto_connect_output_button_group;

	Gtk::CheckButton hw_monitor_button;
	Gtk::CheckButton sw_monitor_button;
	Gtk::CheckButton plugins_stop_button;
	Gtk::CheckButton plugins_on_rec_button;
	Gtk::CheckButton verify_remove_last_capture_button;
	Gtk::CheckButton stop_rec_on_xrun_button;
	Gtk::CheckButton stop_at_end_button;
	Gtk::CheckButton debug_keyboard_button;
	Gtk::CheckButton speed_quieten_button;

	void setup_misc_options ();
	void plugins_stop_with_transport_clicked ();
	void verify_remove_last_capture_clicked ();
	void plugins_on_while_recording_clicked ();
	void auto_connect_inputs_clicked ();
	void auto_connect_output_physical_clicked ();
	void auto_connect_output_master_clicked ();
	void auto_connect_output_manual_clicked ();
	void hw_monitor_clicked ();
	void sw_monitor_clicked ();
	void stop_rec_on_xrun_clicked ();
	void stop_at_end_clicked ();
	void debug_keyboard_clicked ();
	void speed_quieten_clicked ();

	void fixup_combo_size (Gtk::ComboBoxText&, std::vector<std::string>& strings);
};

#endif /* __gtk_ardour_option_editor_h__ */


