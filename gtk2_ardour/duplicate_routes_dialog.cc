/*
    Copyright (C) 2015 Paul Davis

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

#include "gtkmm/stock.h"

#include "duplicate_routes_dialog.h"

#include "i18n.h"

DuplicateRouteDialog::DuplicateRouteDialog ()
	: ArdourDialog (_("Duplicate Tracks & Busses"), false, false)
	, copy_playlists_button (playlist_button_group, _("Copy playlists"))
	, new_playlists_button (playlist_button_group, _("Create new (empty) playlists"))
	, share_playlists_button (playlist_button_group, _("Share playlists"))
	, count_adjustment (1.0, 1.0, 999, 1.0, 10.0)
	, count_spinner (count_adjustment)
	, count_label (_("Duplicate each track/bus this number of times"))
{
	playlist_button_box.pack_start (copy_playlists_button, false, false);
	playlist_button_box.pack_start (new_playlists_button, false, false);
	playlist_button_box.pack_start (share_playlists_button, false, false);

	get_vbox()->pack_start (playlist_button_box, false, false);

	count_box.pack_start (count_label, false, false);
	count_box.pack_start (count_spinner, false, false);

	get_vbox()->pack_start (count_box, false, false, 20);

	get_vbox()->show_all ();

	add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	add_button (Gtk::Stock::OK, Gtk::RESPONSE_OK);
}

DuplicateRouteDialog::~DuplicateRouteDialog ()
{
}

uint32_t
DuplicateRouteDialog::count() const
{
	return count_adjustment.get_value ();
}

ARDOUR::PlaylistDisposition
DuplicateRouteDialog::playlist_disposition() const
{
	if (new_playlists_button.get_active()) {
		return ARDOUR::NewPlaylist;
	} else if (copy_playlists_button.get_active()) {
		return ARDOUR::CopyPlaylist;
	}

	return ARDOUR::SharePlaylist;
}
