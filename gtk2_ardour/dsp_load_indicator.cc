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

#include "ardour_ui.h"
#include "dsp_load_indicator.h"
#include "ui_config.h"

#include "pbd/i18n.h"

#define PADDING 3

DspLoadIndicator::DspLoadIndicator ()
	: _dsp_load (0)
	, _xrun_count (0)
{
	_layout = Pango::Layout::create (get_pango_context ());
	_layout->set_text ("99.9%");
}

DspLoadIndicator::~DspLoadIndicator ()
{
}

void
DspLoadIndicator::on_size_request (Gtk::Requisition* req)
{
	req->width = req->height = 0;
	CairoWidget::on_size_request (req);

	int w, h;
	_layout->get_pixel_size (w, h);

	req->width = std::max (req->width, std::max (12, h + PADDING));
	req->height = std::max (req->height, 20 /*std::max (20, w + PADDING) */);
}

void
DspLoadIndicator::set_xrun_count (const unsigned int xruns)
{
	if (xruns == _xrun_count) {
		return;
	}
	_xrun_count = xruns;
	queue_draw ();
	update_tooltip ();
}

void
DspLoadIndicator::set_dsp_load (const double load)
{
	if (load == _dsp_load) {
		return;
	}
	_dsp_load = load;

	char buf[64];
	snprintf (buf, sizeof (buf), "%.1f%%", _dsp_load);
	_layout->set_text (buf);

	queue_draw ();
	update_tooltip ();
}

void
DspLoadIndicator::update_tooltip ()
{
	char buf[64];
	if (_xrun_count == UINT_MAX) {
		snprintf (buf, sizeof (buf), _("DSP: %.1f%% X: ?"), _dsp_load);
	} else if (_xrun_count > 9999) {
		snprintf (buf, sizeof (buf), _("DSP: %.1f%% X: >10k"), _dsp_load);
	} else {
		snprintf (buf, sizeof (buf), _("DSP: %.1f%% X: %u"), _dsp_load, _xrun_count);
	}
	ArdourWidgets::set_tooltip (*this, buf);
}

void
DspLoadIndicator::render (Cairo::RefPtr<Cairo::Context> const& ctx, cairo_rectangle_t*)
{
	cairo_t* cr = ctx->cobj ();
	Gtkmm2ext::Color base = UIConfiguration::instance ().color ("ruler base");
	Gtkmm2ext::Color text = UIConfiguration::instance ().color ("ruler text");

	const int width = get_width ();
	const int height = get_height ();

	Gtkmm2ext::rounded_rectangle (cr, 0, 0, width, height, PADDING + 1);
	Gtkmm2ext::set_source_rgba (cr, base);
	cairo_fill (cr);

	if (_xrun_count > 0) {
		Gtkmm2ext::rounded_rectangle (cr, 1, 1, width - 2, height - 2, PADDING + 1);
		cairo_set_source_rgba (cr, 0.5, 0, 0, 1.0);
		cairo_fill (cr);
	}

	Gtkmm2ext::rounded_rectangle (cr, PADDING, PADDING, width - PADDING - PADDING, height - PADDING - PADDING, PADDING + 1);
	cairo_clip (cr);

	int bh = (height - PADDING - PADDING) * _dsp_load / 100.f;
	cairo_rectangle (cr, PADDING, height - PADDING - bh, width - PADDING, bh);

	if (_dsp_load > 90) {
		cairo_set_source_rgba (cr, .9, 0, 0, 1.0);
	} else if (_dsp_load > 80) {
		cairo_set_source_rgba (cr, .7, .6, 0, 1.0);
	} else {
		cairo_set_source_rgba (cr, 0, .5, 0, 1.0);
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

bool
DspLoadIndicator::on_button_release_event (GdkEventButton *ev)
{
	ARDOUR::Session* s = ARDOUR_UI::instance ()->the_session ();
	if (s) {
		s->reset_xrun_count ();
	}
	return true;
}
