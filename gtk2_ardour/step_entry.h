/*
    Copyright (C) 2010 Paul Davis

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

#ifndef __gtk2_ardour_step_entry_h__
#define __gtk2_ardour_step_entry_h__

#include <gtkmm/togglebutton.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/box.h>
#include <gtkmm/adjustment.h>
#include <gtkmm2ext/bindings.h>

#include "ardour_dialog.h"
#include "gtk_pianokeyboard.h"

class MidiTimeAxisView;

class StepEntry : public ArdourDialog
{
  public:
        StepEntry (MidiTimeAxisView&);
        ~StepEntry ();

        void note_off_event_handler (int note);
        void rest_event_handler ();

        Evoral::MusicalTime note_length() const;
        uint8_t note_velocity() const;
        uint8_t note_channel() const;

  private:
        Gtk::VBox packer;
        Gtk::HBox upper_box;
        Gtk::HBox note_length_box;
        Gtk::HBox note_velocity_box;

        Gtk::ToggleButton chord_button;
        Gtk::ToggleButton triplet_button;
        Gtk::ToggleButton dot_button;
        Gtk::ToggleButton restart_button;

        Gtk::VBox   resync_box;
        Gtk::Button beat_resync_button;
        Gtk::Button bar_resync_button;

        Gtk::Button sustain_button;
        Gtk::Button rest_button;
        Gtk::Button grid_rest_button;
        Gtk::VBox   rest_box;

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

        PianoKeyboard* _piano;
        Gtk::Widget* piano;
        MidiTimeAxisView* _mtv;

        void rest_click ();
        void grid_rest_click ();
        void sustain_click ();
        void chord_toggled ();
        void triplet_toggled ();
        void beat_resync_click ();
        void bar_resync_click ();

        bool piano_enter_notify_event (GdkEventCrossing *ev);
        bool on_key_release_event (GdkEventKey*);
        bool on_key_press_event (GdkEventKey*);

        void on_show ();

        /* actions */

        void register_actions ();
        Gtkmm2ext::ActionMap myactions;

        void insert_a ();
        void insert_asharp ();
        void insert_b ();
        void insert_bsharp ();
        void insert_c ();
        void insert_csharp ();
        void insert_d ();
        void insert_dsharp ();
        void insert_e ();
        void insert_f ();
        void insert_fsharp ();
        void insert_g ();
        
        void note_length_whole ();
        void note_length_half ();
        void note_length_quarter ();
        void note_length_eighth ();
        void note_length_sixteenth ();
        void note_length_thirtysecond ();
        void note_length_sixtyfourth ();

        void note_velocity_ppp ();
        void note_velocity_pp ();
        void note_velocity_p ();
        void note_velocity_mp ();
        void note_velocity_mf ();
        void note_velocity_f ();
        void note_velocity_ff ();
        void note_velocity_fff ();

        void load_bindings ();
        Gtkmm2ext::Bindings  bindings;
};

#endif /* __gtk2_ardour_step_entry_h__ */
