/*
    Copyright (C) 2014 Paul Davis

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

#ifndef __gtk_ardour_ruler_h__
#define __gtk_ardour_ruler _h__

#include <string>
#include <gtkmm/checkbutton.h>

#include "ardour_dialog.h"


class Editor;

class RulerDialog : public ArdourDialog
{
  public:
	RulerDialog ();
	~RulerDialog ();

  private:
	Gtk::CheckButton samples_button;
	Gtk::CheckButton timecode_button;
	Gtk::CheckButton minsec_button;
	Gtk::CheckButton bbt_button;
	Gtk::CheckButton tempo_button;
	Gtk::CheckButton meter_button;
	Gtk::CheckButton loop_punch_button;
	Gtk::CheckButton range_button;
	Gtk::CheckButton mark_button;
	Gtk::CheckButton cdmark_button;
	Gtk::CheckButton video_button;

	void connect_action (Gtk::CheckButton& button, std::string const &action_name_part);
	void on_response (int);
};

#endif /* __gtk_ardour_add_route_dialog_h__ */
