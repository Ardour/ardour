/*
    Copyright (C) 1999 Paul Barton-Davis

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

#ifndef _WIDGETS_CLICK_BOX_H_
#define _WIDGETS_CLICK_BOX_H_

#ifdef interface
#undef interface
#endif

#include <string>

#include <gtkmm/adjustment.h>
#include <gtkmm/drawingarea.h>

#include "widgets/auto_spin.h"
#include "widgets/binding_proxy.h"
#include "widgets/visibility.h"

namespace PBD {
	class Controllable;
}

namespace ArdourWidgets {

class LIBWIDGETS_API ClickBox : public Gtk::DrawingArea, public AutoSpin
{
  public:
	ClickBox (Gtk::Adjustment *adj, const std::string &name, bool round_to_steps = false);
	~ClickBox ();

	/** Set a slot to `print' the value to put in the box.
	 *  The slot should write the value of the Gtk::Adjustment
	 *  into the char array, and should return true if it has done the printing,
	 *  or false to use the ClickBox's default printing method.
	 */
	void set_printer (sigc::slot<bool, char *, Gtk::Adjustment &>);

	void set_controllable (boost::shared_ptr<PBD::Controllable> c) {
		_binding_proxy.set_controllable (c);
	}

  protected:
	bool on_expose_event (GdkEventExpose*);
	bool on_enter_notify_event (GdkEventCrossing* ev);
	bool on_leave_notify_event (GdkEventCrossing* ev);

	BindingProxy _binding_proxy;

  private:
	Glib::RefPtr<Pango::Layout> layout;
	int twidth;
	int theight;

	void set_label ();
	void style_changed (const Glib::RefPtr<Gtk::Style> &);
	bool button_press_handler (GdkEventButton *);
	bool button_release_handler (GdkEventButton *);
	bool on_scroll_event (GdkEventScroll*);

	sigc::slot<bool, char *, Gtk::Adjustment &> _printer;
};

} /* namespace */

#endif
