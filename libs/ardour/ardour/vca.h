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

#include "ardour/automatable.h"
#include "ardour/session_handle.h"

namespace ARDOUR {

class GainControl;
class Route;

class LIBARDOUR_API VCA : public SessionHandleRef, public PBD::StatefulDestructible, public Automatable {
  public:
	VCA (Session& session, const std::string& name, uint32_t num);
	VCA (Session& session, XMLNode const&, int version);
	~VCA();

	std::string name() const { return _name; }
	uint32_t number () const { return _number; }

	void set_name (std::string const&);

	void set_value (double val, PBD::Controllable::GroupControlDisposition group_override);
	double get_value () const;

	boost::shared_ptr<GainControl> control() const { return _control; }

	XMLNode& get_state();
	int set_state (XMLNode const&, int version);

	void add_solo_mute_target (boost::shared_ptr<Route>);
	void remove_solo_mute_target (boost::shared_ptr<Route>);

	void set_solo (bool yn);
	bool soloed () const;

	void set_mute (bool yn);
	bool muted () const;

	PBD::Signal0<void> SoloChange;
	PBD::Signal0<void> MuteChange;

	static std::string default_name_template ();
	static int next_vca_number ();
	static std::string xml_node_name;

  private:
	uint32_t    _number;
	std::string _name;
	boost::shared_ptr<GainControl> _control;
	RouteList solo_mute_targets;
	PBD::ScopedConnectionList solo_mute_connections;
	mutable Glib::Threads::RWLock solo_mute_lock;
	bool _solo_requested;
	bool _mute_requested;

	static gint next_number;

	void solo_mute_target_going_away (boost::weak_ptr<Route>);
	bool soloed_locked () const;
	bool muted_locked () const;

};

} /* namespace */

#endif /* __ardour_vca_h__ */
