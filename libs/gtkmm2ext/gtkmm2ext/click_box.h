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

    $Id$
*/

#ifndef __gtkmm2ext_click_box_h__
#define __gtkmm2ext_click_box_h__

#include <string>
#include <gtkmm.h>

#include <gtkmm2ext/auto_spin.h>

namespace Gtkmm2ext {

class ClickBox : public Gtk::DrawingArea, public AutoSpin
{
  public:
	ClickBox (Gtk::Adjustment *adj, const std::string &name, bool round_to_steps = false);
	~ClickBox ();

	void set_print_func(void (*pf)(char buf[32], Gtk::Adjustment &, void *),
			    void *arg) {
		print_func = pf;
		print_arg = arg;
		set_label ();
	}


  protected:
	bool on_expose_event (GdkEventExpose*);

  private:
	void (*print_func) (char buf[32], Gtk::Adjustment &, void *);
	void *print_arg;

	Glib::RefPtr<Pango::Layout> layout;
	int twidth;
	int theight;

	void set_label ();
	bool button_press_handler (GdkEventButton *);
	bool button_release_handler (GdkEventButton *);

	static void default_printer (char buf[32], Gtk::Adjustment &, void *);
};

} /* namespace */

#endif  /* __gtkmm2ext_click_box_h__ */
