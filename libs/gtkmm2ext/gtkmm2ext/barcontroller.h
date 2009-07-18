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

*/

#ifndef __gtkmm2ext_bar_controller_h__
#define __gtkmm2ext_bar_controller_h__

#include <gtkmm/frame.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm2ext/binding_proxy.h>

namespace ARDOUR {
	class Controllable;
}

namespace Gtkmm2ext {

class BarController : public Gtk::Frame
{
  public:
	BarController (Gtk::Adjustment& adj, PBD::Controllable&, sigc::slot<void,char*,unsigned int>);
	virtual ~BarController () {}
	
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

	void set_sensitive (bool yn);
	
	void set_logarithmic (bool yn) { logarithmic = yn; }

	Gtk::SpinButton& get_spin_button() { return spinner; }

	sigc::signal<void> StartGesture;
	sigc::signal<void> StopGesture;

	/* export this to allow direct connection to button events */

	Gtk::Widget& event_widget() { return darea; }
	PBD::Controllable* get_controllable() { return binding_proxy.get_controllable(); }

  protected:
	Gtk::Adjustment&    adjustment;
	BindingProxy        binding_proxy;
	Gtk::DrawingArea    darea;
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
	bool                logarithmic;

	virtual bool button_press (GdkEventButton *);
	virtual bool button_release (GdkEventButton *);
	virtual bool motion (GdkEventMotion *);
	virtual bool expose (GdkEventExpose *);
	virtual bool scroll (GdkEventScroll *);
	virtual bool entry_focus_out (GdkEventFocus*);

	gint mouse_control (double x, GdkWindow* w, double scaling);

	gint switch_to_bar ();
	gint switch_to_spinner ();

	void entry_activated ();
	void drop_grab ();
	
	int entry_input (double* new_value);
	bool entry_output ();
};


}; /* namespace */

#endif // __gtkmm2ext_bar_controller_h__		
