/*
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2012-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2016 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015-2018 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2015 Johannes Mueller <github@johannes-mueller.org>
 * Copyright (C) 2016-2018 Len Ovens <len@ovenwerks.net>
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

#include "ardour/session.h"

#include "ardour_websockets.h"

using namespace ARDOUR;
using namespace ArdourSurface;

#include "pbd/abstract_ui.cc" // instantiate template

ArdourWebsockets::ArdourWebsockets (Session& s)
    : ControlProtocol (s, X_ (surface_name))
    , AbstractUI<ArdourWebsocketsUIRequest> (name ())
    , _mixer (*this)
    , _transport (*this)
    , _server (*this)
    , _feedback (*this)
    , _dispatcher (*this)
{
	_components.push_back (&_mixer);
	_components.push_back (&_transport);
	_components.push_back (&_server);
	_components.push_back (&_feedback);
	_components.push_back (&_dispatcher);
}

ArdourWebsockets::~ArdourWebsockets ()
{
	stop ();
}

void*
ArdourWebsockets::request_factory (uint32_t num_requests)
{
	/* AbstractUI<T>::request_buffer_factory() is a template method only
       instantiated in this source module. To provide something visible for
       use in the interface/descriptor, we have this static method that is
       template-free.
    */
	return request_buffer_factory (num_requests);
}

int
ArdourWebsockets::set_active (bool yn)
{
	if (yn != active ()) {
		if (yn) {
			if (start ()) {
				return -1;
			}
		} else {
			if (stop ()) {
				return -1;
			}
		}
	}

	return ControlProtocol::set_active (yn);
}

void
ArdourWebsockets::thread_init ()
{
	pthread_set_name (event_loop_name ().c_str ());
	PBD::notify_event_loops_about_thread_creation (pthread_self (), event_loop_name (), 2048);
	SessionEvent::create_per_thread_pool (event_loop_name (), 128);
}

void
ArdourWebsockets::do_request (ArdourWebsocketsUIRequest* req)
{
	if (req->type == CallSlot) {
		call_slot (MISSING_INVALIDATOR, req->the_slot);
	} else if (req->type == Quit) {
		stop ();
	}
}

int
ArdourWebsockets::start ()
{
	/* startup the event loop thread */
	BaseUI::run ();

	for (std::vector<SurfaceComponent*>::iterator it = _components.begin ();
	     it != _components.end (); ++it) {
		int rc = (*it)->start ();
		if (rc != 0) {
			BaseUI::quit ();
			return -1;
		}
	}

	PBD::info << "ArdourWebsockets: started" << endmsg;

	return 0;
}

int
ArdourWebsockets::stop ()
{
	for (std::vector<SurfaceComponent*>::iterator it = _components.begin ();
	     it != _components.end (); ++it) {
		(*it)->stop ();
	}

	BaseUI::quit ();

	PBD::info << "ArdourWebsockets: stopped" << endmsg;

	return 0;
}
