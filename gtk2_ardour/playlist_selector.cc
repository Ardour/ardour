/*
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
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

#include <gtkmm/button.h>

#include "ardour/audio_track.h"
#include "ardour/audioplaylist.h"
#include "ardour/midi_playlist.h"

#include "ardour/session_playlist.h"

#include <gtkmm2ext/gtk_ui.h>

#include "playlist_selector.h"
#include "route_ui.h"
#include "gui_thread.h"

#include "pbd/i18n.h"

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
	set_modal(false);
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

	Button* close_btn = add_button (_("Close"), RESPONSE_CANCEL);
	Button* ok_btn = add_button (_("OK"), RESPONSE_OK);
	close_btn->signal_clicked().connect (sigc::mem_fun(*this, &PlaylistSelector::close_button_click));
	ok_btn->signal_clicked().connect (sigc::mem_fun(*this, &PlaylistSelector::ok_button_click));
}

void PlaylistSelector::set_rui(RouteUI* ruix)
{
	if (rui == ruix) {
		return;
	}

	rui = ruix;

	boost::shared_ptr<Track> this_track = rui->track();

	if (this_track) {
		this_track->PlaylistAdded.connect(
			signal_connections,
			invalidator(*this),
			boost::bind(&PlaylistSelector::playlist_added, this),
			gui_context()
		);

		this_track->DropReferences.connect(
			signal_connections,
			invalidator(*this),
			boost::bind(&PlaylistSelector::ok_button_click, this),
			gui_context()
		);
	}
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
PlaylistSelector::redisplay()
{
	vector<const char*> item;
	string str;

	set_title (string_compose (_("Playlist for %1"), rui->route()->name()));

	clear_map ();
	select_connection.disconnect ();

	model->clear ();

	_session->playlists()->foreach (this, &PlaylistSelector::add_playlist_to_map);

	boost::shared_ptr<Track> this_track = rui->track();

	TreeModel::Row others = *(model->append ());

	others[columns.text] = _("Other tracks");
	boost::shared_ptr<Playlist> proxy = others[columns.playlist];
	proxy.reset ();

	if (this_track->playlist()) {
		current_playlist = this_track->playlist();
	}

	for (TrackPlaylistMap::iterator x = trpl_map.begin(); x != trpl_map.end(); ++x) {

		boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (_session->route_by_id (x->first));

		/* add a node for the track */

		string nodename;

		if (!tr || tr->name().empty()) {
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
	_session->playlists()->unassigned (unassigned);

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
	if (pl->frozen()) {
		return;
	}

	if (!rui) {
		return;
	}

	if (rui->is_midi_track ()) {
		if (boost::dynamic_pointer_cast<MidiPlaylist> (pl) == 0) {
			return;
		}
	} else {
		assert (rui->is_audio_track ());
		if (boost::dynamic_pointer_cast<AudioPlaylist> (pl) == 0) {
			return;
		}
	}

	TrackPlaylistMap::iterator x;

	if ((x = trpl_map.find (pl->get_orig_track_id ())) == trpl_map.end()) {
		x = trpl_map.insert (trpl_map.end(), make_pair (pl->get_orig_track_id(), new list<boost::shared_ptr<Playlist> >));
	}

	x->second->push_back (pl);
}

void
PlaylistSelector::playlist_added()
{
	redisplay();
}

void
PlaylistSelector::close_button_click ()
{
	if (rui && current_playlist) {
		rui->track ()->use_playlist (rui->is_audio_track () ? DataType::AUDIO : DataType::MIDI, current_playlist);
	}
	rui = 0;
	hide ();
}

void
PlaylistSelector::ok_button_click()
{
	rui = 0;
	hide();
}

bool PlaylistSelector::on_delete_event (GdkEventAny*)
{
	close_button_click();
	return false;
}

void
PlaylistSelector::selection_changed ()
{
	boost::shared_ptr<Playlist> pl;

	TreeModel::iterator iter = tree.get_selection()->get_selected();

	if (!iter || rui == 0) {
		/* nothing selected */
		return;
	}

	if ((pl = ((*iter)[columns.playlist])) != 0) {

		if (rui->is_audio_track () && boost::dynamic_pointer_cast<AudioPlaylist> (pl) == 0) {
			return;
		}
		if (rui->is_midi_track () && boost::dynamic_pointer_cast<MidiPlaylist> (pl) == 0) {
			return;
		}

		rui->track ()->use_playlist (rui->is_audio_track () ? DataType::AUDIO : DataType::MIDI, pl);
	}
}
