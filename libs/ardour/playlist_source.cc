/*
 * Copyright (C) 2011-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2011-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016 Tim Mayberry <mojofunk@gmail.com>
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

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <vector>
#include <cstdio>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/error.h"
#include "pbd/types_convert.h"
#include "pbd/enumwriter.h"

#include "ardour/playlist.h"
#include "ardour/playlist_source.h"
#include "ardour/playlist_factory.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

PlaylistSource::PlaylistSource (Session& s, const ID& orig, const std::string& name, boost::shared_ptr<Playlist> p, DataType type,
                                timepos_t const & begin, timepos_t const & len, Source::Flag /*flags*/)
	: Source (s, type, name)
	, _playlist (p)
	, _original (orig)
	, _owner (0) /* zero is never a legal ID for an object */
{
	/* PlaylistSources are never writable, renameable or removable */
	_flags = Flag (_flags & ~(Writable|CanRename|Removable|RemovableIfEmpty|RemoveAtDestroy));

	_playlist = p;
	_playlist->use ();
	_playlist_offset = begin;
	_playlist_length = len;

	_level = _playlist->max_source_level () + 1;
}

PlaylistSource::PlaylistSource (Session& s, const XMLNode& node)
	: Source (s, DataType::AUDIO, "toBeRenamed")
{
	/* PlaylistSources are never writable, renameable or removable */
	_flags = Flag (_flags & ~(Writable|CanRename|Removable|RemovableIfEmpty|RemoveAtDestroy));


	if (set_state (node, Stateful::loading_state_version)) {
		throw failed_constructor ();
	}
}

PlaylistSource::~PlaylistSource ()
{
	_playlist->release ();
}

void
PlaylistSource::set_owner (PBD::ID const &id)
{
	if (_owner == 0) {
		_owner = id;
	}
}

void
PlaylistSource::add_state (XMLNode& node)
{
	node.set_property ("playlist", _playlist->id ());
	node.set_property ("offset", _playlist_offset);
	node.set_property ("length", _playlist_length);
	node.set_property ("original", _original);

	if (_owner != 0) {
		node.set_property ("owner", _owner);
	}

	node.add_child_nocopy (_playlist->get_state());
}

int
PlaylistSource::set_state (const XMLNode& node, int /*version*/)
{
	/* check that we have a playlist ID */

	XMLProperty const * prop = node.property (X_("playlist"));

	if (!prop) {
		error << _("No playlist ID in PlaylistSource XML!") << endmsg;
		throw failed_constructor ();
	}

	/* create playlist from child node */

	XMLNodeList nlist;
	XMLNodeConstIterator niter;

	nlist = node.children();

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == "Playlist") {
			_playlist = PlaylistFactory::create (_session, **niter, true, false);
			break;
		}
	}

	if (!_playlist) {
		error << _("Could not construct playlist for PlaylistSource from session data!") << endmsg;
		throw failed_constructor ();
	}

	/* other properties */

	std::string name;
	if (!node.get_property (X_("name"), name)) {
		throw failed_constructor ();
	}

	set_name (name);

	if (!node.get_property (X_("offset"), _playlist_offset)) {
		throw failed_constructor ();
	}

	if (!node.get_property (X_("length"), _playlist_length)) {
		throw failed_constructor ();
	}

	if (!node.get_property (X_("original"), _original)) {
		throw failed_constructor ();
	}

	/* this is allowed to fail. It either means an older session file
	   format, or a PlaylistSource that wasn't created for a combined
	   region (whose ID would be stored in _owner).
	*/
	node.get_property (X_("owner"), _owner);

	_level = _playlist->max_source_level () + 1;

	return 0;
}
