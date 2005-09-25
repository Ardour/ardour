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

#ifndef __ardour_message_h__
#define __ardour_message_h__

#include <string>

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>

#include "ardour_dialog.h"

class ArdourMessage : public ArdourDialog
{
  public:
	ArdourMessage (Gtk::Window* parent, 
		       std::string name, std::string msg, 
		       bool grabfocus = true, 
		       bool autorun = true);
	~ArdourMessage();

  private:
	Gtk::VBox   packer;
	Gtk::Button ok_button;
	Gtk::Label  label;
	
};

#endif // __ardour_message_h__
