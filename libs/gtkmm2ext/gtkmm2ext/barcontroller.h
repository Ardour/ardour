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
#include <cairo.h>

#include "gtkmm2ext/visibility.h"
#include "gtkmm2ext/binding_proxy.h"

namespace Gtkmm2ext {

class LIBGTKMM2EXT_API BarController : public Gtk::Frame
{
  public:
	BarController (Gtk::Adjustment& adj, boost::shared_ptr<PBD::Controllable>);

	virtual ~BarController ();

	enum barStyle {
		LeftToRight,
		RightToLeft,
		Line,
                Blob,
		CenterOut,
		
		TopToBottom,
		BottomToTop
	};

	barStyle style() const { return _style; }
	void set_style (barStyle);
	void set_use_parent (bool yn);

	void set_sensitive (bool yn);
	
	void set_logarithmic (bool yn) { logarithmic = yn; }

	sigc::signal<void> StartGesture;
	sigc::signal<void> StopGesture;

	/* export this to allow direct connection to button events */

	Gtk::Widget& event_widget() { return darea; }

	boost::shared_ptr<PBD::Controllable> get_controllable() { return binding_proxy.get_controllable(); }
	void set_controllable(boost::shared_ptr<PBD::Controllable> c) { binding_proxy.set_controllable(c); }

	/** Emitted when the adjustment spinner is activated or deactivated;
	 *  the parameter is true on activation, false on deactivation.
	 */
	sigc::signal<void, bool> SpinnerActive;

  protected:
	Gtk::Adjustment&    adjustment;
	BindingProxy        binding_proxy;
	Gtk::DrawingArea    darea;
	Glib::RefPtr<Pango::Layout> layout;
	barStyle              _style;
	bool                grabbed;
	bool                switching;
	bool                switch_on_release;
	double              initial_value;
	double              grab_x;
	GdkWindow*          grab_window;
	Gtk::SpinButton     spinner;
	bool                use_parent;
	bool                logarithmic;
        sigc::slot<std::string> _label_slot;
        bool                    _use_slot;

	virtual std::string get_label (double& /*x*/) {
		return "";
	}
	
	void create_patterns();
	Cairo::RefPtr<Cairo::Pattern> pattern;
	Cairo::RefPtr<Cairo::Pattern> shine_pattern;

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
