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

#ifndef _ardour_surface_websockets_component_h_
#define _ardour_surface_websockets_component_h_

#include <glibmm.h>

#include "ardour/session.h"
#include "pbd/event_loop.h"

namespace ArdourSurface
{
class ArdourWebsockets;
}

class ArdourStrips;
class ArdourGlobals;
class WebsocketsServer;
class WebsocketsDispatcher;

class SurfaceComponent
{
public:
	SurfaceComponent (ArdourSurface::ArdourWebsockets& surface)
	    : _surface (surface){};

	virtual ~SurfaceComponent (){};

	virtual int start ()
	{
		return 0;
	}
	virtual int stop ()
	{
		return 0;
	}

	PBD::EventLoop*              event_loop () const;
	Glib::RefPtr<Glib::MainLoop> main_loop () const;
	ARDOUR::Session&             session () const;
	ArdourStrips&                strips () const;
	ArdourGlobals&               globals () const;
	WebsocketsServer&            server () const;
	WebsocketsDispatcher&        dispatcher () const;

protected:
	ArdourSurface::ArdourWebsockets& _surface;
};

#endif // _ardour_surface_websockets_component_h_
