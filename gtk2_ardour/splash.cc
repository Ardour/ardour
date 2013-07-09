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

#ifdef check
#undef check
#endif

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
	set_resizable (false);
	set_type_hint(Gdk::WINDOW_TYPE_HINT_SPLASHSCREEN);
	the_splash = this;

        expose_done = false;
        expose_is_the_one = false;

	ARDOUR::BootMessage.connect (msg_connection, invalidator (*this), boost::bind (&Splash::boot_message, this, _1), gui_context());
}

Splash::~Splash ()
{
	the_splash = 0;
}

void
Splash::pop_back_for (Gtk::Window& win)
{
#ifdef __APPLE__
        /* April 2013: window layering on OS X is a bit different to X Window. at present,
           the "restack()" functionality in GDK will only operate on windows in the same
           "level" (e.g. two normal top level windows, or two utility windows) and will not
           work across them. The splashscreen is on its own "StatusWindowLevel" so restacking 
           is not going to work.

           So for OS X, we just hide ourselves.
        */
        hide();
#else
	set_keep_above (false);
	get_window()->restack (win.get_window(), false);
#endif
}

void
Splash::pop_front ()
{

#ifdef __APPLE__
        if (get_window()) {
                show ();
        }
#else
	set_keep_above (true);
#endif
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

	/* this must execute AFTER the GDK idle update mechanism 
	 */
       
	if (expose_is_the_one) {
		Glib::signal_idle().connect (sigc::mem_fun (this, &Splash::idle_after_expose),
					     GDK_PRIORITY_REDRAW+2);
	}

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
		expose_is_the_one = false;
	} 

	pop_front ();
	present ();
	
	if (!was_mapped) {
		while (!expose_done) {
			gtk_main_iteration ();
		}
		gdk_display_flush (gdk_display_get_default());
	}
}

void
Splash::message (const string& msg)
{
	string str ("<b>");
	str += Glib::Markup::escape_text (msg);
	str += "</b>";

	layout->set_markup (str);
	Glib::RefPtr<Gdk::Window> win = darea.get_window();
	
	if (win) {
                expose_done = false;

		if (win->is_visible ()) {
			win->invalidate_rect (Gdk::Rectangle (0, darea.get_height() - 30, darea.get_width(), 30), true);
		} else {
			darea.queue_draw ();
		}

                while (!expose_done) {
                        if(gtk_main_iteration ()) return; // quit was called
                }
		gdk_display_flush (gdk_display_get_default());
	}
}

bool
Splash::on_map_event (GdkEventAny* ev)
{
	expose_is_the_one = true;
	return Window::on_map_event (ev);
}
