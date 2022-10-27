/*
    Copyright (C) 2022 Paul Davis

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

#include "pbd/i18n.h"
#include "tempo_map_change.h"

TempoMapChange::TempoMapChange (PublicEditor& e, std::string const & str, bool uoc, bool begin_now)
	: editor (e)
	, name (str)
	, aborted (false)
	, begun (false)
	, update_on_commit (uoc)
	, before (0)
{
	if (begin_now) {
		begin ();
	}
}

TempoMapChange::~TempoMapChange ()
{
	if (!aborted && begun) {
		XMLNode& after = writable_map->get_state();
		editor.session()->add_command (new Temporal::TempoCommand (_("tempo map change"), before, &after));


		editor.commit_tempo_map_edit (writable_map, update_on_commit);
		editor.commit_reversible_command ();
	}
}

void
TempoMapChange::begin ()
{
	writable_map = editor.begin_tempo_map_edit ();
	before = &writable_map->get_state();
	editor.begin_reversible_command (name);
	begun = true;
}

void
TempoMapChange::abort ()
{
	if (begun) {
		editor.abort_tempo_map_edit ();
		editor.abort_reversible_command ();
		aborted = true;
		begun = false;
	}
}

void
TempoMapChange::use_new_map (Temporal::TempoMap::WritableSharedPtr new_map)
{
	/* existing write copy goes out of scope, and is deleted, We continue
	 * on with the new map, and will RCU-update it in our destructor.
	 */
	writable_map = new_map;
}
