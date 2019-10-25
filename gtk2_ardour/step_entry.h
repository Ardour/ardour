/*
 * Copyright (C) 2010-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2011-2015 David Robillard <d@drobilla.net>
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

#ifndef __gtk2_ardour_step_entry_h__
#define __gtk2_ardour_step_entry_h__

#include <gtkmm/togglebutton.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/box.h>
#include <gtkmm/adjustment.h>
#include <gtkmm2ext/bindings.h>

#include "ardour_window.h"
#include "pianokeyboard.h"

class StepEditor;

/** StepEntry is a singleton class which presents a GUI to the user to allow
 * them to carry out step editing. It does not understand the details of making
 * changes to the model directly, but instead calls into a StepEditor object to
 * accomplish that.
 *
 * The StepEntry is a singleton, used over and over each time the user wants to
 * step edit; the StepEditor is owned by a MidiTimeAxisView and re-used for any
 * step editing in the MidiTrack for which the MidiTimeAxisView is a view.
 */

class StepEntry : public ArdourWindow
{
  public:
	static StepEntry& instance();

	~StepEntry ();

	void set_step_editor (StepEditor*);

	Temporal::Beats note_length();
	uint8_t note_velocity() const;
	uint8_t note_channel() const;

	int current_octave () const { return (int) floor (octave_adjustment.get_value()); }

	static void setup_actions_and_bindings ();

  private:
	static StepEntry* _instance;
	StepEntry ();

	void note_off_event_handler (int note);
	void rest_event_handler ();

	Temporal::Beats _current_note_length;
	uint8_t _current_note_velocity;

	Gtk::VBox packer;
	Gtk::HBox upper_box;
	Gtk::HBox note_length_box;
	Gtk::HBox note_velocity_box;

	Gtk::ToggleButton chord_button;
	Gtk::ToggleButton triplet_button;
	Gtk::ToggleButton dot0_button;
	Gtk::ToggleButton dot1_button;
	Gtk::ToggleButton dot2_button;
	Gtk::ToggleButton dot3_button;
	Gtk::Adjustment   dot_adjustment;
	Gtk::VBox dot_box1;
	Gtk::VBox dot_box2;
	Gtk::ToggleButton restart_button;

	Gtk::VBox   resync_box;
	Gtk::Button beat_resync_button;
	Gtk::Button bar_resync_button;
	Gtk::Button resync_button;

	Gtk::Button sustain_button;
	Gtk::Button rest_button;
	Gtk::Button grid_rest_button;
	Gtk::VBox   rest_box;

	Gtk::Button back_button;

	Gtk::RadioButton length_1_button;
	Gtk::RadioButton length_2_button;
	Gtk::RadioButton length_4_button;
	Gtk::RadioButton length_8_button;
	Gtk::RadioButton length_12_button;
	Gtk::RadioButton length_16_button;
	Gtk::RadioButton length_32_button;
	Gtk::RadioButton length_64_button;

	Gtk::RadioButton velocity_ppp_button;
	Gtk::RadioButton velocity_pp_button;
	Gtk::RadioButton velocity_p_button;
	Gtk::RadioButton velocity_mp_button;
	Gtk::RadioButton velocity_mf_button;
	Gtk::RadioButton velocity_f_button;
	Gtk::RadioButton velocity_ff_button;
	Gtk::RadioButton velocity_fff_button;

	Gtk::Adjustment channel_adjustment;
	Gtk::SpinButton channel_spinner;

	Gtk::Adjustment octave_adjustment;
	Gtk::SpinButton octave_spinner;

	Gtk::Adjustment length_divisor_adjustment;
	Gtk::SpinButton length_divisor_spinner;

	Gtk::Adjustment velocity_adjustment;
	Gtk::SpinButton velocity_spinner;

	Gtk::Adjustment bank_adjustment;
	Gtk::SpinButton bank_spinner;
	Gtk::Button     bank_button;

	Gtk::Adjustment program_adjustment;
	Gtk::SpinButton program_spinner;
	Gtk::Button     program_button;

	void length_changed ();
	void velocity_changed ();
	void velocity_value_change ();
	void length_value_change ();

	APianoKeyboard _piano;

	StepEditor*   se;

	void bank_click ();
	void program_click ();
	void beat_resync_click ();
	void bar_resync_click ();

	bool piano_enter_notify_event (GdkEventCrossing *ev);
	bool on_key_release_event (GdkEventKey*);
	bool on_key_press_event (GdkEventKey*);

	void on_show ();

	/* actions */

	void insert_note (uint8_t);
	void insert_rest ();
	void insert_grid_rest ();
	void insert_a ();
	void insert_asharp ();
	void insert_b ();
	void insert_c ();
	void insert_csharp ();
	void insert_d ();
	void insert_dsharp ();
	void insert_e ();
	void insert_f ();
	void insert_fsharp ();
	void insert_g ();
	void insert_gsharp ();
	void note_length_change (GtkAction*);
	void note_velocity_change (GtkAction*);
	bool radio_button_press (GdkEventButton*);
	bool radio_button_release (GdkEventButton*, Gtk::RadioButton*, int);
	void inc_note_velocity ();
	void dec_note_velocity ();
	void next_note_velocity ();
	void prev_note_velocity ();
	void inc_note_length ();
	void dec_note_length ();
	void next_note_length ();
	void prev_note_length ();
	void next_octave ();
	void prev_octave ();
	void octave_n (int n);
	void octave_0 () { octave_n (0); }
	void octave_1 () { octave_n (1); }
	void octave_2 () { octave_n (2); }
	void octave_3 () { octave_n (3); }
	void octave_4 () { octave_n (4); }
	void octave_5 () { octave_n (5); }
	void octave_6 () { octave_n (6); }
	void octave_7 () { octave_n (7); }
	void octave_8 () { octave_n (8); }
	void octave_9 () { octave_n (9); }
	void octave_10 () { octave_n (10); }
	void dot_change (GtkAction*);
	void dot_value_change ();
	void toggle_triplet();
	void toggle_chord();
	void do_sustain ();
	void back();
	void sync_to_edit_point ();

	/* static versions of action methods, so that we can register actions without
	   having an actual StepEntry object.
	*/

	static void se_insert_rest () { if (_instance) { _instance->insert_rest (); } }
	static void se_insert_grid_rest () { if (_instance) { _instance->insert_grid_rest (); } }
	static void se_insert_a () { if (_instance) { _instance->insert_a (); } }
	static void se_insert_asharp () { if (_instance) { _instance->insert_asharp (); } }
	static void se_insert_b () { if (_instance) { _instance->insert_b (); } }
	static void se_insert_c () { if (_instance) { _instance->insert_c (); } }
	static void se_insert_csharp () { if (_instance) { _instance->insert_csharp (); } }
	static void se_insert_d () { if (_instance) { _instance->insert_d (); } }
	static void se_insert_dsharp () { if (_instance) { _instance->insert_dsharp (); } }
	static void se_insert_e () { if (_instance) { _instance->insert_e (); } }
	static void se_insert_f () { if (_instance) { _instance->insert_f (); } }
	static void se_insert_fsharp () { if (_instance) { _instance->insert_fsharp (); } }
	static void se_insert_g () { if (_instance) { _instance->insert_g (); } }
	static void se_insert_gsharp () { if (_instance) { _instance->insert_gsharp (); } }
	static void se_note_length_change (GtkAction* act) { if (_instance) { _instance->note_length_change (act); } }
	static void se_note_velocity_change (GtkAction* act) { if (_instance) { _instance->note_velocity_change (act); } }
	static bool se_radio_button_press (GdkEventButton* ev) { if (_instance) { return _instance->radio_button_press (ev); } return false; }
	static bool se_radio_button_release (GdkEventButton* ev, Gtk::RadioButton* rb, int n) { if (_instance) { return  _instance->radio_button_release (ev, rb, n); } return false; }
	static void se_inc_note_velocity () { if (_instance) { _instance->inc_note_velocity (); } }
	static void se_dec_note_velocity () { if (_instance) { _instance->dec_note_velocity (); } }
	static void se_next_note_velocity () { if (_instance) { _instance->next_note_velocity (); } }
	static void se_prev_note_velocity () { if (_instance) { _instance->prev_note_velocity (); } }
	static void se_inc_note_length () { if (_instance) { _instance->inc_note_length (); } }
	static void se_dec_note_length () { if (_instance) { _instance->dec_note_length (); } }
	static void se_next_note_length () { if (_instance) { _instance->next_note_length (); } }
	static void se_prev_note_length () { if (_instance) { _instance->prev_note_length (); } }
	static void se_next_octave () { if (_instance) { _instance->next_octave (); } }
	static void se_prev_octave () { if (_instance) { _instance->prev_octave (); } }
	static void se_octave_n (int n) { if (_instance) { _instance->octave_n (n); } }
	static void se_octave_0 () { if (_instance) { _instance->octave_0 (); } }
	static void se_octave_1 () { if (_instance) { _instance->octave_1 (); } }
	static void se_octave_2 () { if (_instance) { _instance->octave_2 (); } }
	static void se_octave_3 () { if (_instance) { _instance->octave_3 (); } }
	static void se_octave_4 () { if (_instance) { _instance->octave_4 (); } }
	static void se_octave_5 () { if (_instance) { _instance->octave_5 (); } }
	static void se_octave_6 () { if (_instance) { _instance->octave_6 (); } }
	static void se_octave_7 () { if (_instance) { _instance->octave_7 (); } }
	static void se_octave_8 () { if (_instance) { _instance->octave_8 (); } }
	static void se_octave_9 () { if (_instance) { _instance->octave_9 (); } }
	static void se_octave_10 () { if (_instance) { _instance->octave_10 (); } }
	static void se_dot_change (GtkAction* act) { if (_instance) { _instance->dot_change (act); } }
	static void se_dot_value_change () { if (_instance) { _instance->dot_value_change (); } }
	static void se_toggle_triplet() { if (_instance) { _instance->toggle_triplet (); } }
	static void se_toggle_chord() { if (_instance) { _instance->toggle_chord (); } }
	static void se_do_sustain () { if (_instance) { _instance->do_sustain (); } }
	static void se_back() { if (_instance) { _instance->back (); } }
	static void se_sync_to_edit_point () { if (_instance) { _instance->sync_to_edit_point (); } }

	static void load_bindings ();
	static Gtkmm2ext::Bindings*  bindings;
	static void register_actions ();
};

#endif /* __gtk2_ardour_step_entry_h__ */
