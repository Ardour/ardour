/*
 * Copyright (C) 2016-2018 Len Ovens <len@ovenwerks.net>
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
	void jog_mode (uint32_t jogmode);

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
	std::string mark_text;
	uint32_t gainmode;
	std::bitset<32> feedback;
	ARDOUR::Session* session;
	uint32_t _jog_mode;
	samplepos_t _last_sample;
	uint32_t _heartbeat;
	float _last_meter;
	uint32_t master_timeout;
	uint32_t monitor_timeout;
	uint32_t last_punchin;
	uint32_t last_punchout;
	uint32_t last_click;
	samplepos_t prev_mark;
	samplepos_t next_mark;
	struct LocationMarker {
		LocationMarker (const std::string& l, samplepos_t w)
			: label (l), when (w) {}
		std::string label;
		samplepos_t  when;
	};
	std::vector<LocationMarker> lm;

	struct LocationMarkerSort {
		bool operator() (const LocationMarker& a, const LocationMarker& b) {
			return (a.when < b.when);
		}
	};


	void send_change_message (std::string path, boost::shared_ptr<PBD::Controllable> controllable);
	void send_gain_message (std::string path, boost::shared_ptr<PBD::Controllable> controllable);
	void send_trim_message (std::string path, boost::shared_ptr<PBD::Controllable> controllable);
	void send_transport_state_changed (void);
	void send_record_state_changed (void);
	void solo_active (bool active);
	void session_name (std::string path, std::string name);
	void extra_check (void);
	void marks_changed (void);
	void mark_update (void);
	void group_changed (ARDOUR::RouteGroup*);
	void group_changed (void);
};

#endif /* __osc_oscglobalobserver_h__ */
