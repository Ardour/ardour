/*
    Copyright (C) 2002-2009 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <iostream>
#include "ardour/bundle.h"
#include "ardour/types.h"

#include "gui_thread.h"
#include "port_matrix_body.h"
#include "port_matrix.h"
#include "port_matrix_column_labels.h"
#include "port_matrix_row_labels.h"
#include "port_matrix_grid.h"

#include "i18n.h"

using namespace std;

PortMatrixBody::PortMatrixBody (PortMatrix* p)
	: _matrix (p),
	  _alloc_width (0),
	  _alloc_height (0),
	  _xoffset (0),
	  _yoffset (0),
	  _column_labels_border_x (0),
	  _column_labels_height (0),
	  _ignore_component_size_changed (false)
{
	_column_labels = new PortMatrixColumnLabels (p, this);
	_row_labels = new PortMatrixRowLabels (p, this);
	_grid = new PortMatrixGrid (p, this);

	_components.push_back (_column_labels);
	_components.push_back (_row_labels);
	_components.push_back (_grid);

	add_events (Gdk::LEAVE_NOTIFY_MASK | Gdk::POINTER_MOTION_MASK);
}


PortMatrixBody::~PortMatrixBody ()
{
	for (list<PortMatrixComponent*>::iterator i = _components.begin(); i != _components.end(); ++i) {
		delete *i;
	}
}

bool
PortMatrixBody::on_expose_event (GdkEventExpose* event)
{
	if (
		_matrix->visible_columns() == 0 || _matrix->visible_rows() == 0 ||
		_matrix->visible_columns()->bundles().empty() || _matrix->visible_rows()->bundles().empty()
		) {

		/* nothing to connect */

		cairo_t* cr = gdk_cairo_create (get_window()->gobj());

		cairo_set_source_rgb (cr, 0, 0, 0);
		cairo_rectangle (cr, 0, 0, _alloc_width, _alloc_height);
		cairo_fill (cr);

		string t;
		if (_matrix->type() == ARDOUR::DataType::NIL) {
			t = _("There are no ports to connect.");
		} else {
			t = string_compose (_("There are no %1 ports to connect."), _matrix->type().to_i18n_string());
		}

		cairo_text_extents_t ext;
		cairo_text_extents (cr, t.c_str(), &ext);

		cairo_set_source_rgb (cr, 1, 1, 1);
		cairo_move_to (cr, (_alloc_width - ext.width) / 2, (_alloc_height + ext.height) / 2);
		cairo_show_text (cr, t.c_str ());

		cairo_destroy (cr);

		return true;
	}

	Gdk::Rectangle const exposure (
		event->area.x, event->area.y, event->area.width, event->area.height
		);

	bool intersects;

	for (list<PortMatrixComponent*>::iterator i = _components.begin(); i != _components.end(); ++i) {

		Gdk::Rectangle r = exposure;

		/* the get_pixmap call may cause things to be rerendered and sizes to change,
		   so fetch the pixmap before calculating where to put it */
		GdkPixmap* p = (*i)->get_pixmap (get_window()->gobj());
		r.intersect ((*i)->parent_rectangle(), intersects);

		if (intersects) {

			gdk_draw_drawable (
				get_window()->gobj(),
				get_style()->get_fg_gc (Gtk::STATE_NORMAL)->gobj(),
				p,
				(*i)->parent_to_component_x (r.get_x()),
				(*i)->parent_to_component_y (r.get_y()),
				r.get_x(),
				r.get_y(),
				r.get_width(),
				r.get_height()
				);
		}

	}

	cairo_t* cr = gdk_cairo_create (get_window()->gobj());

	for (list<PortMatrixComponent*>::iterator i = _components.begin(); i != _components.end(); ++i) {
		cairo_save (cr);
		set_cairo_clip (cr, (*i)->parent_rectangle ());
		(*i)->draw_extra (cr);
		cairo_restore (cr);
	}

	cairo_destroy (cr);

	return true;
}

void
PortMatrixBody::on_size_request (Gtk::Requisition *req)
{
	pair<int, int> const col = _column_labels->dimensions ();
	pair<int, int> const row = _row_labels->dimensions ();
	pair<int, int> const grid = _grid->dimensions ();

	if (grid.first == 0 && grid.second == 0) {
		/* nothing to display */
		req->width = 256;
		req->height = 64;
		return;
	}

	/* don't ask for the maximum size of our contents, otherwise GTK won't
	   let the containing window shrink below this size */

	/* XXX these shouldn't be hard-coded */
	int const min_width = 512;
	int const min_height = 512;

	req->width = min (min_width, max (col.first, grid.first + row.first));
	req->height = min (min_height / _matrix->min_height_divisor(), col.second + grid.second);
}

void
PortMatrixBody::on_size_allocate (Gtk::Allocation& alloc)
{
	Gtk::EventBox::on_size_allocate (alloc);

	_alloc_width = alloc.get_width ();
	_alloc_height = alloc.get_height ();

	compute_rectangles ();
	_matrix->setup_scrollbars ();
}

void
PortMatrixBody::compute_rectangles ()
{
	/* full sizes of components */
	pair<uint32_t, uint32_t> const col = _column_labels->dimensions ();
	uint32_t const col_overhang = _column_labels->overhang ();
	pair<uint32_t, uint32_t> const row = _row_labels->dimensions ();
	pair<uint32_t, uint32_t> const grid = _grid->dimensions ();

	Gdk::Rectangle col_rect;
	Gdk::Rectangle row_rect;
	Gdk::Rectangle grid_rect;

	if (_matrix->arrangement() == PortMatrix::TOP_TO_RIGHT) {

		col_rect.set_x (0);
		_column_labels_border_x = col_overhang;
		col_rect.set_y (0);
		grid_rect.set_x (0);

		col_rect.set_width (min (col.first, _alloc_width));

		uint32_t const y = min (_alloc_height, col.second);
		col_rect.set_height (y);
		row_rect.set_y (y);
		row_rect.set_height (_alloc_height - y);
		grid_rect.set_y (y);
		grid_rect.set_height (_alloc_height - y);

		uint32_t x = 0;
		if (_alloc_width > (grid.first + row.first)) {
			x = grid.first;
		} else if (_alloc_width > row.first) {
			x = _alloc_width - row.first;
		}

		grid_rect.set_width (x);
		row_rect.set_x (x);
		row_rect.set_width (_alloc_width - x);


	} else if (_matrix->arrangement() == PortMatrix::LEFT_TO_BOTTOM) {

		col_rect.set_height (min (_alloc_height, col.second));
		row_rect.set_height (std::min (_alloc_height - col_rect.get_height(), row.second));

		row_rect.set_x (0);
		row_rect.set_y (_alloc_height - row_rect.get_height() - col_rect.get_height());
		row_rect.set_width (min (_alloc_width, row.first));

		grid_rect.set_x (row_rect.get_width());
		grid_rect.set_y (_alloc_height - row_rect.get_height() - col_rect.get_height());
		grid_rect.set_width (std::min (_alloc_width - row_rect.get_width(), grid.first));
		grid_rect.set_height (row_rect.get_height ());

		col_rect.set_width (grid_rect.get_width () + col_overhang);
		col_rect.set_x (row_rect.get_width() + grid_rect.get_width() - col_rect.get_width());
		_column_labels_border_x = col_rect.get_x () >= 0 ? col_rect.get_x () : 0;
		col_rect.set_y (_alloc_height - col_rect.get_height());
	}

	_column_labels_height = col_rect.get_height ();

	_row_labels->set_parent_rectangle (row_rect);
	_column_labels->set_parent_rectangle (col_rect);
	_grid->set_parent_rectangle (grid_rect);

	DimensionsChanged (); /* EMIT SIGNAL */
}

void
PortMatrixBody::setup ()
{
	/* Discard any old connections to bundles */

	_bundle_connections.drop_connections ();

	/* Connect to bundles so that we find out when their names change */

	if (_matrix->visible_rows()) {
		PortGroup::BundleList r = _matrix->visible_rows()->bundles ();
		for (PortGroup::BundleList::iterator i = r.begin(); i != r.end(); ++i) {

			(*i)->bundle->Changed.connect (_bundle_connections, invalidator (*this), boost::bind (&PortMatrixBody::rebuild_and_draw_row_labels, this), gui_context());

		}
	}

	if (_matrix->visible_columns()) {
		PortGroup::BundleList c = _matrix->visible_columns()->bundles ();
		for (PortGroup::BundleList::iterator i = c.begin(); i != c.end(); ++i) {
			(*i)->bundle->Changed.connect (_bundle_connections, invalidator (*this), boost::bind (&PortMatrixBody::rebuild_and_draw_column_labels, this), gui_context());
		}
	}

	for (list<PortMatrixComponent*>::iterator i = _components.begin(); i != _components.end(); ++i) {
		(*i)->setup ();
	}

	set_mouseover (PortMatrixNode ());

	_ignore_component_size_changed = true;
	compute_rectangles ();
	_ignore_component_size_changed = false;
}

uint32_t
PortMatrixBody::full_scroll_width ()
{
	return _grid->dimensions().first;

}

uint32_t
PortMatrixBody::alloc_scroll_width ()
{
	return _grid->parent_rectangle().get_width();
}

uint32_t
PortMatrixBody::full_scroll_height ()
{
	return _grid->dimensions().second;
}

uint32_t
PortMatrixBody::alloc_scroll_height ()
{
	return _grid->parent_rectangle().get_height();
}

/** Set x offset (for scrolling) */
void
PortMatrixBody::set_xoffset (uint32_t xo)
{
	_xoffset = xo;
	queue_draw ();
}

/** Set y offset (for scrolling) */
void
PortMatrixBody::set_yoffset (uint32_t yo)
{
	_yoffset = yo;
	queue_draw ();
}

bool
PortMatrixBody::on_button_press_event (GdkEventButton* ev)
{
	for (list<PortMatrixComponent*>::iterator i = _components.begin(); i != _components.end(); ++i) {
		if (Gdk::Region ((*i)->parent_rectangle()).point_in (ev->x, ev->y)) {
			(*i)->button_press (
				(*i)->parent_to_component_x (ev->x),
				(*i)->parent_to_component_y (ev->y),
				ev
				);
		}
	}

	return true;
}

bool
PortMatrixBody::on_button_release_event (GdkEventButton* ev)
{
	for (list<PortMatrixComponent*>::iterator i = _components.begin(); i != _components.end(); ++i) {
		if (Gdk::Region ((*i)->parent_rectangle()).point_in (ev->x, ev->y)) {
			(*i)->button_release (
				(*i)->parent_to_component_x (ev->x),
				(*i)->parent_to_component_y (ev->y),
				ev
				);
		} else {
			(*i)->button_release (
				-1, -1,
				ev
				);
		}
	}

	return true;
}

void
PortMatrixBody::rebuild_and_draw_grid ()
{
	_grid->require_rebuild ();
	queue_draw ();
}

void
PortMatrixBody::rebuild_and_draw_column_labels ()
{
	_column_labels->require_rebuild ();
	queue_draw ();
}

void
PortMatrixBody::rebuild_and_draw_row_labels ()
{
	_row_labels->require_rebuild ();
	queue_draw ();
}

bool
PortMatrixBody::on_leave_notify_event (GdkEventCrossing* ev)
{
	if (ev->type == GDK_LEAVE_NOTIFY) {
		set_mouseover (PortMatrixNode ());
	}

	return true;
}

bool
PortMatrixBody::on_motion_notify_event (GdkEventMotion* ev)
{
	bool done = false;

	for (list<PortMatrixComponent*>::iterator i = _components.begin(); i != _components.end(); ++i) {
		if (Gdk::Region ((*i)->parent_rectangle()).point_in (ev->x, ev->y)) {
			(*i)->motion (
				(*i)->parent_to_component_x (ev->x),
				(*i)->parent_to_component_y (ev->y)
				);

			done = true;
		}
	}


	if (!done) {
		set_mouseover (PortMatrixNode ());
	}

	return true;
}

void
PortMatrixBody::set_mouseover (PortMatrixNode const & n)
{
	list<PortMatrixNode> m;
	m.push_back (n);
	set_mouseover (m);
}

void
PortMatrixBody::set_mouseover (list<PortMatrixNode> const & n)
{
	if (n == _mouseover) {
		return;
	}

	/* Channel highlights are set up only on mouseovers, so
	   it's reasonable to remove all channel highlights here.
	   We can't let individual components clear their own highlights
	   because of the case where, say, the row labels set up some column
	   highlights, and then we ask the column labels to set up their
	   own highlights and they clear them out before they start.
	*/

	_row_labels->clear_channel_highlights ();
	_column_labels->clear_channel_highlights ();

	list<PortMatrixNode> old = _mouseover;
	_mouseover = n;

	for (list<PortMatrixComponent*>::iterator i = _components.begin(); i != _components.end(); ++i) {
		(*i)->mouseover_changed (old);
	}
}

void
PortMatrixBody::highlight_associated_channels (int dim, ARDOUR::BundleChannel h)
{
	ARDOUR::BundleChannel bc[2];
	bc[dim] = h;

	if (!PortMatrix::bundle_with_channels (bc[dim].bundle)) {
		return;
	}

	if (dim == _matrix->column_index()) {
		_column_labels->add_channel_highlight (bc[dim]);
	} else {
		_row_labels->add_channel_highlight (bc[dim]);
	}

	PortGroup::BundleList const b = _matrix->visible_ports(1 - dim)->bundles ();

	for (PortGroup::BundleList::const_iterator i = b.begin(); i != b.end(); ++i) {
	        for (uint32_t j = 0; j < (*i)->bundle->nchannels().n_total(); ++j) {

			if (!_matrix->should_show ((*i)->bundle->channel_type(j))) {
				continue;
			}

			bc[1 - dim] = ARDOUR::BundleChannel ((*i)->bundle, j);

			PortMatrixNode n;
			n.row = bc[_matrix->row_index()];
			n.column = bc[_matrix->column_index()];

			if (_matrix->get_association(n) != PortMatrixNode::NOT_ASSOCIATED) {
				if (dim == _matrix->column_index()) {
					_row_labels->add_channel_highlight (bc[1 - dim]);
				} else {
					_column_labels->add_channel_highlight (bc[1 - dim]);
				}
			}
		}
	}
}

void
PortMatrixBody::set_cairo_clip (cairo_t* cr, Gdk::Rectangle const & r) const
{
	cairo_rectangle (cr, r.get_x(), r.get_y(), r.get_width(), r.get_height());
	cairo_clip (cr);
}

void
PortMatrixBody::component_size_changed ()
{
	if (_ignore_component_size_changed) {
		return;
	}

	compute_rectangles ();
	_matrix->setup_scrollbars ();
}

pair<uint32_t, uint32_t>
PortMatrixBody::max_size () const
{
	pair<uint32_t, uint32_t> const col = _column_labels->dimensions ();
	pair<uint32_t, uint32_t> const row = _row_labels->dimensions ();
	pair<uint32_t, uint32_t> const grid = _grid->dimensions ();

	return make_pair (std::max (row.first, _column_labels->overhang()) + grid.first, col.second + grid.second);
}

/** @return x position at which the column labels meet the border of the matrix */
uint32_t
PortMatrixBody::column_labels_border_x () const
{
	return _column_labels_border_x;
}

uint32_t
PortMatrixBody::column_labels_height () const
{
	return _column_labels_height;
}
