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

#ifndef __pbd_gtkmm_bindable_button_h__
#define __pbd_gtkmm_bindable_button_h__

#include <gtkmm2ext/stateful_button.h>
#include <gtkmm2ext/popup.h>


namespace MIDI {
	class Controllable;
}


namespace Gtkmm2ext {

class BindableToggleButton : public Gtk::ToggleButton
{
   public:
	BindableToggleButton(MIDI::Controllable *);

	//: Create a check button with a label.
	//- You won't be able
	//- to add a widget in this button since it already has a {\class Gtk_Label}
	//- in it.
	explicit BindableToggleButton(MIDI::Controllable *, const string &label);

	virtual ~BindableToggleButton() {}
	
	void set_bind_button_state (guint button, guint statemask);
	void get_bind_button_state (guint &button, guint &statemask);
	
	void midicontrol_set_tip ();

	void midi_learn ();
	
  protected:

	Gtkmm2ext::PopUp     prompter;
	
	MIDI::Controllable* midi_control;

	guint bind_button;
	guint bind_statemask;

	bool prompting, unprompting;
	
	void init_events ();
	bool prompter_hiding (GdkEventAny *);
	void midicontrol_prompt ();
	void midicontrol_unprompt ();
	
	bool on_button_press_event (GdkEventButton *);
};

};

#endif
