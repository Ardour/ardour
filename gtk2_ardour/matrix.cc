#include <gtkmm.h>
#include <cairo/cairo.h>
#include <stdlib.h>
#include <iostream>
#include <algorithm>
#include <stdint.h>
#include <cmath>
#include <map>
#include <vector>

#include "matrix.h"

using namespace std;
using namespace Gtk;
using namespace ARDOUR;

Matrix::Matrix ()
{
	alloc_width = 0;
	alloc_height = 0;
	line_width = 0;
	line_height = 0;
	labels_y_shift = 0;
	labels_x_shift = 0;
	arc_radius = 0;
	xstep = 0;
	ystep = 0;
	pixmap = 0;
	drawn = false;
	angle_radians = M_PI/4.0;
	motion_x = -1;
	motion_y = -1;

	add_events (Gdk::POINTER_MOTION_MASK|Gdk::LEAVE_NOTIFY_MASK);
}

void 
Matrix::set_ports (const list<string>& ports)
{
	ours = ports;
	reset_size ();
}

void 
Matrix::add_group (PortGroup& pg)
{
	for (vector<string>::const_iterator s = pg.ports.begin(); s != pg.ports.end(); ++s) {
		others.push_back (OtherPort (*s, pg));
	}
	reset_size ();
}

void
Matrix::remove_group (PortGroup& pg)
{
	for (list<OtherPort>::iterator o = others.begin(); o != others.end(); ) {
		if (&(*o).group() == &pg) {
			o = others.erase (o);
		} else {
			++o;
		}
	}
	reset_size ();
}

void
Matrix::hide_group (PortGroup& pg)
{
	reset_size();
}

void
Matrix::show_group (PortGroup& pg)
{
	reset_size ();
}

void
Matrix::setup_nodes ()
{
	int n, x, y;
	list<string>::iterator m;
	list<OtherPort>::iterator s;

	for (vector<MatrixNode*>::iterator p = nodes.begin(); p != nodes.end(); ++p) {
		delete *p;
	}
	nodes.clear ();

	list<OtherPort>::size_type visible_others = 0;
	
	for (list<OtherPort>::iterator s = others.begin(); s != others.end(); ++s) {
		if ((*s).visible()) {
			++visible_others;
		}
	}
	
	nodes.assign (ours.size() * visible_others, 0);

	for (n = 0, y = 0, m = ours.begin(); m != ours.end(); ++m, ++y) {
		for (x = 0, s = others.begin(); s != others.end(); ++s) {
			if ((*s).visible()) {
				nodes[n] = new MatrixNode (*m, *s, x, y);
				n++;
				x++;
			}
		}
	}
}

void
Matrix::reset_size ()
{
	list<OtherPort>::size_type visible_others = 0;
	
	for (list<OtherPort>::iterator s = others.begin(); s != others.end(); ++s) {
		if ((*s).visible()) {
			++visible_others;
		}
	}
	
	border = 10;

	if (alloc_width > line_width) {

		xstep = (alloc_width - labels_x_shift - (2 * border) - (2 * arc_radius)) / visible_others;
		line_width = xstep * (others.size() - 1);

		ystep = (alloc_height - labels_y_shift - (2 * border) - (2 * arc_radius)) / (ours.size() - 1);
		line_height = ystep * (ours.size() - 1);

	} else {

		xstep = 20;
		ystep = 20;
		
		line_height = (ours.size() - 1) * ystep;
		line_width = visible_others * xstep;
	}

	int half_step = min (ystep/2,xstep/2);
	if (half_step > 3) {
		arc_radius = half_step - 5;
	} else {
		arc_radius = 3;
	}

	arc_radius = min (arc_radius, 10);

	/* scan all the port names that will be rotated, and compute
	   how much space we need for them
	*/
	
	float w = 0;
	float h = 0;
	cairo_text_extents_t extents;
	cairo_t* cr;
	GdkPixmap* pm;

	pm = gdk_pixmap_new (NULL, 1, 1, 24);
	gdk_drawable_set_colormap (pm, gdk_colormap_get_system());

	cr = gdk_cairo_create (pm);

	for (list<OtherPort>::iterator s = others.begin(); s != others.end(); ++s) {
		if ((*s).visible()) {
			cairo_text_extents (cr, (*s).name().c_str(), &extents);
			w = max ((float) extents.width, w);
			h = max ((float) extents.height, h);
		}
	}

	cairo_destroy (cr);
	gdk_pixmap_unref (pm);

	/* transform */

	w = fabs (w * cos (angle_radians) + h * sin (angle_radians));
	h = fabs (w * sin (angle_radians) + h * cos (angle_radians));

	labels_y_shift = (int) ceil (h) + 10;
	labels_x_shift = (int) ceil (w);

	setup_nodes ();
}

bool
Matrix::on_motion_notify_event (GdkEventMotion* ev)
{
	motion_x = ev->x;
	motion_y = ev->y;
	queue_draw ();
	return false;
}

bool
Matrix::on_leave_notify_event (GdkEventCrossing *ev)
{
	motion_x = -1;
	motion_y = -1;
	queue_draw ();
	return false;
}

void
Matrix::on_size_request (Requisition* req)
{
	req->width = labels_x_shift + line_width + (2*border) + (2*arc_radius);
	req->height = labels_y_shift + line_height + (2*border) + (2*arc_radius);
}

MatrixNode*
Matrix::get_node (int32_t x, int32_t y)
{
	int half_xstep = xstep / 2;
	int half_ystep = ystep / 2;

	x -= labels_x_shift - border;
	if (x < half_xstep) {
		return 0;
	}

	y -= labels_y_shift - border;
	if (y < half_ystep) {
		return 0;
	}

	x = (x - half_xstep) / xstep;
	y = (y - half_ystep) / ystep;

	x = y*ours.size() + x;

	if (x >= nodes.size()) {
		return 0;
	}

	return nodes[x];
}

bool
Matrix::on_button_press_event (GdkEventButton* ev)
{
	MatrixNode* node;
	
	if ((node = get_node (ev->x, ev->y)) != 0) {
		cerr << "Event in node " << node->our_name() << " x " << node->their_name () << endl;
		node->set_connected (!node->connected());
		drawn = false;
		queue_draw();
	} 
}

void
Matrix::alloc_pixmap ()
{
	if (pixmap) {
		gdk_pixmap_unref (pixmap);
	}

	pixmap = gdk_pixmap_new (get_window()->gobj(),
				 alloc_width,
				 alloc_height,
				 -1);

	drawn = false;
}

void
Matrix::on_size_allocate (Allocation& alloc)
{
	EventBox::on_size_allocate (alloc);

  	alloc_width = alloc.get_width();
  	alloc_height = alloc.get_height();

	if (is_realized()) {
		alloc_pixmap ();
		reset_size ();
#ifdef MATRIX_USE_BACKING_PIXMAP
		redraw (pixmap, 0, 0, alloc_width, alloc_height);
#endif
	}
}

void
Matrix::on_realize ()
{
	EventBox::on_realize ();
	alloc_pixmap ();
}

void
Matrix::redraw (GdkDrawable* drawable, GdkRectangle* rect)
{
	list<string>::iterator o;
	list<OtherPort>::iterator t;
	int x, y;
	uint32_t top_shift, bottom_shift, left_shift, right_shift;
	cairo_t* cr;

	cr = gdk_cairo_create (drawable);

	cairo_set_source_rgb (cr, 0.83, 0.83, 0.83);
	cairo_rectangle (cr, rect->x, rect->y, rect->width, rect->height);
	cairo_fill (cr);

	cairo_set_line_width (cr, 0.5);
	
	top_shift = labels_y_shift + border;
	left_shift = labels_x_shift + border;
	bottom_shift = 0;
	right_shift = 0;

	/* horizontal grid lines and side labels */

	for (y = top_shift, o = ours.begin(); o != ours.end(); ++o, y += ystep) {
		
		cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
		cairo_move_to (cr, left_shift, y);
		cairo_line_to (cr, left_shift+line_width, y);
		cairo_stroke (cr);
#if 0
		
		cairo_text_extents_t extents;
		cairo_text_extents (cr, (*o).c_str(),&extents);
		cairo_move_to (cr, border, y+extents.height/2);
		cairo_show_text (cr, (*o).c_str());
#endif

	}

	/* vertical grid lines and rotated labels*/

	for (x = left_shift, t = others.begin(); t != others.end(); ++t, x += xstep) {

		cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
		cairo_move_to (cr, x, top_shift);
		cairo_line_to (cr, x, top_shift+line_height);
		cairo_stroke (cr);

		cairo_move_to (cr, x-left_shift+12, border);
		cairo_set_source_rgb (cr, 0, 0, 1.0);
		
		cairo_save (cr);
		cairo_rotate (cr, angle_radians);
		cairo_show_text (cr, (*t).name().c_str());
		cairo_restore (cr);

	}

	/* nodes */

	for (vector<MatrixNode*>::iterator n = nodes.begin(); n != nodes.end(); ++n) {

		x = (*n)->x() * xstep;
		y = (*n)->y() * ystep;

		cairo_new_path (cr);

		if (arc_radius) {
			cairo_arc (cr, left_shift+x, top_shift+y, arc_radius, 0, 2.0 * M_PI);
			if ((*n)->connected()) {
				cairo_set_source_rgba (cr, 1.0, 0, 0, 1.0);
				cairo_stroke (cr);
			} else {
				cairo_set_source_rgba (cr, 1.0, 0, 0, 0.7);
				cairo_fill (cr);
			}
		}
	}

	/* motion indicators */

	if (motion_x >= left_shift && motion_y >= top_shift) {
		
		int col_left = left_shift + ((motion_x - left_shift) / xstep) * xstep;
		int row_top = top_shift + ((motion_y - top_shift) / ystep) * ystep;

		cairo_set_line_width (cr, 5);
		cairo_set_source_rgba (cr, 1.0, 0.0, 0.0, 0.3);
		
		/* horizontal (row) */

		cairo_line_to (cr, left_shift, row_top);
		cairo_line_to (cr, left_shift + line_width, row_top);
		cairo_stroke (cr);

		/* vertical (col) */
		
		cairo_move_to (cr, col_left, top_shift);
		cairo_line_to (cr, col_left, top_shift + line_height);
		cairo_stroke (cr);
	}

	cairo_destroy (cr);

#ifdef MATRIX_USE_BACKING_PIXMAP
	drawn = true;
#endif
}

bool
Matrix::on_expose_event (GdkEventExpose* event)
{
#ifdef MATRIX_USE_BACKING_PIXMAP
	if (!drawn) {
		redraw (pixmap, 0, 0, alloc_width, alloc_height);
	}

	gdk_draw_drawable (get_window()->gobj(),
			   get_style()->get_fg_gc (STATE_NORMAL)->gobj(),
			   pixmap,
			   event->area.x,
			   event->area.y,
			   event->area.x,
			   event->area.y,
			   event->area.width,
			   event->area.height);
#else
	redraw (get_window()->gobj(), &event->area);
#endif
	


	return true;
}
