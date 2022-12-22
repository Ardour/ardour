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

BBTMarkerDialog::BBTMarkerDialog (timepos_t const & pos, BBT_Time const& bbt)
	: ArdourDialog (_("New Music Time"))
	, _point (0)
	, _position (pos)
	, _bbt (bbt)
	, bar_label (_("Bar"))
	, beat_label (_("Beat"))
	, name_label (_("Name"))

{
	init (true);
}

BBTMarkerDialog::BBTMarkerDialog (MusicTimePoint& p)
	: ArdourDialog (_("Edit Music Time"))
	, _point (&p)
	, _position (timepos_t::from_superclock (p.sclock()))
	, _bbt (TempoMap::use()->bbt_at (_position).round_to_beat ())
	, bar_label (_("Bar"))
	, beat_label (_("Beat"))
	, name_label (_("Name"))
{
	init (false);
}

void
BBTMarkerDialog::init (bool add)
{
	bar_entry.set_range (1, 9999);
	beat_entry.set_range (1, 9999); // XXX (1, time-signature denominator at _position) ?!
	bar_entry.set_digits (0);
	beat_entry.set_digits (0);

	bar_label.set_alignment(Gtk::ALIGN_END, Gtk::ALIGN_CENTER);
	beat_label.set_alignment(Gtk::ALIGN_END, Gtk::ALIGN_CENTER);

	bbt_box.pack_start (bar_label, true, true, 2);
	bbt_box.pack_start (bar_entry, true, true, 2);
	bbt_box.pack_start (beat_label, true, true, 2);
	bbt_box.pack_start (beat_entry, true, true, 2);

	bar_entry.set_value (_bbt.bars);
	beat_entry.set_value (_bbt.beats);

	name_box.pack_start (name_label, true, true, 4);
	name_box.pack_start (name_entry, true, true);

	if (_point) {
		name_entry.set_text (_point->name());
	}

	name_entry.signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &BBTMarkerDialog::response), Gtk::RESPONSE_OK));

	get_vbox()->pack_start (name_box);
	get_vbox()->pack_start (bbt_box);

	bbt_box.show_all ();
	name_box.show_all ();

	add_button (Stock::CANCEL, RESPONSE_CANCEL);

	if (add) {
		add_button (_("Add Marker"), RESPONSE_OK);
	} else {
		add_button (_("Save Changes"), RESPONSE_OK);
	}

	get_vbox()->set_border_width (12);
	get_vbox()->set_spacing (12);
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

std::string
BBTMarkerDialog::name () const
{
	return name_entry.get_text();
}
