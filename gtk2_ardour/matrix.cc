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
#include "port_matrix.h"

using namespace std;
using namespace Gtk;
using namespace ARDOUR;

Matrix::Matrix (PortMatrix* p) : _port_matrix (p)
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
	angle_radians = M_PI / 4.0;
	motion_x = -1;
	motion_y = -1;

	border = 10;

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

	if (pg.visible) {
		reset_size ();
	}
}


void
Matrix::clear ()
{
	others.clear ();
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

	if (pg.visible) {
		reset_size ();
	}
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
	for (vector<MatrixNode*>::iterator p = nodes.begin(); p != nodes.end(); ++p) {
		delete *p;
	}
	
	nodes.clear ();

	nodes.assign (ours.size() * get_visible_others (), 0);

	int n, x, y;
	list<string>::iterator m;
	list<OtherPort>::iterator s;
	
	for (n = 0, y = 0, m = ours.begin(); m != ours.end(); ++m, ++y) {
		for (x = 0, s = others.begin(); s != others.end(); ++s) {
			if (s->visible ()) {
				bool const c = _port_matrix->get_state (y, s->name());
				nodes[n] = new MatrixNode (*m, *s, c, x, y);
				n++;
				x++;
			}
		}
	}
}


void
Matrix::other_name_size_information (double* rotated_width, double* rotated_height, double* typical_height) const
{
	double w = 0;
	double h = 0;

	GdkPixmap* pm = gdk_pixmap_new (NULL, 1, 1, 24);
	gdk_drawable_set_colormap (pm, gdk_colormap_get_system());
	cairo_t* cr = gdk_cairo_create (pm);

	for (list<OtherPort>::const_iterator s = others.begin(); s != others.end(); ++s) {
		if (s->visible()) {
			
			cairo_text_extents_t extents;
			cairo_text_extents (cr, s->short_name().c_str(), &extents);
			
			if (extents.width > w) {
				w = extents.width;
				h = extents.height;
			}
		}
	}

	cairo_destroy (cr);
	gdk_pixmap_unref (pm);

	/* transform */

	*rotated_width = fabs (w * cos (angle_radians) + h * sin (angle_radians));
	*rotated_height = fabs (w * sin (angle_radians) + h * cos (angle_radians));
	*typical_height = h;
}


std::pair<int, int>
Matrix::ideal_size () const
{
	double rw;
	double rh;
	double th;

	other_name_size_information (&rw, &rh, &th);

	double const ideal_xstep = th * 2;
	double const ideal_ystep = 16;

	uint32_t const visible_others = get_visible_others ();

	return std::make_pair (
		int (rw + (2 * border) + ideal_xstep * visible_others),
		int (rh + (2 * border) + ideal_ystep * ours.size ())
		);
}


void
Matrix::reset_size ()
{
	double rw;
	double rh;
	double th;
			  
	other_name_size_information (&rw, &rh, &th);

	/* y shift is the largest transformed text height plus a bit for luck */
	labels_y_shift = int (ceil (rh) + 10);
	/* x shift is the width of the leftmost label */
	labels_x_shift = int (ceil (rw));

	uint32_t const visible_others = get_visible_others ();

	if (!visible_others) {
		xstep = 1;
		ystep = 1;
		line_width = 1;
		line_height = 1;
		arc_radius = 3;
		return;
	}

	if (ours.size () > 1) {

		xstep = (alloc_width - labels_x_shift - (2 * border)) / visible_others;
		line_width = xstep * (visible_others - 1);

		ystep = (alloc_height - labels_y_shift - (2 * border)) / (ours.size() - 1);
		line_height = ystep * (ours.size() - 1);

	} else {

		/* we have <= 1 of our ports, so steps don't matter */
		
		xstep = 20;
		ystep = 20;
		
		line_height = (ours.size() - 1) * ystep;
		line_width = visible_others * xstep;
	}

	int half_step = min (ystep / 2, xstep / 2);
	if (half_step > 3) {
		arc_radius = half_step - 5;
	} else {
		arc_radius = 3;
	}

	arc_radius = min (arc_radius, 10);


	setup_nodes ();

 	// cerr << "Based on ours = " << ours.size() << " others = " << others.size()
 	//      << " dimens = "
 	//      << " xstep " << xstep << endl
 	//      << " ystep " << ystep << endl
 	//      << " line_width " << line_width << endl
 	//      << " line_height " << line_height << endl
 	//      << " border " << border << endl
 	//      << " arc_radius " << arc_radius << endl
 	//      << " labels_x_shift " << labels_x_shift << endl
 	//      << " labels_y_shift " << labels_y_shift << endl;
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
	std::pair<int, int> const is = ideal_size ();
	req->width = is.first;
	req->height = is.second;
}

MatrixNode*
Matrix::get_node (int32_t x, int32_t y)
{
	int const half_xstep = xstep / 2;
	int const half_ystep = ystep / 2;

	x -= labels_x_shift + border;
	if (x < -half_xstep) {
		return 0;
	}

	y -= labels_y_shift + border;
	if (y < -half_ystep) {
		return 0;
	}

	x = (x + half_xstep) / xstep;
	y = (y + half_ystep) / ystep;

	x = y * get_visible_others () + x;

	if (x >= int32_t (nodes.size())) {
		return 0;
	}

	return nodes[x];
}

bool
Matrix::on_button_press_event (GdkEventButton* ev)
{
	MatrixNode* node;
	
	if ((node = get_node (ev->x, ev->y)) != 0) {
		node->set_connected (!node->connected());
		_port_matrix->set_state (node->y (), node->their_name (), node->connected (), 0);
		drawn = false;
		queue_draw();
		return true;
	}

	return false;
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

	cairo_t* cr = gdk_cairo_create (drawable);

	cairo_set_source_rgb (cr, 0.83, 0.83, 0.83);
	cairo_rectangle (cr, rect->x, rect->y, rect->width, rect->height);
	cairo_fill (cr);

	cairo_set_line_width (cr, 0.5);
	
	int32_t const top_shift = labels_y_shift + border;
	int32_t const left_shift = labels_x_shift + border;

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
		cairo_show_text (cr, t->short_name().c_str());
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
				cairo_fill (cr);
			} else {
				cairo_set_source_rgba (cr, 1.0, 0, 0, 0.7);
				cairo_stroke (cr);
			}
		}
	}

	/* motion indicators */

	if (motion_x >= left_shift && motion_y >= top_shift) {
		
		int col_left = left_shift + ((motion_x + (xstep / 2) + - left_shift) / xstep) * xstep;
		int row_top = top_shift + ((motion_y + (ystep / 2) - top_shift) / ystep) * ystep;

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

uint32_t
Matrix::get_visible_others () const
{
	uint32_t v = 0;
	
	for (list<OtherPort>::const_iterator s = others.begin(); s != others.end(); ++s) {
		if (s->visible()) {
			++v;
		}
	}

	return v;
}

MatrixNode::MatrixNode (std::string a, OtherPort o, bool c, int32_t x, int32_t y)
	: _name (a), them (o), _connected (c), _x(x), _y(y)
{
	
}

std::string
OtherPort::name () const
{
	return _group.prefix + _short_name;
}
