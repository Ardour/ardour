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

#ifndef __osc_osccueobserver_h__
#define __osc_osccueobserver_h__

#include <string>
#include <boost/shared_ptr.hpp>
#include <sigc++/sigc++.h>
#include <lo/lo.h>

#include "pbd/controllable.h"
#include "pbd/stateful.h"
#include "ardour/types.h"

class OSCCueObserver
{

  public:
	OSCCueObserver (boost::shared_ptr<ARDOUR::Stripable>, std::vector<boost::shared_ptr<ARDOUR::Stripable> >& sends, lo_address addr);
	~OSCCueObserver ();

	boost::shared_ptr<ARDOUR::Stripable> strip () const { return _strip; }
	lo_address address() const { return addr; };
	void tick (void);
	typedef std::vector<boost::shared_ptr<ARDOUR::Stripable> > Sorted;
	Sorted sends;

  private:

	boost::shared_ptr<ARDOUR::Stripable> _strip;

	PBD::ScopedConnectionList strip_connections;
	PBD::ScopedConnectionList send_connections;

	lo_address addr;
	std::string path;
	float _last_meter;
	std::vector<uint32_t> gain_timeout;
	bool tick_enable;
	std::vector<float> _last_gain;

	void name_changed (const PBD::PropertyChange& what_changed, uint32_t id);
	void send_change_message (std::string path, uint32_t id, boost::shared_ptr<PBD::Controllable> controllable);
	void text_with_id (std::string path, uint32_t id, std::string val);
	void send_gain_message (uint32_t id, boost::shared_ptr<PBD::Controllable> controllable);
	void send_enabled_message (std::string path, uint32_t id, boost::shared_ptr<ARDOUR::Processor> proc);
	void clear_strip (std::string path, float val);
	void send_init (void);
	void send_end (void);
	void send_restart (void);
};

#endif /* __osc_osccueobserver_h__ */
