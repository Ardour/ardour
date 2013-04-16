#ifndef __CANVAS_PIXBUF__
#define __CANVAS_PIXBUF__

#include <glibmm/refptr.h>

#include "canvas/item.h"

namespace Gdk {
	class Pixbuf;
}

namespace ArdourCanvas {

class Pixbuf : public Item
{
public:
	Pixbuf (Group *);

	void render (Rect const &, Cairo::RefPtr<Cairo::Context>) const;
	void compute_bounding_box () const;

	void set (Glib::RefPtr<Gdk::Pixbuf>);

	/* returns the reference to the internal private pixbuf
	 * after changing data in the pixbuf a call to set()
	 * is mandatory to update the data on screen */
	Glib::RefPtr<Gdk::Pixbuf> pixbuf();

private:
	Glib::RefPtr<Gdk::Pixbuf> _pixbuf;
};

}
#endif
