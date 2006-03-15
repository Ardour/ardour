/*
    Copyright (C) 1998-99 Paul Davis
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

#include <string>
#include <climits>

#include <midi++/controllable.h>

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/slider_controller.h>
#include <gtkmm2ext/pixscroller.h>

#include "i18n.h"

using namespace Gtkmm2ext;

SliderController::SliderController (Glib::RefPtr<Gdk::Pixbuf> slide,
				    Glib::RefPtr<Gdk::Pixbuf> rail,
				    Gtk::Adjustment *adj,
				    MIDI::Controllable *mc,
				    bool with_numeric)

	: PixScroller (*adj, slide, rail),
	  spin (*adj, 0, 2),
	  prompter (Gtk::WIN_POS_MOUSE, 30000, false),
	  midi_control (mc),
	  bind_button (2),
	  bind_statemask (Gdk::CONTROL_MASK)

{			  
	signal_button_press_event().connect (mem_fun (this, &SliderController::button_press));
	spin.set_name ("SliderControllerValue");
	spin.set_size_request (70,-1); // should be based on font size somehow
	spin.set_numeric (true);
	spin.set_snap_to_ticks (false);

	prompter.signal_unmap_event().connect (mem_fun (*this, &SliderController::prompter_hiding));
	
	prompting = false;
	unprompting = false;
	
	if (mc) {
		mc->learning_started.connect (mem_fun (*this, &SliderController::midicontrol_prompt));
		mc->learning_stopped.connect (mem_fun (*this, &SliderController::midicontrol_unprompt));
	}
}

void
SliderController::set_value (float v)
{
	adj.set_value (v);
}

void
SliderController::set_bind_button_state (guint button, guint statemask)
{
	bind_button = button;
	bind_statemask = statemask;
}

void
SliderController::get_bind_button_state (guint &button, guint &statemask)
{
	button = bind_button;
	statemask = bind_statemask;
}

void
SliderController::midi_learn()
{
	if (midi_control) {
		prompting = true;
		midi_control->learn_about_external_control ();
	}
}

bool
SliderController::button_press (GdkEventButton *ev)
{
	if ((ev->state & bind_statemask) && ev->button == bind_button) { 
		midi_learn ();
		return true;
	}

	return false;
}

void
SliderController::midicontrol_set_tip ()

{
	if (midi_control) {
		// Gtkmm2ext::UI::instance()->set_tip (this, midi_control->control_description());
	}
}

bool
SliderController::prompter_hiding (GdkEventAny *ev)
{
	if (unprompting) {
		if (midi_control) {
			midi_control->stop_learning();
		}
		unprompting = false;
	}
	
	return false;
}

void
SliderController::midicontrol_prompt ()

{
	if (prompting) {

		string prompt = _("operate MIDI controller now");
		prompter.set_text (prompt);
		Gtkmm2ext::UI::instance()->touch_display (&prompter);

		unprompting = true;
		prompting = false;
	}
}

void
SliderController::midicontrol_unprompt ()

{
	if (unprompting) {
		Gtkmm2ext::UI::instance()->touch_display (&prompter);
		unprompting = false;
	}
}


VSliderController::VSliderController (Glib::RefPtr<Gdk::Pixbuf> slide,
				      Glib::RefPtr<Gdk::Pixbuf> rail,
				      Gtk::Adjustment *adj,
				      MIDI::Controllable *mcontrol,
				      bool with_numeric)

	: SliderController (slide, rail, adj, mcontrol, with_numeric)
{
	if (with_numeric) {
		spin_frame.add (spin);
		spin_frame.set_shadow_type (Gtk::SHADOW_IN);
		spin_frame.set_name ("BaseFrame");
		spin_hbox.pack_start (spin_frame, false, true);
		// pack_start (spin_hbox, false, false);
	}
}

HSliderController::HSliderController (Glib::RefPtr<Gdk::Pixbuf> slide,
				      Glib::RefPtr<Gdk::Pixbuf> rail,
				      Gtk::Adjustment *adj,
				      MIDI::Controllable *mcontrol,
				      bool with_numeric)
	
	: SliderController (slide, rail, adj, mcontrol, with_numeric)
{
	if (with_numeric) {
		spin_frame.add (spin);
		//spin_frame.set_shadow_type (Gtk::SHADOW_IN);
		spin_frame.set_name ("BaseFrame");
		spin_hbox.pack_start (spin_frame, false, true);
		// pack_start (spin_hbox, false, false);
	}
}
