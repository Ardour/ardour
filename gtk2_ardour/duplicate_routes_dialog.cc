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

#include "ardour/route.h"
#include "ardour/session.h"

#include "editor.h"
#include "duplicate_routes_dialog.h"
#include "selection.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace Gtk;

DuplicateRouteDialog::DuplicateRouteDialog ()
	: ArdourDialog (_("Duplicate Tracks & Busses"), false, false)
	, copy_playlists_button (playlist_button_group, _("Copy playlists"))
	, new_playlists_button (playlist_button_group, _("Create new (empty) playlists"))
	, share_playlists_button (playlist_button_group, _("Share playlists"))
	, count_adjustment (1.0, 1.0, 999, 1.0, 10.0)
	, count_spinner (count_adjustment)
	, count_label (_("Duplicate each track/bus this number of times"))
{
	count_box.pack_start (count_label, false, false);
	count_box.pack_start (count_spinner, false, false);
	get_vbox()->pack_start (count_box, false, false, 20);

	playlist_button_box.pack_start (copy_playlists_button, false, false);
	playlist_button_box.pack_start (new_playlists_button, false, false);
	playlist_button_box.pack_start (share_playlists_button, false, false);
	playlist_button_box.show_all ();

	get_vbox()->show_all ();

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::OK, RESPONSE_OK);
}

int
DuplicateRouteDialog::restart (Session* s)
{
	if (!s) {
		return -1;
	}

	set_session (s);

	TrackSelection& tracks  (PublicEditor::instance().get_selection().tracks);
	uint32_t ntracks = 0;
	uint32_t nbusses = 0;

	for (TrackSelection::iterator t = tracks.begin(); t != tracks.end(); ++t) {

		RouteUI* rui = dynamic_cast<RouteUI*> (*t);

		if (!rui) {
			/* some other type of timeaxis view, not a route */
			continue;
		}

		boost::shared_ptr<Route> r (rui->route());

		if (boost::dynamic_pointer_cast<Track> (r)) {
			ntracks++;
		} else {
			if (!r->is_master() && !r->is_monitor()) {
				nbusses++;
			}
		}
	}

	if (ntracks == 0 && nbusses == 0) {
		std::cerr << "You can't do this\n";
		return -1;
	}

	/* XXX grrr. Gtk Boxes do not shrink when children are removed,
	   which is what we really want to happen here.
	*/

	if (playlist_button_box.get_parent()) {
		get_vbox()->remove (playlist_button_box);
	}

	if (ntracks > 0) {
		get_vbox()->pack_end (playlist_button_box, false, false);
	}

	return 0;
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

void
DuplicateRouteDialog::on_response (int response)
{
	hide ();

	if (response != RESPONSE_OK) {
		return;
	}

	ARDOUR::PlaylistDisposition playlist_action = playlist_disposition ();
	uint32_t cnt = count ();

	/* Copy the track selection because it will/may change as we add new ones */
	TrackSelection tracks  (PublicEditor::instance().get_selection().tracks);
	int err = 0;

	for (TrackSelection::iterator t = tracks.begin(); t != tracks.end(); ++t) {

		RouteUI* rui = dynamic_cast<RouteUI*> (*t);

		if (!rui) {
			/* some other type of timeaxis view, not a route */
			continue;
		}

		if (rui->route()->is_master() || rui->route()->is_monitor()) {
			/* no option to duplicate these */
			continue;
		}

		XMLNode& state (rui->route()->get_state());
		RouteList rl = _session->new_route_from_template (cnt, state, std::string(), playlist_action);

		/* normally the state node would be added to a parent, and
		 * ownership would transfer. Because we don't do that here,
		 * we need to delete the node ourselves.
		 */

		delete &state;

		if (rl.empty()) {
			err++;
			break;
		}
	}

	if (err) {
		MessageDialog msg (_("1 or more tracks/busses could not be duplicated"),
		                     true, MESSAGE_ERROR, BUTTONS_OK, true);
		msg.set_position (WIN_POS_MOUSE);
		msg.run ();
	}
}
