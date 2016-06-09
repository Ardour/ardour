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

#ifndef __osc_oscselectobserver_h__
#define __osc_oscselectobserver_h__

#include <string>
#include <bitset>
#include <boost/shared_ptr.hpp>
#include <sigc++/sigc++.h>
#include <lo/lo.h>

#include "pbd/controllable.h"
#include "pbd/stateful.h"
#include "ardour/types.h"

class OSCSelectObserver
{

  public:
	OSCSelectObserver (boost::shared_ptr<ARDOUR::Stripable>, lo_address addr, uint32_t sid, uint32_t gainmode, std::bitset<32> feedback);
	~OSCSelectObserver ();

	boost::shared_ptr<ARDOUR::Stripable> strip () const { return _strip; }
	lo_address address() const { return addr; };
	void tick (void);

  private:
	boost::shared_ptr<ARDOUR::Stripable> _strip;

	PBD::ScopedConnectionList strip_connections;
	// sends need their own
	PBD::ScopedConnectionList send_connections;

	lo_address addr;
	std::string path;
	uint32_t ssid;
	uint32_t gainmode;
	std::bitset<32> feedback;
	float _last_meter;
	uint32_t nsends;


	void name_changed (const PBD::PropertyChange& what_changed);
	void send_change_message (std::string path, boost::shared_ptr<PBD::Controllable> controllable);
	void send_monitor_status (boost::shared_ptr<PBD::Controllable> controllable);
	void send_gain_message (std::string path, boost::shared_ptr<PBD::Controllable> controllable);
	void send_trim_message (std::string path, boost::shared_ptr<PBD::Controllable> controllable);
	// sends stuff
	void send_init (void);
	void send_end (void);
	void send_restart (int);
	void send_gain (std::string path, uint32_t id, boost::shared_ptr<PBD::Controllable> controllable);
	void send_enable (std::string path, uint32_t id, boost::shared_ptr<PBD::Controllable> controllable);
	void send_rename (std::string path, uint32_t id, std::string name);
	std::string set_path (std::string path, uint32_t id);
	void clear_strip (std::string path, float val);
};

#endif /* __osc_oscselectobserver_h__ */
