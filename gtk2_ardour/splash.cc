/*
    Copyright (C) 2008 Paul Davis

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

#include <string>

#include "pbd/failed_constructor.h"
#include "pbd/file_utils.h"

#include "ardour/ardour.h"
#include "ardour/filesystem_paths.h"

#include "gui_thread.h"
#include "splash.h"

#include "i18n.h"

using namespace Gtk;
using namespace Glib;
using namespace PBD;
using namespace std;
using namespace ARDOUR;

Splash* Splash::the_splash = 0;

Splash::Splash ()
{
	assert (the_splash == 0);
	
	std::string splash_file;

	if (!find_file_in_search_path (ardour_data_search_path(), "splash.png", splash_file)) {
                cerr << "Cannot find splash screen image file\n";
		throw failed_constructor();
	}

	try {
		pixbuf = Gdk::Pixbuf::create_from_file (splash_file);
	}

	catch (...) {
                cerr << "Cannot construct splash screen image\n";
		throw failed_constructor();
	}

	darea.set_size_request (pixbuf->get_width(), pixbuf->get_height());
	pop_front ();
	set_position (WIN_POS_CENTER);
	darea.add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
	darea.set_double_buffered (false);

	layout = create_pango_layout ("");
	string str = "<b>";
	string i18n = string_compose (_("%1 loading ..."), PROGRAM_NAME);
	str += i18n;
	str += "</b>";

	layout->set_markup (str);

	darea.show ();
	darea.signal_expose_event().connect (sigc::mem_fun (*this, &Splash::expose));

	add (darea);

	set_default_size (pixbuf->get_width(), pixbuf->get_height());
	the_splash = this;

        expose_done = false;

	ARDOUR::BootMessage.connect (msg_connection, invalidator (*this), boost::bind (&Splash::boot_message, this, _1), gui_context());
}

Splash::~Splash ()
{
	the_splash = 0;
}

void
Splash::pop_back_for (Gtk::Window& win)
{
	set_keep_above (false);
	get_window()->restack (win.get_window(), false);
	win.signal_hide().connect (sigc::mem_fun (*this, &Splash::pop_front));
}

void
Splash::pop_front ()
{
	set_keep_above (true);
}

void
Splash::on_realize ()
{
	Window::on_realize ();
	get_window()->set_decorations (Gdk::WMDecoration(0));
	layout->set_font_description (get_style()->get_font());
}

bool
Splash::on_button_release_event (GdkEventButton* ev)
{
	RefPtr<Gdk::Window> window = get_window();
        
        if (!window || ev->window != window->gobj()) {
                return false;
        }
        
	hide ();
	return true;
}

bool
Splash::expose (GdkEventExpose* ev)
{
	RefPtr<Gdk::Window> window = darea.get_window();

	/* note: height & width need to be constrained to the pixbuf size
	   in case a WM provides us with a screwy allocation
	*/

	window->draw_pixbuf (get_style()->get_bg_gc (STATE_NORMAL), pixbuf,
			     ev->area.x, ev->area.y,
			     ev->area.x, ev->area.y,
			     min ((pixbuf->get_width() - ev->area.x), ev->area.width),
			     min ((pixbuf->get_height() - ev->area.y), ev->area.height),
			     Gdk::RGB_DITHER_NONE, 0, 0);

	Glib::RefPtr<Gtk::Style> style = darea.get_style();
	Glib::RefPtr<Gdk::GC> white = style->get_white_gc();

	window->draw_layout (white, 10, pixbuf->get_height() - 30, layout);
       
	Glib::signal_idle().connect (sigc::mem_fun (this, &Splash::idle_after_expose));

	return true;
}

void
Splash::boot_message (std::string msg)
{
	message (msg);
}

bool
Splash::idle_after_expose ()
{
        expose_done = true;
	return false;
}

void
Splash::display ()
{
	bool was_mapped = is_mapped ();
	
	if (!was_mapped) {
		expose_done = false;
	}

	pop_front ();
	present ();
	
	if (!was_mapped) {
		while (!expose_done) {
			gtk_main_iteration ();
		}
	}
}

void
Splash::message (const string& msg)
{
	string str ("<b>");
	str += msg;
	str += "</b>";

	layout->set_markup (str);
	Glib::RefPtr<Gdk::Window> win = darea.get_window();

	if (win) {
                expose_done = false;
                
		win->invalidate_rect (Gdk::Rectangle (0, darea.get_height() - 30, darea.get_width(), 30), true);

                while (!expose_done) {
                        gtk_main_iteration ();
                }
	}
}
