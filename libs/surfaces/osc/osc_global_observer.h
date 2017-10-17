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
	OSCGlobalObserver (ArdourSurface::OSC& o, ARDOUR::Session& s, ArdourSurface::OSC::OSCSurface* su);
	~OSCGlobalObserver ();

	lo_address address() const { return addr; };
	void tick (void);
	void clear_observer (void);

  private:
	ArdourSurface::OSC& _osc;

	PBD::ScopedConnectionList strip_connections;
	PBD::ScopedConnectionList session_connections;

	enum STRIP {
		Master,
		Monitor,
	};

	ArdourSurface::OSC::OSCSurface* sur;
	bool _init;
	float _last_master_gain;
	float _last_master_trim;
	float _last_monitor_gain;
	lo_address addr;
	std::string path;
	uint32_t gainmode;
	std::bitset<32> feedback;
	ARDOUR::Session* session;
	samplepos_t _last_sample;
	uint32_t _heartbeat;
	float _last_meter;
	uint32_t master_timeout;
	uint32_t monitor_timeout;

	void send_change_message (std::string path, boost::shared_ptr<PBD::Controllable> controllable);
	void send_gain_message (std::string path, boost::shared_ptr<PBD::Controllable> controllable);
	void send_trim_message (std::string path, boost::shared_ptr<PBD::Controllable> controllable);
	void send_transport_state_changed (void);
	void send_record_state_changed (void);
	void solo_active (bool active);
	void session_name (std::string path, std::string name);
};

#endif /* __osc_oscglobalobserver_h__ */
