/*
 * Copyright (C) 2016-2021 Robin Gareus <robin@gareus.org>
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

#include <algorithm>

#include "ardour/dB.h"
#include "ardour/logmeter.h"
#include "audiographer/general/analyser.h"

#include "gtkmm2ext/utils.h"

#include "audio_clock.h"
#include "export_analysis_graphs.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

#define XAXISLABEL(POS, TXT) {                            \
  const float yy = rint (POS);                            \
  layout->set_text (TXT);                                 \
  layout->get_pixel_size (w, h);                          \
  cr->move_to (m_l - 8 - w, rint ((POS) - h * .5));       \
  cr->set_source_rgba (.9, .9, .9, 1.0);                  \
  cr->set_operator (Cairo::OPERATOR_OVER);                \
  layout->show_in_cairo_context (cr);                     \
  cr->move_to (m_l - 4, yy - .5);                         \
  cr->line_to (m_l + width, yy - .5);                     \
  cr->set_source_rgba (.3, .3, .3, 1.0);                  \
  cr->set_operator (Cairo::OPERATOR_ADD);                 \
  cr->stroke ();                                          \
}

Cairo::RefPtr<Cairo::ImageSurface>
ArdourGraphs::draw_waveform (Glib::RefPtr<Pango::Context> pctx, ExportAnalysisPtr p, uint32_t c, int height_2, int m_l, bool log, bool rect)
{
	int w, h, anw;
	const float         ht    = 2.f * height_2;
	const size_t        width = p->width;
	std::vector<double> dashes;
	dashes.push_back (3.0);
	dashes.push_back (5.0);

	Glib::RefPtr<Pango::Layout>        layout = Pango::Layout::create (pctx);
	Cairo::RefPtr<Cairo::ImageSurface> wave   = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, m_l + width, ht);
	Cairo::RefPtr<Cairo::Context>      cr     = Cairo::Context::create (wave);

	layout->set_font_description (UIConfiguration::instance ().get_SmallMonospaceFont ());
	layout->set_text ("-888");
	layout->get_pixel_size (anw, h);

	cr->set_operator (Cairo::OPERATOR_SOURCE);
	cr->rectangle (0, 0, m_l, ht);
	cr->set_source_rgba (0, 0, 0, 0);
	cr->fill ();
	cr->rectangle (m_l, 0, width, ht);
	cr->set_source_rgba (0, 0, 0, 1.0);
	cr->fill ();
	cr->set_operator (Cairo::OPERATOR_OVER);

	cr->set_source_rgba (.7, .7, .7, 1.0);
	cr->set_line_width (1.0);

	// -1dB range
	float clip_top;
	float clip_bot;

	if (rect) {
		clip_bot = ht;

		if (log) {
			clip_top = ht * (1.f - alt_log_meter (-1));
			for (size_t x = 0; x < width; ++x) {
				const float v = alt_log_meter (fast_coefficient_to_dB (std::max (fabsf (p->peaks[c][x].max), fabsf (p->peaks[c][x].min))));
				cr->move_to (m_l + x - .5, ht - ht * v);
				cr->line_to (m_l + x - .5, ht);
			}
			cr->stroke ();
		} else {
			clip_top = ht * (1.f - dB_to_coefficient (-1));
			for (size_t x = 0; x < width; ++x) {
				const float v = std::max (fabsf (p->peaks[c][x].max), fabsf (p->peaks[c][x].min));
				cr->move_to (m_l + x - .5, ht - ht * v);
				cr->line_to (m_l + x - .5, ht);
			}
			cr->stroke ();
		}
	} else {
		if (log) {
			clip_top = height_2 - height_2 * alt_log_meter (-1);
			clip_bot = height_2 + height_2 * alt_log_meter (-1);
			for (size_t x = 0; x < width; ++x) {
				float pmax, pmin;
				if (p->peaks[c][x].max > 0) {
					pmax = alt_log_meter (fast_coefficient_to_dB (p->peaks[c][x].max));
				} else {
					pmax = -alt_log_meter (fast_coefficient_to_dB (-p->peaks[c][x].max));
				}

				if (p->peaks[c][x].min > 0) {
					pmin = alt_log_meter (fast_coefficient_to_dB (p->peaks[c][x].min));
				} else {
					pmin = -alt_log_meter (fast_coefficient_to_dB (-p->peaks[c][x].min));
				}
				cr->move_to (m_l + x - .5, height_2 - height_2 * pmax);
				cr->line_to (m_l + x - .5, height_2 - height_2 * pmin);
			}
			cr->stroke ();
		} else {
			clip_top = height_2 - height_2 * dB_to_coefficient (-1);
			clip_bot = height_2 + height_2 * dB_to_coefficient (-1);
			for (size_t x = 0; x < width; ++x) {
				cr->move_to (m_l + x - .5, height_2 - height_2 * p->peaks[c][x].max);
				cr->line_to (m_l + x - .5, height_2 - height_2 * p->peaks[c][x].min);
			}
			cr->stroke ();
		}
	}

	// >= 0dBFS
	cr->set_source_rgba (1.0, 0, 0, 1.0);
	for (size_t x = 0; x < width; ++x) {
		if (p->peaks[c][x].max >= 1.0) {
			cr->move_to (m_l + x - .5, 0);
			cr->line_to (m_l + x - .5, clip_top);
		}
		if (p->peaks[c][x].min <= -1.0) {
			cr->move_to (m_l + x - .5, clip_bot);
			cr->line_to (m_l + x - .5, ht);
		}
	}
	cr->stroke ();

	// >= -1dBTP (coeff >= .89125, libs/vamp-plugins/TruePeak.cpp)
	cr->set_source_rgba (1.0, 0.7, 0, 0.7);
	for (std::set<samplepos_t>::const_iterator i = p->truepeakpos[c].begin (); i != p->truepeakpos[c].end (); ++i) {
		cr->move_to (m_l + (*i) - .5, clip_top);
		cr->line_to (m_l + (*i) - .5, clip_bot);
		cr->stroke ();
	}

	/* redux */
	cr->set_source_rgba (0.1, 0.4, 1.0, 0.7);
	for (size_t x = 0; x < width; ++x) {
		if (p->limiter_pk[x] > 1.0) {
			float y = p->limiter_pk[x];
			if (log) {
				y = alt_log_meter (fast_coefficient_to_dB (1.f / p->limiter_pk[x]));
			} else {
				y = 1.f / p->limiter_pk[x];
			}
			if (rect) {
				y = ht * (1.f - y);
				cr->move_to (m_l + x - .5, 0);
				cr->line_to (m_l + x - .5, y);
			} else {
				y = height_2 * y;
				cr->move_to (m_l + x - .5, 0);
				cr->line_to (m_l + x - .5, height_2 - y);
				cr->move_to (m_l + x - .5, ht);
				cr->line_to (m_l + x - .5, height_2 + y);
			}
			cr->stroke ();
		}
	}

	if (!rect) {
		// zero line
		cr->set_source_rgba (.3, .3, .3, 0.7);
		cr->move_to (m_l + 0, height_2 - .5);
		cr->line_to (m_l + width, height_2 - .5);
		cr->stroke ();
	}

	// Unit
	layout->set_font_description (UIConfiguration::instance ().get_SmallerFont ());
	layout->set_alignment (Pango::ALIGN_LEFT);
	layout->set_text (_("dBFS"));
	layout->get_pixel_size (w, h);
	cr->move_to (rint (m_l - h - anw - 10), rint (height_2 + w * .5));
	cr->set_source_rgba (.9, .9, .9, 1.0);
	cr->save ();
	cr->rotate (M_PI / -2.0);
	layout->show_in_cairo_context (cr);
	cr->restore ();

	// x-Axis
	cr->set_line_width (1.0);
	cr->set_dash (dashes, 2.0);
	cr->set_line_cap (Cairo::LINE_CAP_ROUND);

	layout->set_font_description (UIConfiguration::instance ().get_SmallMonospaceFont ());

	if (rect) {
		if (log) {
			XAXISLABEL ((ht - ht * alt_log_meter (-36)), _("-36"));
			XAXISLABEL ((ht - ht * alt_log_meter (-18)), _("-18"));
			XAXISLABEL ((ht - ht * alt_log_meter (-9)), _("-9"));
			XAXISLABEL ((ht - ht * alt_log_meter (-3)), _("-3"));
		} else {
			XAXISLABEL ((ht - ht * .1259), _("-18"));
			XAXISLABEL ((ht - ht * .3548), _("-9"));
			XAXISLABEL ((ht - ht * .7079), _("-3"));
		}
	} else {
		if (log) {
			XAXISLABEL ((height_2 - height_2 * alt_log_meter (-18)), _("-18"));
			XAXISLABEL ((height_2 - height_2 * alt_log_meter (-9)), _("-9"));
			XAXISLABEL ((height_2 - height_2 * alt_log_meter (-3)), _("-3"));
			XAXISLABEL ((height_2 + height_2 * alt_log_meter (-18)), _("-18"));
			XAXISLABEL ((height_2 + height_2 * alt_log_meter (-9)), _("-9"));
			XAXISLABEL ((height_2 + height_2 * alt_log_meter (-3)), _("-3"));
		} else {
			XAXISLABEL (height_2 * 0.6452, _("-9"));
			XAXISLABEL (height_2 * 1.3548, _("-9"));
			XAXISLABEL (height_2 * 0.2921, _("-3"));
			XAXISLABEL (height_2 * 1.7079, _("-3"));
		}
	}
	wave->flush ();
	return wave;
}

Cairo::RefPtr<Cairo::ImageSurface>
ArdourGraphs::draw_spectrum (Glib::RefPtr<Pango::Context> pctx, ExportAnalysisPtr p, int height, int m_l)
{
	int w, h, anw;
	const size_t width = p->width;

	assert (height == p->spectrum[0].size ());

	std::vector<double> dashes;
	dashes.push_back (3.0);
	dashes.push_back (5.0);

	Cairo::RefPtr<Cairo::ImageSurface> spec = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, m_l + width, height);
	Cairo::RefPtr<Cairo::Context>      cr   = Cairo::Context::create (spec);

	cr->set_operator (Cairo::OPERATOR_SOURCE);
	cr->rectangle (0, 0, m_l, height);
	cr->set_source_rgba (0, 0, 0, 0);
	cr->fill ();
	cr->rectangle (m_l, 0, width, height);
	cr->set_source_rgba (0, 0, 0, 1.0);
	cr->fill ();
	cr->set_operator (Cairo::OPERATOR_OVER);

	/* draw spectrum */
	for (size_t x = 0; x < width; ++x) {
		for (int y = 0; y < height; ++y) {
			const float      pk = p->spectrum[x][y];
			Gtkmm2ext::Color c  = Gtkmm2ext::hsva_to_color (252 - 260 * pk, .9, sqrt (pk));
			Gtkmm2ext::set_source_rgba (cr, c);
			cr->rectangle (m_l + x - .5, y - .5, 1, 1);
			cr->fill ();
		}
	}

	/* y-Axis Units */
	Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (pctx);
	layout->set_font_description (UIConfiguration::instance ().get_SmallMonospaceFont ());
	layout->set_text ("-888");
	layout->get_pixel_size (anw, h);

	layout->set_font_description (UIConfiguration::instance ().get_SmallerFont ());
	layout->set_text (_("Hz"));
	layout->get_pixel_size (w, h);
	cr->move_to (rint (m_l - h - anw - 10), rint ((height + w) * .5));
	cr->set_source_rgba (.9, .9, .9, 1.0);
	cr->save ();
	cr->rotate (M_PI / -2.0);
	layout->show_in_cairo_context (cr);
	cr->restore ();

	cr->set_line_width (1.0);
	cr->set_dash (dashes, 2.0);
	cr->set_line_cap (Cairo::LINE_CAP_ROUND);

	layout->set_font_description (UIConfiguration::instance ().get_SmallMonospaceFont ());
	//XAXISLABEL (p->freq[0], _("50Hz"));
	XAXISLABEL (p->freq[1], _("100"));
	XAXISLABEL (p->freq[2], _("500"));
	XAXISLABEL (p->freq[3], _("1K"));
	XAXISLABEL (p->freq[4], _("5K"));
	XAXISLABEL (p->freq[5], _("10K"));
	spec->flush ();
	return spec;
}

Cairo::RefPtr<Cairo::ImageSurface>
ArdourGraphs::spectrum_legend (Glib::RefPtr<Pango::Context> pctx, int height, int width)
{
	Glib::RefPtr<Pango::Layout>        layout = Pango::Layout::create (pctx);
	Cairo::RefPtr<Cairo::ImageSurface> scale  = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, width, height);

	int w, h, anw, mnh;
	layout->set_font_description (UIConfiguration::instance ().get_SmallMonospaceFont ());
	layout->set_text ("-888");
	layout->get_pixel_size (anw, h);
	mnh = h + 1; // monospace height

	Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create (scale);
	cr->set_operator (Cairo::OPERATOR_SOURCE);
	cr->rectangle (0, 0, width, height);
	cr->set_source_rgba (0, 0, 0, 0);
	cr->fill ();

	cr->set_operator (Cairo::OPERATOR_OVER);
	layout->set_font_description (UIConfiguration::instance ().get_SmallerFont ());
	layout->set_alignment (Pango::ALIGN_LEFT);
	layout->set_text (_("dBFS"));
	layout->get_pixel_size (w, h);
	cr->move_to (rint (.5 * (width - w)), height - h - 2);
	cr->set_source_rgba (.9, .9, .9, 1.0);
	layout->show_in_cairo_context (cr);

	int    innertop    = ceil (mnh * .5) + 1;
	size_t innerheight = (height - 2 * innertop - h - 2);

	cr->rectangle (1, innertop - 1, width - 2 - anw, innerheight + 2);
	cr->set_source_rgba (0, 0, 0, 1.0);
	cr->fill_preserve ();
	cr->set_line_width (1.0);
	cr->set_source_rgba (.7, .7, .6, 1.0);
	cr->stroke ();

	for (size_t y = 0; y < innerheight - 2; ++y) {
		const float      pk = 1.0 - (float)y / innerheight;
		Gtkmm2ext::Color c  = Gtkmm2ext::hsva_to_color (252 - 260 * pk, .9, sqrt (pk));
		Gtkmm2ext::set_source_rgba (cr, c);
		cr->rectangle (2, innertop + y + .5, width - 4 - anw, 1);
		cr->fill ();
	}

	layout->set_font_description (UIConfiguration::instance ().get_SmallMonospaceFont ());
	for (int i = 0; i <= 4; ++i) {
		const float fract  = (float)i / 4.0;
		const float yalign = .5;

		char buf[16];
		snprintf (buf, sizeof (buf), "%.0f", AudioGrapher::Analyser::fft_range_db * -fract);
		layout->set_text (buf);
		layout->get_pixel_size (w, h);

		cr->move_to (width - anw, rint (innertop + fract * innerheight - h * yalign));
		cr->set_source_rgba (.9, .9, .9, 1.0);
		layout->show_in_cairo_context (cr);
	}
	scale->flush ();
	return scale;
}

Cairo::RefPtr<Cairo::ImageSurface>
ArdourGraphs::loudness_histogram (Glib::RefPtr<Pango::Context> pctx, ARDOUR::ExportAnalysisPtr p, int height, int width)
{
	int w, h;

	std::vector<double> dashes;
	dashes.push_back (3.0);
	dashes.push_back (5.0);

	Glib::RefPtr<Pango::Layout>        layout = Pango::Layout::create (pctx);
	Cairo::RefPtr<Cairo::ImageSurface> hist   = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, width, height);

	Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create (hist);
	cr->set_source_rgba (0, 0, 0, 1.0);
	cr->paint ();

	cr->set_source_rgba (.7, .7, .7, 1.0);
	cr->set_line_width (1.0);

	if (p->loudness_hist_max > 0 && p->have_loudness) {
		for (size_t x = 0; x < 510; ++x) {
			cr->move_to (x - .5, height);
			cr->line_to (x - .5, (float)height * (1.0 - p->loudness_hist[x] / (float)p->loudness_hist_max));
			cr->stroke ();
		}

		layout->set_font_description (UIConfiguration::instance ().get_SmallerFont ());
		layout->set_alignment (Pango::ALIGN_CENTER);

		// x-axis label
		layout->set_text (_("LUFS\n(short)"));
		layout->get_pixel_size (w, h);
		Gtkmm2ext::rounded_rectangle (cr, 5, 5, w + 2, h + 2, 4);
		cr->set_source_rgba (.1, .1, .1, 0.7);
		cr->fill ();
		cr->move_to (6, 6);
		cr->set_source_rgba (.9, .9, .9, 1.0);
		layout->show_in_cairo_context (cr);

		// y-axis label
		layout->set_text (_("Multiplicity"));
		layout->get_pixel_size (w, h);
		Gtkmm2ext::rounded_rectangle (cr, 5, height - w - 3, h + 2, w + 2, 4);
		cr->set_source_rgba (.1, .1, .1, 0.7);
		cr->fill ();
		cr->save ();
		cr->move_to (6, height - 2);
		cr->set_source_rgba (.9, .9, .9, 1.0);
		cr->rotate (M_PI / -2.0);
		layout->show_in_cairo_context (cr);
		cr->restore ();

		// x-Axis labels
		layout->set_font_description (UIConfiguration::instance ().get_SmallMonospaceFont ());
		layout->set_alignment (Pango::ALIGN_LEFT);
		for (int g = -53; g <= -8; g += 5) {
			// grid-lines. [110] -59LUFS .. [650]: -5 LUFS
			layout->set_text (string_compose ("%1", std::setw (3), std::setfill (' '), g));
			layout->get_pixel_size (w, h);

			cr->set_operator (Cairo::OPERATOR_OVER);
			Gtkmm2ext::rounded_rectangle (cr,
			                              rint ((g + 59.0) * 10.0 - h * .5), 5,
			                              h + 2, w + 2, 4);
			const float      pk = (g + 59.0) / 54.0;
			Gtkmm2ext::Color c  = Gtkmm2ext::hsva_to_color (252 - 260 * pk, .9, .3 + pk * .4, .6);
			Gtkmm2ext::set_source_rgba (cr, c);
			cr->fill ();

			cr->save ();
			cr->set_source_rgba (.9, .9, .9, 1.0);
			cr->move_to (rint ((g + 59.0) * 10.0 - h * .5), w + 6.0);
			cr->rotate (M_PI / -2.0);
			layout->show_in_cairo_context (cr);
			cr->restore ();

			cr->set_operator (Cairo::OPERATOR_ADD);
			cr->save ();
			cr->set_source_rgba (.3, .3, .3, 1.0);
			cr->set_dash (dashes, 1.0);
			cr->set_line_cap (Cairo::LINE_CAP_ROUND);
			cr->move_to (rint ((g + 59.0) * 10.0) + .5, w + 8.0);
			cr->line_to (rint ((g + 59.0) * 10.0) + .5, height);
			cr->stroke ();
			cr->restore ();
		}

	} else {
		layout->set_alignment (Pango::ALIGN_CENTER);
		layout->set_font_description (UIConfiguration::instance ().get_LargeFont ());
		layout->set_text (_("Not\nAvailable"));
		layout->get_pixel_size (w, h);
		cr->move_to (rint ((510 - w) * .5), rint ((height - h) * .5));
		layout->show_in_cairo_context (cr);
	}

	hist->flush ();
	return hist;
}

Cairo::RefPtr<Cairo::ImageSurface>
ArdourGraphs::time_axis (Glib::RefPtr<Pango::Context> pctx, int width, int m_l, samplepos_t start, samplecnt_t length, samplecnt_t sample_rate)
{
	Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (pctx);

	int w, h;
	layout->set_font_description (UIConfiguration::instance ().get_SmallMonospaceFont ());
	layout->set_text ("00:00:00.0");
	layout->get_pixel_size (w, h);

	int height   = h * 1.75;
	int n_labels = width / (w * 1.75);

	Cairo::RefPtr<Cairo::ImageSurface> ytme = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, m_l + width, height);
	Cairo::RefPtr<Cairo::Context>      cr   = Cairo::Context::create (ytme);

	cr->set_operator (Cairo::OPERATOR_SOURCE);
	cr->set_source_rgba (0, 0, 0, 1.0);
	cr->paint ();
	cr->rectangle (0, 0, m_l, height);
	cr->set_source_rgba (0, 0, 0, 0);
	cr->fill ();
	cr->set_operator (Cairo::OPERATOR_OVER);

	cr->set_line_width (1.0);

	for (int i = 0; i <= n_labels; ++i) {
		const float fract  = (float)i / n_labels;
		const float xalign = (i == n_labels) ? 1.f : (i == 0) ? 0 : .5f;

		char buf[16];
		AudioClock::print_minsec (start + length * fract, buf, sizeof (buf), sample_rate, 1);

		layout->set_text (&buf[1]);
		layout->get_pixel_size (w, h);
		cr->move_to (rint (m_l + width * fract - w * xalign), rint (.5 * (height - h)));
		cr->set_source_rgba (.9, .9, .9, 1.0);
		layout->show_in_cairo_context (cr);

		cr->set_source_rgba (.7, .7, .7, 1.0);
		cr->move_to (rint (m_l + width * fract) - .5, 0);
		cr->line_to (rint (m_l + width * fract) - .5, ceil  (height * .15));
		cr->move_to (rint (m_l + width * fract) - .5, floor (height * .85));
		cr->line_to (rint (m_l + width * fract) - .5, height);
		cr->stroke ();
	}

	layout->set_font_description (UIConfiguration::instance ().get_SmallFont ());
	layout->set_text (_("Time"));
	cr->set_source_rgba (.9, .9, .9, 1.0);
	layout->get_pixel_size (w, h);
	cr->move_to (rint (m_l - w - 8), rint (.5 * (height - h)));
	layout->show_in_cairo_context (cr);

	ytme->flush ();
	return ytme;
}

Cairo::RefPtr<Cairo::ImageSurface>
ArdourGraphs::plot_loudness (Glib::RefPtr<Pango::Context> pctx, ExportAnalysisPtr p, int height, int margin_left, samplecnt_t sample_rate)
{
	int w, h, anw;
	Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (pctx);
	layout->set_font_description (UIConfiguration::instance ().get_SmallMonospaceFont ());
	layout->set_text ("-888");
	layout->get_pixel_size (anw, h);
	layout->set_text ("00:00:00.0");
	layout->get_pixel_size (w, h);

	const size_t width    = p->width;
	const int    n_labels = width / (w * 1.75);

	if (height < 0) {
		height = width / 3;
	}

	int m_top    = h;
	int m_right  = h;
	int m_bottom = h * 1.75;

	layout->set_text ("-89");
	layout->get_pixel_size (w, h);
	int m_left = w * 1.75;

	bool render_for_export = false;
	if (margin_left > 0) {
		m_left = margin_left - 1;
		m_right = 0;
		m_top = 0;
		m_bottom = 0;
		render_for_export = true;
	} else {
		margin_left = m_left;
	}

	float x0 = m_left + .5;
	float y0 = m_top + .5;

	Cairo::RefPtr<Cairo::ImageSurface> ls = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, width + margin_left + m_right, height + m_top + m_bottom);

	Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create (ls);

	/* background */
	cr->set_source_rgb (0, 0, 0);
	cr->paint ();

	if (render_for_export) {
		cr->set_operator (Cairo::OPERATOR_SOURCE);
		cr->rectangle (0, 0, margin_left - 1, height + m_top);
		cr->set_source_rgba (0, 0, 0, 0);
		cr->fill ();
		cr->set_operator (Cairo::OPERATOR_OVER);
	} else {
		/* border (left, bottom) */
		cr->set_source_rgba (.7, .7, .7, 1.0);
		cr->set_line_width (1.0);

		cr->move_to (x0, y0 + height);
		cr->line_to (x0 + width, y0 + height);
		cr->stroke ();

		cr->move_to (x0, y0);
		cr->line_to (x0, y0 + height);
		cr->stroke ();
	}

	/* determine y-axis range */
	bool  have_data = false;
	float v_max     = -10;
	float v_min     = -40;

	/* determine y-axis range */
	for (size_t x = 0; x < width; ++x) {
		if (p->lgraph_i[x] > -110) {
			v_max     = std::max (v_max, p->lgraph_i[x]);
			v_min     = std::min (v_min, p->lgraph_i[x]);
			have_data = true;
		}
		if (p->lgraph_s[x] > -110) {
			v_max     = std::max (v_max, p->lgraph_s[x]);
			v_min     = std::min (v_min, p->lgraph_s[x]);
			have_data = true;
		}
		if (p->lgraph_m[x] > -110) {
			v_max     = std::max (v_max, p->lgraph_m[x]);
			v_min     = std::min (v_min, p->lgraph_m[x]);
			have_data = true;
		}
	}

	if (render_for_export) {
		v_max = ceilf ((v_max - 5.f) / 10.f) * 10.f + 5.f;
		v_min = floorf ((v_min + 5.f) / 10.f) * 10.f - 5.f;
		v_min = std::max (v_min, v_max - 60);
	} else {
		v_max = ceilf (v_max / 10.f) * 10.f;
		v_min = floorf ((v_min + 5.f) / 10.f) * 10.f - 5.f;
		v_min = std::max (v_min, v_max - 65);
	}

	std::vector<double> dashes;
	dashes.push_back (3.0);
	dashes.push_back (5.0);
	cr->set_line_cap (Cairo::LINE_CAP_ROUND);

	/* time axis labels */
	cr->set_line_width (1.0);

	for (int i = 0; i <= n_labels; ++i) {
		const float fract  = (float)i / n_labels;
		const float xalign = (i == n_labels) ? 1 : (i == 0) ? 0 : 0.5;

		if (!render_for_export) {
			char buf[16];
			AudioClock::print_minsec (p->n_samples * fract, buf, sizeof (buf), sample_rate, 1);

			layout->set_text (&buf[1]);
			layout->get_pixel_size (w, h);
			cr->move_to (rint (m_left + width * fract - w * xalign), height + m_top + m_bottom - h - 1);
			cr->set_source_rgba (.9, .9, .9, 1.0);
			layout->show_in_cairo_context (cr);

			cr->set_source_rgba (.7, .7, .7, 1.0);
			cr->move_to (rint (m_left + width * fract) + .5, m_top + height + .5);
			cr->line_to (rint (m_left + width * fract) + .5, floor (height + m_top * 1.5) + .5);
			cr->stroke ();
		} else {
			cr->set_source_rgba (.7, .7, .7, 1.0);
		}

		if (i == 0 && !render_for_export) {
			continue;
		}

		cr->move_to (rint (m_left + width * fract) + .5, m_top + .5);
		cr->line_to (rint (m_left + width * fract) + .5, m_top + height + .5);
		cr->set_source_rgba (.5, .5, .5, 1.0);
		cr->set_dash (dashes, 2.0);
		cr->stroke ();
		cr->unset_dash ();
	}

#define YPOS(val) (height * std::min (1.f, std::max<float> (0.f, (v_max - (val)) / (v_max - v_min))))

	layout->set_font_description (UIConfiguration::instance ().get_SmallerFont ());
	layout->set_text (_("LUFS"));
	layout->get_pixel_size (w, h);
	if (render_for_export) {
		cr->move_to (rint (margin_left - h - anw - 10), rint ((height + w) * .5));
	} else {
		cr->move_to (m_left - h - 5, m_top + height + rint ((m_bottom + w) * .5) - 3);
	}

	cr->save ();
	cr->set_source_rgba (.9, .9, .9, 1.0);
	cr->rotate (M_PI / -2.f);
	layout->show_in_cairo_context (cr);
	cr->restore ();

	layout->set_font_description (UIConfiguration::instance ().get_SmallMonospaceFont ());
	for (float v = v_min + 5; v <= v_max; v += 10.f) {
		char buf[16];
		snprintf (buf, sizeof (buf), "%.0f", v);

		float y = YPOS (v);

		cr->save ();
		layout->set_text (buf);
		layout->get_pixel_size (w, h);
		cr->move_to (m_left - h - 5, m_top + rint (y + w * .5));
		cr->set_source_rgba (.9, .9, .9, 1.0);
		cr->rotate (M_PI / -2.f);
		layout->show_in_cairo_context (cr);
		cr->restore ();

		y = y0 + rintf (y);

		cr->set_source_rgba (.7, .7, .7, 1.0);
		cr->move_to (m_left - 3.5, y);
		cr->line_to (m_left + 0.5, y);
		cr->stroke ();

		cr->set_source_rgba (.5, .5, .5, 1.0);
		cr->move_to (m_left, y);
		cr->line_to (m_left + width, y);
		cr->set_dash (dashes, 2.0);
		cr->stroke ();
		cr->unset_dash ();
	}

	if (!have_data) {
		layout->set_font_description (UIConfiguration::instance ().get_HugerItalicFont ());
		layout->set_text ("Silence");
		layout->get_pixel_size (w, h);
		cr->set_source_rgba (.7, .7, .7, 0.7);
		cr->move_to (m_left + rint ((width - w) * .5), m_top + rint ((height - h) * .5));
		layout->show_in_cairo_context (cr);
		ls->flush ();
		return ls;
	}

	cr->rectangle (m_left, m_top, width, height);
	cr->clip ();

	/* plot data */
	bool  first = true;
	int   skip  = 0;
	float yp;

	/* values */
	cr->set_line_width (3);
	dashes.clear ();
	dashes.push_back (6.0);
	dashes.push_back (6.0);

	if (p->max_loudness_momentary >= v_min) {
		cr->set_dash (dashes, 2.0);
		cr->set_source_rgba (.1, .4, 1, 0.4);
		float y = y0 + YPOS (p->max_loudness_momentary);
		cr->move_to (x0, y);
		cr->line_to (x0 + width, y);
		cr->stroke ();
	}

	if (p->max_loudness_short >= v_min) {
		cr->set_dash (dashes, 8.0);
		cr->set_source_rgba (1, .2, .1, 0.25);
		float y = y0 + YPOS (p->max_loudness_short);
		cr->move_to (x0, y);
		cr->line_to (x0 + width, y);
		cr->stroke ();
	}

	cr->set_line_width (6);
	cr->unset_dash ();

	if (p->integrated_loudness >= v_min) {
		cr->set_source_rgba (.3, 1, .3, 0.3);
		float y = y0 + YPOS (p->integrated_loudness);
		cr->move_to (x0, y);
		cr->line_to (x0 + width, y);
		cr->stroke ();
	}

	/* integrated */
	cr->set_source_rgba (.1, 1, .1, 1.0);
	cr->set_line_width (1.5);

	for (size_t x = 0; x < width; ++x) {
		if (p->lgraph_i[x] < v_min && first) {
			continue;
		}
		float y = y0 + YPOS (p->lgraph_i[x]);
		if (first) {
			cr->move_to (x0 + x, y);
			yp    = y;
			first = false;
		} else if ((x == width - 1) || fabsf (yp - y) > 0.5) {
			if (skip > 9 && fabsf (yp - y) > 9) {
				cr->line_to (x0 + x - 1, yp);
			}
			yp = y;
			cr->line_to (x0 + x, y);
			skip = 0;
		} else {
			++skip;
		}
	}
	cr->stroke ();

	/* momentary */
	cr->set_source_rgba (.1, .4, 1, 1.0);
	first = true;
	skip  = 0;

	for (size_t x = 0; x < width; ++x) {
		if (p->lgraph_m[x] <= v_min && first) {
			continue;
		}
		float y = y0 + YPOS (p->lgraph_m[x]);
		if (first) {
			cr->move_to (x0 + x, y);
			yp    = y;
			first = false;
		} else if ((x == width - 1) || fabsf (yp - y) > 0.5) {
			if (skip > 5 && fabsf (yp - y) > 5) {
				cr->line_to (x0 + x - 1, yp);
			}
			yp = y;
			cr->line_to (x0 + x, y);
			skip = 0;
		} else {
			++skip;
		}
	}
	cr->stroke ();

	/* short */
	cr->set_source_rgba (1, .2, .1, 1.0);
	cr->set_line_width (1.0);
	first = true;
	skip  = 0;

	for (size_t x = 0; x < width; ++x) {
		if (p->lgraph_s[x] <= v_min && first) {
			continue;
		}
		float y = y0 + YPOS (p->lgraph_s[x]);
		if (first) {
			yp = y;
			cr->move_to (x0 + x, y);
			first = false;
		} else if ((x == width - 1) || fabsf (yp - y) > 0.5) {
			if (skip > 5 && fabsf (yp - y) > 5) {
				cr->line_to (x0 + x - 1, yp);
			}
			yp = y;
			cr->line_to (x0 + x, y);
			skip = 0;
		} else {
			++skip;
		}
	}
	cr->stroke ();

	/* legend */

	float xl = m_left + 10;
	float yl = height + m_top - 8;

	int lw = 0;
	int lh = 0;

	layout->set_text ("Integrated");
	layout->get_pixel_size (w, h);
	lw = w;
	lh = h;

	layout->set_text ("Short");
	layout->get_pixel_size (w, h);
	lw = std::max (lw, w);
	lh = std::max (lh, h);

	layout->set_text ("Momentary");
	layout->get_pixel_size (w, h);
	lw = std::max (lw, w);
	h  = std::max (lh, h);

	lh = ceil (3.6 * h);

	Gtkmm2ext::rounded_rectangle (cr, xl - 4, yl - lh, lw + 20, lh, 5);
	cr->set_source_rgba (.3, .3, .3, .75);
	cr->fill ();
	yl -= h * .7;

	cr->set_line_width (2);

	float yy = rint (yl);
	cr->move_to (xl + 0.5, yy);
	cr->line_to (xl + 7.5, yy);
	cr->set_source_rgba (.1, .4, 1, 1.0);
	cr->stroke ();
	cr->move_to (xl + 11, rint (yl - h * .5));
	cr->set_source_rgba (.9, .9, .9, 1.0);
	layout->show_in_cairo_context (cr);
	yl -= h * 1.1;

	layout->set_text ("Short");
	layout->get_pixel_size (w, h);
	yy = rint (yl);
	cr->move_to (xl + 0.5, yy);
	cr->line_to (xl + 7.5, yy);
	cr->set_source_rgba (1, .2, .1, 1.0);
	cr->stroke ();
	cr->move_to (xl + 11, rint (yl - h * .5));
	cr->set_source_rgba (.9, .9, .9, 1.0);
	layout->show_in_cairo_context (cr);
	yl -= h * 1.1;

	layout->set_text ("Integrated");
	layout->get_pixel_size (w, h);
	yy = rint (yl);
	cr->move_to (xl + 0.5, yy);
	cr->line_to (xl + 7.5, yy);
	cr->set_source_rgba (.1, 1, .1, 1.0);
	cr->stroke ();
	cr->move_to (xl + 11, rint (yl - h * .5));
	cr->set_source_rgba (.9, .9, .9, 1.0);
	layout->show_in_cairo_context (cr);

	ls->flush ();
	return ls;
}
