/*
    Copyright (C) 2004 Paul Davis

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

#ifndef __ardour_playlist_selector_h__
#define __ardour_playlist_selector_h__

#include <boost/shared_ptr.hpp>

#include <gtkmm/box.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/button.h>
#include <gtkmm/treeview.h>
#include <gtkmm/treestore.h>

#include "ardour_dialog.h"
#include "ardour/session_handle.h"

namespace ARDOUR {
	class Session;
	class PluginManager;
	class Plugin;
}

class RouteUI;

class PlaylistSelector : public ArdourDialog
{
public:
	PlaylistSelector ();
	~PlaylistSelector ();

	void show_for (RouteUI*);

protected:
	bool on_unmap_event (GdkEventAny*);

private:
	typedef std::map<PBD::ID,std::list<boost::shared_ptr<ARDOUR::Playlist> >*> TrackPlaylistMap;

	Gtk::ScrolledWindow scroller;
	TrackPlaylistMap trpl_map;
	RouteUI* rui;

	sigc::connection select_connection;

	void add_playlist_to_map (boost::shared_ptr<ARDOUR::Playlist>);
	void clear_map ();
	void close_button_click ();
	void selection_changed ();

	struct ModelColumns : public Gtk::TreeModel::ColumnRecord
	{
		ModelColumns () {
			add (text);
			add (playlist);
		}
		Gtk::TreeModelColumn<std::string> text;
		Gtk::TreeModelColumn<boost::shared_ptr<ARDOUR::Playlist> >   playlist;
	};

	ModelColumns columns;
	Glib::RefPtr<Gtk::TreeStore> model;
	Gtk::TreeView tree;
};

#endif // __ardour_playlist_selector_h__
