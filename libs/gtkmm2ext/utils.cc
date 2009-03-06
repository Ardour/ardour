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

#include <map>

#include <gtk/gtkpaned.h>
#include <gtk/gtk.h>

#include <gtkmm2ext/utils.h>
#include <gtkmm/widget.h>
#include <gtkmm/button.h>
#include <gtkmm/window.h>
#include <gtkmm/paned.h>
#include <gtkmm/comboboxtext.h>

#include "i18n.h"

using namespace std;

void
Gtkmm2ext::get_ink_pixel_size (Glib::RefPtr<Pango::Layout> layout,
			       int& width,
			       int& height)
{
	Pango::Rectangle ink_rect = layout->get_ink_extents ();
	
	width = (ink_rect.get_width() + PANGO_SCALE / 2) / PANGO_SCALE;
	height = (ink_rect.get_height() + PANGO_SCALE / 2) / PANGO_SCALE;
}

void
Gtkmm2ext::set_size_request_to_display_given_text (Gtk::Widget &w, const gchar *text,
						   gint hpadding, gint vpadding)
	
{
	int width, height;
	w.ensure_style ();
	
	get_ink_pixel_size (w.create_pango_layout (text), width, height);
	w.set_size_request(width + hpadding, height + vpadding);
}

void
Gtkmm2ext::set_size_request_to_display_given_text (Gtk::Widget &w, 
				const std::vector<std::string>& strings,
					   gint hpadding, gint vpadding)
	
{
	int width, height;
	int width_max = 0;
	int height_max = 0;
	w.ensure_style ();
	
	for (vector<string>::const_iterator i = strings.begin(); 
		i != strings.end(); ++i) {
	get_ink_pixel_size (w.create_pango_layout (*i), width, height);
	width_max = max(width_max,width);
	height_max = max(height_max, height);
	}
	w.set_size_request(width_max + hpadding, height_max + vpadding);
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

void
Gtkmm2ext::set_decoration (Gtk::Window* win, Gdk::WMDecoration decor)
{
	win->get_window()->set_decorations (decor);
}

void Gtkmm2ext::set_treeview_header_as_default_label(Gtk::TreeViewColumn* c)
{
	gtk_tree_view_column_set_widget( c->gobj(), GTK_WIDGET(0) );
}

