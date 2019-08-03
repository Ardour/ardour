/*
 * Copyright (C) 2000-2007 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#include "widgets/focus_entry.h"

using namespace ArdourWidgets;

FocusEntry::FocusEntry ()
	: next_release_selects (false)
{
}

bool
FocusEntry::on_button_press_event (GdkEventButton* ev)
{
	if (!has_focus()) {
		next_release_selects = true;
	}
	return Entry::on_button_press_event (ev);
}

bool
FocusEntry::on_button_release_event (GdkEventButton* ev)
{
	if (next_release_selects) {
		bool ret = Entry::on_button_release_event (ev);
		select_region (0, -1);
		next_release_selects = false;
		return ret;
	}

	return Entry::on_button_release_event (ev);
}
