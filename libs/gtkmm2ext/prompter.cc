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

#include <string>

#include <gtkmm2ext/prompter.h>

#include "i18n.h"

using namespace std;
using namespace Gtkmm2ext;

Prompter::Prompter (bool modal)
	: Gtk::Window (Gtk::WINDOW_POPUP),
	  ok (_("OK")),
	  cancel (_("Cancel"))
{
	set_position (Gtk::WIN_POS_MOUSE);
	set_name ("Prompter");
	set_modal (modal);

	add (packer);

	entryLabel.set_line_wrap (true);
	entryLabel.set_name ("PrompterLabel");

	entryBox.set_homogeneous (false);
	entryBox.set_spacing (25);
	entryBox.set_border_width (10);
	entryBox.pack_start (entryLabel);
	entryBox.pack_start (entry, false, false);

	buttonBox.set_homogeneous (true);
	buttonBox.set_border_width (10);
	buttonBox.pack_start (ok, false, true);
	buttonBox.pack_start (cancel, false, true);
	
	packer.pack_start (entryBox);
	packer.pack_start (buttonBox);

	entry.signal_activate().connect(mem_fun(*this,&Prompter::activated));
	ok.signal_clicked().connect(mem_fun(*this,&Prompter::activated));
	cancel.signal_clicked().connect(mem_fun(*this,&Prompter::cancel_click));
	signal_delete_event().connect (mem_fun (*this, &Prompter::deleted));

}	

void
Prompter::change_labels (string okstr, string cancelstr)
{
	dynamic_cast<Gtk::Label*>(ok.get_child())->set_text (okstr);
	dynamic_cast<Gtk::Label*>(cancel.get_child())->set_text (cancelstr);
}

void
Prompter::on_realize ()
{
	Gtk::Window::on_realize ();
	Glib::RefPtr<Gdk::Window> win (get_window());
	win->set_decorations (Gdk::WMDecoration (Gdk::DECOR_BORDER|Gdk::DECOR_RESIZEH|Gdk::DECOR_MENU));
}

void
Prompter::on_map ()
{
	entry.grab_focus();
	Gtk::Window::on_map ();
}
	
void
Prompter::activated ()

{
	status = entered;
	hide_all ();
	done ();
}

void
Prompter::cancel_click ()

{
	entry.set_text ("");
	status = cancelled;
	hide_all ();
	done ();
}

bool
Prompter::deleted (GdkEventAny *ev)
{
	cancel_click ();
	return false;
}

void
Prompter::get_result (string &str)

{
	str = entry.get_text ();
}
