/*
 * Copyright (C) 1999-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <string>
#include <gtkmm2ext/textviewer.h>

#include "pbd/i18n.h"

using namespace std;
using namespace Gtkmm2ext;
using namespace sigc;

TextViewer::TextViewer (size_t xsize, size_t ysize) :
	Gtk::Window (Gtk::WINDOW_TOPLEVEL),
	Transmitter (Transmitter::Info), /* channel arg is irrelevant */
	dismiss (_("Close"))
{
	set_size_request (xsize, ysize);

	set_title ("Text Viewer");
	set_name ("TextViewer");
	set_resizable (true);
	set_border_width (0);

	vbox1.set_homogeneous (false);
	vbox1.set_spacing (0);
	add (vbox1);
	vbox1.show ();

	vbox2.set_homogeneous (false);
	vbox2.set_spacing (10);
	//vbox2.set_border_width (10);

	vbox1.pack_start (vbox2, true, true, 0);
	vbox2.show ();

	vbox2.pack_start (scrollwin, TRUE, TRUE, 0);
	scrollwin.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_ALWAYS);
	scrollwin.show ();

	etext.set_editable (false);
	etext.set_wrap_mode (Gtk::WRAP_WORD);
	scrollwin.add (etext);
	etext.show ();

	vbox1.pack_start (dismiss, false, false, 0);
	dismiss.show ();

	dismiss.signal_clicked().connect(mem_fun (*this, &TextViewer::signal_released_handler));
}

void
TextViewer::signal_released_handler()
{
	hide();
}

void
TextViewer::scroll_to_bottom ()

{
        Gtk::Adjustment *adj;

 	adj = scrollwin.get_vadjustment();
 	adj->set_value (MAX(0,(adj->get_upper() - adj->get_page_size())));
}

void
TextViewer::deliver ()

{
	char buf[1024];
	Glib::RefPtr<Gtk::TextBuffer> tb (etext.get_buffer());

	while (!eof()) {
		read (buf, sizeof (buf));
		buf[gcount()] = '\0';
		string foo (buf);
		tb->insert (tb->end(), foo);
	}
	scroll_to_bottom ();
	clear ();
}
