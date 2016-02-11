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
#include <gtkmm/table.h>
#include <gtkmm/stock.h>

#include "gtkmm2ext/utils.h"
#include "canvas/utils.h"
#include "canvas/colors.h"

#include "ardour/audiofilesource.h"
#include "ardour/session.h"

#include "audio_clock.h"
#include "ui_config.h"
#include "export_report.h"

#include "i18n.h"

using namespace Gtk;
using namespace ARDOUR;

ExportReport::ExportReport (Session* _session, StatusPtr s)
	: ArdourDialog (_("Export Report/Analysis"))
	, status (s)
{

	AnalysisResults & ar = status->result_map;

	std::vector<double> dashes;
	dashes.push_back (3.0);
	dashes.push_back (5.0);

	for (AnalysisResults::iterator i = ar.begin (); i != ar.end (); ++i) {
		Label *l;
		VBox *vb = manage (new VBox ());
		Table *t = manage (new Table (4,4));
		t->set_spacings (6);
		vb->set_spacing (6);
		vb->pack_start (*t);

		std::string path = i->first;

		l = manage (new Label (_("File:"), ALIGN_END));
		t->attach (*l, 0, 1, 0, 1);
		l = manage (new Label (path, ALIGN_START));
		t->attach (*l, 1, 4, 0, 1);

		SoundFileInfo info;
		std::string errmsg;

		if (AudioFileSource::get_soundfile_info (path, info, errmsg)) {
			AudioClock * clock;

			framecnt_t const nfr = _session ? _session->nominal_frame_rate() : 25;
			double src_coef = (double) nfr / info.samplerate;

			l = manage (new Label (_("Channels:"), ALIGN_END));
			t->attach (*l, 0, 1, 1, 2);
			l = manage (new Label (string_compose ("%1", info.channels)));
			t->attach (*l, 1, 2, 1, 2);

			l = manage (new Label (_("Format:"), ALIGN_END));
			t->attach (*l, 2, 3, 1, 3);
			l = manage (new Label (info.format_name));
			t->attach (*l, 3, 4, 1, 3);

			l = manage (new Label (_("Sample rate:"), ALIGN_END));
			t->attach (*l, 0, 1, 2, 3);
			l = manage (new Label (string_compose (_("%1 Hz"), info.samplerate)));
			t->attach (*l, 1, 2, 2, 3);

			l = manage (new Label (_("Duration:"), ALIGN_END));
			t->attach (*l, 0, 1, 3, 4);
			clock = manage (new AudioClock ("sfboxLengthClock", true, "", false, false, true, false));
			clock->set_session (_session);
			clock->set_mode (AudioClock::MinSec);
			clock->set (info.length * src_coef + 0.5, true);
			t->attach (*clock, 1, 2, 3, 4);

			l = manage (new Label (_("Timecode:"), ALIGN_END));
			t->attach (*l, 2, 3, 3, 4);
			clock = manage (new AudioClock ("sfboxTimecodeClock", true, "", false, false, false, false));
			clock->set_session (_session);
			clock->set_mode (AudioClock::Timecode);
			clock->set (info.timecode * src_coef + 0.5, true);
			t->attach (*clock, 3, 4, 3, 4);
		} else {
			l = manage (new Label (_("Error:"), ALIGN_END));
			t->attach (*l, 0, 1, 1, 2);
			l = manage (new Label (errmsg, ALIGN_START));
			t->attach (*l, 1, 4, 1, 2);
		}

		ExportAnalysisPtr p = i->second;

		{
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

			if (!i->second->have_loudness) {
				layout->set_alignment (Pango::ALIGN_CENTER);
				layout->set_font_description (UIConfiguration::instance ().get_LargeFont ());
				layout->set_text (string_compose (_("not\navailable"), std::setprecision (1), std::fixed,  p->loudness));
				layout->get_pixel_size (w, h);
				cr->move_to (rint (128 - w * .5), rint (64 - h * .5));
				layout->show_in_cairo_context (cr);
			}
			else if (p->loudness == -200 && p->loudness_range == 0) {
				layout->set_alignment (Pango::ALIGN_CENTER);
				layout->set_font_description (UIConfiguration::instance ().get_LargeFont ());
				layout->set_text (string_compose (_("not\navailable"), std::setprecision (1), std::fixed,  p->loudness));
				layout->get_pixel_size (w, h);
				cr->move_to (rint (128 - w * .5), rint (64 - h * .6));
				layout->show_in_cairo_context (cr);
				int y0 = h * .5;

				layout->set_font_description (UIConfiguration::instance ().get_SmallFont ());
				layout->set_text (_("(too short integration time)"));
				layout->get_pixel_size (w, h);
				cr->move_to (rint (128 - w * .5), rint (64 + y0));
				layout->show_in_cairo_context (cr);

			} else {
				// calc height
				int ht = 0;
				layout->set_font_description (UIConfiguration::instance ().get_SmallFont ());
				layout->set_text (string_compose (_("Integrated Loudness:"), std::setprecision (1), std::fixed,  p->loudness));
				layout->get_pixel_size (w, h);
				ht += h * 1.25;
				layout->set_text (string_compose (_("Loudness Range:"), std::setprecision (1), std::fixed,  p->loudness));
				layout->get_pixel_size (w, h);
				ht += h * 1.25;
				layout->set_font_description (UIConfiguration::instance ().get_LargeFont ());
				layout->set_text (string_compose (_("%1%2%3 LUFS"), std::setprecision (1), std::fixed,  p->loudness));
				layout->get_pixel_size (w, h);
				ht += h * 1.5;
				layout->set_text (string_compose (_("%1%2%3 LU"), std::setprecision (1), std::fixed, p->loudness_range));
				layout->get_pixel_size (w, h);
				ht += h;

				int y0 = (128 - ht) * .5;

				layout->set_font_description (UIConfiguration::instance ().get_SmallFont ());
				layout->set_alignment (Pango::ALIGN_LEFT);
				layout->set_text (string_compose (_("Integrated Loudness:"), std::setprecision (1), std::fixed,  p->loudness));
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

			cr->set_source_rgba (.7, .7, .7, 1.0);
			cr->set_line_width (1.0);

			if (p->loudness_hist_max > 0 && i->second->have_loudness) {
				for (size_t x = 0 ; x < 510; ++x) {
					cr->move_to (x - .5, 128.0);
					cr->line_to (x - .5, 128.0 - 128.0 * p->loudness_hist[x] / (float) p->loudness_hist_max);
					cr->stroke ();
				}
			}

			layout->set_font_description (UIConfiguration::instance ().get_SmallFont ());
			layout->set_alignment (Pango::ALIGN_CENTER);

			layout->set_text (_("LUFS\n(short)"));
			layout->get_pixel_size (w, h);
			Gtkmm2ext::rounded_rectangle (cr, 5, 5, w + 2, h + 2, 4);
			cr->set_source_rgba (.1, .1, .1, 0.7);
			cr->fill ();

			cr->move_to (6, 6);
			cr->set_source_rgba (.9, .9, .9, 1.0);
			layout->show_in_cairo_context (cr);

			layout->set_font_description (UIConfiguration::instance ().get_SmallMonospaceFont ());
			layout->set_alignment (Pango::ALIGN_LEFT);
			for (int g = -53; g <= -8; g += 5) {
				// grid-lines. [110] -59LUFS .. [650]: -5 LUFS
				layout->set_text (string_compose ("%1", g));
				layout->get_pixel_size (w, h);

				cr->set_operator (Cairo::OPERATOR_OVER);
				Gtkmm2ext::rounded_rectangle (cr,
						rint ((g + 59.0) * 10.0 - h * .5), 5,
						h + 2, w + 2, 4);
				const float pk = (g + 59.0) / 54.0;
				ArdourCanvas::Color c = ArdourCanvas::hsva_to_color (252 - 260 * pk, .9, .3 + pk * .4, .6);
				ArdourCanvas::set_source_rgba (cr, c);
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
				cr->line_to (rint ((g + 59.0) * 10.0) + .5, 128.0);
				cr->stroke ();
				cr->restore ();
			}

			cr->set_operator (Cairo::OPERATOR_ADD);


			hist->flush ();
			CimgArea *nu = manage (new CimgArea (nums));
			CimgArea *hi = manage (new CimgArea (hist));
			HBox *hb = manage (new HBox ());
			hb->set_spacing (4);
			hb->pack_start (*nu);
			hb->pack_start (*hi);
			vb->pack_start (*hb);
		}

		for (uint32_t c = 0; c < p->n_channels; ++c) {
			/* draw waveform */
			// TODO re-use Canvas::WaveView::draw_image() somehow.
			const size_t width = sizeof (p->peaks) / sizeof (ARDOUR::PeakData::PeakDatum) / 4;
			const float height_2 = p->n_channels == 2 ? 66.0 : 100.0;
			Cairo::RefPtr<Cairo::ImageSurface> wave = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, width, 2 * height_2);
			Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create (wave);
			cr->rectangle (0, 0, width, 2 * height_2);
			cr->set_source_rgba (0, 0, 0, 1.0);
			cr->fill ();
			cr->set_source_rgba (.7, .7, .7, 1.0);
			cr->set_line_width (1.0);
			for (size_t x = 0 ; x < width; ++x) {
				cr->move_to (x - .5, height_2 - height_2 * p->peaks[c][x].max);
				cr->line_to (x - .5, height_2 - height_2 * p->peaks[c][x].min);
			}
			cr->stroke ();

			// zero line
			cr->set_source_rgba (.3, .3, .3, 0.7);
			cr->move_to (0, height_2 - .5);
			cr->line_to (width, height_2 - .5);
			cr->stroke ();

			cr->set_dash (dashes, 2.0);
			cr->set_line_cap (Cairo::LINE_CAP_ROUND);

			Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (get_pango_context ());
			layout->set_alignment (Pango::ALIGN_LEFT);
			layout->set_font_description (UIConfiguration::instance ().get_SmallerFont ());
			int w, h;

			layout->set_text (_("dBFS"));
			layout->get_pixel_size (w, h);
			Gtkmm2ext::rounded_rectangle (cr,
					7, rint (height_2 - w * .5 - 1), h + 2, w + 2, 4);
			cr->set_source_rgba (.1, .1, .1, 0.7);
			cr->fill ();
			cr->move_to (8, rint (height_2 + w * .5));
			cr->set_source_rgba (.9, .9, .9, 1.0);
			cr->save ();
			cr->rotate (M_PI / -2.0);
			layout->show_in_cairo_context (cr);
			cr->restore ();

			layout->set_font_description (UIConfiguration::instance ().get_SmallMonospaceFont ());
#define PEAKANNOTATION(POS, TXT) {                            \
			const float yy = rint (POS);                            \
			layout->set_text (TXT);                                 \
			layout->get_pixel_size (w, h);                          \
			cr->set_operator (Cairo::OPERATOR_OVER);                \
			Gtkmm2ext::rounded_rectangle (cr,                       \
			    5, rint ((POS) - h * .5 - 1), w + 2, h + 2, 4);     \
			cr->set_source_rgba (.1, .1, .1, 0.7);                  \
			cr->fill ();                                            \
			cr->move_to (6, rint ((POS) - h * .5));                 \
			cr->set_source_rgba (.9, .9, .9, 1.0);                  \
			layout->show_in_cairo_context (cr);                     \
			cr->move_to (8 + w, yy - .5);                           \
			cr->line_to (width, yy - .5);                           \
			cr->set_source_rgba (.3, .3, .3, 1.0);                  \
			cr->set_operator (Cairo::OPERATOR_ADD);                 \
			cr->stroke ();                                          \
			}

			PEAKANNOTATION (height_2 * 0.6452, _("-9"));
			PEAKANNOTATION (height_2 * 1.3548, _("-9"));
			PEAKANNOTATION (height_2 * 0.2921, _("-3"));
			PEAKANNOTATION (height_2 * 1.7079, _("-3"));

			wave->flush ();
			CimgArea *wv = manage (new CimgArea (wave));
			vb->pack_start (*wv);
		}

		{
			int w, h;
			Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (get_pango_context ());
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

			layout->set_font_description (UIConfiguration::instance ().get_SmallMonospaceFont ());
			cr->set_line_width (1.0);
			cr->set_dash (dashes, 2.0);
			cr->set_line_cap (Cairo::LINE_CAP_ROUND);
			//PEAKANNOTATION (p->freq[0], _("50Hz"));
			PEAKANNOTATION (p->freq[1], _("100Hz"));
			PEAKANNOTATION (p->freq[2], _("500Hz"));
			PEAKANNOTATION (p->freq[3], _("1kHz"));
			PEAKANNOTATION (p->freq[4], _("5kHz"));
			PEAKANNOTATION (p->freq[5], _("10kHz"));

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
