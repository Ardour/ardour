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
#include <evoral/TimeConverter.hpp>

#include <ardour/ardour.h>
#include <ardour/session_object.h>
#include <ardour/data_type.h>
#include <ardour/readable.h>

namespace ARDOUR {

class Session;
class Playlist;

class Source : public SessionObject, public ARDOUR::Readable
{
  public:
	enum Flag {
		Writable = 0x1,
		CanRename = 0x2,
		Broadcast = 0x4,
		Removable = 0x8,
		RemovableIfEmpty = 0x10,
		RemoveAtDestroy = 0x20,
		NoPeakFile = 0x40,
		Destructive = 0x80
	};

	Source (Session&, const std::string& name, DataType type, Flag flags=Flag(0));
	Source (Session&, const XMLNode&);
	
	virtual ~Source ();
	
	DataType type() { return _type; }

	time_t timestamp() const { return _timestamp; }
	void stamp (time_t when) { _timestamp = when; }
	
	nframes_t length() const { return _length; }
	
	virtual Glib::ustring path() const = 0;

	virtual nframes_t natural_position() const { return 0; }

	virtual void mark_for_remove() = 0;
	virtual void mark_streaming_write_started () {}
	virtual void mark_streaming_write_completed () = 0;

	virtual void session_saved() {}
	
	XMLNode& get_state ();
	int set_state (const XMLNode&);
	
	virtual bool destructive()    const { return false; }
	virtual bool length_mutable() const { return false; }
	
	void use ()    { _in_use++; }
	void disuse () { if (_in_use) { _in_use--; } }
	
	void add_playlist (boost::shared_ptr<ARDOUR::Playlist>);
	void remove_playlist (boost::weak_ptr<ARDOUR::Playlist>);

	uint32_t used() const;

	static sigc::signal<void,Source*>             SourceCreated;
	sigc::signal<void,boost::shared_ptr<Source> > Switched;

	bool has_been_analysed() const;
	virtual bool can_be_analysed() const { return false; } 
	virtual void set_been_analysed (bool yn);
	virtual bool check_for_analysis_data_on_disk();

	sigc::signal<void> AnalysisChanged;
	
	AnalysisFeatureList transients;
	std::string get_transients_path() const;
	int load_transients (const std::string&);
	
	void update_length (nframes_t pos, nframes_t cnt);
	
	virtual const Evoral::TimeConverter<double, nframes_t>& time_converter() const {
		return Evoral::IdentityConverter<double, nframes_t>();
	}
	
	Flag flags() const { return _flags; }

  protected:
	DataType            _type;
	Flag                _flags;
	time_t              _timestamp;
	nframes_t           _length;
	bool                _analysed;
	mutable Glib::Mutex _analysis_lock;
	Glib::Mutex         _playlist_lock;
	
	typedef std::map<boost::shared_ptr<ARDOUR::Playlist>, uint32_t > PlaylistMap;
	PlaylistMap _playlists;

  private:
	uint32_t _in_use;
};

}

#endif /* __ardour_source_h__ */
