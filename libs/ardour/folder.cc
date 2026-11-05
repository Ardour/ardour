/*
 * Copyright (C) 2026 Paul Davis <paul@linuxaudiosystems.com>
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

#include "ardour/folder.h"
#include "ardour/track.h"

using namespace ARDOUR;

Folder::Folder (Session & s, std::string const & name, PresentationInfo const & pi)
	: Stripable (s, name, pi)
{
}

int
Folder::set_state (XMLNode const&, int)
{
	return 0;
}

void
Folder::get_contents (std::vector<std::shared_ptr<Stripable>> & ret) const
{
	PBD::RWLock::ReaderLock lm (contents_lock);
	ret.insert (ret.begin(), _contents.begin(), _contents.end());
}

void
Folder::get_content_tracks (std::vector<std::shared_ptr<Track>> & ret) const
{
	PBD::RWLock::ReaderLock lm (contents_lock);
	for (auto & s : _contents) {
		std::shared_ptr<Track> t = std::dynamic_pointer_cast<Track> (s);
		if (t) {
			ret.push_back (t);
		}
	}
}

void
Folder::add (std::shared_ptr<Stripable>)
{
}

void
Folder::add (std::vector<std::shared_ptr<Stripable>> adds)
{
	PBD::RWLock::WriterLock lm (contents_lock);
	for (auto & s : adds) {
		_contents.push_back (s);
		s->DropReferences.connect_same_thread (content_connections, std::bind (&Folder::content_going_away, this, std::weak_ptr<Stripable> (s)));
	}
}

void
Folder::remove (std::shared_ptr<Stripable> s)
{
	PBD::RWLock::WriterLock lm (contents_lock);
	for (auto iter = _contents.begin(); iter != _contents.end(); ++iter) {
		if (*iter == s) {
			_contents.erase (iter);
			break;
		}
	}
}

void
Folder::remove (std::vector<std::shared_ptr<Stripable>> ls)
{
	PBD::RWLock::WriterLock lm (contents_lock);
	for (auto const & s : ls) {
		for (auto iter = _contents.begin(); iter != _contents.end(); ++iter) {
			if (*iter == s) {
				_contents.erase (iter);
				break;
			}
		}
	}
}

void
Folder::content_going_away (std::weak_ptr<Stripable> ws)
{
	std::shared_ptr<Stripable> s = ws.lock();
	if (!s) {
		return;
	}

	remove (s);
}

void
Folder::clear ()
{
	PBD::RWLock::WriterLock lm (contents_lock);
	_contents.clear ();
	content_connections.drop_connections ();
}
