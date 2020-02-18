/*
 * Copyright (C) 2018 Robin Gareus <robin@gareus.org>
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

#include "gtkmm2ext/utils.h"

#include "ardour/session.h"

#include "plugin_dspload_ui.h"
#include "timers.h"

#include "pbd/i18n.h"

using namespace Gtkmm2ext;
using namespace Gtk;

PluginLoadStatsGui::PluginLoadStatsGui (boost::shared_ptr<ARDOUR::PluginInsert> insert)
	: _insert (insert)
	, _lbl_min ("", ALIGN_RIGHT, ALIGN_CENTER)
	, _lbl_max ("", ALIGN_RIGHT, ALIGN_CENTER)
	, _lbl_avg ("", ALIGN_RIGHT, ALIGN_CENTER)
	, _lbl_dev ("", ALIGN_RIGHT, ALIGN_CENTER)
	, _reset_button (_("Reset"))
	, _valid (false)
{
	_reset_button.set_name ("generic button");
	_reset_button.signal_clicked.connect (sigc::mem_fun (*this, &PluginLoadStatsGui::clear_stats));
	_darea.signal_expose_event ().connect (sigc::mem_fun (*this, &PluginLoadStatsGui::draw_bar));
	set_size_request_to_display_given_text (_lbl_dev, string_compose (_("%1 [ms]"), 99.123), 0, 0);
	_darea.set_size_request (360, 32); // TODO  max (320, 360 * UIConfiguration::instance().get_ui_scale ())

	attach (*manage (new Gtk::Label (_("Minimum"), ALIGN_RIGHT, ALIGN_CENTER)),
			0, 1, 0, 1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	attach (*manage (new Gtk::Label (_("Maximum"), ALIGN_RIGHT, ALIGN_CENTER)),
			0, 1, 1, 2, Gtk::FILL, Gtk::SHRINK, 2, 0);
	attach (*manage (new Gtk::Label (_("Average"), ALIGN_RIGHT, ALIGN_CENTER)),
			0, 1, 2, 3, Gtk::FILL, Gtk::SHRINK, 2, 0);
	attach (*manage (new Gtk::Label (_("Std.Dev"), ALIGN_RIGHT, ALIGN_CENTER)),
			0, 1, 3, 4, Gtk::FILL, Gtk::SHRINK, 2, 0);

	attach (_lbl_min, 1, 2, 0, 1, Gtk::FILL, Gtk::SHRINK, 2, 0);
	attach (_lbl_max, 1, 2, 1, 2, Gtk::FILL, Gtk::SHRINK, 2, 0);
	attach (_lbl_avg, 1, 2, 2, 3, Gtk::FILL, Gtk::SHRINK, 2, 0);
	attach (_lbl_dev, 1, 2, 3, 4, Gtk::FILL, Gtk::SHRINK, 2, 0);

	attach (*manage (new Gtk::VSeparator ()),
			2, 3, 0, 4, Gtk::FILL, Gtk::FILL, 4, 0);

	attach (_darea, 3, 4, 0, 4, Gtk::FILL|Gtk::EXPAND, Gtk::FILL, 4, 4);

	attach (_reset_button, 4, 5, 2, 4, Gtk::FILL, Gtk::SHRINK);
}

void
PluginLoadStatsGui::start_updating () {
	update_cpu_label ();
	update_cpu_label_connection = Timers::second_connect (sigc::mem_fun(*this, &PluginLoadStatsGui::update_cpu_label));
}

void
PluginLoadStatsGui::stop_updating () {
	_valid = false;
	update_cpu_label_connection.disconnect ();
}


void
PluginLoadStatsGui::update_cpu_label()
{
	if (_insert->get_stats (_min, _max, _avg, _dev)) {
		_valid = true;
		_lbl_min.set_text (string_compose (_("%1 [ms]"), rint (_min / 10.) / 100.));
		_lbl_max.set_text (string_compose (_("%1 [ms]"), rint (_max / 10.) / 100.));
		_lbl_avg.set_text (string_compose (_("%1 [ms]"), rint (_avg) / 1000.));
		_lbl_dev.set_text (string_compose (_("%1 [ms]"), rint (_dev) / 1000.));
		_lbl_dev.set_text (string_compose (_("%1 [ms]"), rint (_dev) / 1000.));
	} else {
		_valid = false;
		_lbl_min.set_text ("-");
		_lbl_max.set_text ("-");
		_lbl_avg.set_text ("-");
		_lbl_dev.set_text ("-");
	}
	_darea.queue_draw ();
}

bool
PluginLoadStatsGui::draw_bar (GdkEventExpose* ev)
{
	Gtk::Allocation a = _darea.get_allocation ();
	const int width = a.get_width ();
	const int height = a.get_height ();
	cairo_t* cr = gdk_cairo_create (_darea.get_window ()->gobj ());
	cairo_rectangle (cr, ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cairo_clip (cr);

	Gdk::Color const bg = get_style ()->get_bg (STATE_NORMAL);
	Gdk::Color const fg = get_style ()->get_fg (STATE_NORMAL);

	cairo_set_source_rgb (cr, bg.get_red_p (), bg.get_green_p (), bg.get_blue_p ());
	cairo_rectangle (cr, 0, 0, width, height);
	cairo_fill (cr);

	int border = (height / 7) | 1;

	int x0 = 2;
	int y0 = border;
	int x1 = width - 2;
	int y1 = (height - 3 * border) & ~1;

	const int w = x1 - x0;
	const int h = y1 - y0;
	const double cycle_ms = 1000. * _insert->session().get_block_size() / (double)_insert->session().nominal_sample_rate();

	const double base_mult = std::max (1.0, cycle_ms / 2.0);
	const double log_base = log1p (base_mult);

#if 0 // Linear
# define DEFLECT(T) ( (T) * w * 8. / (9. * cycle_ms) )
#else
# define DEFLECT(T) ( log1p((T) * base_mult / cycle_ms) * w * 8. / (9 * log_base) )
#endif

	cairo_save (cr);
	rounded_rectangle (cr, x0, y0, w, h, 7);

	cairo_set_source_rgba (cr, .0, .0, .0, 1);
	cairo_set_line_width (cr, 1);
	cairo_stroke_preserve (cr);

	/* TODO statically cache these patterns
	 * like Meter::generate_meter_background
	 */
	if (_valid) {
		cairo_pattern_t *pat = cairo_pattern_create_linear (x0, 0.0, w, 0.0);
		cairo_pattern_add_color_stop_rgba (pat, 0,         0,  1, 0, .2);
		cairo_pattern_add_color_stop_rgba (pat, 6.  / 9.,  0,  1, 0, .2);
		cairo_pattern_add_color_stop_rgba (pat, 6.5 / 9., .8, .8, 0, .2);
		cairo_pattern_add_color_stop_rgba (pat, 7.5 / 9., .8, .8, 0, .2);
		cairo_pattern_add_color_stop_rgba (pat, 8.  / 9.,  1,  0, 0, .2);
		cairo_set_source (cr, pat);
		cairo_fill_preserve (cr);
		cairo_pattern_destroy (pat);
		cairo_clip (cr);

		double xmin = DEFLECT(_min / 1000.);
		double xmax = DEFLECT(_max / 1000.);

		rounded_rectangle (cr, x0 + xmin, y0, xmax - xmin, h, 7);

		pat = cairo_pattern_create_linear (x0, 0.0, w, 0.0);
		cairo_pattern_add_color_stop_rgba (pat, 0,         0,  1, 0, .8);
		cairo_pattern_add_color_stop_rgba (pat, 6.  / 9.,  0,  1, 0, .8);
		cairo_pattern_add_color_stop_rgba (pat, 6.5 / 9., .8, .8, 0, .8);
		cairo_pattern_add_color_stop_rgba (pat, 7.5 / 9., .8, .8, 0, .8);
		cairo_pattern_add_color_stop_rgba (pat, 8.  / 9.,  1,  0, 0, .8);
		cairo_set_source (cr, pat);
		cairo_fill (cr);
		cairo_pattern_destroy (pat);

	} else {
		cairo_set_source_rgba (cr, .4, .3, .1, .5);
		cairo_fill (cr);
	}

	cairo_restore (cr);

	Glib::RefPtr<Pango::Layout> layout;
	layout = Pango::Layout::create (get_pango_context ());

	cairo_set_line_width (cr, 1);

	for (int i = 1; i < 9; ++i) {
		int text_width, text_height;
#if 0 // Linear
		double v = cycle_ms * i / 8.;
#else
		double v = (exp (i * log_base / 8) - 1) * cycle_ms / base_mult;
#endif
		double decimal = v > 10 ? 10 : 100;
		layout->set_text (string_compose ("%1", rint (decimal * v) / decimal));
		layout->get_pixel_size (text_width, text_height);

		const int dx = w * i / 9.; // == DEFLECT (v)

		cairo_move_to (cr, x0 + dx - .5, y0);
		cairo_line_to (cr, x0 + dx - .5, y1);
		cairo_set_source_rgba (cr, 1., 1., 1., 1.);
		cairo_stroke (cr);

		cairo_move_to (cr, x0 + dx - .5 * text_width, y1 + 1);
		cairo_set_source_rgb (cr, fg.get_red_p (), fg.get_green_p (), fg.get_blue_p ());
		pango_cairo_show_layout (cr, layout->gobj ());
	}

	{
		int text_width, text_height;
		layout->set_text ("0");
		cairo_move_to (cr, x0 + 1, y1 + 1);
		cairo_set_source_rgb (cr, fg.get_red_p (), fg.get_green_p (), fg.get_blue_p ());
		pango_cairo_show_layout (cr, layout->gobj ());

		layout->set_text (_("[ms]"));
		layout->get_pixel_size (text_width, text_height);
		cairo_move_to (cr, x0 + w - text_width - 1, y1 + 1);
		pango_cairo_show_layout (cr, layout->gobj ());
	}

	if (_valid) {
		double xavg = round (DEFLECT(_avg / 1000.));
		double xd0 = DEFLECT((_avg - _dev) / 1000.);
		double xd1 = DEFLECT((_avg + _dev) / 1000.);

		cairo_move_to (cr, x0 + xavg - .5, y0 - 1);
		cairo_rel_line_to (cr, -5, -5);
		cairo_rel_line_to (cr, 10, 0);
		cairo_close_path (cr);
		cairo_set_source_rgb (cr, fg.get_red_p (), fg.get_green_p (), fg.get_blue_p ());
		cairo_fill (cr);

		cairo_save (cr);

		rounded_rectangle (cr, x0, y0, w, h, 7);
		cairo_clip (cr);

		const double dashes[] = { 1, 2 };
		cairo_set_dash (cr, dashes, 2, 0);
		cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
		cairo_set_line_width (cr, 1);
		cairo_move_to (cr, x0 + xavg - .5, y0 - .5);
		cairo_line_to (cr, x0 + xavg - .5, y1 + .5);
		cairo_set_source_rgba (cr, .0, .0, .0, 1.);
		cairo_stroke (cr);
		cairo_set_dash (cr, 0, 0, 0);

		if (xd1 - xd0 > 2) {
			cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);
			const double ym = .5 + floor ((double)(y0 + h / 2));
			const int h4 = h / 4;

			cairo_set_line_width (cr, 1);
			cairo_set_source_rgba (cr, 0, 0, 0, 1.);
			cairo_move_to (cr, floor (x0 + xd0), ym);
			cairo_line_to (cr, ceil (x0 + xd1),  ym);
			cairo_stroke (cr);

			cairo_move_to (cr, floor (x0 + xd0) - .5, ym - h4);
			cairo_line_to (cr, floor (x0 + xd0) - .5, ym + h4);
			cairo_stroke (cr);
			cairo_move_to (cr, ceil (x0 + xd1) - .5, ym - h4);
			cairo_line_to (cr, ceil (x0 + xd1) - .5, ym + h4);
			cairo_stroke (cr);
		}
		cairo_restore (cr);
	}
#undef DEFLECT

	cairo_destroy (cr);
	return true;
}



