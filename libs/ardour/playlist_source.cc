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

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <vector>
#include <cstdio>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/error.h"
#include "pbd/convert.h"
#include "pbd/enumwriter.h"

#include "ardour/playlist.h"
#include "ardour/playlist_source.h"
#include "ardour/playlist_factory.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

PlaylistSource::PlaylistSource (Session& s, const ID& orig, const std::string& name, boost::shared_ptr<Playlist> p, DataType type,
				frameoffset_t begin, framecnt_t len, Source::Flag /*flags*/)
	: Source (s, type, name)
	, _playlist (p)
	, _original (orig)
{
	/* PlaylistSources are never writable, renameable, removable or destructive */
	_flags = Flag (_flags & ~(Writable|CanRename|Removable|RemovableIfEmpty|RemoveAtDestroy|Destructive));

	_playlist = p;
	_playlist_offset = begin;
	_playlist_length = len;

	_level = _playlist->max_source_level () + 1;
}

PlaylistSource::PlaylistSource (Session& s, const XMLNode& node)
	: Source (s, DataType::AUDIO, "toBeRenamed")
{
	/* PlaylistSources are never writable, renameable, removable or destructive */
	_flags = Flag (_flags & ~(Writable|CanRename|Removable|RemovableIfEmpty|RemoveAtDestroy|Destructive));


	if (set_state (node, Stateful::loading_state_version)) {
		throw failed_constructor ();
	}
}

PlaylistSource::~PlaylistSource ()
{
}

void
PlaylistSource::add_state (XMLNode& node)
{
	char buf[64];

	_playlist->id().print (buf, sizeof (buf));
	node.add_property ("playlist", buf);
	snprintf (buf, sizeof (buf), "%" PRIi64, _playlist_offset);
	node.add_property ("offset", buf);
	snprintf (buf, sizeof (buf), "%" PRIu64, _playlist_length);
	node.add_property ("length", buf);
	node.add_property ("original", id().to_s());

	node.add_child_nocopy (_playlist->get_state());
}

int
PlaylistSource::set_state (const XMLNode& node, int /*version*/)
{
	/* check that we have a playlist ID */

	const XMLProperty *prop = node.property (X_("playlist"));

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

	if ((prop = node.property (X_("name"))) == 0) {
		throw failed_constructor ();
	}

	set_name (prop->value());

	if ((prop = node.property (X_("offset"))) == 0) {
		throw failed_constructor ();
	}
	sscanf (prop->value().c_str(), "%" PRIi64, &_playlist_offset);

	if ((prop = node.property (X_("length"))) == 0) {
		throw failed_constructor ();
	}

	sscanf (prop->value().c_str(), "%" PRIu64, &_playlist_length);

	/* XXX not quite sure why we set our ID back to the "original" one
	   here. october 2011, paul
	*/

	if ((prop = node.property (X_("original"))) == 0) {
		throw failed_constructor ();
	}

	set_id (prop->value());

	_level = _playlist->max_source_level () + 1;

	return 0;
}
