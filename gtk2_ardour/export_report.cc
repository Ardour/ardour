/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016-2018 Robin Gareus <robin@gareus.org>
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

#include <pangomm/layout.h>
#include <gtkmm/label.h>
#include <gtkmm/table.h>
#include <gtkmm/stock.h>

#include "pbd/openuri.h"
#include "pbd/basename.h"

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/colors.h"

#include "audiographer/general/analyser.h"

#include "ardour/audiofilesource.h"
#include "ardour/audioregion.h"
#include "ardour/auditioner.h"
#include "ardour/dB.h"
#include "ardour/logmeter.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/smf_source.h"
#include "ardour/source_factory.h"
#include "ardour/srcfilesource.h"
#include "ardour/utils.h"

#include "audio_clock.h"
#include "export_report.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;

ExportReport::ExportReport (Session* session, StatusPtr s)
	: ArdourDialog (_("Export Report/Analysis"))
	, _session (session)
	, stop_btn (0)
	, play_btn (0)
	, _audition_num (-1)
	, _page_num (0)
{
	init (s->result_map, true);
}

ExportReport::ExportReport (const std::string & title, const AnalysisResults & ar)
	: ArdourDialog (title)
	, _session (0)
	, stop_btn (0)
	, play_btn (0)
	, _audition_num (-1)
	, _page_num (0)
{
	init (ar, false);
}

void
ExportReport::init (const AnalysisResults & ar, bool with_file)
{
	set_resizable (false);
	pages.set_scrollable ();

	std::vector<double> dashes;
	dashes.push_back (3.0);
	dashes.push_back (5.0);

	int page = 0;
	for (AnalysisResults::const_iterator i = ar.begin (); i != ar.end (); ++i, ++page) {
		Label *l;
		VBox *vb = manage (new VBox ());
		Table *t = manage (new Table (4, 4));
		Table *wtbl = manage (new Table (3, 2));
		int wrow = 0;
		t->set_border_width (0);
		t->set_spacings (4);
		wtbl->set_spacings (4);
		vb->set_spacing (4);
		vb->set_border_width (4);
		vb->pack_start (*t, false, false, 2);
		vb->pack_start (*wtbl, false, false, 2);

		std::string path = i->first;
		ExportAnalysisPtr p = i->second;

		std::list<CimgPlayheadArea*> playhead_widgets;

		if (with_file) {
			l = manage (new Label (_("File:"), ALIGN_END));
			t->attach (*l, 0, 1, 0, 1);
			l = manage (new Label ());
			l->set_ellipsize (Pango::ELLIPSIZE_START);
			l->set_width_chars (48);
			l->set_max_width_chars (48);
			l->set_text (path);
			l->set_alignment (ALIGN_START, ALIGN_CENTER);
			t->attach (*l, 1, 3, 0, 1, FILL, SHRINK);

			Button *b = manage (new Button (_("Open Folder")));
			t->attach (*b, 3, 4, 0, 2, FILL, SHRINK);
			b->signal_clicked ().connect (sigc::bind (sigc::mem_fun (*this, &ExportReport::open_folder), path));
		}

		SoundFileInfo info;
		std::string errmsg;

		samplecnt_t file_length = 0;
		samplecnt_t sample_rate = 0;
		samplecnt_t start_off = 0;
		unsigned int channels = 0;
		std::string file_fmt;

		if (with_file && AudioFileSource::get_soundfile_info (path, info, errmsg)) {
			AudioClock * clock;

			file_length = info.length;
			sample_rate = info.samplerate;
			start_off = info.timecode;
			channels = info.channels;

			files.insert (std::make_pair (page, AuditionInfo (path, channels)));

			/* File Info Table */

			samplecnt_t const nfr = _session ? _session->nominal_sample_rate () : 25;
			double src_coef = (double) nfr / info.samplerate;

			l = manage (new Label (_("Format:"), ALIGN_END));
			t->attach (*l, 0, 1, 1, 2);
			file_fmt = info.format_name;
			std::replace (file_fmt.begin (), file_fmt.end (), '\n', ' ');
			l = manage (new Label ());
			l->set_ellipsize (Pango::ELLIPSIZE_START);
			l->set_width_chars (48);
			l->set_max_width_chars (48);
			l->set_text (file_fmt);
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
		} else if (with_file) {
			with_file = false;
			/* Note: errmsg can have size = 1, and contain "\0\0" */
			if (!errmsg.empty() && 0 != strlen(errmsg.c_str())) {
				l = manage (new Label (_("Error:"), ALIGN_END));
				t->attach (*l, 0, 1, 1, 2);
				l = manage (new Label (errmsg, ALIGN_START));
				t->attach (*l, 1, 4, 1, 2);
			}
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

		int m_r = 0; // right side
		int mnh = 0; // mono height
		int mnw = 0; // max numeric width
		int anw = 0; // spectrum annotation text width

		int lin[6] = { 0, 0, 0, 0, 0, 0 }; // max line height

		TXTSIZE(0, _("(too short integration time)"), get_SmallFont);

		TXTSIZE(0, _("-888"), get_SmallMonospaceFont);
		anw = w;
		m_r = anw + 10;
		mnh = h + 1;

		TXTSIZE(0, _("Peak:"), get_SmallFont);
		TXTSIZE(1, string_compose (_("%1 dBFS"), std::setprecision (1), std::fixed, dbfs), get_LargeFont);
		TXTSIZE(2, _("True Peak:"), get_SmallFont);
		TXTSIZE(3, string_compose (_("%1 dBTP"), std::setprecision (1), std::fixed, dbtp), get_LargeFont);
		TXTSIZE(4, _("Normalization Gain:"), get_SmallFont);
		TXTSIZE(5, _("+888.88 dB"), get_SmallMonospaceFont);

		TXTSIZE(0, _("Integrated Loudness:"), get_SmallFont);
		TXTSIZE(1, string_compose (_("%1 LUFS"), std::setprecision (1), std::fixed, p->integrated_loudness), get_LargeFont);
		TXTSIZE(2, _("Loudness Range:"), get_SmallFont);
		TXTSIZE(3, string_compose (_("%1 LU"), std::setprecision (1), std::fixed, p->loudness_range), get_LargeFont);
		TXTSIZE(4, _("Max Short/Momentary:"), get_SmallFont);
		TXTSIZE(5, string_compose (_("%1/%2 LUFS"), std::setprecision (1), std::fixed, p->max_loudness_short, p->max_loudness_momentary), get_SmallFont);

		mnw += 8;
		const int ht = lin[0] * 1.25 + lin[1] * 1.25 + lin[2] * 1.25 + lin[3] *1.25 + lin[4] * 1.25 + lin[5];
		const int hh = std::max (100, ht + 12);
		const int htn = lin[0] * 1.25 + lin[1] * 1.25 + lin[2] * 1.25 + lin[3];
		int m_l =  2 * mnw + /*hist-width*/ 540 + /*box spacing*/ 8 - /*peak-width*/ 800 - m_r; // margin left

		int mml = 0; // min margin left -- ensure left margin is wide enough
		TXTWIDTH (_("Time"), get_SmallFont);
		TXTWIDTH (_("100"), get_SmallMonospaceFont);
		m_l = (std::max(anw + mnh + 14, std::max (m_l, mml + 8)) + 3) & ~3;

		mnw = (m_l - /*hist-width*/ 540 - /*box spacing*/ 8 + /*peak-width*/ 800 + m_r) / 2;
		const int nw2 = mnw / 2; // nums, horizontal center

		int y0[6];
		if (true /*p->normalized*/) {
			y0[0] = (hh - ht) * .5; // 5 lines
		} else {
			y0[0] = (hh - htn) * .5; // 4 lines
		}
		y0[1] = y0[0] + lin[0] * 1.25;
		y0[2] = y0[1] + lin[1] * 1.25;
		y0[3] = y0[2] + lin[2] * 1.25;
		y0[4] = y0[3] + lin[3] * 1.25;
		y0[5] = y0[4] + lin[4] * 1.25;

		/* calc heights & alignment of png-image */
		const float specth = sizeof (p->spectrum[0]) / sizeof (float);
		const float waveh2 = std::min (100, 8 * lin[0] / (int) p->n_channels);

		Cairo::RefPtr<Cairo::ImageSurface> png_surface;
		int png_w = 0;
		int png_y0 = 0;

		if (with_file && UIConfiguration::instance().get_save_export_analysis_image ()) { /*png image */
			const int top_w = 540 + 2 * (mnw + 4); // 4px spacing
			const int wav_w = m_l + m_r + 4 + sizeof (p->peaks) / sizeof (ARDOUR::PeakData::PeakDatum) / 4;
			const int spc_w = m_l + m_r + 4 + sizeof (p->spectrum) / sizeof (float) / specth;
			int ann_h = 0;
			int linesp = 0;

			if (channels > 0 && file_length > 0 && sample_rate > 0) {
				layout->set_font_description (UIConfiguration::instance ().get_SmallMonospaceFont ());
				layout->set_text (X_("00:00:00.0"));
				layout->get_pixel_size (w, h);
				int height = h * 1.75;
				ann_h = 4 + height /* Time Axis */;

				layout->set_font_description (UIConfiguration::instance ().get_SmallFont ());
				layout->set_text (X_("0|A8"));
				layout->get_pixel_size (w, h);
				linesp = h * 1.5;
				ann_h += 4 + 3 * linesp; /* File Info */;
			}

			const int png_h = hh + 4 + p->n_channels * (2 * waveh2 + 4) + ann_h + specth + 4;
			png_w = std::max (std::max (top_w, wav_w), spc_w);

			png_surface = Cairo::ImageSurface::create (Cairo::FORMAT_RGB24, png_w, png_h);
			Cairo::RefPtr<Cairo::Context> pcx = Cairo::Context::create (png_surface);
			pcx->set_source_rgb (.2, .2, .2);
			pcx->paint ();

			if (channels > 0 && file_length > 0 && sample_rate > 0) {
				png_y0 += 4;
				// Add file-name, format, duration, sample-rate & timecode
				pcx->set_source_rgb (.7, .7, .7);
				layout->set_font_description (UIConfiguration::instance ().get_SmallFont ());
				layout->set_alignment (Pango::ALIGN_LEFT);

#define IMGLABEL(X0, STR, VAL) {       \
  layout->set_text (STR);              \
  pcx->move_to (X0, png_y0);           \
  layout->get_pixel_size (w, h);       \
  layout->show_in_cairo_context (pcx); \
  layout->set_text (VAL);              \
  pcx->move_to (X0 + w + 2, png_y0);   \
  layout->show_in_cairo_context (pcx); \
}

				// TODO get max width of labels per column, right-align labels,  x-align 1/3, 2/3 columns
				const int lx0 = m_l;
				const int lx1 = m_l + png_w * 2 / 3; // right-col is short (channels, SR, duration)
				std::string sha1sum = ARDOUR::compute_sha1_of_file (path);
				if (!sha1sum.empty()) {
					sha1sum = " (sha1: " + sha1sum + ")";
				}

				IMGLABEL (lx0, _("File:"), Glib::path_get_basename (path) + sha1sum);
				IMGLABEL (lx1, _("Channels:"), string_compose ("%1", channels));
				png_y0 += linesp;

				IMGLABEL (lx0, _("Format:"), file_fmt);
				IMGLABEL (lx1, _("Sample rate:"), string_compose (_("%1 Hz"), sample_rate));
				png_y0 += linesp;

				if (_session) {
					Timecode::Time tct;
					_session->sample_to_timecode (start_off, tct, false, false);
					IMGLABEL (lx0, _("Timecode:"), Timecode::timecode_format_time (tct));
				}
				IMGLABEL (lx1, _("Duration:"), Timecode::timecode_format_sampletime (file_length, sample_rate, 1000, false));
				png_y0 += linesp;
			}
		}

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
			layout->set_text (string_compose (_("%1 dBFS"), std::setprecision (1), std::fixed, dbfs));
			layout->get_pixel_size (w, h);
			cr->move_to (rint (nw2 - w * .5), y0[1]);
			if (dbfs >= 0.f) { cr->set_source_rgba (1.0, .1, .1, 1.0); }
			else if (dbfs > -1.f) { cr->set_source_rgba (1.0, .7, .0, 1.0); }
			layout->show_in_cairo_context (cr);

			if (p->have_dbtp) {
				layout->set_font_description (UIConfiguration::instance ().get_SmallFont ());
				layout->set_text (_("True Peak:"));
				layout->get_pixel_size (w, h);
				cr->move_to (rint (nw2 - w * .5), y0[2]);
				cr->set_source_rgba (.7, .7, .7, 1.0);
				layout->show_in_cairo_context (cr);

				layout->set_font_description (UIConfiguration::instance ().get_LargeFont ());
				layout->set_text (string_compose (_("%1 dBTP"), std::setprecision (1), std::fixed, dbtp));
				layout->get_pixel_size (w, h);
				cr->move_to (rint (nw2 - w * .5), y0[3]);
				if (dbtp >= 0.f) { cr->set_source_rgba (1.0, .1, .1, 1.0); }
				else if (dbtp > -1.f) { cr->set_source_rgba (1.0, .7, .0, 1.0); }
				layout->show_in_cairo_context (cr);
			}

			if (p->normalized) {
				const float ndb = accurate_coefficient_to_dB (p->norm_gain_factor);
				layout->set_font_description (UIConfiguration::instance ().get_SmallFont ());
				layout->set_text (_("Normalization Gain:"));
				layout->get_pixel_size (w, h);
				cr->move_to (rint (nw2 - w * .5), y0[4]);
				cr->set_source_rgba (.7, .7, .7, 1.0);
				layout->show_in_cairo_context (cr);

				layout->set_font_description (UIConfiguration::instance ().get_SmallMonospaceFont ());
				layout->set_text (string_compose (_("%1 dB"), std::setprecision (2), std::showpos, std::fixed, ndb));

				layout->get_pixel_size (w, h);
				cr->move_to (rint (nw2 - w * .5), y0[5]);
				// TODO tweak thresholds
				if (p->norm_gain_factor < 1.0) {
					cr->set_source_rgba (1.0, .7, .1, 1.0);
				} else if (p->norm_gain_factor == 1.0) {
					cr->set_source_rgba (.7, .7, .7, 1.0);
				} else if (fabsf (ndb) < 12) {
					cr->set_source_rgba (.1, 1.0, .1, 1.0);
				} else if (fabsf (ndb) < 18) {
					cr->set_source_rgba (1.0, .7, .1, 1.0);
				} else {
					cr->set_source_rgba (1.0, .1, .1, 1.0);
				}
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
			else if (p->integrated_loudness == -200 && p->loudness_range == 0) {
				layout->set_alignment (Pango::ALIGN_CENTER);
				layout->set_font_description (UIConfiguration::instance ().get_LargeFont ());
				layout->set_text (_("Not\nAvailable"));
				layout->get_pixel_size (w, h);
				cr->move_to (rint (nw2 - w * .5), rint (hh * .5 - h * .66));
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
				layout->set_text (string_compose (_("%1 LUFS"), std::setprecision (1), std::fixed,  p->integrated_loudness));
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

				layout->set_font_description (UIConfiguration::instance ().get_SmallFont ());
				layout->set_text (_("Max Short/Momentary:"));
				layout->get_pixel_size (w, h);
				cr->move_to (rint (nw2 - w * .5), y0[4]);
				layout->show_in_cairo_context (cr);

				layout->set_text (string_compose (_("%1/%2 LUFS"), std::setprecision (1), std::fixed, p->max_loudness_short, p->max_loudness_momentary));
				layout->get_pixel_size (w, h);
				cr->move_to (rint (nw2 - w * .5), y0[5]);
				layout->show_in_cairo_context (cr);

				layout->set_font_description (UIConfiguration::instance ().get_LargeFont ());
			}
			ebur->flush ();

			/* draw loudness histogram */
			cr = Cairo::Context::create (hist);
			cr->set_source_rgba (0, 0, 0, 1.0);
			cr->paint ();

			cr->set_source_rgba (.7, .7, .7, 1.0);
			cr->set_line_width (1.0);

			if (p->loudness_hist_max > 0 && i->second->have_loudness) {
				// draw data
				for (size_t x = 0 ; x < 510; ++x) {
					cr->move_to (x - .5, hh);
					cr->line_to (x - .5, (float) hh * (1.0 - p->loudness_hist[x] / (float) p->loudness_hist_max));
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
				Gtkmm2ext::rounded_rectangle (cr, 5, hh - w - 3, h + 2, w + 2, 4);
				cr->set_source_rgba (.1, .1, .1, 0.7);
				cr->fill ();
				cr->save ();
				cr->move_to (6, hh - 2);
				cr->set_source_rgba (.9, .9, .9, 1.0);
				cr->rotate (M_PI / -2.0);
				layout->show_in_cairo_context (cr);
				cr->restore ();

				// x-Axis labels
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
					Gtkmm2ext::Color c = Gtkmm2ext::hsva_to_color (252 - 260 * pk, .9, .3 + pk * .4, .6);
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
					cr->line_to (rint ((g + 59.0) * 10.0) + .5, hh);
					cr->stroke ();
					cr->restore ();
				}

			} else {
				layout->set_alignment (Pango::ALIGN_CENTER);
				layout->set_font_description (UIConfiguration::instance ().get_LargeFont ());
				layout->set_text (_("Not\nAvailable"));
				layout->get_pixel_size (w, h);
				cr->move_to (rint ((510 - w) * .5), rint ((hh - h) * .5));
				layout->show_in_cairo_context (cr);
			}

			hist->flush ();

			if (png_surface) {
				Cairo::RefPtr<Cairo::Context> pcx = Cairo::Context::create (png_surface);
				pcx->set_source (nums, 0, png_y0);
				pcx->paint ();
				pcx->set_source (hist, (png_w - 540) / 2, png_y0);
				pcx->paint ();
				pcx->set_source (ebur, png_w - mnw, png_y0);
				pcx->paint ();
				png_y0 += hh + 4;
			}

			CimgArea *nu = manage (new CimgArea (nums));
			CimgArea *eb = manage (new CimgArea (ebur));
			CimgArea *hi = manage (new CimgArea (hist));
			HBox *hb = manage (new HBox ());
			hb->set_spacing (4);
			hb->pack_start (*nu, false, false);
			hb->pack_start (*hi, false, false);
			hb->pack_start (*eb, false, false);

			wtbl->attach (*hb, 0, 2, wrow, wrow + 1, SHRINK, SHRINK);
			++wrow;
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


		{
			VBox *lrb = manage (new VBox());
			ToggleButton *log = manage (new ToggleButton (S_("Logscale|Lg")));
			ToggleButton *rec = manage (new ToggleButton (S_("Rectified|Rf")));
			Gtkmm2ext::UI::instance()->set_tip (log, _("Logscale"));
			Gtkmm2ext::UI::instance()->set_tip (rec, _("Rectified"));

			lrb->pack_start (*log, false, false, 5);
			lrb->pack_end (*rec, false, false, 5);
			log->signal_toggled ().connect (sigc::bind (sigc::mem_fun (*this, &ExportReport::on_logscale_toggled), log));
			rec->signal_toggled ().connect (sigc::bind (sigc::mem_fun (*this, &ExportReport::on_rectivied_toggled), rec));
			lrb->show_all ();
			wtbl->attach (*lrb, 1, 2, wrow, wrow + p->n_channels, SHRINK, SHRINK);
		}

		for (uint32_t c = 0; c < p->n_channels; ++c) {
			/* draw waveform */
			const size_t width = sizeof (p->peaks) / sizeof (ARDOUR::PeakData::PeakDatum) / 4;

			Cairo::RefPtr<Cairo::ImageSurface> wave;
			Cairo::RefPtr<Cairo::ImageSurface> wave_log;
			Cairo::RefPtr<Cairo::ImageSurface> wave_rect;
			Cairo::RefPtr<Cairo::ImageSurface> wave_lr;
			draw_waveform(wave, p, c, m_l, width, anw, waveh2, false, false);
			draw_waveform(wave_log, p, c, m_l, width, anw, waveh2, true, false);
			draw_waveform(wave_rect, p, c, m_l, width, anw, waveh2, false, true);
			draw_waveform(wave_lr, p, c, m_l, width, anw, waveh2, true, true);

			CimgWaveArea *wv = manage (new CimgWaveArea (wave, wave_log, wave_rect, wave_lr, m_l, width));

			playhead_widgets.push_back (wv);
			waves.push_back (wv);
			wv->seek_playhead.connect (sigc::bind<0> (sigc::mem_fun (*this, &ExportReport::audition_seek), page));
			wtbl->attach (*wv, 0, 1, wrow, wrow + 1, SHRINK, SHRINK);
			++wrow;

			if (png_surface) {
				Cairo::RefPtr<Cairo::Context> pcx = Cairo::Context::create (png_surface);
				pcx->set_source (wave, 0, png_y0);
				pcx->paint ();
				png_y0 += 2 * waveh2 + 4;
			}
		}

		if (channels > 0 && file_length > 0 && sample_rate > 0)
		{
			/* Time Axis  -- re-use waveform width */
			const size_t width = sizeof (p->peaks) / sizeof (ARDOUR::PeakData::PeakDatum) / 4;
			layout->set_font_description (UIConfiguration::instance ().get_SmallMonospaceFont ());
			layout->set_text (_("00:00:00.000"));
			layout->get_pixel_size (w, h);
			int height = h * 1.75;
			int n_labels = width / (w * 1.75);
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

			for (int i = 0; i <= n_labels; ++i) {
				const float fract = (float) i / n_labels;
				const float xalign = (i == n_labels) ? 1.0 : (i == 0) ? 0 : .5;

				char buf[16];
				AudioClock::print_minsec (start_off + file_length * fract,
						buf, sizeof (buf), sample_rate, 1);

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
			CimgPlayheadArea *tm = manage (new CimgPlayheadArea (ytme, m_l, width, true));
			playhead_widgets.push_back (tm);
			tm->seek_playhead.connect (sigc::bind<0> (sigc::mem_fun (*this, &ExportReport::audition_seek), page));
			wtbl->attach (*tm, 0, 1, wrow, wrow + 1, SHRINK, SHRINK);
			++wrow;

			if (png_surface) {
				Cairo::RefPtr<Cairo::Context> pcx = Cairo::Context::create (png_surface);
				pcx->set_source (ytme, 0, png_y0);
				pcx->paint ();
				png_y0 += height + 4;
			}
		}

		{
			/* Draw Spectrum */
			const size_t height = specth;
			const size_t swh = sizeof (p->spectrum) / sizeof (float);
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
					Gtkmm2ext::Color c = Gtkmm2ext::hsva_to_color (252 - 260 * pk, .9, sqrt(pk));
					Gtkmm2ext::set_source_rgba (cr, c);
					cr->rectangle (m_l + x - .5, y - .5, 1, 1);
					cr->fill ();
				}
			}

			// Unit
			layout->set_font_description (UIConfiguration::instance ().get_SmallerFont ());
			layout->set_text (_("Hz"));
			layout->get_pixel_size (w, h);
			cr->move_to (rint (m_l - h - anw - 10), rint ((height + w) * .5));
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

			// annotations
			Cairo::RefPtr<Cairo::ImageSurface> scale = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, m_r, height);
			cr = Cairo::Context::create (scale);
			cr->set_operator (Cairo::OPERATOR_SOURCE);
			cr->rectangle (0, 0, m_r, height);
			cr->set_source_rgba (0, 0, 0, 0);
			cr->fill ();

			cr->set_operator (Cairo::OPERATOR_OVER);
			layout->set_font_description (UIConfiguration::instance ().get_SmallerFont ());
			layout->set_alignment (Pango::ALIGN_LEFT);
			layout->set_text (_("dBFS"));
			layout->get_pixel_size (w, h);
			cr->move_to (rint (.5 * (m_r - w)), height - h - 2);
			cr->set_source_rgba (.9, .9, .9, 1.0);
			layout->show_in_cairo_context (cr);

			int innertop = ceil (mnh * .5) + 1;
			size_t innerheight = (height - 2 * innertop - h - 2);

			cr->rectangle (1, innertop - 1,  m_r - 2 - anw, innerheight + 2);
			cr->set_source_rgba (0, 0, 0, 1.0);
			cr->fill_preserve ();
			cr->set_line_width (1.0);
			cr->set_source_rgba (.7, .7, .6, 1.0);
			cr->stroke ();

			for (size_t y = 0 ; y < innerheight - 2; ++y) {
					const float pk = 1.0 - (float) y / innerheight;
					Gtkmm2ext::Color c = Gtkmm2ext::hsva_to_color (252 - 260 * pk, .9, sqrt(pk));
					Gtkmm2ext::set_source_rgba (cr, c);
					cr->rectangle (2, innertop + y + .5, m_r - 4 - anw, 1);
					cr->fill ();
			}

			layout->set_font_description (UIConfiguration::instance ().get_SmallMonospaceFont ());
			for (int i = 0; i <= 4; ++i) {
				const float fract = (float) i / 4.0;
				const float yalign = .5; // (i == 4) ? 0 : (i == 0) ? 1.0 : .5;
				char buf[16];
				snprintf (buf, sizeof (buf), "%.0f", AudioGrapher::Analyser::fft_range_db * -fract);
				layout->set_text (buf);
				layout->get_pixel_size (w, h);

				cr->move_to (m_r - anw, rint (innertop + fract * innerheight - h * yalign));
				cr->set_source_rgba (.9, .9, .9, 1.0);
				layout->show_in_cairo_context (cr);
			}
			scale->flush ();

			CimgPlayheadArea *sp = manage (new CimgPlayheadArea (spec, m_l, width));
			playhead_widgets.push_back (sp);
			sp->seek_playhead.connect (sigc::bind<0> (sigc::mem_fun (*this, &ExportReport::audition_seek), page));
			CimgArea *an = manage (new CimgArea (scale));
			wtbl->attach (*sp, 0, 1, wrow, wrow + 1, SHRINK, SHRINK);
			wtbl->attach (*an, 1, 2, wrow, wrow + 1, SHRINK, SHRINK);
			++wrow;

			if (png_surface) {
				Cairo::RefPtr<Cairo::Context> pcx = Cairo::Context::create (png_surface);
				pcx->set_source (spec, 0, png_y0);
				pcx->paint ();
				pcx->set_source (scale, png_w - m_r, png_y0);
				pcx->paint ();
			}
		}

		timeline[page] = playhead_widgets;

		HBox *tab = manage (new HBox ());
		l = manage (new Label (Glib::path_get_basename (path)));
		Gtk::Image *img =  manage (new Image (Stock::MEDIA_PLAY, ICON_SIZE_MENU));
		tab->pack_start (*img);
		tab->pack_start (*l);
		l->show();
		tab->show();
		img->hide();
		pages.pages ().push_back (Notebook_Helpers::TabElem (*vb, *tab));
		pages.signal_switch_page().connect (sigc::mem_fun (*this, &ExportReport::on_switch_page));

		if (png_surface) {
			assert (with_file && !path.empty ());
			std::string imgpath = Glib::build_filename (Glib::path_get_dirname (path), PBD::basename_nosuffix (path) + ".png");
			PBD::info << string_compose(_("Writing Export Analysis Image: %1."), imgpath) << endmsg;
			png_surface->write_to_png (imgpath);
		}
	}

	pages.set_show_tabs (true);
	pages.show_all ();
	pages.set_name ("ExportReportNotebook");
	pages.set_current_page (0);

	get_vbox ()->set_spacing (4);
	get_vbox ()->pack_start (pages, false, false);

	if (_session) {
		_session->AuditionActive.connect(auditioner_connections, invalidator (*this), boost::bind (&ExportReport::audition_active, this, _1), gui_context());
		_session->the_auditioner()->AuditionProgress.connect(auditioner_connections, invalidator (*this), boost::bind (&ExportReport::audition_progress, this, _1, _2), gui_context());
	}

	if (_session && with_file) {
		play_btn = add_button (Stock::MEDIA_PLAY, RESPONSE_ACCEPT);
		stop_btn = add_button (Stock::MEDIA_STOP, RESPONSE_ACCEPT);
	}
	add_button (Stock::CLOSE, RESPONSE_CLOSE);

	set_default_response (RESPONSE_CLOSE);
	if (_session && with_file) {
		stop_btn->signal_clicked().connect (sigc::mem_fun (*this, &ExportReport::stop_audition));
		play_btn->signal_clicked().connect (sigc::mem_fun (*this, &ExportReport::play_audition));
		stop_btn->set_sensitive (false);
	}
	show_all ();
}

int
ExportReport::run ()
{
	do {
		int i = ArdourDialog::run ();
		if (i == Gtk::RESPONSE_DELETE_EVENT || i == RESPONSE_CLOSE) {
			break;
		}
	} while (1);

	if (_session) {
		_session->cancel_audition();
	}
	return RESPONSE_CLOSE;
}

void
ExportReport::open_folder (std::string p)
{
	PBD::open_folder (Glib::path_get_dirname(p));
}

void
ExportReport::audition_active (bool active)
{
	if (!stop_btn || !play_btn) {
		return;
	}
	stop_btn->set_sensitive (active);
	play_btn->set_sensitive (!active);

	if (!active && _audition_num == _page_num && timeline.find (_audition_num) != timeline.end ()) {
		for (std::list<CimgPlayheadArea*>::const_iterator i = timeline[_audition_num].begin();
				i != timeline[_audition_num].end();
				++i) {
			(*i)->set_playhead (-1);
		}
	}

	if (_audition_num >= 0 ) {
		Widget *page = pages.get_nth_page (_audition_num);
		HBox *box = static_cast<Gtk::HBox*> (pages.get_tab_label (*page));
		if (!active) {
			(*(box->get_children().begin()))->hide ();
		} else {
			(*(box->get_children().begin()))->show ();
		}
	}

	if (!active) {
		_audition_num = -1;
	}
}

void
ExportReport::audition (std::string path, unsigned n_chn, int page)
{
	assert (_session);
	_session->cancel_audition();

	if (n_chn ==0) { return; }

	/* can't really happen, unless the user replaces the file while the dialog is open.. */
	if (!Glib::file_test (path, Glib::FILE_TEST_EXISTS)) {
		PBD::warning << string_compose(_("Could not read file: %1 (%2)."), path, strerror(errno)) << endmsg;
		return;
	}
	if (SMFSource::valid_midi_file (path)) { return; }

	boost::shared_ptr<Region> r;
	SourceList srclist;
	boost::shared_ptr<AudioFileSource> afs;
	bool old_sbp = AudioSource::get_build_peakfiles ();

	/* don't even think of building peakfiles for these files */
	AudioSource::set_build_peakfiles (false);

	for (unsigned int n = 0; n < n_chn; ++n) {
		try {
			afs = boost::dynamic_pointer_cast<AudioFileSource> (
				SourceFactory::createExternal (DataType::AUDIO, *_session,
										 path, n,
										 Source::Flag (ARDOUR::AudioFileSource::NoPeakFile), false));
			if (afs->sample_rate() != _session->nominal_sample_rate()) {
				boost::shared_ptr<SrcFileSource> sfs (new SrcFileSource(*_session, afs, ARDOUR::SrcGood));
				srclist.push_back(sfs);
			} else {
				srclist.push_back(afs);
			}
		} catch (failed_constructor& err) {
			PBD::error << _("Could not access soundfile: ") << path << endmsg;
			AudioSource::set_build_peakfiles (old_sbp);
			return;
		}
	}

	AudioSource::set_build_peakfiles (old_sbp);

	if (srclist.empty()) {
		return;
	}

	afs = boost::dynamic_pointer_cast<AudioFileSource> (srclist[0]);
	std::string rname = region_name_from_path (afs->path(), false);

	PBD::PropertyList plist;

	plist.add (ARDOUR::Properties::start, 0);
	plist.add (ARDOUR::Properties::length, srclist[0]->length(srclist[0]->natural_position()));
	plist.add (ARDOUR::Properties::name, rname);
	plist.add (ARDOUR::Properties::layer, 0);

	r = boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (srclist, plist, false));

	r->set_position(0);
	_session->audition_region(r);
	_audition_num = page;
}

void
ExportReport::play_audition ()
{
	if (_audition_num >= 0 || !_session) { return; }
	if (files.find (_page_num) == files.end()) { return; }
	audition (files[_page_num].path, files[_page_num].channels, _page_num);
}

void
ExportReport::stop_audition ()
{
	if (_audition_num == _page_num && timeline.find (_audition_num) != timeline.end ()) {
		for (std::list<CimgPlayheadArea*>::const_iterator i = timeline[_audition_num].begin();
				i != timeline[_audition_num].end();
				++i) {
			(*i)->set_playhead (-1);
		}
	}
	if (_session) {
		_session->cancel_audition();
	}
}

void
ExportReport::on_switch_page (GtkNotebookPage*, guint page_num)
{
	if (_audition_num == _page_num) {
		for (std::list<CimgPlayheadArea*>::const_iterator i = timeline[_audition_num].begin();
				i != timeline[_audition_num].end();
				++i) {
			(*i)->set_playhead (-1);
		}
	}
	_page_num = page_num;
}

void
ExportReport::audition_progress (samplecnt_t pos, samplecnt_t len)
{
	if (_audition_num == _page_num && timeline.find (_audition_num) != timeline.end ()) {
		const float p = (float)pos / len;
		for (std::list<CimgPlayheadArea*>::const_iterator i = timeline[_audition_num].begin();
				i != timeline[_audition_num].end();
				++i) {
			(*i)->set_playhead (p);
		}
	}
}

void
ExportReport::audition_seek (int page, float pos)
{
	if (_audition_num == page && _session) {
		_session->the_auditioner()->seek_to_percent (100.f * pos);
	}
}

void
ExportReport::on_logscale_toggled (Gtk::ToggleButton* b)
{
	bool en = b->get_active ();
	for (std::list<CimgWaveArea*>::iterator i = waves.begin (); i != waves.end (); ++i) {
		(*i)->set_logscale (en);
	}
}

void
ExportReport::on_rectivied_toggled (Gtk::ToggleButton* b)
{
	bool en = b->get_active ();
	for (std::list<CimgWaveArea*>::iterator i = waves.begin (); i != waves.end (); ++i) {
		(*i)->set_rectified (en);
	}
}

void
ExportReport::draw_waveform (Cairo::RefPtr<Cairo::ImageSurface>& wave, ExportAnalysisPtr p, uint32_t c, int m_l, size_t width, int anw, int height_2, bool log, bool rect)
{
	int w, h;
	Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (get_pango_context ());
	const float ht = 2.f * height_2;

	std::vector<double> dashes;
	dashes.push_back (3.0);
	dashes.push_back (5.0);

	wave = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, m_l + width, ht);
	Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create (wave);
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
			for (size_t x = 0 ; x < width; ++x) {
				const float v = alt_log_meter (fast_coefficient_to_dB (std::max (fabsf (p->peaks[c][x].max), fabsf (p->peaks[c][x].min))));
				cr->move_to (m_l + x - .5, ht - ht * v);
				cr->line_to (m_l + x - .5, ht);
			}
			cr->stroke ();
		} else {
			clip_top = ht * (1.f - dB_to_coefficient (-1));
			for (size_t x = 0 ; x < width; ++x) {
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
			for (size_t x = 0 ; x < width; ++x) {
				float pmax, pmin;
				if (p->peaks[c][x].max > 0) {
					pmax =  alt_log_meter (fast_coefficient_to_dB (p->peaks[c][x].max));
				} else {
					pmax = -alt_log_meter (fast_coefficient_to_dB (-p->peaks[c][x].max));
				}

				if (p->peaks[c][x].min > 0) {
					pmin =  alt_log_meter (fast_coefficient_to_dB (p->peaks[c][x].min));
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
			for (size_t x = 0 ; x < width; ++x) {
				cr->move_to (m_l + x - .5, height_2 - height_2 * p->peaks[c][x].max);
				cr->line_to (m_l + x - .5, height_2 - height_2 * p->peaks[c][x].min);
			}
			cr->stroke ();
		}
	}

	// >= 0dBFS
	cr->set_source_rgba (1.0, 0, 0, 1.0);
	for (size_t x = 0 ; x < width; ++x) {
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
}
