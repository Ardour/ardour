#include "cairo_widget.h"
#include "gui_thread.h"

CairoWidget::CairoWidget ()
	: _width (1),
	  _height (1),
	  _dirty (true),
	  _pixmap (0)
{

}

CairoWidget::~CairoWidget ()
{
	if (_pixmap) {
		gdk_pixmap_unref (_pixmap);
	}
}

bool
CairoWidget::on_expose_event (GdkEventExpose *event)
{
	Gdk::Rectangle const exposure (
		event->area.x, event->area.y, event->area.width, event->area.height
		);

	Gdk::Rectangle r = exposure;
	Gdk::Rectangle content (0, 0, _width, _height);
	bool intersects;
	r.intersect (content, intersects);
	
	if (intersects) {

		GdkDrawable* drawable = get_window()->gobj ();

		if (_dirty) {

			if (_pixmap) {
				gdk_pixmap_unref (_pixmap);
			}

			_pixmap = gdk_pixmap_new (drawable, _width, _height, -1);

			cairo_t* cr = gdk_cairo_create (_pixmap);
			render (cr);
			cairo_destroy (cr);

			_dirty = false;
		}

		gdk_draw_drawable (
			drawable,
			get_style()->get_fg_gc (Gtk::STATE_NORMAL)->gobj(),
			_pixmap,
 			r.get_x(),
 			r.get_y(),
 			r.get_x(),
 			r.get_y(),
 			r.get_width(),
 			r.get_height()
			);
	}
	
	return true;
}

void
CairoWidget::set_dirty ()
{
	ENSURE_GUI_THREAD (mem_fun (*this, &CairoWidget::set_dirty));

	_dirty = true;
	queue_draw ();
}

/** Handle a size allocation.
 *  @param alloc GTK allocation.
 */
void
CairoWidget::on_size_allocate (Gtk::Allocation& alloc)
{
	Gtk::EventBox::on_size_allocate (alloc);

	_width = alloc.get_width ();
	_height = alloc.get_height ();

	set_dirty ();
}
