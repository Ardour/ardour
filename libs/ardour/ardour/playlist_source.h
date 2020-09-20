/*
 * Copyright (C) 2011-2017 Paul Davis <paul@linuxaudiosystems.com>
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
	const PBD::ID& owner() const { return _owner; }

	void set_owner (PBD::ID const & id);

protected:
	boost::shared_ptr<Playlist> _playlist;
	PBD::ID                     _original;
	PBD::ID                     _owner;
	timepos_t                   _playlist_offset;
	timepos_t                   _playlist_length;

	PlaylistSource (Session&, const PBD::ID&, const std::string& name, boost::shared_ptr<Playlist>, DataType,
	                timepos_t const & begin, timepos_t const & len, Source::Flag flags);
	PlaylistSource (Session&, const XMLNode&);

	void add_state (XMLNode&);
};

} /* namespace */

#endif /* __ardour_playlist_source_h__ */
