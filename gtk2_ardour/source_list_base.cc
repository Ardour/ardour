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

#include "gui_thread.h"
#include "source_list_base.h"

#include "pbd/i18n.h"

using namespace Gtk;

SourceListBase::SourceListBase ()
{
}

void
SourceListBase::set_session (ARDOUR::Session* s)
{
	if (s) {
		s->SourceRemoved.connect (_session_connections, MISSING_INVALIDATOR, boost::bind (&SourceListBase::remove_weak_source, this, _1), gui_context ());
	}
	RegionListBase::set_session (s);
}

void
SourceListBase::remove_weak_source (boost::weak_ptr<ARDOUR::Source> src)
{
	boost::shared_ptr<ARDOUR::Source> source = src.lock ();
	if (source) {
		remove_source (source);
	}
}

void
SourceListBase::remove_source (boost::shared_ptr<ARDOUR::Source> source)
{
	TreeModel::iterator i;
	TreeModel::Children rows = _model->children ();
	for (i = rows.begin (); i != rows.end (); ++i) {
		boost::shared_ptr<ARDOUR::Region> rr = (*i)[_columns.region];
		if (rr->source () == source) {
			RegionRowMap::iterator map_it = region_row_map.find (rr);
			assert (map_it != region_row_map.end () && i == map_it->second);
			region_row_map.erase (map_it);
			_model->erase (i);
			break;
		}
	}
}

bool
SourceListBase::list_region (boost::shared_ptr<ARDOUR::Region> region) const
{
	/* by definition, the Source List only shows whole-file regions
	 * this roughly equates to Source objects, but preserves the stereo-ness
	 * (or multichannel-ness) of a stereo source file.
	 */
	return region->whole_file ();
}

void
SourceListBase::tag_edit (const std::string& path, const std::string& new_text)
{
	RegionListBase::tag_edit (path, new_text);

	TreeIter row_iter;
	if ((row_iter = _model->get_iter (path))) {
		boost::shared_ptr<ARDOUR::Region> region = (*row_iter)[_columns.region];
		if (region) {
			_session->set_dirty (); // whole-file regions aren't in a playlist to catch property changes, so we need to explicitly set the session dirty
		}
	}
}

void
SourceListBase::name_edit (const std::string& path, const std::string& new_text)
{
	RegionListBase::name_edit (path, new_text);

	TreeIter row_iter;
	if ((row_iter = _model->get_iter (path))) {
		boost::shared_ptr<ARDOUR::Region> region = (*row_iter)[_columns.region];
		if (region) {
			_session->set_dirty (); // whole-file regions aren't in a playlist to catch property changes, so we need to explicitly set the session dirty
		}
	}
}
