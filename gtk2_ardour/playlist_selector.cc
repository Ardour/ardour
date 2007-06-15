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

#include <gtkmm/button.h>

#include <ardour/session_playlist.h>
#include <ardour/audio_diskstream.h>
#include <ardour/playlist.h>
#include <ardour/audio_track.h>
#include <ardour/audioplaylist.h>
#include <ardour/configuration.h>

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/window_title.h>

#include "playlist_selector.h"
#include "route_ui.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace std;
using namespace sigc;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;
using namespace PBD;

PlaylistSelector::PlaylistSelector ()
	: ArdourDialog ("playlist selector")
{
	rui = 0;
	
	set_position (WIN_POS_MOUSE);
	set_name ("PlaylistSelectorWindow");
	set_modal(true);
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);
	set_size_request (300, 200);

	WindowTitle title(Glib::get_application_name());
	title += _("Playlists");
	set_title(title.get_string());

	model = TreeStore::create (columns);
	tree.set_model (model);
	tree.append_column (_("Playlists grouped by track"), columns.text);

	scroller.add (tree);
	scroller.set_policy (POLICY_AUTOMATIC, POLICY_AUTOMATIC);

	get_vbox()->set_border_width (6);
	get_vbox()->set_spacing (12);

	get_vbox()->pack_start (scroller);

	Button* b = add_button (_("close"), RESPONSE_CANCEL);
	b->signal_clicked().connect (mem_fun(*this, &PlaylistSelector::close_button_click));

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

bool
PlaylistSelector::on_unmap_event (GdkEventAny* ev)
{
	clear_map ();
	if (model) {
		model->clear ();
	}
	return Dialog::on_unmap_event (ev);
}

void
PlaylistSelector::show_for (RouteUI* ruix)
{
	vector<const char*> item;
	boost::shared_ptr<Diskstream> this_ds;
	string str;

	rui = ruix;

	WindowTitle title(Glib::get_application_name());
	title += string_compose (_("Playlist for %1"), rui->route()->name());
	set_title (title.get_string());

	clear_map ();
	select_connection.disconnect ();

	model->clear ();
	
	session->foreach_playlist (this, &PlaylistSelector::add_playlist_to_map);

	this_ds = rui->get_diskstream();

	TreeModel::Row others = *(model->append ());

	others[columns.text] = _("Other tracks");
	boost::shared_ptr<Playlist> proxy = others[columns.playlist];
	proxy.reset ();
	
	for (DSPL_Map::iterator x = dspl_map.begin(); x != dspl_map.end(); ++x) {

		boost::shared_ptr<Diskstream> ds = session->diskstream_by_id (x->first);

		if (ds == 0) {
			continue;
		}

		/* add a node for the diskstream */

		string nodename;

		if (ds->name().empty()) {
			nodename = _("unassigned");
		} else {
			nodename = ds->name().c_str();
		}
		
		TreeModel::Row row;
		TreeModel::Row* selected_row = 0;
		TreePath this_path;

		if (ds == this_ds) {
			row = *(model->prepend());
			row[columns.text] = nodename;
			boost::shared_ptr<Playlist> proxy = row[columns.playlist];
			proxy.reset ();
		} else {
			row = *(model->append (others.children()));
			row[columns.text] = nodename;
			boost::shared_ptr<Playlist> proxy = row[columns.playlist];
			proxy.reset ();
		}

		/* Now insert all the playlists for this diskstream/track in a subtree */
		
		list<boost::shared_ptr<Playlist> > *pls = x->second;
		
		for (list<boost::shared_ptr<Playlist> >::iterator p = pls->begin(); p != pls->end(); ++p) {

			TreeModel::Row child_row;

			child_row = *(model->append (row.children()));
			child_row[columns.text] = (*p)->name();
			child_row[columns.playlist] = *p;

			if (*p == this_ds->playlist()) {
				selected_row = &child_row;
			} 
		}
		
		if (selected_row != 0) {
			tree.get_selection()->select (*selected_row);
		}
	}

	show_all ();
	select_connection = tree.get_selection()->signal_changed().connect (mem_fun(*this, &PlaylistSelector::selection_changed));
}

void
PlaylistSelector::add_playlist_to_map (boost::shared_ptr<Playlist> pl)
{
	boost::shared_ptr<AudioPlaylist> apl;

	if (pl->frozen()) {
		return;
	}
	
	if ((apl = boost::dynamic_pointer_cast<AudioPlaylist> (pl)) == 0) {
		return;
	}

	DSPL_Map::iterator x;

	if ((x = dspl_map.find (apl->get_orig_diskstream_id())) == dspl_map.end()) {

		pair<PBD::ID,list<boost::shared_ptr<Playlist> >*> newp (apl->get_orig_diskstream_id(), new list<boost::shared_ptr<Playlist> >);
		
		x = dspl_map.insert (dspl_map.end(), newp);
	}

	x->second->push_back (pl);
}

void
PlaylistSelector::set_session (Session* s)
{
	ENSURE_GUI_THREAD(bind (mem_fun(*this, &PlaylistSelector::set_session), s));

	session = s;

	if (session) {
		session->GoingAway.connect (bind (mem_fun(*this, &PlaylistSelector::set_session), static_cast<Session*> (0)));
	}
}

void
PlaylistSelector::close_button_click ()
{
	rui = 0;
	hide ();
}

void
PlaylistSelector::selection_changed ()
{
	boost::shared_ptr<Playlist> playlist;

	TreeModel::iterator iter = tree.get_selection()->get_selected();

	if (!iter || rui == 0) {
		/* nothing selected */
		return;
	}

	if ((playlist = ((*iter)[columns.playlist])) != 0) {
		
		boost::shared_ptr<AudioTrack> at;
		boost::shared_ptr<AudioPlaylist> apl;
		
		if ((at = rui->audio_track()) == 0) {
			/* eh? */
			return;
		}
		
		if ((apl = boost::dynamic_pointer_cast<AudioPlaylist> (playlist)) == 0) {
			/* eh? */
			return;
		}
		
		at->diskstream()->use_playlist (apl);

		hide ();
	}

}
       
