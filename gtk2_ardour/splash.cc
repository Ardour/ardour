#include <string>

#include <pbd/failed_constructor.h>
#include <ardour/ardour.h>

#include "splash.h"

using namespace Gtk;
using namespace Glib;
using namespace std;
using namespace ARDOUR;

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
	
	set_size_request (pixbuf->get_width(), pixbuf->get_height());
	set_type_hint (Gdk::WINDOW_TYPE_HINT_SPLASHSCREEN);
	set_keep_above (true);
	set_position (WIN_POS_CENTER);
	add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
}

bool
Splash::on_button_release_event (GdkEventButton* ev)
{
	hide ();
}

bool
Splash::on_expose_event (GdkEventExpose* ev)
{
	RefPtr<Gdk::Window> window = get_window();

	Window::on_expose_event (ev);

	window->draw_pixbuf (get_style()->get_bg_gc (STATE_NORMAL), pixbuf,
			     ev->area.x, ev->area.y,
			     ev->area.x, ev->area.y,
			     ev->area.width, ev->area.height,
			     Gdk::RGB_DITHER_NONE, 0, 0);
	return true;
}
