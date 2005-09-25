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

    $Id$

*/

#include <gtkmm/button.h>
#include <gtkmm/ctree.h>

#include <ardour/session_playlist.h>
#include <ardour/diskstream.h>
#include <ardour/playlist.h>
#include <ardour/audio_track.h>
#include <ardour/audioplaylist.h>
#include <ardour/configuration.h>

#include <gtkmm2ext/gtk_ui.h>

#include "playlist_selector.h"
#include "route_ui.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace std;
using namespace sigc;
using namespace Gtk;
using namespace ARDOUR;

static const gchar *tree_display_titles[] = {
	N_("Playlists grouped by track"), 
	0
};

PlaylistSelector::PlaylistSelector ()
	: ArdourDialog ("playlist selector"),
	  tree (internationalize (tree_display_titles)),
	  close_button (_("close"))
{
	rui = 0;
	
	set_position (Gtk::WIN_POS_MOUSE);
	set_name ("PlaylistSelectorWindow");
	set_title (_("ardour: playlists"));
	set_modal(true);
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);
	set_size_request (300, 200);

	scroller.add_with_viewport (tree);
	scroller.set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

	close_button.signal_clicked().connect (slot (*this, &PlaylistSelector::close_button_click));

	vpacker.set_border_width (6);
	vpacker.set_spacing (12);
	vpacker.pack_start (scroller);
	vpacker.pack_start (close_button, false, false);

	add (vpacker);
}

PlaylistSelector::~PlaylistSelector ()
{
	clear_map ();
}

void
PlaylistSelector::clear_map ()
{
	for (DSPL_Map::iterator x = dspl_map.begin(); x != dspl_map.end(); ++x) {
		delete x->second;
	}
	dspl_map.clear ();
}

void
PlaylistSelector::show_for (RouteUI* ruix)
{
	using namespace CTree_Helpers;
	vector<const char*> item;
	RowList::iterator i;
	RowList::iterator tmpi;
	RowList::iterator others;
	DiskStream* this_ds;
	string str;

	rui = ruix;

	str = _("ardour: playlist for ");
	str += rui->route().name();

	set_title (str);

	clear_map ();
	select_connection.disconnect ();

	/* ---------------------------------------- */
	/* XXX MAKE ME A FUNCTION (no CTree::clear() in gtkmm 1.2) */
	gtk_ctree_remove_node (tree.gobj(), NULL);
	/* ---------------------------------------- */
	
	session->foreach_playlist (this, &PlaylistSelector::add_playlist_to_map);

	this_ds = rui->get_diskstream();

	item.clear();
	item.push_back (_("Other tracks"));
	others = tree.rows().insert (tree.rows().begin(), Element (item));

	for (DSPL_Map::iterator x = dspl_map.begin(); x != dspl_map.end(); ++x) {

		DiskStream* ds = session->diskstream_by_id (x->first);

		if (ds == 0) {
			continue;
		}

		/* add a node for the diskstream */

		item.clear ();

		if (ds->name().empty()) {
			item.push_back (_("unassigned"));
		} else {
			item.push_back (ds->name().c_str());
		}

		if (ds == this_ds) {
			i = tree.rows().insert (tree.rows().begin(),
						Gtk::CTree_Helpers::Element (item));
		} else {
			i = others->subtree().insert (others->subtree().end(),
						      Gtk::CTree_Helpers::Element (item));
		}
		
		/* Now insert all the playlists for this diskstream/track in a subtree */
		
		list<Playlist*> *pls = x->second;

		for (list<Playlist*>::iterator p = pls->begin(); p != pls->end(); ++p) {

			item.clear ();
			item.push_back ((*p)->name().c_str());

			tmpi = i->subtree().insert (i->subtree().end(), Element (item));

			if (*p == this_ds->playlist()) {
				(*tmpi).select ();
			} 

			(*tmpi).set_data (*p);
			
		}

		if (ds == this_ds) {
			i->expand ();
		}
	}

	show_all ();
	select_connection = tree.tree_select_row.connect (slot (*this, &PlaylistSelector::row_selected));
}

void
PlaylistSelector::add_playlist_to_map (Playlist *pl)
{
	AudioPlaylist* apl;

	if (pl->frozen()) {
		return;
	}

	if ((apl = dynamic_cast<AudioPlaylist*> (pl)) == 0) {
		return;
	}

	DSPL_Map::iterator x;

	if ((x = dspl_map.find (apl->get_orig_diskstream_id())) == dspl_map.end()) {

		pair<ARDOUR::id_t,list<Playlist*>*> newp (apl->get_orig_diskstream_id(), new list<Playlist*>);
		
		x = dspl_map.insert (dspl_map.end(), newp);
	}

	x->second->push_back (pl);
}

void
PlaylistSelector::set_session (Session* s)
{
	ENSURE_GUI_THREAD(bind (slot (*this, &PlaylistSelector::set_session), s));

	session = s;

	if (session) {
		session->going_away.connect (bind (slot (*this, &PlaylistSelector::set_session), static_cast<Session*> (0)));
	}
}

void
PlaylistSelector::close_button_click ()
{
	rui = 0;
	hide ();
}

void
PlaylistSelector::row_selected (Gtk::CTree::Row row, gint col)
{
	Playlist *playlist;
	
	if ((playlist = (Playlist *) row.get_data()) != 0) {
		
		AudioTrack* at;
		AudioPlaylist* apl;
		
		if ((at = dynamic_cast<AudioTrack*> (&rui->route())) == 0) {
			/* eh? */
			return;
		}
		
		if ((apl = dynamic_cast<AudioPlaylist*> (playlist)) == 0) {
			/* eh? */
			return;
		}
		
		at->disk_stream().use_playlist (apl);

		hide ();
	}

}
	
