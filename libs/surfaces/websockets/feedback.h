/*
 * Copyright (C) 2020 Luciano Iam <lucianito@gmail.com>
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

#ifndef _ardour_surface_websockets_feedback_h_
#define _ardour_surface_websockets_feedback_h_

#include <boost/shared_ptr.hpp>
#include <glibmm/main.h>

#include "component.h"
#include "typed_value.h"

class ArdourFeedback : public SurfaceComponent
{
public:
	ArdourFeedback (ArdourSurface::ArdourWebsockets& surface)
	    : SurfaceComponent (surface){};
	virtual ~ArdourFeedback (){};

	int start ();
	int stop ();

	void update_all (std::string, TypedValue) const;
	void update_all (std::string, uint32_t, TypedValue) const;
	void update_all (std::string, uint32_t, uint32_t, TypedValue) const;
	void update_all (std::string, uint32_t, uint32_t, uint32_t, TypedValue) const;

private:
	Glib::Threads::Mutex      _client_state_lock;
	PBD::ScopedConnectionList _signal_connections;
	sigc::connection          _periodic_connection;

	bool poll () const;

	void observe_globals ();
	void observe_strips ();
	void observe_strip_plugins (uint32_t, boost::shared_ptr<ARDOUR::Stripable>);
	void observe_strip_plugin_param_values (uint32_t, uint32_t,
	                                        boost::shared_ptr<ARDOUR::PluginInsert>);
};

#endif // ardour_feedback_h
