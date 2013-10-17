/*
    Copyright (C) 2000-2010 Paul Davis

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
#include "pbd/properties.h"

#include "ardour/ardour.h"
#include "ardour/session_handle.h"

namespace ARDOUR {

namespace Properties {
	extern PBD::PropertyDescriptor<std::string> name;
}

class Session;

/** A named object associated with a Session. Objects derived from this class are
    expected to be destroyed before the session calls drop_references().
 */

class LIBARDOUR_API SessionObject : public SessionHandleRef, public PBD::StatefulDestructible
{
  public:
	static void make_property_quarks ();

	SessionObject (Session& session, const std::string& name)
		: SessionHandleRef (session)
		, _name (Properties::name, name)
	{
		add_property (_name);
	}

	Session&    session() const { return _session; }
	std::string name()    const { return _name; }

	virtual bool set_name (const std::string& str) {
		if (_name != str) {
			_name = str;
			PropertyChanged (PBD::PropertyChange (Properties::name));
		}
		return true;
	}

  protected:
	PBD::Property<std::string> _name;
};

} // namespace ARDOUR

#endif /*__ardour_io_h__ */
