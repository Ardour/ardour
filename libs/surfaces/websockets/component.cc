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

#include "component.h"
#include "ardour_websockets.h"

PBD::EventLoop*
SurfaceComponent::event_loop () const
{
	return static_cast<PBD::EventLoop*> (&_surface);
}

Glib::RefPtr<Glib::MainLoop>
SurfaceComponent::main_loop () const
{
	return _surface.main_loop ();
}

ARDOUR::Session&
SurfaceComponent::session () const
{
	return _surface.ardour_session ();
}

ArdourStrips&
SurfaceComponent::strips () const
{
	return _surface.strips_component ();
}

ArdourGlobals&
SurfaceComponent::globals () const
{
	return _surface.globals_component ();
}

WebsocketsServer&
SurfaceComponent::server () const
{
	return _surface.server_component ();
}

WebsocketsDispatcher&
SurfaceComponent::dispatcher () const
{
	return _surface.dispatcher_component ();
}
