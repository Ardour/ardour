/*
    Copyright (C) 2016 Paul Davis

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

#ifndef __ardour_slavable_h__
#define __ardour_slavable_h__

#include <set>
#include <string>
#include <stdint.h>

#include <boost/shared_ptr.hpp>

#include "pbd/signals.h"

#include "evoral/Parameter.hpp"

#include "ardour/libardour_visibility.h"

class XMLNode;

namespace ARDOUR {

class VCA;
class VCAManager;
class AutomationControl;

class LIBARDOUR_API Slavable
{
    public:
	Slavable ();
	virtual ~Slavable() {}

	XMLNode& get_state () const;
	int set_state (XMLNode const&, int);

	void assign (boost::shared_ptr<VCA>, bool loading);
	void unassign (boost::shared_ptr<VCA>);

	PBD::Signal2<void,boost::shared_ptr<VCA>,bool> AssignmentChange;

	virtual boost::shared_ptr<AutomationControl> automation_control (const Evoral::Parameter& id) = 0;

	static std::string xml_node_name;

	/* signal sent VCAManager once assignment is possible */
	static PBD::Signal1<void,VCAManager*> Assign;

    protected:
	virtual int assign_controls (boost::shared_ptr<VCA>, bool loading);
	virtual int unassign_controls (boost::shared_ptr<VCA>);

    private:
	mutable Glib::Threads::RWLock master_lock;
	std::set<uint32_t> _masters;
	PBD::ScopedConnection assign_connection;
	PBD::ScopedConnectionList unassign_connections;

	int do_assign (VCAManager* s);
	void weak_unassign (boost::weak_ptr<VCA>);

};

} // namespace ARDOUR

#endif /* __ardour_slavable_h__ */
