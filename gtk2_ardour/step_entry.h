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
};

#endif /* __gtk2_ardour_step_entry_h__ */
