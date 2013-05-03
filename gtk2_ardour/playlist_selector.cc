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

#include "ardour/audio_track.h"
#include "ardour/audioplaylist.h"
#include "ardour/playlist.h"
#include "ardour/session_playlist.h"

#include <gtkmm2ext/gtk_ui.h>

#include "playlist_selector.h"
#include "route_ui.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;
using namespace PBD;

PlaylistSelector::PlaylistSelector ()
	: ArdourDialog (_("Playlists"))
{
	rui = 0;

	set_name ("PlaylistSelectorWindow");
	set_modal(true);
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);
	set_size_request (300, 200);

	model = TreeStore::create (columns);
	tree.set_model (model);
	tree.append_column (_("Playlists grouped by track"), columns.text);

	scroller.add (tree);
	scroller.set_policy (POLICY_AUTOMATIC, POLICY_AUTOMATIC);

	get_vbox()->set_border_width (6);
	get_vbox()->set_spacing (12);

	get_vbox()->pack_start (scroller);

	Button* b = add_button (_("Close"), RESPONSE_CANCEL);
	b->signal_clicked().connect (sigc::mem_fun(*this, &PlaylistSelector::close_button_click));

}

PlaylistSelector::~PlaylistSelector ()
{
	clear_map ();
}

void
PlaylistSelector::clear_map ()
{
	for (TrackPlaylistMap::iterator x = trpl_map.begin(); x != trpl_map.end(); ++x) {
		delete x->second;
	}
	trpl_map.clear ();
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
	string str;

	rui = ruix;

	set_title (string_compose (_("Playlist for %1"), rui->route()->name()));

	clear_map ();
	select_connection.disconnect ();

	model->clear ();

	_session->playlists->foreach (this, &PlaylistSelector::add_playlist_to_map);

	boost::shared_ptr<Track> this_track = rui->track();

	TreeModel::Row others = *(model->append ());

	others[columns.text] = _("Other tracks");
	boost::shared_ptr<Playlist> proxy = others[columns.playlist];
	proxy.reset ();

	for (TrackPlaylistMap::iterator x = trpl_map.begin(); x != trpl_map.end(); ++x) {

		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (_session->route_by_id (x->first));
		
		/* legacy sessions stored the diskstream ID as the original
		 * playlist owner. so try there instead.
		 */

		if (tr == 0) {
			tr = _session->track_by_diskstream_id (x->first);
		}

		if (tr == 0) {
			continue;
		}

		/* add a node for the track */

		string nodename;

		if (tr->name().empty()) {
			nodename = _("unassigned");
		} else {
			nodename = tr->name().c_str();
		}

		TreeModel::Row row;
		TreeModel::Row selected_row;
		bool have_selected = false;
		TreePath this_path;

		if (tr == this_track) {
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

		list<boost::shared_ptr<Playlist> >* pls = x->second;

		for (list<boost::shared_ptr<Playlist> >::iterator p = pls->begin(); p != pls->end(); ++p) {

			TreeModel::Row child_row;

			child_row = *(model->append (row.children()));
			child_row[columns.text] = (*p)->name();
			child_row[columns.playlist] = *p;

			if (*p == this_track->playlist()) {
				selected_row = child_row;
				have_selected = true;
			}
		}

		if (have_selected) {
			tree.get_selection()->select (selected_row);
		}
	}

	// Add unassigned (imported) playlists to the list
	list<boost::shared_ptr<Playlist> > unassigned;
	_session->playlists->unassigned (unassigned);

	TreeModel::Row row;
	TreeModel::Row selected_row;
	bool have_selected = false;
	TreePath this_path;

	row = *(model->append (others.children()));
	row[columns.text] = _("Imported");
	proxy = row[columns.playlist];
	proxy.reset ();

	for (list<boost::shared_ptr<Playlist> >::iterator p = unassigned.begin(); p != unassigned.end(); ++p) {
		TreeModel::Row child_row;

		child_row = *(model->append (row.children()));
		child_row[columns.text] = (*p)->name();
		child_row[columns.playlist] = *p;

		if (*p == this_track->playlist()) {
			selected_row = child_row;
			have_selected = true;
		}

		if (have_selected) {
			tree.get_selection()->select (selected_row);
		}
	}

	show_all ();
	select_connection = tree.get_selection()->signal_changed().connect (sigc::mem_fun(*this, &PlaylistSelector::selection_changed));
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

	TrackPlaylistMap::iterator x;

	if ((x = trpl_map.find (apl->get_orig_track_id())) == trpl_map.end()) {
		x = trpl_map.insert (trpl_map.end(), make_pair (apl->get_orig_track_id(), new list<boost::shared_ptr<Playlist> >));
	}

	x->second->push_back (pl);
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

		at->use_playlist (apl);

		hide ();
	}

}

