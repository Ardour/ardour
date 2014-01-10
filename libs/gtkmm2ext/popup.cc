/*
    Copyright (C) 1998-99 Paul Barton-Davis
 
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

#include <iostream>

#include <gtkmm2ext/popup.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/gtk_ui.h>

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

PopUp::PopUp (Gtk::WindowPosition pos, unsigned int showfor_msecs, bool doh)
	: Window (WINDOW_POPUP)
{
	add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
	signal_button_press_event().connect(mem_fun(*this,&PopUp::button_click));
	set_border_width (12);
	add (label);
	set_position (pos);

	delete_on_hide = doh;
	popdown_time = showfor_msecs;
	timeout = -1;
}


PopUp::~PopUp ()
{
}

void
PopUp::on_realize ()
{
	Gtk::Window::on_realize();
	get_window()->set_decorations (Gdk::WMDecoration (Gdk::DECOR_BORDER|Gdk::DECOR_RESIZEH));
}

gint
PopUp::remove_prompt_timeout (void *arg)
{
	PopUp *pup = (PopUp *) arg;

	pup->remove ();
	return FALSE;
}

static gint idle_delete (void *arg)
{
	delete static_cast<PopUp*> (arg);
	return FALSE;
}

void
PopUp::remove ()
{
	hide ();

	if (popdown_time != 0 && timeout != -1) {
		g_source_remove (timeout);
	}

	if (delete_on_hide) {
		std::cerr << "deleting prompter\n";
		g_idle_add (idle_delete, this);
	}
}
#define ENSURE_GUI_THREAD(slot) \
     if (!Gtkmm2ext::UI::instance()->caller_is_ui_thread()) {\
             Gtkmm2ext::UI::instance()->call_slot (MISSING_INVALIDATOR, (slot)); \
        return;\
     }


void
PopUp::touch ()
{
	ENSURE_GUI_THREAD (mem_fun (*this, &PopUp::touch));

	if (is_visible ()) {
		remove ();
	} else {
		set_size_request_to_display_given_text (label, my_text.c_str(), 25, 10);
		label.set_text (my_text);
		show_all ();
		
		if (popdown_time != 0) {
			timeout = g_timeout_add (popdown_time, 
						   remove_prompt_timeout, 
						   this);
		}
	}
}

gint
PopUp::button_click (GdkEventButton* /*ev*/)
{
	remove ();
	return TRUE;
}

void
PopUp::set_text (string txt)
{
	my_text = txt;
}

void
PopUp::set_name (string name)
{
	Window::set_name (name);
	label.set_name (name);
}

bool
PopUp::on_delete_event (GdkEventAny* /*ev*/)
{
	hide();

	if (popdown_time != 0 && timeout != -1) {
		g_source_remove (timeout);
	}	

	if (delete_on_hide) {
		std::cerr << "deleting prompter\n" << endl;
		g_idle_add (idle_delete, this);
	}

	return true;
}
