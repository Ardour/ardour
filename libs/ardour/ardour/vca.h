/*
    Copyright (C) 2016 Paul Davis

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __ardour_vca_h__
#define __ardour_vca_h__

#include <string>
#include <boost/shared_ptr.hpp>

#include "pbd/controllable.h"
#include "pbd/statefuldestructible.h"

#include "ardour/session_handle.h"

namespace ARDOUR {

class GainControl;
class Route;

class LIBARDOUR_API VCA : public SessionHandleRef, public PBD::StatefulDestructible  {
  public:
	VCA (Session& session, const std::string& name, uint32_t num);

	std::string name() const { return _name; }
	uint32_t number () const { return _number; }

	void set_name (std::string const&);

	void set_value (double val, PBD::Controllable::GroupControlDisposition group_override);
	double get_value () const;

	boost::shared_ptr<GainControl> control() const { return _control; }

	void add (boost::shared_ptr<Route>);
	void remove (boost::shared_ptr<Route>);

	XMLNode& get_state();
	int set_state (XMLNode const&, int version);

	static std::string default_name_template ();
	static int next_vca_number ();
	static std::string xml_node_name;

  private:
	uint32_t    _number;
	std::string _name;
	boost::shared_ptr<GainControl> _control;

	static gint next_number;
};

} /* namespace */

#endif /* __ardour_vca_h__ */
