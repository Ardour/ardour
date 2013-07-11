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

#ifndef __gtkmm2ext_click_box_h__
#define __gtkmm2ext_click_box_h__

#ifdef interface
#undef interface
#endif

#include <string>
#include <gtkmm.h>

#include <gtkmm2ext/auto_spin.h>

namespace Gtkmm2ext {

class ClickBox : public Gtk::DrawingArea, public AutoSpin
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

  protected:
	bool on_expose_event (GdkEventExpose*);

  private:
	Glib::RefPtr<Pango::Layout> layout;
	int twidth;
	int theight;

	void set_label ();
	void style_changed (const Glib::RefPtr<Gtk::Style> &);
	bool button_press_handler (GdkEventButton *);
	bool button_release_handler (GdkEventButton *);

	sigc::slot<bool, char *, Gtk::Adjustment &> _printer;
};

} /* namespace */

#endif  /* __gtkmm2ext_click_box_h__ */
