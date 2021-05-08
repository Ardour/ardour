/*
 * Copyright (C) 2020 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <cassert>

#include "gtkmm2ext/colors.h"
#include "gtkmm2ext/utils.h"

#include "widgets/frame.h"
#include "widgets/ui_config.h"

using namespace std;
using namespace Gtk;
using namespace ArdourWidgets;

Frame::Frame (Orientation orientation, bool boxy)
	: _orientation (orientation)
	, _w (0)
	, _current_parent (0)
	, _border (0)
	, _padding (4)
	, _label_pad_w (5)
	, _label_pad_h (2)
	, _label_left (12)
	, _text_width (0)
	, _text_height (0)
	, _alloc_x0 (0)
	, _alloc_y0 (0)
	, _boxy (boxy)
{
	set_name ("Frame");
	ensure_style ();
	_min_size.width = _min_size.height = 0;
	_layout = Pango::Layout::create (get_pango_context ());

	/* provide bg color for cairo child widgets */
	g_object_set_data (G_OBJECT (gobj ()), "has_cairo_widget_background_info", (void*)0xfeedface);
	UIConfigurationBase::instance().DPIReset.connect (sigc::mem_fun (*this, &Frame::on_name_changed));
}

Frame::~Frame ()
{
	if (_parent_style_change) {
		_parent_style_change.disconnect ();
	}
	if (_w) {
		_w->unparent ();
	}
}

void
Frame::on_add (Widget* w)
{
	if (_w || !w) {
		return;
	}

	Bin::on_add (w);
	_w = w;
	queue_resize ();
}

void
Frame::on_remove (Gtk::Widget* w)
{
	Bin::on_remove (w);
	assert (_w == w);
	_w = 0;
}

void
Frame::on_size_request (GtkRequisition* r)
{
	Bin::on_size_request (r);
	_border = get_border_width ();

	_layout->set_markup (_label_text);
	if (!_layout->get_text ().empty ()) {
		_layout->get_pixel_size (_text_width, _text_height);
	} else {
		_text_width = _text_height = 0;
	}

	if (_w) {
		_w->size_request (*r);
	} else {
		r->width  = 0;
		r->height = 0;
	}

	if (_text_width > 0) {
		if (_orientation == Horizontal) {
			r->width  = max (r->width, _text_width + _label_pad_w * 2 + _label_left);
			r->width  += 2 * (_padding + _border);
			r->height += 2 * (_padding + _border + _label_pad_h) + _text_height;
		} else {
			r->height = max (r->height, _text_width + _label_pad_w * 2 + _label_left);
			r->width  += 2 * (_padding + _border + _label_pad_h) + _text_height;
			r->height += 2 * (_padding + _border);
		}
	} else {
		r->height += 2 * (_padding + _border);
		r->width += 2 * (_padding + _border);
	}
	_min_size = *r;
}

void
Frame::on_size_allocate (Allocation& alloc)
{
	Bin::on_size_allocate (alloc);
	_alloc_x0 = alloc.get_x ();
	_alloc_y0 = alloc.get_y ();

	Allocation child_alloc;
	if (alloc.get_width () < _min_size.width || alloc.get_height () < _min_size.height) {
#if 0
		printf ("Frame::on_size_allocate %dx%d < %dx%d\n", alloc.get_width (), alloc.get_height (), _min_size.width, _min_size.height);
#endif
		return;
	}

	int pb_l, pb_t;
	if (_orientation == Horizontal) {
		pb_l = _padding + _border;
		pb_t = _padding + _border + (_text_width > 0 ? _label_pad_h : 0);

		child_alloc.set_x (alloc.get_x () + pb_l);
		child_alloc.set_y (alloc.get_y () + pb_t + _text_height);
		child_alloc.set_width (alloc.get_width () - pb_l * 2);
		child_alloc.set_height (alloc.get_height () - pb_t * 2 - _text_height);
	} else {
		pb_l = _padding + _border + (_text_width > 0 ? _label_pad_h : 0);;
		pb_t = _padding + _border;

		child_alloc.set_x (alloc.get_x () + pb_l + _text_height);
		child_alloc.set_y (alloc.get_y () + pb_t);
		child_alloc.set_width (alloc.get_width () - pb_l * 2 - _text_height);
		child_alloc.set_height (alloc.get_height () - pb_t * 2);
	}

	if (child_alloc.get_width () < 1 || child_alloc.get_height () < 1) {
		return;
	}

	if (_w) {
		_w->size_allocate (child_alloc);
	}
}

void
Frame::on_style_changed (const Glib::RefPtr<Gtk::Style>& style)
{
	Bin::on_style_changed (style);
	Glib::RefPtr<Gtk::Style> const& new_style = get_style ();
	if (_layout && (_layout->get_font_description ().gobj () == 0 || _layout->get_font_description () != new_style->get_font ())) {
		_layout->set_font_description (new_style->get_font ());
		queue_resize ();
	} else if (is_realized ()) {
		queue_resize ();
	}
}

void
Frame::on_name_changed ()
{
	ensure_style ();
	queue_resize ();
	queue_draw ();
}

Glib::RefPtr<Style>
Frame::get_parent_style ()
{
	Widget* parent = get_parent ();

	while (parent) {
		if (parent->get_has_window ()) {
			break;
		}
		parent = parent->get_parent ();
	}

	if (parent && parent->get_has_window ()) {
		if (_current_parent != parent) {
			if (_parent_style_change) {
				_parent_style_change.disconnect ();
			}
			_current_parent      = parent;
			_parent_style_change = parent->signal_style_changed ().connect (mem_fun (*this, &Frame::on_style_changed));
		}
		return parent->get_style ();
	}

	return get_style ();
}

bool
Frame::on_expose_event (GdkEventExpose* ev)
{
	Glib::RefPtr<Style> pstyle (get_parent_style ());
	Glib::RefPtr<Style> style (get_style ());

	const bool  boxy = _boxy | boxy_buttons ();
	const float crad = boxy ? 0 : std::max (2.f, 3.f * UIConfigurationBase::instance ().get_ui_scale ());
	const int   lbl  = ceil (_text_height / 2.0);
	Gdk::Color  pbg  = pstyle->get_bg (get_state ());
	Gdk::Color  edge = pstyle->get_dark (get_state ());
	Gdk::Color  bg   = style->get_bg (get_state ());
	Gdk::Color  text = style->get_text (get_state ());

	if (_edge_color) {
		edge = _edge_color.value ();
	}

	Cairo::RefPtr<Cairo::Context> cr = get_window ()->create_cairo_context ();
	cr->rectangle (ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cr->clip_preserve ();
	cr->set_source_rgb (pbg.get_red_p (), pbg.get_green_p (), pbg.get_blue_p ());
	cr->fill ();

	cr->translate (_alloc_x0, _alloc_y0);

	int ll, tp, tw2, th2;
	if (_orientation == Horizontal) {
		ll  = _border;
		tp  = _border + (_text_width > 0 ? _label_pad_h : 0);
		th2 = lbl;
		tw2 = 0;
	} else {
		ll = _border + (_text_width > 0 ? _label_pad_h : 0);
		tp = _border;
		th2 = 0;
		tw2 = lbl;
	}

	/* Edge */
	assert (_padding >= 2);
	Gtkmm2ext::rounded_rectangle (cr, ll + tw2, tp + th2, get_width () - ll * 2 - tw2, get_height () - tp * 2 - th2, crad + 1.5);
	cr->set_source_rgb (edge.get_red_p (), edge.get_green_p (), edge.get_blue_p ());
	cr->fill ();
	Gtkmm2ext::rounded_rectangle (cr, ll + tw2 + 1, tp + th2 + 1, get_width () - ll * 2 - tw2 - 2, get_height () - tp * 2 - th2 - 2, crad);
	cr->set_source_rgb (bg.get_red_p (), bg.get_green_p (), bg.get_blue_p ());
	cr->fill ();

	if (_text_width > 0) {
		int lft, top;
		static const double degrees = M_PI / 180.0;
		double r = crad + 1.5;

		cr->set_source_rgb (bg.get_red_p (), bg.get_green_p (), bg.get_blue_p ());

		if (_orientation == Horizontal) {
			lft = ll + _padding + _label_left;
			top = _border;
			Gtkmm2ext::rounded_top_rectangle (cr, lft, top, _text_width + 2 * _label_pad_w, _text_height + 2 * _label_pad_h, crad + 1.5);
			cr->fill ();

			double x = lft + .5;
			double y = top + .5;
			double w = _text_width + 2 * _label_pad_w;
			double h = _label_pad_h + th2;

			cr->move_to (x, y + h);
			cr->arc (x + r, y + r, r, 180 * degrees, 270 * degrees);   //tl
			cr->arc (x + w - r, y + r, r, -90 * degrees, 0 * degrees); //tr
			cr->line_to (x + w, y + h);
		} else {
			lft = _border;
			top = get_height () - ll - _padding - _label_left - _text_width;
			Gtkmm2ext::rounded_left_half_rectangle (cr, lft, top, _text_height + 2 * _label_pad_h, _text_width + 2 * _label_pad_w, crad + 1.5);
			cr->fill ();
			double x = lft + .5;
			double y = top + .5;
			double w = _label_pad_h + tw2;
			double h = _text_width + 2 * _label_pad_w;

			cr->move_to (x+w, y + h);
			cr->arc (x + r, y + h - r, r, 90 * degrees, 180 * degrees);  //bl
			cr->arc (x + r, y + r, r, 180 * degrees, 270 * degrees);  //tl
			cr->line_to (x + w, y);
		}

		cr->set_line_width (1);
		cr->set_source_rgb (edge.get_red_p (), edge.get_green_p (), edge.get_blue_p ());
		cr->stroke ();

		cr->save ();
		cr->set_source_rgb (text.get_red_p (), text.get_green_p (), text.get_blue_p ());
		if (_orientation == Horizontal) {
			cr->move_to (lft + _label_pad_w, top + _padding + _label_pad_h - th2 / 2 - 1);
		} else {
			cr->move_to (lft + _padding + _label_pad_h - tw2 / 2 - 1, top + _label_pad_w + _text_width);
			cr->rotate (M_PI / -2.0);
		}
		_layout->update_from_cairo_context (cr);
		_layout->show_in_cairo_context (cr);
		cr->restore ();
	}

	if (_w->is_visible ()) {
		propagate_expose (*_w, ev);
	}
	return true;
}

void
Frame::set_padding (int p)
{
	if (_padding == p + 2 || p < 0) {
		return;
	}
	_padding = p + 2;
	queue_resize ();
}

void
Frame::reset_edge_color ()
{
	_edge_color.reset ();
}

void
Frame::set_edge_color (Gtkmm2ext::Color c)
{
	double r, g, b, a;
	Gdk::Color color;

	Gtkmm2ext::color_to_rgba (c, r, g, b, a);
	color.set_rgb_p (r, g, b);

	if (_edge_color == color) {
		return;
	}
	_edge_color = color;
	queue_draw ();
}

void
Frame::set_label (std::string const& t)
{
	if (_label_text == t) {
		return;
	}
	_label_text = t;
	queue_resize ();
}
