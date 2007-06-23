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

#ifndef __ardour_source_h__
#define __ardour_source_h__

#include <string>
#include <set>

#include <sigc++/signal.h>

#include <pbd/statefuldestructible.h> 

#include <ardour/ardour.h>
#include <ardour/session_object.h>
#include <ardour/data_type.h>

namespace ARDOUR {

class Session;
class Playlist;

class Source : public SessionObject
{
  public:
	Source (Session&, const std::string& name, DataType type);
	Source (Session&, const XMLNode&);
	
	virtual ~Source ();
	
	DataType type() { return _type; }

	time_t timestamp() const { return _timestamp; }
	void stamp (time_t when) { _timestamp = when; }
	
	/** @return the number of items in this source */
	nframes_t length() const { return _length; }

	virtual nframes_t natural_position() const { return 0; }

	virtual void mark_for_remove() = 0;
	virtual void mark_streaming_write_completed () = 0;
	
	XMLNode& get_state ();
	int set_state (const XMLNode&);
	
	void use () { _in_use++; }
	void disuse () { if (_in_use) { _in_use--; } }
	
	void add_playlist (boost::shared_ptr<ARDOUR::Playlist>);
	void remove_playlist (boost::weak_ptr<ARDOUR::Playlist>);

	uint32_t used() const;

	
	static sigc::signal<void,Source*> SourceCreated;

  protected:
	void update_length (nframes_t pos, nframes_t cnt);
	
	DataType  _type;
	time_t    _timestamp;
	nframes_t _length;

	Glib::Mutex playlist_lock;
	typedef std::map<boost::shared_ptr<ARDOUR::Playlist>, uint32_t > PlaylistMap;
	PlaylistMap _playlists;

  private:
	uint32_t _in_use;
};

}

#endif /* __ardour_source_h__ */
