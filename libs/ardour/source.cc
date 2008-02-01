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

Source::Source (Session& s, string name)
	: _session (s)
{
	_name = name;
	_analysed = false;
	_timestamp = 0;
	_in_use = 0;
}

Source::Source (Session& s, const XMLNode& node) 
	: _session (s)
{
	_timestamp = 0;
	_analysed = false;
	_in_use = 0;

	if (set_state (node)) {
		throw failed_constructor();
	}
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

	if ((prop = node.property ("timestamp")) != 0) {
		sscanf (prop->value().c_str(), "%ld", &_timestamp);
	}

	return 0;
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

bool
Source::has_been_analysed() const
{
	Glib::Mutex::Lock lm (_analysis_lock);
	return _analysed;
}

void
Source::set_been_analysed (bool yn)
{
	{
		Glib::Mutex::Lock lm (_analysis_lock);
		_analysed = yn;
	}
	
	if (yn) {
		AnalysisChanged(); // EMIT SIGNAL
	}
}
       
