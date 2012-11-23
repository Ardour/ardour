#include <string>

#include <pbd/failed_constructor.h>
#include <ardour/ardour.h>

#include "gui_thread.h"
#include "splash.h"

#include "i18n.h"

using namespace Gtk;
using namespace Glib;
using namespace std;
using namespace ARDOUR;

Splash* Splash::the_splash = 0;

Splash::Splash ()
{
	string path = find_data_file ("splash.png");

	if (path.empty()) {
		throw failed_constructor();
	}

	try {
		pixbuf = Gdk::Pixbuf::create_from_file (path);
	}

	catch (...) {
		throw failed_constructor();
	}
	
	darea.set_size_request (pixbuf->get_width(), pixbuf->get_height());
	set_keep_above (true);
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
	darea.signal_expose_event().connect (mem_fun (*this, &Splash::expose));

	add (darea);

	set_default_size (pixbuf->get_width(), pixbuf->get_height());
	the_splash = this;

	ARDOUR::BootMessage.connect (mem_fun (*this, &Splash::boot_message));
}

void
Splash::pop_back ()
{
	set_keep_above (false);
}

void
Splash::on_realize ()
{
	Window::on_realize ();

	// Without override redirect, splash screen has redraw problems with ion3.
	// With override redirect, it is not properly on top with some other
	// popular (metacity for example) window managers.
	// Maybe setting override redirect and something else (like the splash wm hint)
	// will make the splash to work for everybody
	// Override redirect only does not work on OS X too.
	// Until we find solution that works for everybody this env var kludge is used
	if (getenv ("ARDOUR_USE_OVERRIDE_REDIRECT_SPLASH") != 0) {
		get_window()->set_override_redirect (true);
	} else {
		get_window()->set_decorations (Gdk::WMDecoration (0));
	}

	layout->set_font_description (get_style()->get_font());
}


bool
Splash::on_button_release_event (GdkEventButton* ev)
{
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

	return true;
}

void
Splash::boot_message (std::string msg)
{
	message (msg);
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
		win->invalidate_rect (Gdk::Rectangle (0, darea.get_height() - 30,
						      darea.get_width(), 30), true);
		win->process_updates (true);
		gdk_flush ();
	}
}
