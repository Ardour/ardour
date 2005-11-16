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

#include <gtk/gtkpaned.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/gtkutils.h>
#include <gtkmm/comboboxtext.h>

#include "i18n.h"

using namespace std;

void
Gtkmm2ext::set_size_request_to_display_given_text (Gtk::Widget &w, const gchar *text,
					    gint hpadding, gint vpadding)

{
	w.ensure_style ();
	set_size_request_to_display_given_text(w, text, hpadding, vpadding);
}

void
Gtkmm2ext::init ()
{
	// Necessary for gettext
	(void) bindtextdomain(PACKAGE, LOCALEDIR);
}

void
Gtkmm2ext::set_popdown_strings (Gtk::ComboBoxText& cr, const vector<string>& strings)
{
	cr.clear ();

	for (vector<string>::const_iterator i = strings.begin(); i != strings.end(); ++i) {
		cr.append_text (*i);
	}
}

GdkWindow*
Gtkmm2ext::get_paned_handle (Gtk::Paned& paned)
{
	return GTK_PANED(paned.gobj())->handle;
}
