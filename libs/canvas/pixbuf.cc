#include <cairomm/cairomm.h>
#include <gdkmm/general.h>

#include "pbd/xml++.h"

#include "canvas/pixbuf.h"

using namespace std;
using namespace ArdourCanvas;

Pixbuf::Pixbuf (Group* g)
	: Item (g)
{
	
}

void
Pixbuf::render (Rect const & /*area*/, Cairo::RefPtr<Cairo::Context> context) const
{
	Gdk::Cairo::set_source_pixbuf (context, _pixbuf, 0, 0);
	context->paint ();
}
	
void
Pixbuf::compute_bounding_box () const
{
	if (_pixbuf) {
		_bounding_box = boost::optional<Rect> (Rect (0, 0, _pixbuf->get_width(), _pixbuf->get_height()));
	} else {
		_bounding_box = boost::optional<Rect> ();
	}

	_bounding_box_dirty = false;
}

void
Pixbuf::set (Glib::RefPtr<Gdk::Pixbuf> pixbuf)
{
	begin_change ();
	
	_pixbuf = pixbuf;
	_bounding_box_dirty = true;

	end_change ();
}

Glib::RefPtr<Gdk::Pixbuf>
Pixbuf::pixbuf() {
	return _pixbuf;
}

