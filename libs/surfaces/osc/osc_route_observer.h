/*
 * Copyright (C) 2010-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016-2020 Len Ovens <len@ovenwerks.net>
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

#ifndef __osc_oscrouteobserver_h__
#define __osc_oscrouteobserver_h__

#include <string>
#include <bitset>
#include <boost/shared_ptr.hpp>
#include <sigc++/sigc++.h>
#include <lo/lo.h>

#include "pbd/controllable.h"
#include "pbd/stateful.h"
#include "ardour/types.h"
#include "ardour/panner_shell.h"

#include "osc.h"

class OSCRouteObserver
{

  public:
	OSCRouteObserver (ArdourSurface::OSC& o, uint32_t sid, ArdourSurface::OSC::OSCSurface* sur);
	~OSCRouteObserver ();

	boost::shared_ptr<ARDOUR::Stripable> strip () const { return _strip; }
	uint32_t strip_id () const { return ssid; }
	lo_address address () const { return addr; };
	void tick (void);
	void send_select_status (const PBD::PropertyChange&);
	void refresh_strip (boost::shared_ptr<ARDOUR::Stripable> strip, bool force);
	void refresh_send (boost::shared_ptr<ARDOUR::Send> send, bool force);
	void set_expand (uint32_t expand);
	void set_link_ready (uint32_t not_ready);
	void clear_strip ();

  private:
	boost::shared_ptr<ARDOUR::Stripable> _strip;
	boost::shared_ptr<ARDOUR::Send> _send;
	boost::shared_ptr<ARDOUR::GainControl> _gain_control;

	PBD::ScopedConnectionList strip_connections;
	PBD::ScopedConnectionList pan_connections;

	ArdourSurface::OSC& _osc;
	lo_address addr;
	std::string path;
	uint32_t gainmode;
	std::bitset<32> feedback;
	uint32_t ssid;
	ArdourSurface::OSC::OSCSurface* sur;
	float _last_meter;
	uint32_t gain_timeout;
	float _last_gain;
	float _last_trim;
	bool _init;
	uint32_t _expand;
	bool in_line;
	ARDOUR::AutoState as;
	bool _tick_busy;
	boost::shared_ptr<ARDOUR::PannerShell> current_pan_shell;

	void send_clear ();
	void name_changed (const PBD::PropertyChange& what_changed);
	void panner_changed (boost::shared_ptr<ARDOUR::PannerShell>);
	void group_name ();
	void pi_changed (PBD::PropertyChange const&);
	void send_change_message (std::string path, boost::shared_ptr<PBD::Controllable> controllable);
	void send_monitor_status (boost::shared_ptr<PBD::Controllable> controllable);
	void send_gain_message ();
	void gain_automation ();
	void send_automation (std::string path, boost::shared_ptr<PBD::Controllable> control);
	void send_trim_message ();
	void no_strip ();
};

#endif /* __osc_oscrouteobserver_h__ */
