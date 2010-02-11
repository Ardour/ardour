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
#include "pbd/signals.h"

#include "ardour/session_handle.h"

#include "i18n.h"

namespace ARDOUR {

class Session;

/** A named object associated with a Session. Objects derived from this class are
    expected to be destroyed before the session calls drop_references().
 */

class SessionObject : public SessionHandleRef, public PBD::StatefulDestructible
{
  public:
	SessionObject (Session& session, const std::string& name)
		: SessionHandleRef (session)
		, _name (X_("name"), PBD::Change (0), name)
	{
		add_state (_name);
	}
	
	Session&    session() const { return _session; }
	std::string name()    const { return _name; }

	virtual bool set_name (const std::string& str) {
		if (_name != str) {
			_name = str;
			NameChanged();
		}
		return true;
	}

	PBD::Signal0<void> NameChanged;

  protected:
	PBD::State<std::string> _name;
};

} // namespace ARDOUR

#endif /*__ardour_io_h__ */
