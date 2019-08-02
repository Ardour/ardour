/*
 * Copyright (C) 2011-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#include "midi_channel_dialog.h"

#include "pbd/i18n.h"

using namespace Gtk;

MidiChannelDialog::MidiChannelDialog (uint8_t active_channel)
	: ArdourDialog (X_("MIDI Channel Chooser"), true)
	, selector (active_channel)
{
	selector.show_all ();
	get_vbox()->pack_start (selector);
	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::OK, RESPONSE_OK);
}

uint8_t
MidiChannelDialog::active_channel () const
{
	return selector.get_active_channel();
}
