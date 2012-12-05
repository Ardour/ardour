/*
    Copyright (C) 2011-2012 Paul Davis

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

#ifndef __gtk2_ardour_region_layering_order_editor_h__
#define __gtk2_ardour_region_layering_order_editor_h__

#include <gtkmm/dialog.h>
#include <gtkmm/liststore.h>
#include <gtkmm/treeview.h>
#include <gtkmm/scrolledwindow.h>

#include "ardour/region.h"
#include "ardour/playlist.h"

#include "ardour_window.h"
#include "audio_clock.h"

class PublicEditor;

namespace ARDOUR {
	class Session;
}

class RegionLayeringOrderEditor : public ArdourWindow
{
  public:
	RegionLayeringOrderEditor (PublicEditor&);
	virtual ~RegionLayeringOrderEditor ();

	void set_context (const std::string &, ARDOUR::Session *, TimeAxisView *, boost::shared_ptr<ARDOUR::Playlist>, ARDOUR::framepos_t);
	void maybe_present ();

  protected:
	virtual bool on_key_press_event (GdkEventKey* event);

  private:
	framepos_t position;
	bool in_row_change;
	uint32_t regions_at_position;

        PBD::ScopedConnection playlist_modified_connection;

	struct LayeringOrderColumns : public Gtk::TreeModel::ColumnRecord {
		LayeringOrderColumns () {
			add (name);
			add (region_view);
		}
		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<RegionView *> region_view;
	};
	LayeringOrderColumns layering_order_columns;
	Glib::RefPtr<Gtk::ListStore> layering_order_model;
	Gtk::TreeView layering_order_display;
	AudioClock clock;
	Gtk::Label track_label;
	Gtk::Label track_name_label;
	Gtk::Label clock_label;
	Gtk::ScrolledWindow scroller;   // Available layers
	PublicEditor& editor;
	TimeAxisView* _time_axis_view;

        void row_selected ();
	void refill ();
	void playlist_modified ();
};

#endif /* __gtk2_ardour_region_layering_order_editor_h__ */
