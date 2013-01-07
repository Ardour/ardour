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

#include <iostream>

#include "pbd/file_utils.h"

#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/bindings.h"

#include "ardour/filesystem_paths.h"

#include "ardour_ui.h"
#include "midi_channel_selector.h"
#include "midi_time_axis.h"
#include "step_editor.h"
#include "step_entry.h"
#include "utils.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace PBD;
using namespace ARDOUR;

static void
_note_off_event_handler (GtkWidget* /*widget*/, int note, gpointer arg)
{
	((StepEntry*)arg)->note_off_event_handler (note);
}

static void
_rest_event_handler (GtkWidget* /*widget*/, gpointer arg)
{
	((StepEntry*)arg)->rest_event_handler ();
}

StepEntry::StepEntry (StepEditor& seditor)
	: ArdourWindow (string_compose (_("Step Entry: %1"), seditor.name()))
        , _current_note_length (1.0)
        , _current_note_velocity (64)
	, triplet_button ("3")
        , dot_adjustment (0.0, 0.0, 3.0, 1.0, 1.0)
	, beat_resync_button (_(">beat"))
	, bar_resync_button (_(">bar"))
	, resync_button (_(">EP"))
	, sustain_button (_("sustain"))
	, rest_button (_("rest"))
	, grid_rest_button (_("g-rest"))
	, back_button (_("back"))
	, channel_adjustment (1, 1, 16, 1, 4)
	, channel_spinner (channel_adjustment)
        , octave_adjustment (4, 0, 10, 1, 4) // start in octave 4
        , octave_spinner (octave_adjustment)
        , length_divisor_adjustment (1.0, 1.0, 128, 1.0, 4.0)
        , length_divisor_spinner (length_divisor_adjustment)
        , velocity_adjustment (64.0, 0.0, 127.0, 1.0, 4.0)
        , velocity_spinner (velocity_adjustment)
        , bank_adjustment (0, 0.0, 127.0, 1.0, 4.0)
        , bank_spinner (bank_adjustment)
        , bank_button (_("+"))
        , program_adjustment (0, 0.0, 127.0, 1.0, 4.0)
        , program_spinner (program_adjustment)
        , program_button (_("+"))
	, _piano (0)
	, piano (0)
	, se (&seditor)
{
        register_actions ();
        load_bindings ();

#if 0
	/* set channel selector to first selected channel. if none
	   are selected, it will remain at the value set in its
	   constructor, above (1)
	*/

	uint16_t chn_mask = se->channel_selector().get_selected_channels();

	for (uint32_t i = 0; i < 16; ++i) {
		if (chn_mask & (1<<i)) {
			channel_adjustment.set_value (i+1);
			break;
		}
	}

#endif

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

        RefPtr<Action> act;

        act = myactions.find_action ("StepEditing/note-length-whole");
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (length_1_button.gobj()), act->gobj());
        act = myactions.find_action ("StepEditing/note-length-half");
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (length_2_button.gobj()), act->gobj());
        act = myactions.find_action ("StepEditing/note-length-quarter");
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (length_4_button.gobj()), act->gobj());
        act = myactions.find_action ("StepEditing/note-length-eighth");
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (length_8_button.gobj()), act->gobj());
        act = myactions.find_action ("StepEditing/note-length-sixteenth");
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (length_16_button.gobj()), act->gobj());
        act = myactions.find_action ("StepEditing/note-length-thirtysecond");
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (length_32_button.gobj()), act->gobj());
        act = myactions.find_action ("StepEditing/note-length-sixtyfourth");
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (length_64_button.gobj()), act->gobj());

        length_1_button.signal_button_press_event().connect (sigc::mem_fun (*this, &StepEntry::radio_button_press), false);
        length_1_button.signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &StepEntry::radio_button_release), &length_1_button, 1), false);
        length_2_button.signal_button_press_event().connect (sigc::mem_fun (*this, &StepEntry::radio_button_press), false);
        length_2_button.signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &StepEntry::radio_button_release), &length_1_button, 2), false);
        length_4_button.signal_button_press_event().connect (sigc::mem_fun (*this, &StepEntry::radio_button_press), false);
        length_4_button.signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &StepEntry::radio_button_release), &length_1_button, 4), false);
        length_8_button.signal_button_press_event().connect (sigc::mem_fun (*this, &StepEntry::radio_button_press), false);
        length_8_button.signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &StepEntry::radio_button_release), &length_1_button, 8), false);
        length_16_button.signal_button_press_event().connect (sigc::mem_fun (*this, &StepEntry::radio_button_press), false);
        length_16_button.signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &StepEntry::radio_button_release), &length_1_button, 16), false);
        length_32_button.signal_button_press_event().connect (sigc::mem_fun (*this, &StepEntry::radio_button_press), false);
        length_32_button.signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &StepEntry::radio_button_release), &length_1_button, 32), false);
        length_64_button.signal_button_press_event().connect (sigc::mem_fun (*this, &StepEntry::radio_button_press), false);
        length_64_button.signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &StepEntry::radio_button_release), &length_1_button, 64), false);

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

	ARDOUR_UI::instance()->set_tip (&length_1_button, _("Set note length to a whole note"), "");
	ARDOUR_UI::instance()->set_tip (&length_2_button, _("Set note length to a half note"), "");
	ARDOUR_UI::instance()->set_tip (&length_4_button, _("Set note length to a quarter note"), "");
	ARDOUR_UI::instance()->set_tip (&length_8_button, _("Set note length to a eighth note"), "");
	ARDOUR_UI::instance()->set_tip (&length_16_button, _("Set note length to a sixteenth note"), "");
	ARDOUR_UI::instance()->set_tip (&length_32_button, _("Set note length to a thirty-second note"), "");
	ARDOUR_UI::instance()->set_tip (&length_64_button, _("Set note length to a sixty-fourth note"), "");

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

        act = myactions.find_action ("StepEditing/note-velocity-ppp");
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (velocity_ppp_button.gobj()), act->gobj());
        act = myactions.find_action ("StepEditing/note-velocity-pp");
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (velocity_pp_button.gobj()), act->gobj());
        act = myactions.find_action ("StepEditing/note-velocity-p");
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (velocity_p_button.gobj()), act->gobj());
        act = myactions.find_action ("StepEditing/note-velocity-mp");
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (velocity_mp_button.gobj()), act->gobj());
        act = myactions.find_action ("StepEditing/note-velocity-mf");
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (velocity_mf_button.gobj()), act->gobj());
        act = myactions.find_action ("StepEditing/note-velocity-f");
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (velocity_f_button.gobj()), act->gobj());
        act = myactions.find_action ("StepEditing/note-velocity-ff");
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (velocity_ff_button.gobj()), act->gobj());
        act = myactions.find_action ("StepEditing/note-velocity-fff");
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (velocity_fff_button.gobj()), act->gobj());

        velocity_ppp_button.signal_button_press_event().connect (sigc::mem_fun (*this, &StepEntry::radio_button_press), false);
        velocity_ppp_button.signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &StepEntry::radio_button_release), &velocity_ppp_button, 1), false);
        velocity_pp_button.signal_button_press_event().connect (sigc::mem_fun (*this, &StepEntry::radio_button_press), false);
        velocity_pp_button.signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &StepEntry::radio_button_release), &velocity_pp_button, 16), false);
        velocity_p_button.signal_button_press_event().connect (sigc::mem_fun (*this, &StepEntry::radio_button_press), false);
        velocity_p_button.signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &StepEntry::radio_button_release), &velocity_p_button, 32), false);
        velocity_mp_button.signal_button_press_event().connect (sigc::mem_fun (*this, &StepEntry::radio_button_press), false);
        velocity_mp_button.signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &StepEntry::radio_button_release), &velocity_mp_button, 64), false);
        velocity_mf_button.signal_button_press_event().connect (sigc::mem_fun (*this, &StepEntry::radio_button_press), false);
        velocity_mf_button.signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &StepEntry::radio_button_release), &velocity_mf_button, 80), false);
        velocity_f_button.signal_button_press_event().connect (sigc::mem_fun (*this, &StepEntry::radio_button_press), false);
        velocity_f_button.signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &StepEntry::radio_button_release), &velocity_f_button, 96), false);
        velocity_ff_button.signal_button_press_event().connect (sigc::mem_fun (*this, &StepEntry::radio_button_press), false);
        velocity_ff_button.signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &StepEntry::radio_button_release), &velocity_ff_button, 112), false);
        velocity_fff_button.signal_button_press_event().connect (sigc::mem_fun (*this, &StepEntry::radio_button_press), false);
        velocity_fff_button.signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &StepEntry::radio_button_release), &velocity_fff_button, 127), false);

	velocity_ppp_button.property_draw_indicator() = false;
	velocity_pp_button.property_draw_indicator() = false;
	velocity_p_button.property_draw_indicator() = false;
	velocity_mp_button.property_draw_indicator() = false;
	velocity_mf_button.property_draw_indicator() = false;
	velocity_f_button.property_draw_indicator() = false;
	velocity_ff_button.property_draw_indicator() = false;
	velocity_fff_button.property_draw_indicator() = false;

	ARDOUR_UI::instance()->set_tip (&velocity_ppp_button, _("Set volume (velocity) to pianississimo"), "");
	ARDOUR_UI::instance()->set_tip (&velocity_pp_button, _("Set volume (velocity) to pianissimo"), "");
	ARDOUR_UI::instance()->set_tip (&velocity_p_button, _("Set volume (velocity) to piano"), "");
	ARDOUR_UI::instance()->set_tip (&velocity_mp_button, _("Set volume (velocity) to mezzo-piano"), "");
	ARDOUR_UI::instance()->set_tip (&velocity_mf_button, _("Set volume (velocity) to mezzo-forte"), "");
	ARDOUR_UI::instance()->set_tip (&velocity_f_button, _("Set volume (velocity) to forte"), "");
	ARDOUR_UI::instance()->set_tip (&velocity_ff_button, _("Set volume (velocity) to forteissimo"), "");
	ARDOUR_UI::instance()->set_tip (&velocity_fff_button, _("Set volume (velocity) to forteississimo"), "");

	note_velocity_box.pack_start (velocity_ppp_button, false, false);
	note_velocity_box.pack_start (velocity_pp_button, false, false);
	note_velocity_box.pack_start (velocity_p_button, false, false);
	note_velocity_box.pack_start (velocity_mp_button, false, false);
	note_velocity_box.pack_start (velocity_mf_button, false, false);
	note_velocity_box.pack_start (velocity_f_button, false, false);
	note_velocity_box.pack_start (velocity_ff_button, false, false);
	note_velocity_box.pack_start (velocity_fff_button, false, false);

	Label* l = manage (new Label);
	l->set_markup ("<b><big>-</big></b>");
	l->show ();
	dot0_button.add (*l);

	l = manage (new Label);
	l->set_markup ("<b><big>.</big></b>");
	l->show ();
	dot1_button.add (*l);

	l = manage (new Label);
	l->set_markup ("<b><big>..</big></b>");
	l->show ();
	dot2_button.add (*l);

	l = manage (new Label);
	l->set_markup ("<b><big>...</big></b>");
	l->show ();
	dot3_button.add (*l);

	w = manage (new Image (::get_icon (X_("chord"))));
	w->show();
	chord_button.add (*w);

	dot_box1.pack_start (dot0_button, true, false);
	dot_box1.pack_start (dot1_button, true, false);
	dot_box2.pack_start (dot2_button, true, false);
	dot_box2.pack_start (dot3_button, true, false);

	rest_box.pack_start (rest_button, true, false);
	rest_box.pack_start (grid_rest_button, true, false);
	rest_box.pack_start (back_button, true, false);

	resync_box.pack_start (beat_resync_button, true, false);
	resync_box.pack_start (bar_resync_button, true, false);
	resync_box.pack_start (resync_button, true, false);

	ARDOUR_UI::instance()->set_tip (&chord_button, _("Stack inserted notes to form a chord"), "");
	ARDOUR_UI::instance()->set_tip (&sustain_button, _("Extend selected notes by note length"), "");
	ARDOUR_UI::instance()->set_tip (&dot0_button, _("Use undotted note lengths"), "");
	ARDOUR_UI::instance()->set_tip (&dot1_button, _("Use dotted (* 1.5) note lengths"), "");
	ARDOUR_UI::instance()->set_tip (&dot2_button, _("Use double-dotted (* 1.75) note lengths"), "");
	ARDOUR_UI::instance()->set_tip (&dot3_button, _("Use triple-dotted (* 1.875) note lengths"), "");
	ARDOUR_UI::instance()->set_tip (&rest_button, _("Insert a note-length's rest"), "");
	ARDOUR_UI::instance()->set_tip (&grid_rest_button, _("Insert a grid-unit's rest"), "");
	ARDOUR_UI::instance()->set_tip (&beat_resync_button, _("Insert a rest until the next beat"), "");
	ARDOUR_UI::instance()->set_tip (&bar_resync_button, _("Insert a rest until the next bar"), "");
	ARDOUR_UI::instance()->set_tip (&bank_button, _("Insert a bank change message"), "");
	ARDOUR_UI::instance()->set_tip (&program_button, _("Insert a program change message"), "");
	ARDOUR_UI::instance()->set_tip (&back_button, _("Move Insert Position Back by Note Length"), "");
	ARDOUR_UI::instance()->set_tip (&resync_button, _("Move Insert Position to Edit Point"), "");

        act = myactions.find_action ("StepEditing/back");
        gtk_activatable_set_use_action_appearance (GTK_ACTIVATABLE (back_button.gobj()), false);
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (back_button.gobj()), act->gobj());
        act = myactions.find_action ("StepEditing/sync-to-edit-point");
        gtk_activatable_set_use_action_appearance (GTK_ACTIVATABLE (resync_button.gobj()), false);
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (resync_button.gobj()), act->gobj());
        act = myactions.find_action ("StepEditing/toggle-triplet");
        gtk_activatable_set_use_action_appearance (GTK_ACTIVATABLE (triplet_button.gobj()), false);
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (triplet_button.gobj()), act->gobj());
        act = myactions.find_action ("StepEditing/no-dotted");
        gtk_activatable_set_use_action_appearance (GTK_ACTIVATABLE (dot0_button.gobj()), false);
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (dot0_button.gobj()), act->gobj());
        act = myactions.find_action ("StepEditing/toggle-dotted");
        gtk_activatable_set_use_action_appearance (GTK_ACTIVATABLE (dot1_button.gobj()), false);
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (dot1_button.gobj()), act->gobj());
        act = myactions.find_action ("StepEditing/toggle-double-dotted");
        gtk_activatable_set_use_action_appearance (GTK_ACTIVATABLE (dot2_button.gobj()), false);
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (dot2_button.gobj()), act->gobj());
        act = myactions.find_action ("StepEditing/toggle-triple-dotted");
        gtk_activatable_set_use_action_appearance (GTK_ACTIVATABLE (dot3_button.gobj()), false);
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (dot3_button.gobj()), act->gobj());
        act = myactions.find_action ("StepEditing/toggle-chord");
        gtk_activatable_set_use_action_appearance (GTK_ACTIVATABLE (chord_button.gobj()), false);
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (chord_button.gobj()), act->gobj());
        act = myactions.find_action ("StepEditing/insert-rest");
        gtk_activatable_set_use_action_appearance (GTK_ACTIVATABLE (rest_button.gobj()), false);
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (rest_button.gobj()), act->gobj());
        act = myactions.find_action ("StepEditing/insert-snap-rest");
        gtk_activatable_set_use_action_appearance (GTK_ACTIVATABLE (grid_rest_button.gobj()), false);
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (grid_rest_button.gobj()), act->gobj());
        act = myactions.find_action ("StepEditing/sustain");
        gtk_activatable_set_use_action_appearance (GTK_ACTIVATABLE (sustain_button.gobj()), false);
        gtk_activatable_set_related_action (GTK_ACTIVATABLE (sustain_button.gobj()), act->gobj());

	upper_box.set_spacing (6);
	upper_box.pack_start (chord_button, false, false);
	upper_box.pack_start (note_length_box, false, false, 12);
	upper_box.pack_start (triplet_button, false, false);
	upper_box.pack_start (dot_box1, false, false);
	upper_box.pack_start (dot_box2, false, false);
	upper_box.pack_start (sustain_button, false, false);
	upper_box.pack_start (rest_box, false, false);
	upper_box.pack_start (resync_box, false, false);
	upper_box.pack_start (note_velocity_box, false, false, 12);

	VBox* v;

        v = manage (new VBox);
	l = manage (new Label (_("Channel")));
	v->set_spacing (6);
	v->pack_start (*l, false, false);
	v->pack_start (channel_spinner, false, false);
	upper_box.pack_start (*v, false, false);

        v = manage (new VBox);
	l = manage (new Label (_("1/Note")));
	v->set_spacing (6);
	v->pack_start (*l, false, false);
	v->pack_start (length_divisor_spinner, false, false);
	upper_box.pack_start (*v, false, false);

        v = manage (new VBox);
	l = manage (new Label (_("Velocity")));
	v->set_spacing (6);
	v->pack_start (*l, false, false);
	v->pack_start (velocity_spinner, false, false);
	upper_box.pack_start (*v, false, false);

        v = manage (new VBox);
	l = manage (new Label (_("Octave")));
	v->set_spacing (6);
	v->pack_start (*l, false, false);
	v->pack_start (octave_spinner, false, false);
	upper_box.pack_start (*v, false, false);

        v = manage (new VBox);
	l = manage (new Label (_("Bank")));
	v->set_spacing (6);
	v->pack_start (*l, false, false);
	v->pack_start (bank_spinner, false, false);
	v->pack_start (bank_button, false, false);
	upper_box.pack_start (*v, false, false);

        v = manage (new VBox);
	l = manage (new Label (_("Program")));
	v->set_spacing (6);
	v->pack_start (*l, false, false);
	v->pack_start (program_spinner, false, false);
	v->pack_start (program_button, false, false);
	upper_box.pack_start (*v, false, false);

        velocity_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &StepEntry::velocity_value_change));
        length_divisor_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &StepEntry::length_value_change));
        dot_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &StepEntry::dot_value_change));

	_piano = (PianoKeyboard*) piano_keyboard_new ();
	piano = wrap ((GtkWidget*) _piano);

	piano->set_flags (Gtk::CAN_FOCUS);

	g_signal_connect(G_OBJECT(_piano), "note-off", G_CALLBACK(_note_off_event_handler), this);
	g_signal_connect(G_OBJECT(_piano), "rest", G_CALLBACK(_rest_event_handler), this);

	program_button.signal_clicked().connect (sigc::mem_fun (*this, &StepEntry::program_click));
	bank_button.signal_clicked().connect (sigc::mem_fun (*this, &StepEntry::bank_click));
	beat_resync_button.signal_clicked().connect (sigc::mem_fun (*this, &StepEntry::beat_resync_click));
	bar_resync_button.signal_clicked().connect (sigc::mem_fun (*this, &StepEntry::bar_resync_click));

        length_divisor_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &StepEntry::length_changed));

	packer.set_spacing (6);
	packer.pack_start (upper_box, false, false);
	packer.pack_start (*piano, false, false);
	packer.show_all ();

	add (packer);

	/* initial settings: quarter note and mezzo forte */

        act = myactions.find_action ("StepEditing/note-length-quarter");
	RefPtr<RadioAction> r = RefPtr<RadioAction>::cast_dynamic (act);
	assert (r);
	r->set_active (true);

        act = myactions.find_action ("StepEditing/note-velocity-mf");
	r = RefPtr<RadioAction>::cast_dynamic (act);
	assert (r);
	r->set_active (true);
}

StepEntry::~StepEntry()
{
}

void
StepEntry::length_changed ()
{
        length_1_button.queue_draw ();
        length_2_button.queue_draw ();
        length_4_button.queue_draw ();
        length_8_button.queue_draw ();
        length_16_button.queue_draw ();
        length_32_button.queue_draw ();
        length_64_button.queue_draw ();
}

bool
StepEntry::on_key_press_event (GdkEventKey* ev)
{
        /* focus widget gets first shot, then bindings, otherwise
           forward to main window
        */

	if (!gtk_window_propagate_key_event (GTK_WINDOW(gobj()), ev)) {
                KeyboardKey k (ev->state, ev->keyval);

                if (bindings.activate (k, Bindings::Press)) {
                        return true;
                }
	}

        return forward_key_press (ev);
}

bool
StepEntry::on_key_release_event (GdkEventKey* ev)
{
	if (!gtk_window_propagate_key_event (GTK_WINDOW(gobj()), ev)) {
                KeyboardKey k (ev->state, ev->keyval);

                if (bindings.activate (k, Bindings::Release)) {
                        return true;
                }
	}

        /* don't forward releases */

        return true;
}

void
StepEntry::rest_event_handler ()
{
	se->step_edit_rest (0.0);
}

Evoral::MusicalTime
StepEntry::note_length ()
{
        Evoral::MusicalTime base_time = 4.0 / (Evoral::MusicalTime) length_divisor_adjustment.get_value();

        RefPtr<Action> act = myactions.find_action ("StepEditing/toggle-triplet");
        RefPtr<ToggleAction> tact = RefPtr<ToggleAction>::cast_dynamic (act);
        bool triplets = tact->get_active ();

        if (triplets) {
                base_time *= (2.0/3.0);
        }

        double dots = dot_adjustment.get_value ();

        if (dots > 0) {
                dots = pow (2.0, dots);
                base_time *= 1 + ((dots - 1.0)/dots);
        }

        return base_time;
}

uint8_t
StepEntry::note_velocity () const
{
        return (Evoral::MusicalTime) velocity_adjustment.get_value();
}

uint8_t
StepEntry::note_channel() const
{
	return channel_adjustment.get_value() - 1;
}

void
StepEntry::note_off_event_handler (int note)
{
        insert_note (note);
}


void
StepEntry::on_show ()
{
	ArdourWindow::on_show ();
	//piano->grab_focus ();
}

void
StepEntry::beat_resync_click ()
{
	se->step_edit_beat_sync ();
}

void
StepEntry::bar_resync_click ()
{
        se->step_edit_bar_sync ();
}

void
StepEntry::register_actions ()
{
	/* add named actions for the editor */

	myactions.register_action ("StepEditing", "insert-a", _("Insert Note A"), sigc::mem_fun (*this, &StepEntry::insert_a));
	myactions.register_action ("StepEditing", "insert-asharp", _("Insert Note A-sharp"), sigc::mem_fun (*this, &StepEntry::insert_asharp));
	myactions.register_action ("StepEditing", "insert-b", _("Insert Note B"), sigc::mem_fun (*this, &StepEntry::insert_b));
	myactions.register_action ("StepEditing", "insert-c", _("Insert Note C"), sigc::mem_fun (*this, &StepEntry::insert_c));
	myactions.register_action ("StepEditing", "insert-csharp", _("Insert Note C-sharp"), sigc::mem_fun (*this, &StepEntry::insert_csharp));
	myactions.register_action ("StepEditing", "insert-d", _("Insert Note D"), sigc::mem_fun (*this, &StepEntry::insert_d));
	myactions.register_action ("StepEditing", "insert-dsharp", _("Insert Note D-sharp"), sigc::mem_fun (*this, &StepEntry::insert_dsharp));
	myactions.register_action ("StepEditing", "insert-e", _("Insert Note E"), sigc::mem_fun (*this, &StepEntry::insert_e));
	myactions.register_action ("StepEditing", "insert-f", _("Insert Note F"), sigc::mem_fun (*this, &StepEntry::insert_f));
	myactions.register_action ("StepEditing", "insert-fsharp", _("Insert Note F-sharp"), sigc::mem_fun (*this, &StepEntry::insert_fsharp));
	myactions.register_action ("StepEditing", "insert-g", _("Insert Note G"), sigc::mem_fun (*this, &StepEntry::insert_g));
	myactions.register_action ("StepEditing", "insert-gsharp", _("Insert Note G-sharp"), sigc::mem_fun (*this, &StepEntry::insert_gsharp));

	myactions.register_action ("StepEditing", "insert-rest", _("Insert a Note-length Rest"), sigc::mem_fun (*this, &StepEntry::insert_rest));
	myactions.register_action ("StepEditing", "insert-snap-rest", _("Insert a Snap-length Rest"), sigc::mem_fun (*this, &StepEntry::insert_grid_rest));

	myactions.register_action ("StepEditing", "next-octave", _("Move to next octave"), sigc::mem_fun (*this, &StepEntry::next_octave));
	myactions.register_action ("StepEditing", "prev-octave", _("Move to next octave"), sigc::mem_fun (*this, &StepEntry::prev_octave));

	myactions.register_action ("StepEditing", "next-note-length", _("Move to Next Note Length"), sigc::mem_fun (*this, &StepEntry::next_note_length));
	myactions.register_action ("StepEditing", "prev-note-length", _("Move to Previous Note Length"), sigc::mem_fun (*this, &StepEntry::prev_note_length));

	myactions.register_action ("StepEditing", "inc-note-length", _("Increase Note Length"), sigc::mem_fun (*this, &StepEntry::inc_note_length));
	myactions.register_action ("StepEditing", "dec-note-length", _("Decrease Note Length"), sigc::mem_fun (*this, &StepEntry::dec_note_length));

	myactions.register_action ("StepEditing", "next-note-velocity", _("Move to Next Note Velocity"), sigc::mem_fun (*this, &StepEntry::next_note_velocity));
	myactions.register_action ("StepEditing", "prev-note-velocity", _("Move to Previous Note Velocity"), sigc::mem_fun (*this, &StepEntry::prev_note_velocity));

	myactions.register_action ("StepEditing", "inc-note-velocity", _("Increase Note Velocity"), sigc::mem_fun (*this, &StepEntry::inc_note_velocity));
	myactions.register_action ("StepEditing", "dec-note-velocity", _("Decrease Note Velocity"), sigc::mem_fun (*this, &StepEntry::dec_note_velocity));

	myactions.register_action ("StepEditing", "octave-0", _("Switch to the 1st octave"), sigc::mem_fun (*this, &StepEntry::octave_0));
	myactions.register_action ("StepEditing", "octave-1", _("Switch to the 2nd octave"), sigc::mem_fun (*this, &StepEntry::octave_1));
	myactions.register_action ("StepEditing", "octave-2", _("Switch to the 3rd octave"), sigc::mem_fun (*this, &StepEntry::octave_2));
	myactions.register_action ("StepEditing", "octave-3", _("Switch to the 4th octave"), sigc::mem_fun (*this, &StepEntry::octave_3));
	myactions.register_action ("StepEditing", "octave-4", _("Switch to the 5th octave"), sigc::mem_fun (*this, &StepEntry::octave_4));
	myactions.register_action ("StepEditing", "octave-5", _("Switch to the 6th octave"), sigc::mem_fun (*this, &StepEntry::octave_5));
	myactions.register_action ("StepEditing", "octave-6", _("Switch to the 7th octave"), sigc::mem_fun (*this, &StepEntry::octave_6));
	myactions.register_action ("StepEditing", "octave-7", _("Switch to the 8th octave"), sigc::mem_fun (*this, &StepEntry::octave_7));
	myactions.register_action ("StepEditing", "octave-8", _("Switch to the 9th octave"), sigc::mem_fun (*this, &StepEntry::octave_8));
	myactions.register_action ("StepEditing", "octave-9", _("Switch to the 10th octave"), sigc::mem_fun (*this, &StepEntry::octave_9));
	myactions.register_action ("StepEditing", "octave-10", _("Switch to the 11th octave"), sigc::mem_fun (*this, &StepEntry::octave_10));

        RadioAction::Group note_length_group;

        myactions.register_radio_action ("StepEditing", note_length_group, "note-length-whole",
                                         _("Set Note Length to Whole"), sigc::mem_fun (*this, &StepEntry::note_length_change), 1);
        myactions.register_radio_action ("StepEditing", note_length_group, "note-length-half",
                                         _("Set Note Length to 1/2"), sigc::mem_fun (*this, &StepEntry::note_length_change), 2);
        myactions.register_radio_action ("StepEditing", note_length_group, "note-length-third",
                                         _("Set Note Length to 1/3"), sigc::mem_fun (*this, &StepEntry::note_length_change), 3);
        myactions.register_radio_action ("StepEditing", note_length_group, "note-length-quarter",
                                         _("Set Note Length to 1/4"), sigc::mem_fun (*this, &StepEntry::note_length_change), 4);
        myactions.register_radio_action ("StepEditing", note_length_group, "note-length-eighth",
                                         _("Set Note Length to 1/8"), sigc::mem_fun (*this, &StepEntry::note_length_change), 8);
        myactions.register_radio_action ("StepEditing", note_length_group, "note-length-sixteenth",
                                         _("Set Note Length to 1/16"), sigc::mem_fun (*this, &StepEntry::note_length_change), 16);
        myactions.register_radio_action ("StepEditing", note_length_group, "note-length-thirtysecond",
                                         _("Set Note Length to 1/32"), sigc::mem_fun (*this, &StepEntry::note_length_change), 32);
        myactions.register_radio_action ("StepEditing", note_length_group, "note-length-sixtyfourth",
                                         _("Set Note Length to 1/64"), sigc::mem_fun (*this, &StepEntry::note_length_change), 64);

        RadioAction::Group note_velocity_group;

	myactions.register_radio_action ("StepEditing", note_velocity_group, "note-velocity-ppp",
                                         _("Set Note Velocity to Pianississimo"), sigc::mem_fun (*this, &StepEntry::note_velocity_change), 1);
	myactions.register_radio_action ("StepEditing", note_velocity_group, "note-velocity-pp",
                                         _("Set Note Velocity to Pianissimo"), sigc::mem_fun (*this, &StepEntry::note_velocity_change), 16);
	myactions.register_radio_action ("StepEditing", note_velocity_group, "note-velocity-p",
                                         _("Set Note Velocity to Piano"), sigc::mem_fun (*this, &StepEntry::note_velocity_change), 32);
	myactions.register_radio_action ("StepEditing", note_velocity_group, "note-velocity-mp",
                                         _("Set Note Velocity to Mezzo-Piano"), sigc::mem_fun (*this, &StepEntry::note_velocity_change), 64);
	myactions.register_radio_action ("StepEditing", note_velocity_group, "note-velocity-mf",
                                         _("Set Note Velocity to Mezzo-Forte"), sigc::mem_fun (*this, &StepEntry::note_velocity_change), 80);
	myactions.register_radio_action ("StepEditing", note_velocity_group, "note-velocity-f",
                                         _("Set Note Velocity to Forte"), sigc::mem_fun (*this, &StepEntry::note_velocity_change), 96);
	myactions.register_radio_action ("StepEditing", note_velocity_group, "note-velocity-ff",
                                         _("Set Note Velocity to Fortississimo"), sigc::mem_fun (*this, &StepEntry::note_velocity_change), 112);
	myactions.register_radio_action ("StepEditing", note_velocity_group, "note-velocity-fff",
                                         _("Set Note Velocity to Fortississimo"), sigc::mem_fun (*this, &StepEntry::note_velocity_change), 127);

        myactions.register_toggle_action ("StepEditing", "toggle-triplet", _("Toggle Triple Notes"),
                                          sigc::mem_fun (*this, &StepEntry::toggle_triplet));

        RadioAction::Group dot_group;

        myactions.register_radio_action ("StepEditing", dot_group, "no-dotted", _("No Dotted Notes"),
                                         sigc::mem_fun (*this, &StepEntry::dot_change), 0);
        myactions.register_radio_action ("StepEditing", dot_group, "toggle-dotted", _("Toggled Dotted Notes"),
                                         sigc::mem_fun (*this, &StepEntry::dot_change), 1);
        myactions.register_radio_action ("StepEditing", dot_group, "toggle-double-dotted", _("Toggled Double-Dotted Notes"),
                                         sigc::mem_fun (*this, &StepEntry::dot_change), 2);
        myactions.register_radio_action ("StepEditing", dot_group, "toggle-triple-dotted", _("Toggled Triple-Dotted Notes"),
                                         sigc::mem_fun (*this, &StepEntry::dot_change), 3);

        myactions.register_toggle_action ("StepEditing", "toggle-chord", _("Toggle Chord Entry"),
                                          sigc::mem_fun (*this, &StepEntry::toggle_chord));
        myactions.register_action ("StepEditing", "sustain", _("Sustain Selected Notes by Note Length"),
                                   sigc::mem_fun (*this, &StepEntry::do_sustain));

        myactions.register_action ("StepEditing", "sync-to-edit-point", _("Move Insert Position to Edit Point"),
                                   sigc::mem_fun (*this, &StepEntry::sync_to_edit_point));
        myactions.register_action ("StepEditing", "back", _("Move Insert Position Back by Note Length"),
                                   sigc::mem_fun (*this, &StepEntry::back));
}

void
StepEntry::load_bindings ()
{
        /* XXX move this to a better place */

        bindings.set_action_map (myactions);

	std::string binding_file;

	if (find_file_in_search_path (ardour_config_search_path(), "step_editing.bindings", binding_file)) {
                bindings.load (binding_file);
        }
}

void
StepEntry::toggle_triplet ()
{
        se->set_step_edit_cursor_width (note_length());
}

void
StepEntry::toggle_chord ()
{
        se->step_edit_toggle_chord ();
}

void
StepEntry::dot_change (GtkAction* act)
{
        if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION(act))) {
                gint v = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (act));
                dot_adjustment.set_value (v);
        }
}

void
StepEntry::dot_value_change ()
{
        RefPtr<Action> act;
        RefPtr<RadioAction> ract;
        double val = dot_adjustment.get_value();
        bool inconsistent = true;
        vector<const char*> dot_actions;

        dot_actions.push_back ("StepEditing/no-dotted");
        dot_actions.push_back ("StepEditing/toggle-dotted");
        dot_actions.push_back ("StepEditing/toggle-double-dotted");
        dot_actions.push_back ("StepEditing/toggle-triple-dotted");

        for (vector<const char*>::iterator i = dot_actions.begin(); i != dot_actions.end(); ++i) {

                act = myactions.find_action (*i);

                if (act) {
                        ract = RefPtr<RadioAction>::cast_dynamic (act);

                        if (ract) {
                                if (ract->property_value() == val) {
                                        ract->set_active (true);
                                        inconsistent = false;
                                        break;
                                }
                        }
                }
        }

        dot1_button.set_inconsistent (inconsistent);
        dot2_button.set_inconsistent (inconsistent);
        dot3_button.set_inconsistent (inconsistent);

        se->set_step_edit_cursor_width (note_length());
}

void
StepEntry::program_click ()
{
        se->step_add_program_change (note_channel(), (int8_t) floor (program_adjustment.get_value()));
}

void
StepEntry::bank_click ()
{
        se->step_add_bank_change (note_channel(), (int8_t) floor (bank_adjustment.get_value()));
}

void
StepEntry::insert_rest ()
{
	se->step_edit_rest (note_length());
}

void
StepEntry::insert_grid_rest ()
{
	se->step_edit_rest (0.0);
}

void
StepEntry::insert_note (uint8_t note)
{
        if (note > 127) {
                return;
        }

	se->step_add_note (note_channel(), note, note_velocity(), note_length());
}
void
StepEntry::insert_c ()
{
        insert_note (0 + (current_octave() * 12));
}
void
StepEntry::insert_csharp ()
{
        insert_note (1 + (current_octave() * 12));
}
void
StepEntry::insert_d ()
{
        insert_note (2 + (current_octave() * 12));
}
void
StepEntry::insert_dsharp ()
{
        insert_note (3 + (current_octave() * 12));
}
void
StepEntry::insert_e ()
{
        insert_note (4 + (current_octave() * 12));
}
void
StepEntry::insert_f ()
{
        insert_note (5 + (current_octave() * 12));
}
void
StepEntry::insert_fsharp ()
{
        insert_note (6 + (current_octave() * 12));
}
void
StepEntry::insert_g ()
{
        insert_note (7 + (current_octave() * 12));
}
void
StepEntry::insert_gsharp ()
{
        insert_note (8 + (current_octave() * 12));
}

void
StepEntry::insert_a ()
{
        insert_note (9 + (current_octave() * 12));
}

void
StepEntry::insert_asharp ()
{
        insert_note (10 + (current_octave() * 12));
}
void
StepEntry::insert_b ()
{
        insert_note (11 + (current_octave() * 12));
}

void
StepEntry::note_length_change (GtkAction* act)
{
        /* it doesn't matter which note length action we look up - we are interested
           in the current_value which is global across the whole group of note length
           actions. this method is called twice for every user operation,
           once for the action that became "inactive" and once for the action that
           becaome "active". so ... only bother to actually change the value when this
           is called for the "active" action.
        */

        if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION(act))) {
                gint v = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (act));
                length_divisor_adjustment.set_value (v);
        }
}

void
StepEntry::note_velocity_change (GtkAction* act)
{
        /* it doesn't matter which note length action we look up - we are interested
           in the current_value which is global across the whole group of note length
           actions. this method is called twice for every user operation,
           once for the action that became "inactive" and once for the action that
           becaome "active". so ... only bother to actually change the value when this
           is called for the "active" action.
        */

        if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION(act))) {
                gint v = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (act));
                velocity_adjustment.set_value (v);
        }
}

void
StepEntry::velocity_value_change ()
{
        RefPtr<Action> act;
        RefPtr<RadioAction> ract;
        double val = velocity_adjustment.get_value();
        bool inconsistent = true;
        vector<const char*> velocity_actions;

        velocity_actions.push_back ("StepEditing/note-velocity-ppp");
        velocity_actions.push_back ("StepEditing/note-velocity-pp");
        velocity_actions.push_back ("StepEditing/note-velocity-p");
        velocity_actions.push_back ("StepEditing/note-velocity-mp");
        velocity_actions.push_back ("StepEditing/note-velocity-mf");
        velocity_actions.push_back ("StepEditing/note-velocity-f");
        velocity_actions.push_back ("StepEditing/note-velocity-ff");
        velocity_actions.push_back ("StepEditing/note-velocity-fff");

        for (vector<const char*>::iterator i = velocity_actions.begin(); i != velocity_actions.end(); ++i) {

                act = myactions.find_action (*i);

                if (act) {
                        ract = RefPtr<RadioAction>::cast_dynamic (act);

                        if (ract) {
                                if (ract->property_value() == val) {
                                        ract->set_active (true);
                                        inconsistent = false;
                                        break;
                                }
                        }
                }
        }

        velocity_ppp_button.set_inconsistent (inconsistent);
        velocity_pp_button.set_inconsistent (inconsistent);
        velocity_p_button.set_inconsistent (inconsistent);
        velocity_mp_button.set_inconsistent (inconsistent);
        velocity_mf_button.set_inconsistent (inconsistent);
        velocity_f_button.set_inconsistent (inconsistent);
        velocity_ff_button.set_inconsistent (inconsistent);
        velocity_fff_button.set_inconsistent (inconsistent);
}

void
StepEntry::length_value_change ()
{
        RefPtr<Action> act;
        RefPtr<RadioAction> ract;
        double val = length_divisor_adjustment.get_value();
        bool inconsistent = true;
        vector<const char*> length_actions;

        length_actions.push_back ("StepEditing/note-length-whole");
        length_actions.push_back ("StepEditing/note-length-half");
        length_actions.push_back ("StepEditing/note-length-quarter");
        length_actions.push_back ("StepEditing/note-length-eighth");
        length_actions.push_back ("StepEditing/note-length-sixteenth");
        length_actions.push_back ("StepEditing/note-length-thirtysecond");
        length_actions.push_back ("StepEditing/note-length-sixtyfourth");

        for (vector<const char*>::iterator i = length_actions.begin(); i != length_actions.end(); ++i) {

                act = myactions.find_action (*i);

                if (act) {
                        ract = RefPtr<RadioAction>::cast_dynamic (act);

                        if (ract) {
                                if (ract->property_value() == val) {
                                        ract->set_active (true);
                                        inconsistent = false;
                                        break;
                                }
                        }
                }
        }

        length_1_button.set_inconsistent (inconsistent);
        length_2_button.set_inconsistent (inconsistent);
        length_4_button.set_inconsistent (inconsistent);
        length_8_button.set_inconsistent (inconsistent);
        length_16_button.set_inconsistent (inconsistent);
        length_32_button.set_inconsistent (inconsistent);
        length_64_button.set_inconsistent (inconsistent);

        se->set_step_edit_cursor_width (note_length());
}

bool
StepEntry::radio_button_press (GdkEventButton* ev)
{
        if (ev->button == 1) {
                return true;
        }

        return false;
}

bool
StepEntry::radio_button_release (GdkEventButton* ev, RadioButton* btn, int v)
{
        if (ev->button == 1) {
                GtkAction* act = gtk_activatable_get_related_action (GTK_ACTIVATABLE (btn->gobj()));

                if (act) {
                        gtk_radio_action_set_current_value (GTK_RADIO_ACTION(act), v);
                }

                return true;
        }

        return false;
}

void
StepEntry::next_octave ()
{
        octave_adjustment.set_value (octave_adjustment.get_value() + 1.0);
}

void
StepEntry::prev_octave ()
{
        octave_adjustment.set_value (octave_adjustment.get_value() - 1.0);
}

void
StepEntry::inc_note_length ()
{
        length_divisor_adjustment.set_value (length_divisor_adjustment.get_value() - 1.0);
}

void
StepEntry::dec_note_length ()
{
        length_divisor_adjustment.set_value (length_divisor_adjustment.get_value() + 1.0);
}

void
StepEntry::prev_note_length ()
{
        double l = length_divisor_adjustment.get_value();
        int il = (int) lrintf (l); // round to nearest integer
        il = (il/2) * 2; // round to power of 2

        if (il == 0) {
                il = 1;
        }

        il *= 2; // double

        length_divisor_adjustment.set_value (il);
}

void
StepEntry::next_note_length ()
{
        double l = length_divisor_adjustment.get_value();
        int il = (int) lrintf (l); // round to nearest integer
        il = (il/2) * 2; // round to power of 2

        if (il == 0) {
                il = 1;
        }

        il /= 2; // half

        if (il > 0) {
                length_divisor_adjustment.set_value (il);
        }
}

void
StepEntry::inc_note_velocity ()
{
        velocity_adjustment.set_value (velocity_adjustment.get_value() + 1.0);
}

void
StepEntry::dec_note_velocity ()
{
        velocity_adjustment.set_value (velocity_adjustment.get_value() - 1.0);
}

void
StepEntry::next_note_velocity ()
{
        double l = velocity_adjustment.get_value ();

        if (l < 16) {
                l = 16;
        } else if (l < 32) {
                l = 32;
        } else if (l < 48) {
                l = 48;
        } else if (l < 64) {
                l = 64;
        } else if (l < 80) {
                l = 80;
        } else if (l < 96) {
                l = 96;
        } else if (l < 112) {
                l = 112;
        } else if (l < 127) {
                l = 127;
        }

        velocity_adjustment.set_value (l);
}

void
StepEntry::prev_note_velocity ()
{
        double l = velocity_adjustment.get_value ();

        if (l > 112) {
                l = 112;
        } else if (l > 96) {
                l = 96;
        } else if (l > 80) {
                l = 80;
        } else if (l > 64) {
                l = 64;
        } else if (l > 48) {
                l = 48;
        } else if (l > 32) {
                l = 32;
        } else if (l > 16) {
                l = 16;
        } else {
                l = 1;
        }

        velocity_adjustment.set_value (l);
}

void
StepEntry::octave_n (int n)
{
        octave_adjustment.set_value (n);
}

void
StepEntry::do_sustain ()
{
        se->step_edit_sustain (note_length());
}

void
StepEntry::back ()
{
        se->move_step_edit_beat_pos (-note_length());
}

void
StepEntry::sync_to_edit_point ()
{
        se->resync_step_edit_to_edit_point ();
}
