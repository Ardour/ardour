/*
    Copyright (C) 2000-2006 Paul Davis 

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

#include <pbd/error.h>

#include <ardour/playlist.h>
#include <ardour/audioplaylist.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

Playlist*
Playlist::copyPlaylist (const Playlist& playlist, nframes_t start, nframes_t length,
			string name, bool result_is_hidden)
{
	const AudioPlaylist* apl;

	if ((apl = dynamic_cast<const AudioPlaylist*> (&playlist)) != 0) {
		return new AudioPlaylist (*apl, start, length, name, result_is_hidden);
	} else {
		fatal << _("programming error: Playlist::copyPlaylist called with unknown Playlist type")
		      << endmsg;
		/*NOTREACHED*/
		return 0;
	}
}
