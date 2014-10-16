/*
    Copyright (C) 2011 Paul Davis

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

#ifndef __ardour_playlist_source_h__
#define __ardour_playlist_source_h__

#include <string>

#include <boost/shared_ptr.hpp>

#include "ardour/ardour.h"
#include "ardour/source.h"

namespace ARDOUR {

class Playlist;

class LIBARDOUR_API PlaylistSource : virtual public Source {
public:
	virtual ~PlaylistSource ();

	int set_state (const XMLNode&, int version);
	boost::shared_ptr<const Playlist> playlist() const { return _playlist; }
	const PBD::ID& original() const { return _original; }

protected:
	boost::shared_ptr<Playlist> _playlist;
	PBD::ID                     _original;
	frameoffset_t               _playlist_offset;
	framecnt_t                  _playlist_length;

	PlaylistSource (Session&, const PBD::ID&, const std::string& name, boost::shared_ptr<Playlist>, DataType,
	                frameoffset_t begin, framecnt_t len, Source::Flag flags);
	PlaylistSource (Session&, const XMLNode&);

	void add_state (XMLNode&);
};

} /* namespace */

#endif /* __ardour_playlist_source_h__ */
