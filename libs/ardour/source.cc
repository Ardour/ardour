/*
    Copyright (C) 2000 Paul Davis 

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

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <float.h>
#include <cerrno>
#include <ctime>
#include <cmath>
#include <iomanip>
#include <algorithm>

#include <glibmm/thread.h>
#include <pbd/xml++.h>
#include <pbd/pthread_utils.h>

#include <ardour/source.h>
#include <ardour/playlist.h>

#include "i18n.h"

using std::min;
using std::max;

using namespace ARDOUR;

Source::Source (Session& s, string name, DataType type)
	: _session (s)
	, _type(type)
{
	assert(_name.find("/") == string::npos);

	_name = name;
	_timestamp = 0;
	_length = 0;
	_in_use = 0;
}

Source::Source (Session& s, const XMLNode& node) 
	: _session (s)
	, _type(DataType::AUDIO)
{
	_timestamp = 0;
	_length = 0;
	_in_use = 0;

	if (set_state (node) || _type == DataType::NIL) {
		throw failed_constructor();
	}
	assert(_name.find("/") == string::npos);
}

Source::~Source ()
{
	notify_callbacks ();
}

XMLNode&
Source::get_state ()
{
	XMLNode *node = new XMLNode ("Source");
	char buf[64];

	node->add_property ("name", _name);
	node->add_property ("type", _type.to_string());
	_id.print (buf, sizeof (buf));
	node->add_property ("id", buf);

	if (_timestamp != 0) {
		snprintf (buf, sizeof (buf), "%ld", _timestamp);
		node->add_property ("timestamp", buf);
	}

	return *node;
}

int
Source::set_state (const XMLNode& node)
{
	const XMLProperty* prop;

	if ((prop = node.property ("name")) != 0) {
		_name = prop->value();
	} else {
		return -1;
	}
	
	if ((prop = node.property ("id")) != 0) {
		_id = prop->value ();
	} else {
		return -1;
	}

	if ((prop = node.property ("type")) != 0) {
		_type = DataType(prop->value());
	}

	if ((prop = node.property ("timestamp")) != 0) {
		sscanf (prop->value().c_str(), "%ld", &_timestamp);
	}
	
	// Don't think this is valid, absolute paths fail
	//assert(_name.find("/") == string::npos);

	return 0;
}

void
Source::update_length (nframes_t pos, nframes_t cnt)
{
	if (pos + cnt > _length) {
		_length = pos+cnt;
	}
}

void
Source::add_playlist (boost::shared_ptr<Playlist> pl)
{
	std::pair<PlaylistMap::iterator,bool> res;
	std::pair<boost::shared_ptr<Playlist>, uint32_t> newpair (pl, 1);
	Glib::Mutex::Lock lm (playlist_lock);

	res = _playlists.insert (newpair);

	if (!res.second) {
		/* it already existed, bump count */
		res.first->second++;
	}
		
	pl->GoingAway.connect (bind (mem_fun (*this, &Source::remove_playlist), boost::weak_ptr<Playlist> (pl)));
}

void
Source::remove_playlist (boost::weak_ptr<Playlist> wpl)
{
	boost::shared_ptr<Playlist> pl (wpl.lock());

	if (!pl) {
		return;
	}

	PlaylistMap::iterator x;
	Glib::Mutex::Lock lm (playlist_lock);

	if ((x = _playlists.find (pl)) != _playlists.end()) {
		if (x->second > 1) {
			x->second--;
		} else {
			_playlists.erase (x);
		}
	}
}

uint32_t
Source::used () const
{
	return _playlists.size();
}
