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
#include "ardour/processor.h"

#include "osc.h"

class OSCSelectObserver
{

  public:
	OSCSelectObserver (ArdourSurface::OSC& o, ARDOUR::Session& s, ArdourSurface::OSC::OSCSurface* sur);
	~OSCSelectObserver ();

	boost::shared_ptr<ARDOUR::Stripable> strip () const { return _strip; }
	lo_address address() const { return addr; };
	void tick (void);
	void renew_sends (void);
	void renew_plugin (void);
	void eq_restart (int);
	void clear_observer (void);
	void refresh_strip (boost::shared_ptr<ARDOUR::Stripable> new_strip, uint32_t nsends, uint32_t g_mode, bool force);
	void set_expand (uint32_t expand);
	void set_send_page (uint32_t page);
	void set_send_size (uint32_t size);
	void set_plugin_id (int id, uint32_t page);
	void set_plugin_page (uint32_t page);
	void set_plugin_size (uint32_t size);

  private:
	boost::shared_ptr<ARDOUR::Stripable> _strip;
	ArdourSurface::OSC& _osc;

	PBD::ScopedConnectionList strip_connections;
	// pans, sends, plugins and eq need their own
	PBD::ScopedConnectionList pan_connections;
	PBD::ScopedConnectionList send_connections;
	PBD::ScopedConnectionList plugin_connections;
	PBD::ScopedConnectionList eq_connections;

	lo_address addr;
	std::string path;
	uint32_t gainmode;
	std::bitset<32> feedback;
	bool in_line;
	ArdourSurface::OSC::OSCSurface* sur;
	std::vector<int> send_timeout;
	uint32_t gain_timeout;
	float _last_meter;
	uint32_t nsends;
	float _last_gain;
	float _last_trim;
	std::vector<float> _last_send;
	bool _init;
	float _comp_redux;
	ARDOUR::AutoState as;
	uint32_t send_page_size;
	uint32_t send_size;
	uint32_t send_page;

	uint32_t nplug_params;
	uint32_t plug_page_size;
	uint32_t plug_page;
	int plug_id;
	uint32_t plug_size;
	std::vector<int> plug_params;
	int eq_bands;
	uint32_t _expand;
	std::bitset<16> _group_sharing;
	bool _tick_busy;
	ARDOUR::Session* session;

	void name_changed (const PBD::PropertyChange& what_changed);
	void panner_changed ();
	void group_name ();
	void group_sharing (ARDOUR::RouteGroup *rg_c);
	void comment_changed ();
	void pi_changed (PBD::PropertyChange const&);
	void change_message (std::string path, boost::shared_ptr<PBD::Controllable> controllable);
	void enable_message (std::string path, boost::shared_ptr<PBD::Controllable> controllable);
	void comp_mode (void);
	void change_message_with_id (std::string path, uint32_t id, boost::shared_ptr<PBD::Controllable> controllable);
	void enable_message_with_id (std::string path, uint32_t id, boost::shared_ptr<PBD::Controllable> controllable);
	void monitor_status (boost::shared_ptr<PBD::Controllable> controllable);
	void gain_message ();
	void gain_automation ();
	void send_automation (std::string path, boost::shared_ptr<PBD::Controllable> control);
	void trim_message (std::string path, boost::shared_ptr<PBD::Controllable> controllable);
	// sends stuff
	void send_init (void);
	void send_end (void);
	void plugin_init (void);
	void plugin_end (void);
	void plugin_parameter_changed (int pid, bool swtch, boost::shared_ptr<PBD::Controllable> controllable);
	void send_gain (uint32_t id, boost::shared_ptr<PBD::Controllable> controllable);
	void send_enable (std::string path, uint32_t id, boost::shared_ptr<ARDOUR::Processor> proc);
	void plug_enable (std::string path, boost::shared_ptr<ARDOUR::Processor> proc);
	void eq_init (void);
	void eq_end (void);
	void no_strip ();
	void slaved_changed (boost::shared_ptr<ARDOUR::VCA> vca, bool state);
};

#endif /* __osc_oscselectobserver_h__ */
