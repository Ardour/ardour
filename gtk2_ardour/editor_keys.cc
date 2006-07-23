/*
    Copyright (C) 2000 Paul Davis 

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

    $Id$
*/

#include <cstdlib>
#include <cmath>
#include <string>

#include <pbd/error.h>

#include <ardour/session.h>
#include <ardour/region.h>

#include "ardour_ui.h"
#include "editor.h"
#include "time_axis_view.h"
#include "regionview.h"
#include "selection.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace sigc;

void
Editor::keyboard_selection_finish (bool add)
{
	if (session && have_pending_keyboard_selection) {
		begin_reversible_command (_("keyboard selection"));
		if (add) {
			selection->add (pending_keyboard_selection_start, session->audible_frame());
		} else {
			selection->set (0, pending_keyboard_selection_start, session->audible_frame());
		}
		commit_reversible_command ();
		have_pending_keyboard_selection = false;
	}
}

void
Editor::keyboard_selection_begin ()
{
	if (session) {
		pending_keyboard_selection_start = session->audible_frame();
		have_pending_keyboard_selection = true;
	}
}

void
Editor::keyboard_duplicate_region ()
{
	if (selection->regions.empty()) {
		return;
	}

	float prefix;
	bool was_floating;

	if (get_prefix (prefix, was_floating) == 0) {
		duplicate_some_regions (selection->regions, prefix);
	} else {
		duplicate_some_regions (selection->regions, 1);
	}
}

void
Editor::keyboard_duplicate_selection ()
{
	float prefix;
	bool was_floating;

	if (get_prefix (prefix, was_floating) == 0) {
		duplicate_selection (prefix);
	} else {
		duplicate_selection (1);
	}
}

void
Editor::keyboard_paste ()
{
	float prefix;
	bool was_floating;

	if (get_prefix (prefix, was_floating) == 0) {
		paste (prefix);
	} else {
		paste (1);
	}
}

void
Editor::keyboard_insert_region_list_selection ()
{
	float prefix;
	bool was_floating;

	if (get_prefix (prefix, was_floating) == 0) {
		insert_region_list_selection (prefix);
	} else {
		insert_region_list_selection (1);
	}
}

int
Editor::get_prefix (float& val, bool& was_floating)
{
	return Keyboard::the_keyboard().get_prefix (val, was_floating);
}

