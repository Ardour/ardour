/*
    Copyright (C) 2006 Paul Davis

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

#ifndef __ardour_analysis_window_h__
#define __ardour_analysis_window_h__

#include <glibmm.h>
#include <glibmm/refptr.h>

#include <gtkmm/radiobutton.h>
#include <gtkmm/dialog.h>
#include <gtkmm/layout.h>
#include <gtkmm/treeview.h>
#include <gtkmm/notebook.h>
#include <gtkmm/label.h>
#include <gtkmm/liststore.h>
#include <gtkmm/separator.h>
#include <gtkmm/window.h>

#include <gtkmm2ext/dndtreeview.h>

#include <glibmm/threads.h>

#include "ardour/session_handle.h"

#include "fft_graph.h"
#include "fft_result.h"

namespace ARDOUR {
	class Session;
}


class AnalysisWindow : public Gtk::Window, public ARDOUR::SessionHandlePtr
{
public:
	AnalysisWindow  ();
	~AnalysisWindow ();

	void set_rangemode();
	void set_regionmode();

	void track_list_row_changed(const Gtk::TreeModel::Path& path, const Gtk::TreeModel::iterator& iter);

	void analyze ();

private:
	void clear_tracklist();

	void source_selection_changed (Gtk::RadioButton *);
	void display_model_changed    (Gtk::RadioButton *);
	void show_minmax_changed	();
	void show_normalized_changed	();

	void analyze_data				(Gtk::Button *);

	struct TrackListColumns : public Gtk::TreeModel::ColumnRecord {
		public:
			TrackListColumns () {
				add (trackname);
				add (visible);
				add (color);
				add (graph);
			}
			Gtk::TreeModelColumn<std::string> trackname;
			Gtk::TreeModelColumn<bool>        visible;
			Gtk::TreeModelColumn<Gdk::Color>  color;
			Gtk::TreeModelColumn<FFTResult *>  graph;
	};

	// Packing essentials
	Gtk::HBox hbox;
	Gtk::VBox vbox;

	// Left  side
	Glib::RefPtr<Gtk::ListStore> tlmodel;
	TrackListColumns tlcols;
	Gtk::TreeView track_list;

	Gtk::Label source_selection_label;


	Gtk::RadioButton source_selection_ranges_rb;
	Gtk::RadioButton source_selection_regions_rb;

	Gtk::HSeparator hseparator1;

	Gtk::Label display_model_label;
	Gtk::RadioButton display_model_composite_separate_rb;
	Gtk::RadioButton display_model_composite_all_tracks_rb;

	Gtk::Button refresh_button;


	Gtk::CheckButton show_minmax_button;
	Gtk::CheckButton show_normalized_button;


	// The graph
	FFTGraph fft_graph;

	bool track_list_ready;
	Glib::Threads::Mutex track_list_lock;

	friend class FFTGraph;
};

#endif // __ardour_analysis_window_h

