/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystems.com>
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

#include "bbt_marker_dialog.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;
using namespace PBD;
using namespace Temporal;

BBTMarkerDialog::BBTMarkerDialog (timepos_t const & pos)
	: ArdourDialog (_("New Music Time"))
	, _point (0)
	, _position (pos)
	, entry_label (_("Position"))

{
	BBT_Time bbt = TempoMap::use()->bbt_at (pos).round_to_beat ();

	bar_entry.set_range (1, 9999);
	beat_entry.set_range (1, 9999);
	bar_entry.set_digits (0);
	beat_entry.set_digits (0);

	bbt_box.pack_start (entry_label);
	bbt_box.pack_start (bar_entry);
	bbt_box.pack_start (beat_entry);

	bar_entry.set_value (bbt.bars);
	beat_entry.set_value (bbt.beats);

	get_vbox()->pack_start (bbt_box);
	bbt_box.show_all ();

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (_("Add Marker"), RESPONSE_OK);
}

BBT_Time
BBTMarkerDialog::bbt_value () const
{
	int bars = bar_entry.get_value_as_int();
	int beats = beat_entry.get_value_as_int();

	return BBT_Time (bars, beats, 0);
}

timepos_t
BBTMarkerDialog::position() const
{
	return _position;
}
