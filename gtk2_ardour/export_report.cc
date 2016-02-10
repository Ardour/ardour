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

#include "export_report.h"

#include <gtkmm/label.h>
#include <gtkmm/stock.h>

#include "canvas/utils.h"
#include "canvas/colors.h"

#include "i18n.h"

using namespace Gtk;
using namespace ARDOUR;

ExportReport::ExportReport (StatusPtr s)
	: ArdourDialog (_("Export Report/Analysis"))
	, status (s)
{

	AnalysisResults & ar = status->result_map;
	//size_t n_results = ar.size();

	for (AnalysisResults::iterator i = ar.begin(); i != ar.end(); ++i) {
		Label *l;
		VBox *vb = manage (new VBox());
		vb->set_spacing (6);

		l = manage (new Label(string_compose (_("File: %1"), i->first)));
		vb->pack_start (*l);

		ExportAnalysisPtr p = i->second;
		if (i->second->have_loudness) {
			// TODO loudness histogram, HBox
			// TODO use cairo-widget and BIG numbers
			l = manage (new Label(string_compose (_("Loudness: %1 LUFS"), p->loudness)));
			vb->pack_start (*l);
			l = manage (new Label(string_compose (_("Loudness Range: %1 LU"), p->loudness_range)));
			vb->pack_start (*l);
		}

		{
			// TODO re-use Canvas::WaveView::draw_image() somehow.
			const size_t peaks = sizeof(p->_peaks) / sizeof (ARDOUR::PeakData::PeakDatum) / 2;
			const float height_2 = 150.0;
			Cairo::RefPtr<Cairo::ImageSurface> wave = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, peaks, 2 * height_2);
			Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create (wave);
			cr->rectangle (0, 0, peaks, 2 * height_2);
			cr->set_source_rgba (0, 0, 0, 1.0);
			cr->fill ();
			cr->set_source_rgba (.7, .7, .7, 1.0);
			cr->set_line_width (1.0);
			for (size_t x = 0 ; x < peaks; ++x) {
				cr->move_to (x - .5, height_2 - height_2 * p->_peaks[x].max);
				cr->line_to (x - .5, height_2 - height_2 * p->_peaks[x].min);
			}
			cr->stroke ();
			wave->flush ();

			CimgArea *wv = manage (new CimgArea (wave));
			vb->pack_start (*wv);
		}

		{
			// TODO: get geometry from ExportAnalysis
			const size_t width = 800;
			const size_t height = 256;
			Cairo::RefPtr<Cairo::ImageSurface> spec = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, width, height);
			Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create (spec);
			cr->rectangle (0, 0, width, height);
			cr->set_source_rgba (0, 0, 0, 1.0);
			cr->fill ();
			for (size_t x = 0 ; x < width; ++x) {
				for (size_t y = 0 ; y < height; ++y) {
					const float pk = p->_spectrum[x][y];
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
		pages.pages().push_back (Notebook_Helpers::TabElem (*vb, Glib::path_get_basename (i->first)));
	}

	pages.set_show_tabs (true);
	pages.show_all ();
	pages.set_name ("ExportReportNotebook");
	pages.set_current_page (0);

	get_vbox()->set_spacing (12);
	get_vbox()->pack_start (pages);

	add_button (Stock::CLOSE, RESPONSE_ACCEPT);
	set_default_response (RESPONSE_ACCEPT);
	show_all ();
	//pages.signal_switch_page().connect (sigc::mem_fun (*this, &ExportReport::handle_page_change));
}

int
ExportReport::run ()
{
	return ArdourDialog::run ();
}
