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

#include <algorithm>

#include <pangomm/layout.h>
#include <gtkmm/label.h>
#include <gtkmm/table.h>
#include <gtkmm/stock.h>

#include "pbd/openuri.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/utils.h"
#include "canvas/utils.h"
#include "canvas/colors.h"

#include "ardour/audiofilesource.h"
#include "ardour/session.h"
#include "ardour/ardour/dB.h"

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
	set_resizable (false);
	pages.set_scrollable ();

	AnalysisResults & ar = status->result_map;

	std::vector<double> dashes;
	dashes.push_back (3.0);
	dashes.push_back (5.0);

	for (AnalysisResults::iterator i = ar.begin (); i != ar.end (); ++i) {
		Label *l;
		VBox *vb = manage (new VBox ());
		Table *t = manage (new Table (4, 4));
		t->set_border_width (0);
		t->set_spacings (4);
		vb->set_spacing (4);
		vb->set_border_width (4);
		vb->pack_start (*t, false, false, 2);

		std::string path = i->first;
		ExportAnalysisPtr p = i->second;

		l = manage (new Label (_("File:"), ALIGN_END));
		t->attach (*l, 0, 1, 0, 1);
		l = manage (new Label ());
		l->set_ellipsize (Pango::ELLIPSIZE_START);
		l->set_width_chars (64);
		l->set_max_width_chars (64);
		l->set_text (path);
		l->set_alignment (ALIGN_START, ALIGN_CENTER);
		t->attach (*l, 1, 3, 0, 1, FILL, SHRINK);

		Button *b = manage (new Button (_("Open Folder")));
		t->attach (*b, 3, 4, 0, 2, FILL, SHRINK);
		b->signal_clicked ().connect (sigc::bind (sigc::mem_fun (*this, &ExportReport::open_clicked), path));

		SoundFileInfo info;
		std::string errmsg;

		framecnt_t file_length = 0;
		framecnt_t sample_rate = 0;
		framecnt_t start_off = 0;

		if (AudioFileSource::get_soundfile_info (path, info, errmsg)) {
			AudioClock * clock;

			file_length = info.length;
			sample_rate = info.samplerate;
			start_off = info.timecode;

			/* File Info Table */

			framecnt_t const nfr = _session ? _session->nominal_frame_rate () : 25;
			double src_coef = (double) nfr / info.samplerate;

			l = manage (new Label (_("Format:"), ALIGN_END));
			t->attach (*l, 0, 1, 1, 2);
			std::string fmt = info.format_name;
			std::replace (fmt.begin (), fmt.end (), '\n', ' ');
			l = manage (new Label ());
			l->set_ellipsize (Pango::ELLIPSIZE_START);
			l->set_width_chars (64);
			l->set_max_width_chars (64);
			l->set_text (fmt);
			l->set_alignment (ALIGN_START, ALIGN_CENTER);
			t->attach (*l, 1, 3, 1, 2, FILL, SHRINK);

			l = manage (new Label (_("Channels:"), ALIGN_END));
			t->attach (*l, 0, 1, 2, 3);
			l = manage (new Label (string_compose ("%1", info.channels), ALIGN_START));
			t->attach (*l, 1, 2, 2, 3);

			l = manage (new Label (_("Sample rate:"), ALIGN_END));
			t->attach (*l, 0, 1, 3, 4);
			l = manage (new Label (string_compose (_("%1 Hz"), info.samplerate), ALIGN_START));
			t->attach (*l, 1, 2, 3, 4);

			l = manage (new Label (_("Duration:"), ALIGN_END));
			t->attach (*l, 2, 3, 2, 3);
			clock = manage (new AudioClock ("sfboxLengthClock", true, "", false, false, true, false));
			clock->set_session (_session);
			clock->set_mode (AudioClock::MinSec);
			clock->set (info.length * src_coef + 0.5, true);
			t->attach (*clock, 3, 4, 2, 3);

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

		int w, h;
		Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (get_pango_context ());

		// calc geometry of numerics
		const float dbfs = accurate_coefficient_to_dB (p->peak);
		const float dbtp = accurate_coefficient_to_dB (p->truepeak);

#define TXTSIZE(LINE, TXT, FONT) {                                     \
  layout->set_font_description (UIConfiguration::instance ().FONT ()); \
  layout->set_text (TXT);                                              \
  layout->get_pixel_size (w, h);                                       \
  if (w > mnw) { mnw = w; }                                            \
  if (h > lin[LINE]) { lin[LINE] = h; }                                \
}

#define TXTWIDTH(TXT, FONT) {                                          \
  layout->set_font_description (UIConfiguration::instance ().FONT ()); \
  layout->set_text (TXT);                                              \
  layout->get_pixel_size (w, h);                                       \
  if (w > mml) { mml = w; }                                            \
}

		int mnw = 0; // max numeric width
		int lin[4] = { 0, 0, 0, 0 }; // max line height

		TXTSIZE(0, _("(too short integration time)"), get_SmallFont);

		TXTSIZE(0, _("Peak:"), get_SmallFont);
		TXTSIZE(1, string_compose (_("%1 dBFS"), std::setprecision (1), std::fixed, dbfs), get_LargeFont);
		TXTSIZE(2, _("True Peak:"), get_SmallFont);
		TXTSIZE(3, string_compose (_("%1 dBTP"), std::setprecision (1), std::fixed, dbtp), get_LargeFont);

		TXTSIZE(0, _("Integrated Loudness:"), get_SmallFont);
		TXTSIZE(1, string_compose (_("%1 LUFS"), std::setprecision (1), std::fixed, p->loudness), get_LargeFont);
		TXTSIZE(2, _("Loudness Range:"), get_SmallFont);
		TXTSIZE(3, string_compose (_("%1 LU"), std::setprecision (1), std::fixed, p->loudness_range), get_LargeFont);

		mnw += 8;
		const int ht = lin[0] * 1.25 + lin[1] * 1.25 + lin[2] * 1.25 + lin[3] + 8;
		const int hh = std::max (100, ht);
		int m_l =  2 * mnw + /*hist-width*/ 540 + /*box spacing*/ 8 - /*peak-width*/ 800; // margin left

		int mml = 0; // min margin left -- ensure left margin is wide enough
		TXTWIDTH (_("Time"), get_SmallFont);
		TXTWIDTH (_("100"), get_SmallMonospaceFont);
		m_l = (std::max (m_l, mml + 8) + 3) & ~3;

		mnw = (m_l - /*hist-width*/ 540 - /*box spacing*/ 8 + /*peak-width*/ 800) / 2;
		const int nw2 = mnw / 2; // nums, horizontal center

		int y0[4];
		y0[0] = (hh - ht) * .5 + lin[0] * .25;
		y0[1] = y0[0] + lin[0] * 1.25;
		y0[2] = y0[1] + lin[1] * 1.25;
		y0[3] = y0[2] + lin[2] * 1.25;


		{ /* peak, loudness and R128 histogram */
			Cairo::RefPtr<Cairo::ImageSurface> nums = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, mnw, hh);
			Cairo::RefPtr<Cairo::ImageSurface> ebur = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, mnw, hh);
			Cairo::RefPtr<Cairo::ImageSurface> hist = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, 540, hh);

			/* peak and true-peak numerics */
			Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create (nums);
			cr->set_source_rgba (0, 0, 0, 1.0);
			cr->paint ();

			layout->set_font_description (UIConfiguration::instance ().get_SmallFont ());
			layout->set_alignment (Pango::ALIGN_LEFT);
			layout->set_text (_("Peak:"));
			layout->get_pixel_size (w, h);
			cr->move_to (rint (nw2 - w * .5), y0[0]);
			cr->set_source_rgba (.7, .7, .7, 1.0);
			layout->show_in_cairo_context (cr);

			layout->set_font_description (UIConfiguration::instance ().get_LargeFont ());
			layout->set_text (string_compose (_("%1 dBFS"), std::setprecision (1), std::fixed,
						accurate_coefficient_to_dB (p->peak)));
			layout->get_pixel_size (w, h);
			cr->move_to (rint (nw2 - w * .5), y0[1]);
			if (p->peak > .944) { cr->set_source_rgba (1.0, .5, .5, 1.0); }
			layout->show_in_cairo_context (cr);

			if (p->have_dbtp) {
				layout->set_font_description (UIConfiguration::instance ().get_SmallFont ());
				layout->set_text (_("True Peak:"));
				layout->get_pixel_size (w, h);
				cr->move_to (rint (nw2 - w * .5), y0[2]);
				cr->set_source_rgba (.7, .7, .7, 1.0);
				layout->show_in_cairo_context (cr);

				layout->set_font_description (UIConfiguration::instance ().get_LargeFont ());
				layout->set_text (string_compose (_("%1 dBTP"), std::setprecision (1), std::fixed,
						accurate_coefficient_to_dB (p->truepeak)));
				layout->get_pixel_size (w, h);
				cr->move_to (rint (nw2 - w * .5), y0[3]);
				if (p->truepeak > .944) { cr->set_source_rgba (1.0, .5, .5, 1.0); }
				layout->show_in_cairo_context (cr);
			}

			nums->flush ();

			/* EBU R128 numerics */
			cr = Cairo::Context::create (ebur);
			cr->set_source_rgba (0, 0, 0, 1.0);
			cr->paint ();

			cr->set_source_rgba (.7, .7, .7, 1.0);

			if (!i->second->have_loudness) {
				layout->set_alignment (Pango::ALIGN_CENTER);
				layout->set_font_description (UIConfiguration::instance ().get_LargeFont ());
				layout->set_text (_("Not\nAvailable"));
				layout->get_pixel_size (w, h);
				cr->move_to (rint (nw2 - w * .5), rint ((hh - h) * .5));
				layout->show_in_cairo_context (cr);
			}
			else if (p->loudness == -200 && p->loudness_range == 0) {
				layout->set_alignment (Pango::ALIGN_CENTER);
				layout->set_font_description (UIConfiguration::instance ().get_LargeFont ());
				layout->set_text (_("Not\nAvailable"));
				layout->get_pixel_size (w, h);
				cr->move_to (rint (nw2 - w * .5), rint (hh * .5 - h * .6));
				layout->show_in_cairo_context (cr);
				int yy = h * .5;

				layout->set_font_description (UIConfiguration::instance ().get_SmallFont ());
				layout->set_text (_("(too short integration time)"));
				layout->get_pixel_size (w, h);
				cr->move_to (rint (nw2 - w * .5), rint (hh * .5 + yy));
				layout->show_in_cairo_context (cr);

			} else {
				layout->set_font_description (UIConfiguration::instance ().get_SmallFont ());
				layout->set_alignment (Pango::ALIGN_LEFT);
				layout->set_text (_("Integrated Loudness:"));
				layout->get_pixel_size (w, h);
				cr->move_to (rint (nw2 - w * .5), y0[0]);
				layout->show_in_cairo_context (cr);

				layout->set_font_description (UIConfiguration::instance ().get_LargeFont ());
				layout->set_text (string_compose (_("%1 LUFS"), std::setprecision (1), std::fixed,  p->loudness));
				layout->get_pixel_size (w, h);
				cr->move_to (rint (nw2 - w * .5), y0[1]);
				layout->show_in_cairo_context (cr);

				layout->set_font_description (UIConfiguration::instance ().get_SmallFont ());
				layout->set_text (_("Loudness Range:"));
				layout->get_pixel_size (w, h);
				cr->move_to (rint (nw2 - w * .5), y0[2]);
				layout->show_in_cairo_context (cr);

				layout->set_font_description (UIConfiguration::instance ().get_LargeFont ());
				layout->set_text (string_compose (_("%1 LU"), std::setprecision (1), std::fixed, p->loudness_range));
				layout->get_pixel_size (w, h);
				cr->move_to (rint (nw2 - w * .5), y0[3]);
				layout->show_in_cairo_context (cr);
			}
			ebur->flush ();

			/* draw loudness histogram */
			cr = Cairo::Context::create (hist);
			cr->set_source_rgba (0, 0, 0, 1.0);
			cr->paint ();

			cr->set_source_rgba (.7, .7, .7, 1.0);
			cr->set_line_width (1.0);

			if (p->loudness_hist_max > 0 && i->second->have_loudness) {
				for (size_t x = 0 ; x < 510; ++x) {
					cr->move_to (x - .5, hh);
					cr->line_to (x - .5, (float) hh * (1.0 - p->loudness_hist[x] / (float) p->loudness_hist_max));
					cr->stroke ();
				}
			}

			layout->set_font_description (UIConfiguration::instance ().get_SmallerFont ());
			layout->set_alignment (Pango::ALIGN_CENTER);

			// Label
			layout->set_text (_("LUFS\n(short)"));
			layout->get_pixel_size (w, h);
			Gtkmm2ext::rounded_rectangle (cr, 5, rint (.5 * (hh - w) - 1), h + 2, w + 2, 4);
			cr->set_source_rgba (.1, .1, .1, 0.7);
			cr->fill ();
			cr->save ();
			cr->move_to (6, rint (.5 * (hh + w)));
			cr->set_source_rgba (.9, .9, .9, 1.0);
			cr->rotate (M_PI / -2.0);
			layout->show_in_cairo_context (cr);
			cr->restore ();

			// x-Axis
			layout->set_font_description (UIConfiguration::instance ().get_SmallMonospaceFont ());
			layout->set_alignment (Pango::ALIGN_LEFT);
			for (int g = -53; g <= -8; g += 5) {
				// grid-lines. [110] -59LUFS .. [650]: -5 LUFS
				layout->set_text (string_compose ("%1", std::setw(3), std::setfill(' '), g));
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
				cr->line_to (rint ((g + 59.0) * 10.0) + .5, hh);
				cr->stroke ();
				cr->restore ();
			}

			hist->flush ();

			CimgArea *nu = manage (new CimgArea (nums));
			CimgArea *eb = manage (new CimgArea (ebur));
			CimgArea *hi = manage (new CimgArea (hist));
			HBox *hb = manage (new HBox ());
			hb->set_spacing (4);
			hb->pack_start (*nu, false, false);
			hb->pack_start (*hi, false, false);
			hb->pack_start (*eb, false, false);
			vb->pack_start (*hb, false, false);
		}

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

		for (uint32_t c = 0; c < p->n_channels; ++c) {
			/* draw waveform */
			const size_t width = sizeof (p->peaks) / sizeof (ARDOUR::PeakData::PeakDatum) / 4;
			const float height_2 = std::min (100, 8 * lin[0] / (int) p->n_channels); // TODO refine

			Cairo::RefPtr<Cairo::ImageSurface> wave = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, m_l + width, 2 * height_2);
			Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create (wave);
			cr->set_operator (Cairo::OPERATOR_SOURCE);
			cr->rectangle (0, 0, m_l, 2 * height_2);
			cr->set_source_rgba (0, 0, 0, 0);
			cr->fill ();
			cr->rectangle (m_l, 0, width, 2 * height_2);
			cr->set_source_rgba (0, 0, 0, 1.0);
			cr->fill ();
			cr->set_operator (Cairo::OPERATOR_OVER);

			cr->set_source_rgba (.7, .7, .7, 1.0);
			cr->set_line_width (1.0);
			for (size_t x = 0 ; x < width; ++x) {
				cr->move_to (m_l + x - .5, height_2 - height_2 * p->peaks[c][x].max);
				cr->line_to (m_l + x - .5, height_2 - height_2 * p->peaks[c][x].min);
			}
			cr->stroke ();

			// zero line
			cr->set_source_rgba (.3, .3, .3, 0.7);
			cr->move_to (m_l + 0, height_2 - .5);
			cr->line_to (m_l + width, height_2 - .5);
			cr->stroke ();

			// Unit
			layout->set_font_description (UIConfiguration::instance ().get_SmallerFont ());
			layout->set_alignment (Pango::ALIGN_LEFT);
			layout->set_text (_("dBFS"));
			layout->get_pixel_size (w, h);
			cr->move_to (rint (.5 * (m_l - h)), rint (height_2 + w * .5));
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
			XAXISLABEL (height_2 * 0.6452, _("-9"));
			XAXISLABEL (height_2 * 1.3548, _("-9"));
			XAXISLABEL (height_2 * 0.2921, _("-3"));
			XAXISLABEL (height_2 * 1.7079, _("-3"));

			wave->flush ();
			CimgArea *wv = manage (new CimgArea (wave));
			vb->pack_start (*wv);
		}

		if (file_length > 0 && sample_rate > 0)
		{
			/* Time Axis  -- re-use waveform width */
			const size_t width = sizeof (p->peaks) / sizeof (ARDOUR::PeakData::PeakDatum) / 4;
			layout->set_font_description (UIConfiguration::instance ().get_SmallMonospaceFont ());
			layout->set_text (_("00:00:00.000"));
			layout->get_pixel_size (w, h);
			int height = h * 1.75;
			Cairo::RefPtr<Cairo::ImageSurface> ytme = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, m_l + width, height);
			Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create (ytme);
			cr->set_operator (Cairo::OPERATOR_SOURCE);
			cr->set_source_rgba (0, 0, 0, 1.0);
			cr->paint ();
			cr->rectangle (0, 0, m_l, height);
			cr->set_source_rgba (0, 0, 0, 0);
			cr->fill ();
			cr->set_operator (Cairo::OPERATOR_OVER);

			cr->set_line_width (1.0);
			for (int i = 0; i <= 4; ++i) {
				const float fract = (float) i / 4.0;
				// " XX:XX:XX.XXX"  [space/minus] 12 chars = 13.
				const float xalign = (i == 4) ? 1.0 : (i == 0) ? 1.0 / 13.0 : 7.0 / 13.0;

				char buf[16];
				AudioClock::print_minsec (start_off + file_length * fract,
						buf, sizeof (buf), sample_rate);

				layout->set_text (buf);
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
			cr->move_to (rint (.5 * (m_l - w)), rint (.5 * (height - h)));
			layout->show_in_cairo_context (cr);

			ytme->flush ();
			CimgArea *tm = manage (new CimgArea (ytme));
			vb->pack_start (*tm);
		}

		{
			/* Draw Spectrum */
			const size_t swh = sizeof (p->spectrum) / sizeof (float);
			const size_t height = sizeof (p->spectrum[0]) / sizeof (float);
			const size_t width = swh / height;

			Cairo::RefPtr<Cairo::ImageSurface> spec = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, m_l + width, height);
			Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create (spec);
			cr->set_operator (Cairo::OPERATOR_SOURCE);
			cr->rectangle (0, 0, m_l, height);
			cr->set_source_rgba (0, 0, 0, 0);
			cr->fill ();
			cr->rectangle (m_l, 0, width, height);
			cr->set_source_rgba (0, 0, 0, 1.0);
			cr->fill ();
			cr->set_operator (Cairo::OPERATOR_OVER);

			for (size_t x = 0 ; x < width; ++x) {
				for (size_t y = 0 ; y < height; ++y) {
					const float pk = p->spectrum[x][y];
					ArdourCanvas::Color c = ArdourCanvas::hsva_to_color (252 - 260 * pk, .9, .3 + pk * .4);
					ArdourCanvas::set_source_rgba (cr, c);
					cr->rectangle (m_l + x - .5, y - .5, 1, 1);
					cr->fill ();
				}
			}

			// Unit
			layout->set_font_description (UIConfiguration::instance ().get_SmallerFont ());
			layout->set_text (_("Hz"));
			layout->get_pixel_size (w, h);
			cr->move_to (rint (.5 * (m_l - h)), rint ((height + w) * .5));
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
			//XAXISLABEL (p->freq[0], _("50Hz"));
			XAXISLABEL (p->freq[1], _("100"));
			XAXISLABEL (p->freq[2], _("500"));
			XAXISLABEL (p->freq[3], _("1K"));
			XAXISLABEL (p->freq[4], _("5K"));
			XAXISLABEL (p->freq[5], _("10K"));

			spec->flush ();
			CimgArea *sp = manage (new CimgArea (spec));
			vb->pack_start (*sp);
		}

		pages.pages ().push_back (Notebook_Helpers::TabElem (*vb, Glib::path_get_basename (i->first)));
	}

	pages.set_show_tabs (true);
	pages.show_all ();
	pages.set_name ("ExportReportNotebook");
	pages.set_current_page (0);

	get_vbox ()->set_spacing (4);
	get_vbox ()->pack_start (pages, false, false);

	add_button (Stock::CLOSE, RESPONSE_ACCEPT);
	set_default_response (RESPONSE_ACCEPT);
	show_all ();
}

int
ExportReport::run ()
{
	return ArdourDialog::run ();
}

void
ExportReport::open_clicked (std::string p)
{
	PBD::open_uri (Glib::path_get_dirname(p));
}
