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

#include "midi_time_axis.h"
#include "step_entry.h"
#include "utils.h"

#include "i18n.h"

using namespace Gtk;

static void
_note_off_event_handler (GtkWidget* widget, int note, gpointer arg)
{
        ((StepEntry*)arg)->note_off_event_handler (note);
}


StepEntry::StepEntry (MidiTimeAxisView& mtv)
        : ArdourDialog (_("Step Entry Editor"))
        , triplet_button ("3")
        , sustain_button ("sustain")
        , rest_button ("rest")
        , channel_adjustment (0, 15, 0, 1, 4)
        , channel_spinner (channel_adjustment)
        , _piano (0)
        , piano (0)
        , _mtv (&mtv)
{
	RadioButtonGroup length_group = length_1_button.get_group();
	length_2_button.set_group (length_group);
	length_4_button.set_group (length_group);
	length_8_button.set_group (length_group);
	length_12_button.set_group (length_group);
	length_16_button.set_group (length_group);
	length_32_button.set_group (length_group);
	length_64_button.set_group (length_group);

        Widget* w;

	w = manage (new Image (::get_icon (X_("wholenote"))));
	w->show();
	length_1_button.add (*w);
	w = manage (new Image (::get_icon (X_("halfnote"))));
	w->show();
	length_2_button.add (*w);
	w = manage (new Image (::get_icon (X_("quarternote"))));
	w->show();
	length_4_button.add (*w);
	w = manage (new Image (::get_icon (X_("eighthnote"))));
	w->show();
	length_8_button.add (*w);
	w = manage (new Image (::get_icon (X_("sixteenthnote"))));
	w->show();
	length_16_button.add (*w);
	w = manage (new Image (::get_icon (X_("thirtysecondnote"))));
	w->show();
	length_32_button.add (*w);
	w = manage (new Image (::get_icon (X_("sixtyfourthnote"))));
	w->show();
	length_64_button.add (*w);

        length_1_button.property_draw_indicator() = false;
        length_2_button.property_draw_indicator() = false;
        length_4_button.property_draw_indicator() = false;
        length_8_button.property_draw_indicator() = false;
        length_16_button.property_draw_indicator() = false;
        length_32_button.property_draw_indicator() = false;
        length_64_button.property_draw_indicator() = false;

        note_length_box.pack_start (length_1_button, false, false);
        note_length_box.pack_start (length_2_button, false, false);
        note_length_box.pack_start (length_4_button, false, false);
        note_length_box.pack_start (length_8_button, false, false);
        note_length_box.pack_start (length_16_button, false, false);
        note_length_box.pack_start (length_32_button, false, false);
        note_length_box.pack_start (length_64_button, false, false);

	RadioButtonGroup velocity_group = velocity_ppp_button.get_group();
        velocity_pp_button.set_group (velocity_group);
        velocity_p_button.set_group (velocity_group);
        velocity_mp_button.set_group (velocity_group);
        velocity_mf_button.set_group (velocity_group);
        velocity_f_button.set_group (velocity_group);
        velocity_ff_button.set_group (velocity_group);
        velocity_fff_button.set_group (velocity_group);

	w = manage (new Image (::get_icon (X_("pianississimo"))));
	w->show();
	velocity_ppp_button.add (*w);
	w = manage (new Image (::get_icon (X_("pianissimo"))));
	w->show();
	velocity_pp_button.add (*w);
	w = manage (new Image (::get_icon (X_("piano"))));
	w->show();
	velocity_p_button.add (*w);
	w = manage (new Image (::get_icon (X_("mezzopiano"))));
	w->show();
	velocity_mp_button.add (*w);
	w = manage (new Image (::get_icon (X_("mezzoforte"))));
	w->show();
	velocity_mf_button.add (*w);
	w = manage (new Image (::get_icon (X_("forte"))));
	w->show();
	velocity_f_button.add (*w);
	w = manage (new Image (::get_icon (X_("fortissimo"))));
	w->show();
	velocity_ff_button.add (*w);
	w = manage (new Image (::get_icon (X_("fortississimo"))));
	w->show();
	velocity_fff_button.add (*w);

        velocity_ppp_button.property_draw_indicator() = false;
        velocity_pp_button.property_draw_indicator() = false;
        velocity_p_button.property_draw_indicator() = false;
        velocity_mp_button.property_draw_indicator() = false;
        velocity_mf_button.property_draw_indicator() = false;
        velocity_f_button.property_draw_indicator() = false;
        velocity_ff_button.property_draw_indicator() = false;
        velocity_fff_button.property_draw_indicator() = false;

        note_velocity_box.pack_start (velocity_ppp_button, false, false);
        note_velocity_box.pack_start (velocity_pp_button, false, false);
        note_velocity_box.pack_start (velocity_p_button, false, false);
        note_velocity_box.pack_start (velocity_mp_button, false, false);
        note_velocity_box.pack_start (velocity_mf_button, false, false);
        note_velocity_box.pack_start (velocity_f_button, false, false);
        note_velocity_box.pack_start (velocity_ff_button, false, false);
        note_velocity_box.pack_start (velocity_fff_button, false, false);

        Label* l = manage (new Label);
        l->set_markup ("<b><big>.</big></b>");
        l->show ();
        dot_button.add (*l);

	w = manage (new Image (::get_icon (X_("chord"))));
	w->show();
	chord_button.add (*w);

        upper_box.set_spacing (6);
        upper_box.pack_start (chord_button, false, false);
        upper_box.pack_start (note_length_box, false, false, 12);
        upper_box.pack_start (triplet_button, false, false);
        upper_box.pack_start (dot_button, false, false);
        upper_box.pack_start (sustain_button, false, false);
        upper_box.pack_start (rest_button, false, false);
        upper_box.pack_start (note_velocity_box, false, false, 12);
        upper_box.pack_start (channel_spinner, false, false);

        _piano = (PianoKeyboard*) piano_keyboard_new ();
        piano = Glib::wrap ((GtkWidget*) _piano);

	g_signal_connect(G_OBJECT(_piano), "note-off", G_CALLBACK(_note_off_event_handler), this);

        rest_button.signal_clicked().connect (sigc::mem_fun (*this, &StepEntry::rest_click));

        packer.set_spacing (6);
        packer.pack_start (upper_box, false, false);
        packer.pack_start (*piano, false, false);
        packer.show_all ();

        get_vbox()->add (packer);
}

StepEntry::~StepEntry()
{
}

void
StepEntry::note_off_event_handler (int note)
{
        Evoral::MusicalTime length = 1.0;
        uint8_t velocity = 64;

        if (length_64_button.get_active()) {
                length = 1.0/64.0;
        } else if (length_32_button.get_active()) {
                length = 1.0/32.0;
        } else if (length_16_button.get_active()) {
                length = 1.0/16.0;
        } else if (length_8_button.get_active()) {
                length = 1.0/8.0;
        } else if (length_4_button.get_active()) {
                length = 1.0/4.0;
        } else if (length_2_button.get_active()) {
                length = 1.0/2.0;
        } else if (length_1_button.get_active()) {
                length = 1.0/1.0;
        }

        if (dot_button.get_active()) {
                length *= 0.5;
        }

        if (velocity_ppp_button.get_active()) {
                velocity = 16;
        } else if (velocity_pp_button.get_active()) {
                velocity = 32;
        } else if (velocity_p_button.get_active()) {
                velocity = 48;
        } else if (velocity_mp_button.get_active()) {
                velocity = 64;
        } else if (velocity_mf_button.get_active()) {
                velocity = 80;
        } else if (velocity_f_button.get_active()) {
                velocity = 96;
        } else if (velocity_ff_button.get_active()) {
                velocity = 112;
        } else if (velocity_fff_button.get_active()) {
                velocity = 127;
        }

        if (!triplet_button.get_active()) {
                _mtv->step_add_note (channel_adjustment.get_value(), note, velocity, length);
        } else {
                length *= 2.0/3.0;
                _mtv->step_add_note (channel_adjustment.get_value(), note, velocity, length);
                _mtv->step_add_note (channel_adjustment.get_value(), note, velocity, length);
                _mtv->step_add_note (channel_adjustment.get_value(), note, velocity, length);
        }
}

void
StepEntry::rest_click ()
{
        _mtv->step_edit_rest ();
}
