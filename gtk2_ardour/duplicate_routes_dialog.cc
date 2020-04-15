/*
 * Copyright (C) 2015-2019 Paul Davis <paul@linuxaudiosystems.com>
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

#include "gtkmm/stock.h"

#include "ardour/route.h"
#include "ardour/session.h"

#include "ardour_ui.h"
#include "editor.h"
#include "duplicate_routes_dialog.h"
#include "selection.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace Gtk;

DuplicateRouteDialog::DuplicateRouteDialog ()
	: ArdourDialog (_("Duplicate Tracks/Busses"), false, false)
	, playlist_option_label (_("For each Track:"))
	, copy_playlists_button (playlist_button_group, _("Copy playlist"))
	, new_playlists_button (playlist_button_group, _("New playlist"))
	, share_playlists_button (playlist_button_group, _("Share playlist"))
	, count_adjustment (1.0, 1.0, 999, 1.0, 10.0)
	, count_spinner (count_adjustment)
	, count_label (_("Duplicate each track/bus this number of times:"))
{
	count_box.pack_start (count_label, false, false);
	count_box.pack_start (count_spinner, false, false, 5);
	get_vbox()->pack_start (count_box, false, false, 10);

	Gtk::HBox* hb = manage (new HBox);
	hb->pack_start (playlist_option_label, false, false);
	get_vbox()->pack_start (*hb, false, false, 10);

	playlist_button_box.pack_start (copy_playlists_button, false, false);
	playlist_button_box.pack_start (new_playlists_button, false, false);
	playlist_button_box.pack_start (share_playlists_button, false, false);
	playlist_button_box.show_all ();

	insert_at_combo.append_text (_("First"));
	insert_at_combo.append_text (_("Before Selection"));
	insert_at_combo.append_text (_("After Selection"));
	insert_at_combo.append_text (_("Last"));
	insert_at_combo.set_active (3);

	Gtk::Label* l = manage (new Label (_("Insert duplicates at: ")));
	Gtk::HBox* b = manage (new HBox);
	b->pack_start (*l, false, false, 10);
	b->pack_start (insert_at_combo, true, true);

	get_vbox()->pack_end (*b, false, false, 10);

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

	/* Track Selection should be sorted into presentation order before
	 * duplicating, so that new tracks appear in same order as the
	 * originals.
	 */

	StripableList sl;

	for (TrackSelection::iterator t = tracks.begin(); t != tracks.end(); ++t) {
		RouteUI* rui = dynamic_cast<RouteUI*> (*t);
		if (rui) {
			sl.push_back (rui->route());
		}
	}

	sl.sort (Stripable::Sorter());

	for (StripableList::iterator s = sl.begin(); s != sl.end(); ++s) {

		boost::shared_ptr<Route> r;

		if ((r = boost::dynamic_pointer_cast<Route> (*s)) == 0) {
			/* some other type of Stripable, not a route */
			continue;
		}

		if ((*s)->is_master() || (*s)->is_monitor()) {
			/* no option to duplicate these */
			continue;
		}

		XMLNode& state (r->get_state());
		RouteList rl = _session->new_route_from_template (cnt, ARDOUR_UI::instance()->translate_order (insert_at()), state, std::string(), playlist_action);

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

RouteDialogs::InsertAt
DuplicateRouteDialog::insert_at ()
{
	using namespace RouteDialogs;

	std::string str = insert_at_combo.get_active_text();

	if (str == _("First")) {
		return First;
	} else if (str == _("After Selection")) {
		return AfterSelection;
	} else if (str == _("Before Selection")){
		return BeforeSelection;
	}
	return Last;
}
