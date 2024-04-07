/*
 * Copyright (C) 2024 Paul Davis <paul@linuxaudiosystems.com>
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

#include <gtkmm/stock.h>

#include "livetrax_add_track_dialog.h"

#include "pbd/i18n.h"

using namespace Gtk;

LiveTraxAddTrackDialog::LiveTraxAddTrackDialog ()
	: ArdourDialog (_("Add Tracks"))
	, track_count (1.0, 1.0, 1024.0, 1.0, 10.)
	, track_count_spinner (track_count)
	, mono_button (channel_button_group, _("Mono"))
	, stereo_button (channel_button_group, _("Stereo"))
{

	get_vbox()->pack_start (track_count_spinner);
	get_vbox()->pack_start (mono_button);
	get_vbox()->pack_start (stereo_button);

	mono_button.set_active();

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::OK, RESPONSE_OK);

	show_all ();
}

LiveTraxAddTrackDialog::~LiveTraxAddTrackDialog()
{
}

int
LiveTraxAddTrackDialog::num_tracks() const
{
	return track_count.get_value();
}

bool
LiveTraxAddTrackDialog::stereo() const
{
	return stereo_button.get_active();
}
