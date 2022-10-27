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

#include <string>

#include "temporal/tempo.h"

#include "public_editor.h"

/* RAII for tempo map edits. This object manages both Tempo map RCU stuff, plus
 * reversible command state, removing the need for repeated boilerplate at each
 * map edit site.
 *
 * One complication: GUI tempo map markers are all reassociated with the
 * relevant points of the write-copy of the map during ::begin() (typically
 * called in the constructor, unless it's begin argument is false). You must
 * delay getting a reference on a point to edit until after the TempoMapChange
 * object has called begin() (i.e. has been constructed), otherwise the
 * reference will point to the "old" copy of the map.
 */

class TempoMapChange {
  public:
	TempoMapChange (PublicEditor& e, std::string const & name, bool update_on_commit = true, bool begin = true);
	~TempoMapChange ();

	void begin ();
	void abort ();
	void use_new_map (Temporal::TempoMap::WritableSharedPtr);

	Temporal::TempoMap& map() const { return *writable_map.get(); }

  private:
	PublicEditor& editor;
	Temporal::TempoMap::WritableSharedPtr writable_map;
	std::string name;
	bool aborted;
	bool begun;
	bool update_on_commit;
	XMLNode* before;
};
