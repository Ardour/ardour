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

#include "component.h"
#include "ardour_websockets.h"

using namespace ArdourSurface;

BasicUI&
SurfaceComponent::basic_ui () const
{
	return _surface;
}

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

ArdourMixer&
SurfaceComponent::mixer () const
{
	return _surface.mixer_component ();
}

ArdourTransport&
SurfaceComponent::transport () const
{
	return _surface.transport_component ();
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
