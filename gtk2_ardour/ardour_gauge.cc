/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "gtkmm2ext/utils.h"
#include "widgets/tooltips.h"

#include "ardour_gauge.h"
#include "ui_config.h"

#define PADDING 3

ArdourGauge::ArdourGauge (std::string const& max_text)
{
	_layout = Pango::Layout::create (get_pango_context ());
	_layout->set_text (max_text);
}

ArdourGauge::~ArdourGauge ()
{
}

void
ArdourGauge::on_size_request (Gtk::Requisition* req)
{
	req->width = req->height = 0;
	CairoWidget::on_size_request (req);

	int w, h;
	_layout->get_pixel_size (w, h);

	req->width = std::max (req->width, std::max (12, h + PADDING));
	req->height = std::max (req->height, 20 /*std::max (20, w + PADDING) */);
}

void
ArdourGauge::update ()
{
	queue_draw ();
	ArdourWidgets::set_tooltip (*this, tooltip_text ());
}

void
ArdourGauge::update (std::string const& txt)
{
	_layout->set_text (txt);
	update ();
}

void
ArdourGauge::blink (bool onoff)
{
	_blink = onoff;
	queue_draw ();
}

void
ArdourGauge::render (Cairo::RefPtr<Cairo::Context> const& ctx, cairo_rectangle_t*)
{
	cairo_t* cr = ctx->cobj ();
	Gtkmm2ext::Color bg = UIConfiguration::instance().color ("gtk_background");
	Gtkmm2ext::Color base = UIConfiguration::instance ().color ("ruler base");
	Gtkmm2ext::Color text = UIConfiguration::instance ().color ("ruler text");

	const int width = get_width ();
	const int height = get_height ();

	cairo_rectangle (cr, 0, 0, width, height);
	cairo_set_source_rgba (cr, 0,0,0,1 );
	cairo_fill (cr);

	cairo_rectangle (cr, 1, 1, width-2, height-2);
	Gtkmm2ext::set_source_rgba (cr, bg);
	cairo_fill (cr);

	if (alert () && _blink) {
		Gtkmm2ext::rounded_rectangle (cr, 1, 1, width - 2, height - 2, 1);
		cairo_set_source_rgba (cr, 0.5, 0, 0, 1.0);
		cairo_fill (cr);
	}

	cairo_rectangle (cr, PADDING, PADDING, width - PADDING - PADDING, height - PADDING - PADDING);
	cairo_clip (cr);

	const float lvl = level ();

	int bh = (height - PADDING - PADDING) * lvl;
	cairo_rectangle (cr, PADDING, height - PADDING - bh, width - PADDING, bh);

	switch (indicator ()) {
		case Level_OK:
			cairo_set_source_rgba (cr, 0, .5, 0, 1.0);
			break;
		case Level_WARN:
			cairo_set_source_rgba (cr, .7, .6, 0, 1.0);
			break;
		case Level_CRIT:
			cairo_set_source_rgba (cr, .9, 0, 0, 1.0);
			break;
	}
	cairo_fill (cr);

	int w, h;
	_layout->get_pixel_size (w, h);

	cairo_save (cr);
	cairo_new_path (cr);
	cairo_translate (cr, width * .5, height * .5);
	cairo_rotate (cr, M_PI * -.5);

	cairo_move_to (cr, w * -.5, h * -.5);
	pango_cairo_update_layout (cr, _layout->gobj());
	Gtkmm2ext::set_source_rgb_a (cr, base, 0.5);
	pango_cairo_layout_path (cr, _layout->gobj());
	cairo_set_line_width (cr, 1.5);
	cairo_stroke (cr);

	cairo_move_to (cr, w * -.5, h * -.5);
	pango_cairo_update_layout (cr, _layout->gobj());
	Gtkmm2ext::set_source_rgba (cr, text);
	pango_cairo_show_layout (cr, _layout->gobj());

	cairo_restore (cr);
}
