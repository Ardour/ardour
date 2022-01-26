/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
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

#include "ardour/region.h"
#include "ardour/session.h"

#include "trigger_source_list.h"

#include "pbd/i18n.h"

using namespace Gtk;

TriggerSourceList::TriggerSourceList ()
{
	add_name_column ();
	setup_col (append_col (_columns.channels, "Chans    "), 1, ALIGN_LEFT, _("# Ch"), _("# Channels in the region"));
	add_tag_column ();

	setup_col (append_col (_columns.captd_xruns, "1234567890"), 21, ALIGN_RIGHT, _("# Xruns"), _("Number of dropouts that occurred during recording"));
	setup_col (append_col (_columns.take_id, "2021-01-19 02:34:03"), 18, ALIGN_LEFT, _("Take ID"), _("Take ID"));
	_display.get_column (0)->set_resizable (true);
}
