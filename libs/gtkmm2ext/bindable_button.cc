/*
    Copyright (C) 2004 Paul Davis
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
#include <iostream>

#include <midi++/controllable.h>

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/bindable_button.h>

#include "i18n.h"

using namespace Gtkmm2ext;
using namespace std;

BindableToggleButton::BindableToggleButton (MIDI::Controllable *mc)
	: ToggleButton (),
	  prompter (Gtk::WIN_POS_MOUSE, 30000, false),
	  midi_control (mc),
	  bind_button (2),
	  bind_statemask (Gdk::CONTROL_MASK)

{			  
	init_events ();
}

BindableToggleButton::BindableToggleButton(MIDI::Controllable *mc, const string &label)
	: ToggleButton (label),
	  prompter (Gtk::WIN_POS_MOUSE, 30000, false),
	  midi_control (mc),
	  bind_button (2),
	  bind_statemask (Gdk::CONTROL_MASK)
{			  
	init_events ();
}


void
BindableToggleButton::init_events ()
{
	prompter.signal_unmap_event().connect (mem_fun (*this, &BindableToggleButton::prompter_hiding));
	
	prompting = false;
	unprompting = false;
	
	if (midi_control) {
		midi_control->learning_started.connect (mem_fun (*this, &BindableToggleButton::midicontrol_prompt));
		midi_control->learning_stopped.connect (mem_fun (*this, &BindableToggleButton::midicontrol_unprompt));
	}
}

void
BindableToggleButton::set_bind_button_state (guint button, guint statemask)
{
	bind_button = button;
	bind_statemask = statemask;
}

void
BindableToggleButton::get_bind_button_state (guint &button, guint &statemask)
{
	button = bind_button;
	statemask = bind_statemask;
}

void
BindableToggleButton::midi_learn()
{
	if (midi_control) {
		prompting = true;
		midi_control->learn_about_external_control ();
	}
}

bool
BindableToggleButton::on_button_press_event (GdkEventButton *ev)
{
	if ((ev->state & bind_statemask) && ev->button == bind_button) { 
		midi_learn ();
		return true;
	}
	
	return false;
}

bool
BindableToggleButton::prompter_hiding (GdkEventAny *ev)
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
BindableToggleButton::midicontrol_set_tip ()

{
	if (midi_control) {
		// Gtkmm2ext::UI::instance()->set_tip (evbox, midi_control->control_description());
	}
}

void
BindableToggleButton::midicontrol_prompt ()

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
BindableToggleButton::midicontrol_unprompt ()

{
	if (unprompting) {
		Gtkmm2ext::UI::instance()->touch_display (&prompter);
		unprompting = false;
	}
}


