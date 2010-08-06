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

#include "pbd/filesystem.h"
#include "pbd/file_utils.h"

#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/bindings.h"

#include "ardour/filesystem_paths.h"

#include "ardour_ui.h"
#include "midi_channel_selector.h"
#include "midi_time_axis.h"
#include "step_entry.h"
#include "utils.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;
using namespace ARDOUR;

static void
_note_off_event_handler (GtkWidget* widget, int note, gpointer arg)
{
	((StepEntry*)arg)->note_off_event_handler (note);
}

static void
_rest_event_handler (GtkWidget* widget, gpointer arg)
{
	((StepEntry*)arg)->rest_event_handler ();
}

StepEntry::StepEntry (MidiTimeAxisView& mtv)
	: ArdourDialog (string_compose (_("Step Entry: %1"), mtv.name()))
	, triplet_button ("3")
	, beat_resync_button (_(">beat"))
	, bar_resync_button (_(">bar"))
	, sustain_button (_("sustain"))
	, rest_button (_("rest"))
	, grid_rest_button (_("g-rest"))
	, channel_adjustment (1, 1, 16, 1, 4) 
	, channel_spinner (channel_adjustment)
	, _piano (0)
	, piano (0)
	, _mtv (&mtv)
{
	/* set channel selector to first selected channel. if none
	   are selected, it will remain at the value set in its
	   constructor, above (1)
	*/

	uint16_t chn_mask = _mtv->channel_selector().get_selected_channels();
        
	for (uint32_t i = 0; i < 16; ++i) {
		if (chn_mask & (1<<i)) {
			channel_adjustment.set_value (i+1);
			break;
		}
	}

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
	l->set_markup ("<b><big>.</big></b>");
	l->show ();
	dot_button.add (*l);

	w = manage (new Image (::get_icon (X_("chord"))));
	w->show();
	chord_button.add (*w);

	rest_box.pack_start (rest_button, false, false);
	rest_box.pack_start (grid_rest_button, false, false);

	resync_box.pack_start (beat_resync_button, false, false);
	resync_box.pack_start (bar_resync_button, false, false);

	ARDOUR_UI::instance()->set_tip (&chord_button, _("Stack inserted notes to form a chord"), "");
	ARDOUR_UI::instance()->set_tip (&sustain_button, _("Extend selected notes by note length"), "");
	ARDOUR_UI::instance()->set_tip (&dot_button, _("Use dotted note lengths"), "");
	ARDOUR_UI::instance()->set_tip (&rest_button, _("Insert a note-length's rest"), "");
	ARDOUR_UI::instance()->set_tip (&grid_rest_button, _("Insert a grid-unit's rest"), "");
	ARDOUR_UI::instance()->set_tip (&beat_resync_button, _("Insert a rest until the next beat"), "");
	ARDOUR_UI::instance()->set_tip (&bar_resync_button, _("Insert a rest until the next bar"), "");

	VBox* v = manage (new VBox);
	l = manage (new Label (_("Channel")));
	v->set_spacing (6);
	v->pack_start (*l, false, false);
	v->pack_start (channel_spinner, false, false);

	upper_box.set_spacing (6);
	upper_box.pack_start (chord_button, false, false);
	upper_box.pack_start (note_length_box, false, false, 12);
	upper_box.pack_start (triplet_button, false, false);
	upper_box.pack_start (dot_button, false, false);
	upper_box.pack_start (sustain_button, false, false);
	upper_box.pack_start (rest_box, false, false);
	upper_box.pack_start (resync_box, false, false);
	upper_box.pack_start (note_velocity_box, false, false, 12);
	upper_box.pack_start (*v, false, false);

	_piano = (PianoKeyboard*) piano_keyboard_new ();
	piano = Glib::wrap ((GtkWidget*) _piano);

	piano->set_flags (Gtk::CAN_FOCUS);

	g_signal_connect(G_OBJECT(_piano), "note-off", G_CALLBACK(_note_off_event_handler), this);
	g_signal_connect(G_OBJECT(_piano), "rest", G_CALLBACK(_rest_event_handler), this);
        
	rest_button.signal_clicked().connect (sigc::mem_fun (*this, &StepEntry::rest_click));
	grid_rest_button.signal_clicked().connect (sigc::mem_fun (*this, &StepEntry::grid_rest_click));
	chord_button.signal_toggled().connect (sigc::mem_fun (*this, &StepEntry::chord_toggled));
	triplet_button.signal_toggled().connect (sigc::mem_fun (*this, &StepEntry::triplet_toggled));
	beat_resync_button.signal_clicked().connect (sigc::mem_fun (*this, &StepEntry::beat_resync_click));
	bar_resync_button.signal_clicked().connect (sigc::mem_fun (*this, &StepEntry::bar_resync_click));

	packer.set_spacing (6);
	packer.pack_start (upper_box, false, false);
	packer.pack_start (*piano, false, false);
	packer.show_all ();

	get_vbox()->add (packer);

        register_actions ();
        load_bindings ();
}

StepEntry::~StepEntry()
{
}

bool
StepEntry::on_key_press_event (GdkEventKey* ev)
{
        KeyboardKey k (ev->state, ev->keyval);
        
        if (bindings.activate (k, KeyboardKey::Press)) {
                return true;
        }
        
	if (!gtk_window_propagate_key_event (GTK_WINDOW(gobj()), ev)) {
		return gtk_window_activate_key (GTK_WINDOW(gobj()), ev);
	}
	return true;
}

bool
StepEntry::on_key_release_event (GdkEventKey* ev)
{
	if (!gtk_window_propagate_key_event (GTK_WINDOW(gobj()), ev)) {
		return gtk_window_activate_key (GTK_WINDOW(gobj()), ev);
	}
	return true;
}

void
StepEntry::rest_event_handler ()
{
	_mtv->step_edit_rest (0.0);
}

Evoral::MusicalTime
StepEntry::note_length () const
{
	Evoral::MusicalTime length = 0.0;

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

	if (_mtv->step_edit_within_triplet()) {
		length *= 2.0/3.0;
	}

	return length;
}

uint8_t
StepEntry::note_velocity () const
{
	uint8_t velocity = 64;

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

	return velocity;
}

uint8_t 
StepEntry::note_channel() const
{
	return channel_adjustment.get_value() - 1;
}

void
StepEntry::note_off_event_handler (int note)
{
	_mtv->step_add_note (note_channel(), note, note_velocity(), note_length());
}

void
StepEntry::rest_click ()
{
	_mtv->step_edit_rest (note_length());
}

void
StepEntry::grid_rest_click ()
{
	_mtv->step_edit_rest (0.0);
}

void
StepEntry::triplet_toggled ()
{
	if (triplet_button.get_active () != _mtv->step_edit_within_triplet()) {
		_mtv->step_edit_toggle_triplet ();
	}
}

void
StepEntry::chord_toggled ()
{
	if (chord_button.get_active() != _mtv->step_edit_within_chord ()) {
		_mtv->step_edit_toggle_chord ();
	}
}

void
StepEntry::on_show ()
{
	ArdourDialog::on_show ();
	piano->grab_focus ();
}

void
StepEntry::beat_resync_click ()
{
	_mtv->step_edit_beat_sync ();
}

void
StepEntry::bar_resync_click ()
{
        _mtv->step_edit_bar_sync ();
}

void
StepEntry::register_actions ()
{
	/* add named actions for the editor */

	myactions.register_action ("StepEditing", "insert-a", _("Insert Note A"), sigc::mem_fun (*this, &StepEntry::insert_a));
	myactions.register_action ("StepEditing", "insert-bsharp", _("Insert Note A-sharp"), sigc::mem_fun (*this, &StepEntry::insert_asharp));
	myactions.register_action ("StepEditing", "insert-b", _("Insert Note B"), sigc::mem_fun (*this, &StepEntry::insert_b));
	myactions.register_action ("StepEditing", "insert-bsharp", _("Insert Note B-sharp"), sigc::mem_fun (*this, &StepEntry::insert_bsharp));
	myactions.register_action ("StepEditing", "insert-c", _("Insert Note C"), sigc::mem_fun (*this, &StepEntry::insert_c));
	myactions.register_action ("StepEditing", "insert-csharp", _("Insert Note C-sharp"), sigc::mem_fun (*this, &StepEntry::insert_csharp));
	myactions.register_action ("StepEditing", "insert-d", _("Insert Note D"), sigc::mem_fun (*this, &StepEntry::insert_d));
	myactions.register_action ("StepEditing", "insert-dsharp", _("Insert Note D-sharp"), sigc::mem_fun (*this, &StepEntry::insert_dsharp));
	myactions.register_action ("StepEditing", "insert-e", _("Insert Note E"), sigc::mem_fun (*this, &StepEntry::insert_e));
	myactions.register_action ("StepEditing", "insert-f", _("Insert Note F"), sigc::mem_fun (*this, &StepEntry::insert_f));
	myactions.register_action ("StepEditing", "insert-fsharp", _("Insert Note F-sharp"), sigc::mem_fun (*this, &StepEntry::insert_fsharp));
	myactions.register_action ("StepEditing", "insert-g", _("Insert Note G"), sigc::mem_fun (*this, &StepEntry::insert_g));

        RadioAction::Group note_length_group;

	myactions.register_radio_action ("StepEditing", note_length_group, "note-length-whole", 
                                              _("Set Note Length to Whole"), sigc::mem_fun (*this, &StepEntry::note_length_whole));
	myactions.register_radio_action ("StepEditing", note_length_group, "note-length-half", 
                                              _("Set Note Length to 1/2"), sigc::mem_fun (*this, &StepEntry::note_length_half));
	myactions.register_radio_action ("StepEditing", note_length_group, "note-length-quarter",
                                              _("Set Note Length to 1/4"), sigc::mem_fun (*this, &StepEntry::note_length_quarter));
	myactions.register_radio_action ("StepEditing", note_length_group, "note-length-eighth",
                                              _("Set Note Length to 1/8"), sigc::mem_fun (*this, &StepEntry::note_length_eighth));
	myactions.register_radio_action ("StepEditing", note_length_group, "note-length-sixteenth",
                                              _("Set Note Length to 1/16"), sigc::mem_fun (*this, &StepEntry::note_length_sixteenth));
	myactions.register_radio_action ("StepEditing", note_length_group, "note-length-thirtysecond",
                                              _("Set Note Length to 1/32"), sigc::mem_fun (*this, &StepEntry::note_length_thirtysecond));
	myactions.register_radio_action ("StepEditing", note_length_group, "note-length-sixtyfourth",
                                              _("Set Note Length to 1/64"), sigc::mem_fun (*this, &StepEntry::note_length_sixtyfourth));
        
        RadioAction::Group note_velocity_group;
        
	myactions.register_radio_action ("StepEditing", note_velocity_group, "note-velocity-ppp",
                                              _("Set Note Velocity to Pianississimo"), sigc::mem_fun (*this, &StepEntry::note_velocity_ppp));
	myactions.register_radio_action ("StepEditing", note_velocity_group, "note-velocity-pp",
                                              _("Set Note Velocity to Pianissimo"), sigc::mem_fun (*this, &StepEntry::note_velocity_pp));
	myactions.register_radio_action ("StepEditing", note_velocity_group, "note-velocity-p",
                                              _("Set Note Velocity to Piano"), sigc::mem_fun (*this, &StepEntry::note_velocity_p));
	myactions.register_radio_action ("StepEditing", note_velocity_group, "note-velocity-mp",
                                              _("Set Note Velocity to Mezzo-Piano"), sigc::mem_fun (*this, &StepEntry::note_velocity_mp));
	myactions.register_radio_action ("StepEditing", note_velocity_group, "note-velocity-mf",
                                              _("Set Note Velocity to Mezzo-Forte"), sigc::mem_fun (*this, &StepEntry::note_velocity_mf));
	myactions.register_radio_action ("StepEditing", note_velocity_group, "note-velocity-f",
                                              _("Set Note Velocity to Forte"), sigc::mem_fun (*this, &StepEntry::note_velocity_f));
	myactions.register_radio_action ("StepEditing", note_velocity_group, "note-velocity-ff",
                                              _("Set Note Velocity to Fortississimo"), sigc::mem_fun (*this, &StepEntry::note_velocity_ff));
	myactions.register_radio_action ("StepEditing", note_velocity_group, "note-velocity-fff",
                                              _("Set Note Velocity to Fortississimo"), sigc::mem_fun (*this, &StepEntry::note_velocity_fff));
}

void
StepEntry::load_bindings ()
{
        bindings.set_action_map (myactions);

	sys::path binding_file;
	SearchPath spath = ardour_search_path() + user_config_directory() + system_config_search_path();

	if (find_file_in_search_path (spath, "step_editing.bindings", binding_file)) {
                bindings.load (binding_file.to_string());
        }
}

void
StepEntry::insert_a ()
{
}
void
StepEntry::insert_asharp ()
{
}
void
StepEntry::insert_b ()
{
}
void
StepEntry::insert_bsharp ()
{
}
void
StepEntry::insert_c ()
{
}
void
StepEntry::insert_csharp ()
{
}
void
StepEntry::insert_d ()
{
}
void
StepEntry::insert_dsharp ()
{
}
void
StepEntry::insert_e ()
{
}
void
StepEntry::insert_f ()
{
}
void
StepEntry::insert_fsharp ()
{
}
void
StepEntry::insert_g ()
{
}

void
StepEntry::note_length_whole ()
{
}
void
StepEntry::note_length_half ()
{
}
void
StepEntry::note_length_quarter ()
{
}
void
StepEntry::note_length_eighth ()
{
}
void
StepEntry::note_length_sixteenth ()
{
}
void
StepEntry::note_length_thirtysecond ()
{
}
void
StepEntry::note_length_sixtyfourth ()
{
}

void
StepEntry::note_velocity_ppp ()
{
}
void
StepEntry::note_velocity_pp ()
{
}
void
StepEntry::note_velocity_p ()
{
}
void
StepEntry::note_velocity_mp ()
{
}
void
StepEntry::note_velocity_mf ()
{
}
void
StepEntry::note_velocity_f ()
{
}
void
StepEntry::note_velocity_ff ()
{
}
void
StepEntry::note_velocity_fff ()
{
}
