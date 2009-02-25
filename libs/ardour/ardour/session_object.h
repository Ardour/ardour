/*
    Copyright (C) 2000-2007 Paul Davis 

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

#ifndef __ardour_session_object_h__
#define __ardour_session_object_h__

#include <string>
#include "pbd/statefuldestructible.h" 

namespace ARDOUR {

class Session;


/** An object associated with a Session.
 *
 * This is a few common things factored out of IO which weren't IO specific
 * (to fix the problem with e.g. PluginInsert being an IO which it shouldn't be).
 * collection of input and output ports with connections.
 */
class SessionObject : public PBD::StatefulDestructible
{
public:
	SessionObject(Session& session, const std::string& name)
		: _session(session)
		, _name(name)
	{}
	
	Session&           session() const { return _session; }
	const std::string& name()    const { return _name; }
	
	virtual bool set_name (const std::string& str) {
		if (_name != str) {
			_name = str;
			NameChanged();
		}
		return true;
	}
	
	sigc::signal<void> NameChanged;

protected:
	Session&    _session;
	std::string _name;
};

} // namespace ARDOUR

#endif /*__ardour_io_h__ */
