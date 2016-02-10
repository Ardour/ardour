/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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

#include <pangomm/layout.h>
#include <gtkmm/label.h>
#include <gtkmm/stock.h>

#include "canvas/utils.h"
#include "canvas/colors.h"

#include "ui_config.h"
#include "export_report.h"

#include "i18n.h"

using namespace Gtk;
using namespace ARDOUR;

ExportReport::ExportReport (StatusPtr s)
	: ArdourDialog (_("Export Report/Analysis"))
	, status (s)
{

	AnalysisResults & ar = status->result_map;

	for (AnalysisResults::iterator i = ar.begin (); i != ar.end (); ++i) {
		Label *l;
		VBox *vb = manage (new VBox ());
		vb->set_spacing (6);

		l = manage (new Label (string_compose (_("File: %1"), i->first)));
		vb->pack_start (*l);

		ExportAnalysisPtr p = i->second;

		if (i->second->have_loudness) {
			/* EBU R128 loudness numerics and histogram */
			int w, h;
			Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (get_pango_context ());
			Cairo::RefPtr<Cairo::ImageSurface> nums = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, 256, 128);
			Cairo::RefPtr<Cairo::ImageSurface> hist = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, 540, 128);

			Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create (nums);
			cr->rectangle (0, 0, 256, 128);
			cr->set_source_rgba (0, 0, 0, 1.0);
			cr->fill ();

			layout->set_font_description (UIConfiguration::instance ().get_SmallFont ());
			layout->set_alignment (Pango::ALIGN_LEFT);
			layout->set_text (_("Ebu R128"));
			layout->get_pixel_size (w, h);

			cr->save ();
			cr->set_source_rgba (.5, .5, .5, 1.0);
			cr->move_to (6, rint (64 + w * .5));
			cr->rotate (M_PI / -2.0);
			layout->show_in_cairo_context (cr);
			cr->restore ();

			cr->set_source_rgba (.7, .7, .7, 1.0);

			if (p->loudness == -200 &&  p->loudness_range == 0) {
				layout->set_font_description (UIConfiguration::instance ().get_LargeFont ());
				layout->set_text (string_compose (_("not available"), std::setprecision (1), std::fixed,  p->loudness));
				layout->get_pixel_size (w, h);
				cr->move_to (rint (128 - w * .5), rint (64 - h));
				layout->show_in_cairo_context (cr);

				layout->set_font_description (UIConfiguration::instance ().get_SmallFont ());
				layout->set_text (_("(too short integration time)"));
				layout->get_pixel_size (w, h);
				cr->move_to (rint (128 - w * .5), rint (64 + h));
				layout->show_in_cairo_context (cr);

			} else {
				int y0 = 6;
				layout->set_font_description (UIConfiguration::instance ().get_SmallFont ());
				layout->set_text (string_compose (_("Loudness:"), std::setprecision (1), std::fixed,  p->loudness));
				layout->get_pixel_size (w, h);
				cr->move_to (rint (128 - w * .5), y0);
				layout->show_in_cairo_context (cr);
				y0 += h * 1.25;

				layout->set_font_description (UIConfiguration::instance ().get_LargeFont ());
				layout->set_text (string_compose (_("%1%2%3 LUFS"), std::setprecision (1), std::fixed,  p->loudness));
				layout->get_pixel_size (w, h);
				cr->move_to (rint (128 - w * .5), y0);
				layout->show_in_cairo_context (cr);
				y0 += h * 1.5;

				layout->set_font_description (UIConfiguration::instance ().get_SmallFont ());
				layout->set_text (string_compose (_("Loudness Range:"), std::setprecision (1), std::fixed,  p->loudness));
				layout->get_pixel_size (w, h);
				cr->move_to (rint (128 - w * .5), y0);
				layout->show_in_cairo_context (cr);
				y0 += h * 1.25;

				layout->set_font_description (UIConfiguration::instance ().get_LargeFont ());
				layout->set_text (string_compose (_("%1%2%3 LU"), std::setprecision (1), std::fixed, p->loudness_range));
				layout->get_pixel_size (w, h);
				cr->move_to (rint (128 - w * .5), y0);
				layout->show_in_cairo_context (cr);
			}
			nums->flush ();

			/* draw loudness histogram */
			cr = Cairo::Context::create (hist);
			cr->rectangle (0, 0, 540, 128);
			cr->set_source_rgba (0, 0, 0, 1.0);
			cr->fill ();

			layout->set_font_description (UIConfiguration::instance ().get_SmallMonospaceFont ());
			layout->set_alignment (Pango::ALIGN_LEFT);
			layout->set_text (_("LUFS"));
			cr->move_to (6, 6);
			cr->set_source_rgba (.9, .9, .9, 1.0);
			layout->show_in_cairo_context (cr);

			std::vector<double> dashes;
			dashes.push_back (3.0);
			dashes.push_back (5.0);

			for (int g = -53; g <= -8; g += 5) {
				// grid-lines. [110] -59LUFS .. [650]: -5 LUFS
				layout->set_text (string_compose ("%1", g));
				layout->get_pixel_size (w, h);

				cr->save ();
				cr->set_source_rgba (.9, .9, .9, 1.0);
				cr->move_to (rint ((g + 59.0) * 10.0 - h * .5), w + 6.0);
				cr->rotate (M_PI / -2.0);
				layout->show_in_cairo_context (cr);
				cr->restore ();

				cr->save ();
				cr->set_source_rgba (.3, .3, .3, 1.0);
				cr->set_dash (dashes, 1.0);
				cr->set_line_cap (Cairo::LINE_CAP_ROUND);
				cr->move_to (rint ((g + 59.0) * 10.0) + .5, w + 8.0);
				cr->line_to (rint ((g + 59.0) * 10.0) + .5, 128.0);
				cr->stroke ();
				cr->restore ();
			}

			cr->set_operator (Cairo::OPERATOR_ADD);
			cr->set_source_rgba (.7, .7, .7, 1.0);
			cr->set_line_width (1.0);

			if (p->loudness_hist_max > 0) {
				for (size_t x = 0 ; x < 510; ++x) {
					cr->move_to (x - .5, 128.0);
					cr->line_to (x - .5, 128.0 - 128.0 * p->loudness_hist[x] / (float) p->loudness_hist_max);
					cr->stroke ();
				}
			}

			hist->flush ();
			CimgArea *nu = manage (new CimgArea (nums));
			CimgArea *hi = manage (new CimgArea (hist));
			HBox *hb = manage (new HBox ());
			hb->set_spacing (4);
			hb->pack_start (*nu);
			hb->pack_start (*hi);
			vb->pack_start (*hb);
		}

		{
			/* draw waveform */
			// TODO re-use Canvas::WaveView::draw_image() somehow.
			const size_t peaks = sizeof (p->peaks) / sizeof (ARDOUR::PeakData::PeakDatum) / 2;
			const float height_2 = 100.0;
			Cairo::RefPtr<Cairo::ImageSurface> wave = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, peaks, 2 * height_2);
			Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create (wave);
			cr->rectangle (0, 0, peaks, 2 * height_2);
			cr->set_source_rgba (0, 0, 0, 1.0);
			cr->fill ();
			cr->set_source_rgba (.7, .7, .7, 1.0);
			cr->set_line_width (1.0);
			for (size_t x = 0 ; x < peaks; ++x) {
				cr->move_to (x - .5, height_2 - height_2 * p->peaks[x].max);
				cr->line_to (x - .5, height_2 - height_2 * p->peaks[x].min);
			}
			cr->stroke ();

			cr->set_source_rgba (.3, .3, .3, 0.7);
			cr->move_to (0, height_2 - .5);
			cr->line_to (peaks, height_2 - .5);
			cr->stroke ();

			wave->flush ();
			CimgArea *wv = manage (new CimgArea (wave));
			vb->pack_start (*wv);
		}

		{
			const size_t swh = sizeof (p->spectrum) / sizeof (float);
			const size_t height = sizeof (p->spectrum[0]) / sizeof (float);
			const size_t width = swh / height;

			Cairo::RefPtr<Cairo::ImageSurface> spec = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, width, height);
			Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create (spec);
			cr->rectangle (0, 0, width, height);
			cr->set_source_rgba (0, 0, 0, 1.0);
			cr->fill ();
			for (size_t x = 0 ; x < width; ++x) {
				for (size_t y = 0 ; y < height; ++y) {
					const float pk = p->spectrum[x][y];
					ArdourCanvas::Color c = ArdourCanvas::hsva_to_color (252 - 260 * pk, .9, .3 + pk * .4);
					ArdourCanvas::set_source_rgba (cr, c);
					cr->rectangle (x - .5, y - .5, 1, 1);
					cr->fill ();
				}
			}
			spec->flush ();
			CimgArea *sp = manage (new CimgArea (spec));
			vb->pack_start (*sp);
		}

		// TODO ellipsize tab-text
		pages.pages ().push_back (Notebook_Helpers::TabElem (*vb, Glib::path_get_basename (i->first)));
	}

	pages.set_show_tabs (true);
	pages.show_all ();
	pages.set_name ("ExportReportNotebook");
	pages.set_current_page (0);

	get_vbox ()->set_spacing (12);
	get_vbox ()->pack_start (pages);

	add_button (Stock::CLOSE, RESPONSE_ACCEPT);
	set_default_response (RESPONSE_ACCEPT);
	show_all ();
	//pages.signal_switch_page ().connect (sigc::mem_fun (*this, &ExportReport::handle_page_change));
}

int
ExportReport::run ()
{
	return ArdourDialog::run ();
}
