/*
    Copyright (C) 2012 Paul Davis 

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

#ifndef __gtk_ardour_nag_h__
#define __gtk_ardour_nag_h__

#include "ardour_dialog.h"

#include <gtkmm/label.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/buttonbox.h>

class NagScreen : public ArdourDialog
{
  public:
	~NagScreen();

	static NagScreen* maybe_nag (std::string context);
	void nag ();

  private:
	NagScreen (std::string context, bool maybe_subscriber);

	Gtk::Label message;
	Gtk::VButtonBox button_box;
	Gtk::RadioButtonGroup button_group;
	Gtk::RadioButton donate_button;
	Gtk::RadioButton subscribe_button;
	Gtk::RadioButton existing_button;
	Gtk::RadioButton next_time_button;
	Gtk::RadioButton never_again_button;

	void mark_never_again ();
	void mark_subscriber ();
	void mark_affirmed_subscriber ();
	void offer_to_donate ();
	void offer_to_subscribe ();
	static bool is_subscribed (bool& really);
};

#endif /* __gtk_ardour_nag_h__ */
