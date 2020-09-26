/*
 * Copyright (C) 2007-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2013 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
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
	LIBARDOUR_API extern PBD::PropertyDescriptor<std::string> name;
	/* this has no inherent connection to SessionObject, but it needs to go * somewhere so .... */
	LIBARDOUR_API extern PBD::PropertyDescriptor<Temporal::TimeDomain> time_domain;
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
