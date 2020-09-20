/*
 * Copyright (C) 2007-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
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

#ifndef __ardour_playlist_factory_h__
#define __ardour_playlist_factory_h__

#include "ardour/playlist.h"

class XMLNode;

namespace ARDOUR {

class Session;

class LIBARDOUR_API PlaylistFactory {

  public:
	static PBD::Signal2<void,boost::shared_ptr<Playlist>, bool> PlaylistCreated;

	static boost::shared_ptr<Playlist> create (Session&, const XMLNode&, bool hidden = false, bool unused = false);
	static boost::shared_ptr<Playlist> create (DataType type, Session&, std::string name, bool hidden = false);
	static boost::shared_ptr<Playlist> create (boost::shared_ptr<const Playlist>, std::string name, bool hidden = false);
	static boost::shared_ptr<Playlist> create (boost::shared_ptr<const Playlist>, timepos_t const & start, timepos_t const & cnt, std::string name, bool hidden = false);
};

}

#endif /* __ardour_playlist_factory_h__  */
