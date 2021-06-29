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
#include "ardour/playlist_factory.h"

#include "ardour/session_playlist.h"

#include <gtkmm/stock.h>

#include <gtkmm2ext/gtk_ui.h>

#include "ardour_ui.h"
#include "public_editor.h"
#include "playlist_selector.h"
#include "route_ui.h"
#include "gui_thread.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;
using namespace PBD;

PlaylistSelector::PlaylistSelector ()
	: ArdourDialog (_("Playlists"))
{
	_rui = 0;
	_mode = plSelect;

	set_name ("PlaylistSelectorWindow");
	set_modal(false);
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);
	set_size_request (400, 250);

	model = TreeStore::create (columns);
	tree.set_model (model);
	tree.append_column (_("Playlists grouped by track"), columns.text);
	tree.append_column (_("Playlist Group"), columns.pgrp);
	tree.set_headers_visible (false);

	scroller.add (tree);
	scroller.set_policy (POLICY_AUTOMATIC, POLICY_AUTOMATIC);

	get_vbox()->set_border_width (6);
	get_vbox()->set_spacing (12);

	get_vbox()->pack_start (scroller);

	get_vbox()->show_all();

	Button* ok_btn = add_button (Gtk::Stock::OK, RESPONSE_OK);
	ok_btn->signal_clicked().connect (sigc::mem_fun(*this, &PlaylistSelector::ok_button_click));

	select_connection = tree.get_selection()->signal_changed().connect (sigc::mem_fun(*this, &PlaylistSelector::selection_changed));

	_ignore_selection = false;
}

void PlaylistSelector::prepare(RouteUI* ruix, plMode mode)
{
	_mode = mode;
	
	if (_rui == ruix) {
		return;
	}

	_rui = ruix;

	boost::shared_ptr<Track> this_track = _rui->track();

	if (this_track) {
		this_track->PlaylistChanged.connect(
			signal_connections,
			invalidator(*this),
			boost::bind(&PlaylistSelector::redisplay, this),
			gui_context()
		);

		this_track->PlaylistAdded.connect(
			signal_connections,
			invalidator(*this),
			boost::bind(&PlaylistSelector::redisplay, this),
			gui_context()
		);

		this_track->DropReferences.connect(
			signal_connections,
			invalidator(*this),
			boost::bind(&PlaylistSelector::ok_button_click, this),
			gui_context()
		);
	}

	redisplay();
}

PlaylistSelector::~PlaylistSelector ()
{
	clear_map();

	if (model) {
		model->clear ();
	}
	if (current_playlist) {
		current_playlist.reset();
	}

	signal_connections.drop_connections();
	select_connection.disconnect ();
}

void
PlaylistSelector::clear_map ()
{
	for (TrackPlaylistMap::iterator x = trpl_map.begin(); x != trpl_map.end(); ++x) {
		delete x->second;
	}
	trpl_map.clear ();
}

void
PlaylistSelector::redisplay()
{
	if (!_rui ) {
		return;
	}

	vector<const char*> item;
	string str;

	set_title (string_compose (_("Playlist for %1"), _rui->route()->name()));

	clear_map ();
	if (model) {
		model->clear ();
	}

	_session->playlists()->foreach (this, &PlaylistSelector::add_playlist_to_map);

	boost::shared_ptr<Track> this_track = _rui->track();

	boost::shared_ptr<Playlist> proxy;

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

		//make a heading for each other track, if needed
		if (tr != this_track && _mode != plSelect) {
			row = *(model->prepend());
			row[columns.text] = nodename;
			boost::shared_ptr<Playlist> proxy = row[columns.playlist];
			proxy.reset ();
		}

		/* Now insert all the playlists for this diskstream/track in a subtree */
		vector<boost::shared_ptr<Playlist> > pls = *(x->second);

		/* sort the playlists to match the order they appear in the track menu */
		PlaylistSorterByID cmp;
		sort (pls.begin(), pls.end(), cmp);

		for (vector<boost::shared_ptr<Playlist> >::iterator p = pls.begin(); p != pls.end(); ++p) {

			(*p)->PropertyChanged.connect (signal_connections, invalidator (*this), boost::bind (&PlaylistSelector::pl_property_changed, this, _1), gui_context());

			TreeModel::Row child_row;

			if (tr == this_track && _mode==plSelect) {
				child_row = *(model->append());
			} else if (tr != this_track && _mode != plSelect) {
				child_row = *(model->append (row.children()));
			}
			
			if (child_row) {
				child_row[columns.text] = (*p)->name();
				child_row[columns.pgrp] = (*p)->pgroup_id();
				child_row[columns.playlist] = *p;

				if (*p == this_track->playlist()) {
					selected_row = child_row;
					have_selected = true;
				}
			}
		}
		if (have_selected) {
			_ignore_selection = true;
			tree.get_selection()->select (selected_row);
			_ignore_selection = false;
		}
	}

	if (_mode != plSelect) {
		// Add unassigned (imported) playlists to the list
		list<boost::shared_ptr<Playlist> > unassigned;
		_session->playlists()->unassigned (unassigned);

		if ( unassigned.begin() != unassigned.end() ) {
		
			TreeModel::Row row;
			TreeModel::Row selected_row;
			bool have_selected = false;
			TreePath this_path;

			row = *(model->append ());
			row[columns.text] = _("Imported");
			proxy = row[columns.playlist];
			proxy.reset ();

			for (list<boost::shared_ptr<Playlist> >::iterator p = unassigned.begin(); p != unassigned.end(); ++p) {
				TreeModel::Row child_row;

				child_row = *(model->append (row.children()));
				child_row[columns.text] = (*p)->name();
				child_row[columns.pgrp] = (*p)->pgroup_id();
				child_row[columns.playlist] = *p;

				if (*p == this_track->playlist()) {
					selected_row = child_row;
					have_selected = true;
				}

				if (have_selected) {
					_ignore_selection = true;
					tree.get_selection()->select (selected_row);
					_ignore_selection = false;
				}
			}
		}
	} //if !plSelect
}

void
PlaylistSelector::pl_property_changed (PBD::PropertyChange const & what_changed)
{
	redisplay();
}

void
PlaylistSelector::add_playlist_to_map (boost::shared_ptr<Playlist> pl)
{
	if (pl->frozen()) {
		return;
	}

	if (!_rui) {
		return;
	}

	if (_rui->is_midi_track ()) {
		if (boost::dynamic_pointer_cast<MidiPlaylist> (pl) == 0) {
			return;
		}
	} else {
		assert (_rui->is_audio_track ());
		if (boost::dynamic_pointer_cast<AudioPlaylist> (pl) == 0) {
			return;
		}
	}

	TrackPlaylistMap::iterator x;

	if ((x = trpl_map.find (pl->get_orig_track_id ())) == trpl_map.end()) {
		x = trpl_map.insert (trpl_map.end(), make_pair (pl->get_orig_track_id(), new vector<boost::shared_ptr<Playlist> >));
	}

	x->second->push_back (pl);
}

void
PlaylistSelector::ok_button_click()
{
	hide();
}

void
PlaylistSelector::selection_changed ()
{
	if (_ignore_selection) {
		/* selection came from libardour, not the user's action */
		return;
	}

	boost::shared_ptr<Playlist> pl;

	TreeModel::iterator iter = tree.get_selection()->get_selected();

	if (!iter || _rui == 0) {
		/* nothing selected */
		return;
	}

	if ((pl = ((*iter)[columns.playlist])) != 0) {

		if (_rui->is_audio_track () && boost::dynamic_pointer_cast<AudioPlaylist> (pl) == 0) {
			return;
		}
		if (_rui->is_midi_track () && boost::dynamic_pointer_cast<MidiPlaylist> (pl) == 0) {
			return;
		}

		switch (_mode) {
			/*  @Robin:  I dont see a way to undo these playlist actions */
			case plCopy: {
					boost::shared_ptr<Playlist> playlist = PlaylistFactory::create (pl, string_compose ("%1.1", pl->name()));
					/* playlist->reset_shares ();  @Robin is this needed? */
					_rui->track ()->use_playlist (_rui->is_audio_track () ? DataType::AUDIO : DataType::MIDI, playlist);
				} break;
			case plShare:
				_rui->track ()->use_playlist (_rui->is_audio_track () ? DataType::AUDIO : DataType::MIDI, pl, false);  /* share pl but do NOT set me as the owner */
				break;
			case plSteal:
				_rui->track ()->use_playlist (_rui->is_audio_track () ? DataType::AUDIO : DataType::MIDI, pl); /* share the playlist and set ME as the owner */
				break;
			case plSelect:
				_rui->use_playlist (NULL, pl);  //call route_ui::use_playlist because it is group-aware
				break;
		}
	}
}

bool
PlaylistSelector::on_key_press_event (GdkEventKey* ev)
{
	/* Allow these keys to have their in-dialog effect */

	switch (ev->keyval) {
	case GDK_Up:
	case GDK_Down:
		return ArdourDialog::on_key_press_event (ev);
	}

	/* Don't just forward the key press ... make it act as if it occured in
	   whatever the main window currently is.
	*/
	Gtk::Window& main_window (ARDOUR_UI::instance()->main_window());
	return ARDOUR_UI_UTILS::relay_key_press (ev, &main_window);
}
