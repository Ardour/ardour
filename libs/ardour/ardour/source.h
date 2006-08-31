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

    $Id$
*/

#ifndef __ardour_source_h__
#define __ardour_source_h__

#include <string>

#include <sigc++/signal.h>

#include <pbd/statefuldestructible.h> 

#include <ardour/ardour.h>
#include <ardour/data_type.h>

namespace ARDOUR {

class Source : public PBD::StatefulDestructible, public sigc::trackable
{
  public:
	Source (std::string name, DataType type);
	Source (const XMLNode&);
	virtual ~Source ();

	std::string name() const { return _name; }
	int set_name (std::string str, bool destructive);

	DataType type() { return _type; }

	const PBD::ID&  id() const   { return _id; }

	time_t timestamp() const { return _timestamp; }
	void stamp (time_t when) { _timestamp = when; }
	
	/** @return the number of items in this source */
	jack_nframes_t length() const { return _length; }

	virtual jack_nframes_t natural_position() const { return 0; }

	virtual void mark_for_remove() = 0;
	virtual void mark_streaming_write_completed () = 0;

	XMLNode& get_state ();
	int set_state (const XMLNode&);
	
	static sigc::signal<void,Source*> SourceCreated;

  protected:
	void update_length (jack_nframes_t pos, jack_nframes_t cnt);

	string            _name;
	DataType          _type;
	time_t            _timestamp;
	jack_nframes_t    _length;

  private:
	PBD::ID _id;
};

}

#endif /* __ardour_source_h__ */
