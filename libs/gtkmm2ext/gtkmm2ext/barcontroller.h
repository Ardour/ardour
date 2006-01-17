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

#ifndef __gtkmm2ext_bar_controller_h__
#define __gtkmm2ext_bar_controller_h__

#include <gtkmm.h>
#include <gtkmm2ext/popup.h>

namespace MIDI {
	class Controllable;
}

namespace Gtkmm2ext {

class BarController : public Gtk::Frame
{
  public:
	BarController (Gtk::Adjustment& adj, MIDI::Controllable*, sigc::slot<void,char*,unsigned int>);
	virtual ~BarController () {}
	
	void set_bind_button_state (guint button, guint statemask);
	void get_bind_button_state (guint &button, guint &statemask);
	void midicontrol_set_tip ();
	void midi_learn ();

	void set_sensitive (bool yn) {
		darea.set_sensitive (yn);
	}
	
	enum Style {
		LeftToRight,
		RightToLeft,
		Line,
		CenterOut,
		TopToBottom,
		BottomToTop
	};

	Style style() const { return _style; }
	void set_style (Style);
	void set_with_text (bool yn);
	void set_use_parent (bool yn);

	Gtk::SpinButton& get_spin_button() { return spinner; }

	sigc::signal<void> StartGesture;
	sigc::signal<void> StopGesture;

	/* export this to allow direct connection to button events */

	Gtk::Widget& event_widget() { return darea; }

  protected:
	Gtk::Adjustment&    adjustment;
	Gtk::DrawingArea    darea;
	Gtkmm2ext::PopUp     prompter;
	MIDI::Controllable* midi_control;
	sigc::slot<void,char*,unsigned int> label_callback;
	Glib::RefPtr<Pango::Layout> layout;
	Style              _style;
	bool                grabbed;
	bool                switching;
	bool                switch_on_release;
	bool                with_text;
	double              initial_value;
	double              grab_x;
	GdkWindow*          grab_window;
	Gtk::SpinButton     spinner;
	bool                use_parent;

	guint bind_button;
	guint bind_statemask;
	bool prompting, unprompting;
	
	gint button_press (GdkEventButton *);
	gint button_release (GdkEventButton *);
	gint motion (GdkEventMotion *);
	gint expose (GdkEventExpose *);

	gint mouse_control (double x, GdkWindow* w, double scaling);

	gint prompter_hiding (GdkEventAny *);
	void midicontrol_prompt ();
	void midicontrol_unprompt ();
	void update_midi_control ();

	gint switch_to_bar ();
	gint switch_to_spinner ();

	void entry_activated ();
	gint entry_focus_out (GdkEventFocus*);
};


}; /* namespace */

#endif // __gtkmm2ext_bar_controller_h__		
