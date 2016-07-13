/*
    Copyright (C) 2009 Paul Davis

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

#ifndef __osc_oscglobalobserver_h__
#define __osc_oscglobalobserver_h__

#include <string>
#include <boost/shared_ptr.hpp>
#include <sigc++/sigc++.h>
#include <lo/lo.h>

#include "pbd/controllable.h"
#include "pbd/stateful.h"
#include "ardour/types.h"

class OSCGlobalObserver
{

  public:
	OSCGlobalObserver (ARDOUR::Session& s, lo_address addr, uint32_t gainmode, std::bitset<32> feedback);
	~OSCGlobalObserver ();

	lo_address address() const { return addr; };
	void tick (void);

  private:

	PBD::ScopedConnectionList strip_connections;
	PBD::ScopedConnectionList session_connections;


	lo_address addr;
	std::string path;
	uint32_t gainmode;
	std::bitset<32> feedback;
	ARDOUR::Session* session;
	framepos_t _last_frame;
	uint32_t _heartbeat;
	float _last_meter;

	void send_change_message (std::string path, boost::shared_ptr<PBD::Controllable> controllable);
	void send_gain_message (std::string path, boost::shared_ptr<PBD::Controllable> controllable);
	void send_trim_message (std::string path, boost::shared_ptr<PBD::Controllable> controllable);
	void send_transport_state_changed(void);
	void send_record_state_changed (void);
	void send_session_saved (std::string name);
	void solo_active (bool active);
};

#endif /* __osc_oscglobalobserver_h__ */
