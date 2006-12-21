#include <iostream>

#include "panner.h"

using namespace std;

static const int triangle_size = 7;

static void
null_label_callback (char* buf, unsigned int bufsize)
{
	/* no label */

	buf[0] = '\0';
}


PannerBar::PannerBar (Gtk::Adjustment& adj, PBD::Controllable& c)
	: BarController (adj, c, sigc::ptr_fun (null_label_callback))
{
	set_style (BarController::Line);
}

PannerBar::~PannerBar ()
{
}

bool
PannerBar::expose (GdkEventExpose* ev)
{
	Glib::RefPtr<Gdk::Window> win (darea.get_window());
	Glib::RefPtr<Gdk::GC> gc (get_style()->get_base_gc (get_state()));

	BarController::expose (ev);

	/* now draw triangles for left, right and center */

	GdkPoint points[3];

	// left
	
	points[0].x = 0;
	points[0].y = 0;

	points[1].x = triangle_size;
	points[1].y = 0;
	
	points[2].x = 0;
	points[2].y = triangle_size;

	gdk_draw_polygon (win->gobj(), gc->gobj(), true, points, 3);

	// center

	points[0].x = (darea.get_width()/2 - (triangle_size/2)) - 1;
	points[0].y = 0;

	points[1].x = (darea.get_width()/2 + (triangle_size/2)) - 1;
	points[1].y = 0;
	
	points[2].x = darea.get_width()/2 - 1;
	points[2].y = triangle_size - 1;

	gdk_draw_polygon (win->gobj(), gc->gobj(), true, points, 3); 

	// right

	points[0].x = (darea.get_width() - triangle_size) - 1;
	points[0].y = 0;

	points[1].x = darea.get_width();
	points[1].y = 0;
	
	points[2].x = darea.get_width();
	points[2].y = triangle_size;

	gdk_draw_polygon (win->gobj(), gc->gobj(), true, points, 3);

	return true;
}

bool
PannerBar::button_press (GdkEventButton* ev)
{
	if (ev->button == 1 && ev->type == GDK_BUTTON_PRESS && ev->y < 10) {
		if (ev->x < triangle_size) {
			return true;
		} else if (ev->x > (darea.get_width() - triangle_size)) {
			return true;
		} else if (ev->x > (darea.get_width()/2 - triangle_size) &&
			   ev->x < (darea.get_width()/2 + triangle_size)) {
			return true;
		}
	}

	return BarController::button_press (ev);
}

bool
PannerBar::button_release (GdkEventButton* ev)
{
	if (ev->button == 1 && ev->type == GDK_BUTTON_RELEASE && ev->y < 10) {
		if (ev->x < triangle_size) {
			adjustment.set_value (adjustment.get_lower());
			return true;
		} else if (ev->x > (darea.get_width() - triangle_size)) {
			adjustment.set_value (adjustment.get_upper());
			return true;
		} else if (ev->x > (darea.get_width()/2 - triangle_size) &&
			   ev->x < (darea.get_width()/2 + triangle_size)) {
			adjustment.set_value (adjustment.get_lower() + ((adjustment.get_upper() - adjustment.get_lower()) / 2.0));
			return true;
		}
	}

	return BarController::button_release (ev);
}

