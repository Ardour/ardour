/*
 * Copyright (C) 2008-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2012-2014 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2012-2017 Robin Gareus <robin@gareus.org>
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

#include "pbd/failed_constructor.h"
#include "pbd/file_utils.h"

#include "ardour/ardour.h"
#include "ardour/filesystem_paths.h"

#include "gtkmm2ext/utils.h"

#ifdef check
#undef check
#endif

#include "gui_thread.h"
#include "opts.h"
#include "splash.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace Glib;
using namespace PBD;
using namespace std;
using namespace ARDOUR;

Splash* Splash::the_splash = 0;

Splash*
Splash::instance()
{
	if (!the_splash) {
		the_splash = new Splash;
	}
	return the_splash;
}

bool
Splash::exists ()
{
	return the_splash;
}

void
Splash::drop ()
{
	delete the_splash;
	the_splash = 0;
}

Splash::Splash ()
{
	assert (the_splash == 0);

	std::string splash_file;

	Searchpath rc (ARDOUR::ardour_data_search_path());
	rc.add_subdirectory_to_paths ("resources");

	if (!find_file (rc, PROGRAM_NAME "-splash.png", splash_file)) {
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

	if (!ARDOUR_COMMAND_LINE::no_splash) {
		ARDOUR::BootMessage.connect (msg_connection, invalidator (*this), boost::bind (&Splash::boot_message, this, _1), gui_context());
		present ();
	}
}

Splash::~Splash ()
{
	idle_connection.disconnect ();
	expose_done = true;
	hide ();
	the_splash = 0;
}

void
Splash::pop_back_for (Gtk::Window& win)
{
	set_keep_above (false);
#if defined  __APPLE__ || defined PLATFORM_WINDOWS
	/* April 2013: window layering on OS X is a bit different to X Window. at present,
	 * the "restack()" functionality in GDK will only operate on windows in the same
	 * "level" (e.g. two normal top level windows, or two utility windows) and will not
	 * work across them. The splashscreen is on its own "StatusWindowLevel" so restacking
	 * is not going to work.
	 *
	 * So for OS X, we just hide ourselves.
	 *
	 * Oct 2014: The Windows situation is similar, although it should be possible
	 * to play tricks with gdk's set_type_hint() or directly hack things using
	 * SetWindowLong() and UpdateLayeredWindow()
	 */
	(void) win;
	hide();
#else
	if (UIConfiguration::instance().get_hide_splash_screen ()) {
		hide ();
	} else if (is_mapped()) {
		get_window()->restack (win.get_window(), false);
		if (0 == win.get_transient_for ()) {
			win.set_transient_for (*this);
		}
	}
#endif
	_window_stack.insert (&win);
}

void
Splash::pop_front_for (Gtk::Window& win)
{
#ifndef NDEBUG
	assert (1 == _window_stack.erase (&win));
#else
	_window_stack.erase (&win);
#endif
	if (_window_stack.empty ()) {
		display ();
	}
}

void
Splash::pop_front ()
{
	if (!_window_stack.empty ()) {
		return;
	}

	if (ARDOUR_COMMAND_LINE::no_splash) {
		return;
	}

	if (get_window()) {
#if defined  __APPLE__ || defined PLATFORM_WINDOWS
		show ();
#else
		if (UIConfiguration::instance().get_hide_splash_screen ()) {
			show ();
		} else {
			unset_transient_for ();
			gdk_window_restack (get_window()->gobj(), NULL, true);
		}
#endif
		set_keep_above (true);
	}
}

void
Splash::hide ()
{
	Gtk::Window::hide();
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
		idle_connection = Glib::signal_idle().connect (
				sigc::mem_fun (this, &Splash::idle_after_expose),
				GDK_PRIORITY_REDRAW+2);
	}

	return true;
}

void
Splash::boot_message (std::string msg)
{
	if (!is_visible() && _window_stack.empty ()) {
		display ();
	}
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

	if (ARDOUR_COMMAND_LINE::no_splash) {
		return;
	}

	if (!was_mapped) {
		expose_done = false;
		expose_is_the_one = false;
	}

	pop_front ();
	present ();

	if (!was_mapped) {
		int timeout = 50;
		darea.queue_draw ();
		while (!expose_done && --timeout) {
			gtk_main_iteration ();
		}
		gdk_display_flush (gdk_display_get_default());
	}
}

void
Splash::message (const string& msg)
{
	string str ("<b>");
	str += Gtkmm2ext::markup_escape_text (msg);
	str += "</b>";

	layout->set_markup (str);
	Glib::RefPtr<Gdk::Window> win = darea.get_window();

	if (win) {
		if (win->is_visible ()) {
			win->invalidate_rect (Gdk::Rectangle (0, darea.get_height() - 30, darea.get_width(), 30), true);
		} else {
			darea.queue_draw ();
		}
		if (expose_done) {
			ARDOUR::GUIIdle ();
		}
	}
}

bool
Splash::on_map_event (GdkEventAny* ev)
{
	expose_is_the_one = true;
	return Window::on_map_event (ev);
}
