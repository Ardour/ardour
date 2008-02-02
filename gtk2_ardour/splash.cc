#include <string>

#include <pbd/failed_constructor.h>
#include <pbd/file_utils.h>
#include <ardour/ardour.h>
#include <ardour/filesystem_paths.h>

#include "splash.h"

#include "i18n.h"

using namespace Gtk;
using namespace Glib;
using namespace std;
using namespace ARDOUR;

Splash* Splash::the_splash = 0;

Splash::Splash ()
{
	sys::path splash_file;

	if (!find_file_in_search_path (ardour_search_path(), "splash.png", splash_file)) {
		throw failed_constructor();
	}

	try {
		pixbuf = Gdk::Pixbuf::create_from_file (splash_file.to_string());
	}

	catch (...) {
		throw failed_constructor();
	}
	
	darea.set_size_request (pixbuf->get_width(), pixbuf->get_height());
	set_keep_above (true);
	set_position (WIN_POS_CENTER);
	darea.add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);

	layout = create_pango_layout ("");
	string str = "<b>";
	string i18n = _("Ardour loading ...");
	str += i18n;
	str += "</b>";

	layout->set_markup (str);

	darea.show ();
	darea.signal_expose_event().connect (mem_fun (*this, &Splash::expose));

	add (darea);

	the_splash = this;
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
	get_window()->set_decorations (Gdk::WMDecoration(0));
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

	window->draw_pixbuf (get_style()->get_bg_gc (STATE_NORMAL), pixbuf,
			     ev->area.x, ev->area.y,
			     ev->area.x, ev->area.y,
			     ev->area.width, ev->area.height,
			     Gdk::RGB_DITHER_NONE, 0, 0);

	Glib::RefPtr<Gtk::Style> style = darea.get_style();
	Glib::RefPtr<Gdk::GC> white = style->get_white_gc();

	window->draw_layout (white, 10, pixbuf->get_height() - 30, layout);

	return true;
}

void
Splash::message (const string& msg)
{
	string str ("<b>");
	str += msg;
	str += "</b>";

	layout->set_markup (str);
	darea.queue_draw ();
	get_window()->process_updates (true);
}
