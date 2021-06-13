/*
 * Copyright (C) 2020 Luciano Iam <oss@lucianoiam.com>
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

#include "control_protocol/basic_ui.h"

#include <glibmm.h>

#include "ardour/session.h"
#include "pbd/event_loop.h"

namespace ArdourSurface
{

class ArdourWebsockets;
class ArdourMixer;
class ArdourTransport;
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

	BasicUI&                     basic_ui () const;
	virtual PBD::EventLoop*      event_loop () const;
	Glib::RefPtr<Glib::MainLoop> main_loop () const;
	ARDOUR::Session&             session () const;
	ArdourMixer&                 mixer () const;
	ArdourTransport&             transport () const;
	WebsocketsServer&            server () const;
	WebsocketsDispatcher&        dispatcher () const;

protected:
	ArdourSurface::ArdourWebsockets& _surface;
};

} // namespace ArdourSurface

#endif // _ardour_surface_websockets_component_h_
