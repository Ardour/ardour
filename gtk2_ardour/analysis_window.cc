/*
    Copyright (C) 2006 Paul Davis
    Written by Sampo Savolainen

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm/stock.h>
#include <gtkmm/label.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treeiter.h>

#include "ardour/audioregion.h"
#include "ardour/audioplaylist.h"
#include "ardour/types.h"

#include "analysis_window.h"

#include "route_ui.h"
#include "time_axis_view.h"
#include "public_editor.h"
#include "selection.h"
#include "audio_region_view.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

AnalysisWindow::AnalysisWindow() :

	  source_selection_label       (_("Signal source")),
	  source_selection_ranges_rb   (_("Selected ranges")),
	  source_selection_regions_rb  (_("Selected regions")),

	  display_model_label                   (_("Display model")),
	  display_model_composite_separate_rb   (_("Composite graphs for each track")),
	  display_model_composite_all_tracks_rb (_("Composite graph of all tracks")),

	  show_minmax_button	 (_("Show frequency power range")),
	  show_normalized_button (_("Normalize values")),

	  fft_graph (16384)
{
	set_name(_("FFT analysis window"));
	set_title (_("Spectral Analysis"));

	track_list_ready = false;

	// Left side: track list + controls
	tlmodel = Gtk::ListStore::create(tlcols);
	track_list.set_model (tlmodel);
	track_list.append_column(_("Track"), tlcols.trackname);
	track_list.append_column_editable(_("Show"), tlcols.visible);
	track_list.set_headers_visible(true);
	track_list.set_reorderable(false);
	track_list.get_selection()->set_mode (Gtk::SELECTION_NONE);


	Gtk::TreeViewColumn* track_col = track_list.get_column(0);
	Gtk::CellRendererText* renderer = dynamic_cast<Gtk::CellRendererText*>(track_list.get_column_cell_renderer (0));

	track_col->add_attribute(renderer->property_foreground_gdk(), tlcols.color);
	track_col->set_expand(true);


	tlmodel->signal_row_changed().connect (
			sigc::mem_fun(*this, &AnalysisWindow::track_list_row_changed) );

	fft_graph.set_analysis_window(this);

	vbox.pack_start(track_list);


	// "Signal source"
	vbox.pack_start(source_selection_label, false, false);

	{
		Gtk::RadioButtonGroup group = source_selection_ranges_rb.get_group();
		source_selection_regions_rb.set_group(group);

		source_selection_ranges_rb.set_active();

		vbox.pack_start (source_selection_ranges_rb,  false, false);
		vbox.pack_start (source_selection_regions_rb, false, false);

		// "Selected ranges" radio
		source_selection_ranges_rb.signal_toggled().connect (
				sigc::bind ( sigc::mem_fun(*this, &AnalysisWindow::source_selection_changed), &source_selection_ranges_rb));

		// "Selected regions" radio
		source_selection_regions_rb.signal_toggled().connect (
				sigc::bind ( sigc::mem_fun(*this, &AnalysisWindow::source_selection_changed), &source_selection_regions_rb));
	}

	vbox.pack_start(hseparator1, false, false);

	// "Display model"
	vbox.pack_start(display_model_label, false, false);
	{
		Gtk::RadioButtonGroup group = display_model_composite_separate_rb.get_group();
		display_model_composite_all_tracks_rb.set_group (group);

		display_model_composite_separate_rb.set_active();

		vbox.pack_start (display_model_composite_separate_rb,   false, false);
		vbox.pack_start (display_model_composite_all_tracks_rb, false, false);

		// "Composite graphs for all tracks"
		display_model_composite_separate_rb.signal_toggled().connect (
				sigc::bind ( sigc::mem_fun(*this, &AnalysisWindow::display_model_changed), &display_model_composite_separate_rb));

		// "Composite graph of all tracks"
		display_model_composite_all_tracks_rb.signal_toggled().connect (
				sigc::bind ( sigc::mem_fun(*this, &AnalysisWindow::display_model_changed), &display_model_composite_all_tracks_rb));
	}

	// Analyze button

	refresh_button.set_name("EditorGTKButton");
	refresh_button.set_label(_("Re-analyze data"));

	refresh_button.signal_clicked().connect ( sigc::bind ( sigc::mem_fun(*this, &AnalysisWindow::analyze_data), &refresh_button));

	vbox.pack_start(refresh_button, false, false, 10);


	// Feature checkboxes

	// minmax
	show_minmax_button.signal_toggled().connect( sigc::mem_fun(*this, &AnalysisWindow::show_minmax_changed));
	vbox.pack_start(show_minmax_button, false, false);

	// normalize
	show_normalized_button.signal_toggled().connect( sigc::mem_fun(*this, &AnalysisWindow::show_normalized_changed));
	vbox.pack_start(show_normalized_button, false, false);





	hbox.pack_start(vbox, Gtk::PACK_SHRINK);

	// Analysis window on the right
	fft_graph.ensure_style();

	hbox.add(fft_graph);



	// And last we pack the hbox
	add(hbox);
	show_all();
	track_list.show_all();
}

AnalysisWindow::~AnalysisWindow()
{

}

void
AnalysisWindow::show_minmax_changed()
{
	fft_graph.set_show_minmax(show_minmax_button.get_active());
}

void
AnalysisWindow::show_normalized_changed()
{
	fft_graph.set_show_normalized(show_normalized_button.get_active());
}

void
AnalysisWindow::set_rangemode()
{
	source_selection_ranges_rb.set_active(true);
}

void
AnalysisWindow::set_regionmode()
{
	source_selection_regions_rb.set_active(true);
}

void
AnalysisWindow::track_list_row_changed(const Gtk::TreeModel::Path& /*path*/, const Gtk::TreeModel::iterator& /*iter*/)
{
	if (track_list_ready) {
		fft_graph.redraw();
	}
}


void
AnalysisWindow::clear_tracklist()
{
	// Empty track list & free old graphs
	Gtk::TreeNodeChildren children = track_list.get_model()->children();

	for (Gtk::TreeIter i = children.begin(); i != children.end(); i++) {
		Gtk::TreeModel::Row row = *i;

		FFTResult *delete_me = row[tlcols.graph];
		if (delete_me == 0)
			continue;

		// Make sure it's not drawn
		row[tlcols.graph] = 0;

		delete delete_me;
	}

	tlmodel->clear();
}

void
AnalysisWindow::analyze()
{
	analyze_data(&refresh_button);
}

void
AnalysisWindow::analyze_data (Gtk::Button * /*button*/)
{
	track_list_ready = false;
	{
		Glib::Threads::Mutex::Lock lm  (track_list_lock);

		// Empty track list & free old graphs
		clear_tracklist();

		// first we gather the FFTResults of all tracks

		Sample *buf    = (Sample *) malloc(sizeof(Sample) * fft_graph.windowSize());
		Sample *mixbuf = (Sample *) malloc(sizeof(Sample) * fft_graph.windowSize());
		float  *gain   = (float *)  malloc(sizeof(float) * fft_graph.windowSize());

		Selection& s (PublicEditor::instance().get_selection());


		// if timeSelection
		if (source_selection_ranges_rb.get_active()) {
			TimeSelection ts = s.time;

			for (TrackSelection::iterator i = s.tracks.begin(); i != s.tracks.end(); ++i) {
				boost::shared_ptr<AudioPlaylist> pl
					= boost::dynamic_pointer_cast<AudioPlaylist>((*i)->playlist());

				if (!pl)
					continue;

				RouteUI *rui = dynamic_cast<RouteUI *>(*i);
				int n_inputs = rui->route()->n_inputs().n_audio(); // FFT is audio only

				// Busses don't have playlists, so we need to check that we actually are working with a playlist
				if (!pl || !rui)
					continue;

				// std::cerr << "Analyzing ranges on track " << rui->route()->name() << std::endl;

				FFTResult *res = fft_graph.prepareResult(rui->color(), rui->route()->name());
				for (std::list<AudioRange>::iterator j = ts.begin(); j != ts.end(); ++j) {

					int n;
					for (int channel = 0; channel < n_inputs; channel++) {
						framecnt_t x = 0;

						while (x < j->length()) {
							// TODO: What about stereo+ channels? composite all to one, I guess

							n = fft_graph.windowSize();

							if (x + n >= (*j).length() ) {
								n = (*j).length() - x;
							}

							n = pl->read(buf, mixbuf, gain, (*j).start + x, n, channel);

							if ( n < fft_graph.windowSize()) {
								for (int j = n; j < fft_graph.windowSize(); j++) {
									buf[j] = 0.0;
								}
							}

							res->analyzeWindow(buf);

							x += n;
						}
					}
				}
				res->finalize();

				Gtk::TreeModel::Row newrow = *(tlmodel)->append();
				newrow[tlcols.trackname]   = rui->route()->name();
				newrow[tlcols.visible]     = true;
				newrow[tlcols.color]       = rui->color();
				newrow[tlcols.graph]       = res;
			} 
		} else if (source_selection_regions_rb.get_active()) {
			RegionSelection ars = s.regions;
			// std::cerr << "Analyzing selected regions" << std::endl;

			for (RegionSelection::iterator j = ars.begin(); j != ars.end(); ++j) {
				// Check that the region is actually audio (so we can analyze it)
				AudioRegionView* arv = dynamic_cast<AudioRegionView*>(*j);
				if (!arv)
					continue;

				// std::cerr << " - " << (*j)->region().name() << ": " << (*j)->region().length() << " samples starting at " << (*j)->region().position() << std::endl;
				RouteTimeAxisView *rtav = dynamic_cast<RouteTimeAxisView *>(&arv->get_time_axis_view());
				if (!rtav) {
					/* shouldn't happen... */
					continue;
				}
				FFTResult *res = fft_graph.prepareResult(rtav->color(), arv->get_item_name());
				int n;
				for (unsigned int channel = 0; channel < arv->region()->n_channels(); channel++) {

					framecnt_t x = 0;
					framecnt_t length = arv->region()->length();

					while (x < length) {
						// TODO: What about stereo+ channels? composite all to one, I guess

						n = fft_graph.windowSize();
						if (x + n >= length ) {
							n = length - x;
						}

						memset (buf, 0, n * sizeof (Sample));
						n = arv->audio_region()->read_at(buf, mixbuf, gain, arv->region()->position() + x, n, channel);

						if (n == 0)
							break;

						if ( n < fft_graph.windowSize()) {
							for (int j = n; j < fft_graph.windowSize(); j++) {
								buf[j] = 0.0;
							}
						}

						res->analyzeWindow(buf);
						x += n;
					}
				}
				// std::cerr << "Found: " << (*j)->get_item_name() << std::endl;
				res->finalize();

				Gtk::TreeModel::Row newrow = *(tlmodel)->append();
				newrow[tlcols.trackname]   = arv->get_item_name();
				newrow[tlcols.visible]     = true;
				newrow[tlcols.color]       = rtav->color();
				newrow[tlcols.graph]       = res;

			}

		}


		free(buf);
		free(mixbuf);

		track_list_ready = true;
	} /* end lock */

	fft_graph.redraw();
}

void
AnalysisWindow::source_selection_changed (Gtk::RadioButton *button)
{
	// We are only interested in activation signals, not deactivation signals
	if (!button->get_active())
		return;

	/*
	cerr << "AnalysisWindow: signal source = ";

	if (button == &source_selection_ranges_rb) {
		cerr << "selected ranges" << endl;

	} else if (button == &source_selection_regions_rb) {
		cerr << "selected regions" << endl;

	} else {
		cerr << "unknown?" << endl;
	}
	*/
}

void
AnalysisWindow::display_model_changed (Gtk::RadioButton *button)
{
	// We are only interested in activation signals, not deactivation signals
	if (!button->get_active())
		return;

	/*
	cerr << "AnalysisWindow: display model = ";

	if (button == &display_model_composite_separate_rb) {
		cerr << "separate composites of tracks" << endl;
	} else if (button == &display_model_composite_all_tracks_rb) {
		cerr << "composite of all tracks" << endl;
	} else {
		cerr << "unknown?" << endl;
	}
	*/
}


