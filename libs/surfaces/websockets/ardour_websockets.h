/*
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2013-2016 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Johannes Mueller <github@johannes-mueller.org>
 * Copyright (C) 2016-2018 Len Ovens <len@ovenwerks.net>
 * Copyright (C) 2016 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2020 Luciano Iam <oss@lucianoiam.com>
 
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

#ifndef _ardour_surface_websockets_h_
#define _ardour_surface_websockets_h_

#define ABSTRACT_UI_EXPORTS
#include "pbd/abstract_ui.h"

#include "ardour/plugin.h"
#include "control_protocol/control_protocol.h"

#include "component.h"
#include "dispatcher.h"
#include "feedback.h"
#include "transport.h"
#include "server.h"
#include "mixer.h"

namespace ArdourSurface {

const char * const surface_name = "WebSockets Server (Experimental)";
const char * const surface_id = "uri://ardour.org/surfaces/ardour_websockets:0";

struct ArdourWebsocketsUIRequest : public BaseUI::BaseRequestObject {
public:
	ArdourWebsocketsUIRequest () {}
	~ArdourWebsocketsUIRequest () {}
};

class ArdourWebsockets : public ARDOUR::ControlProtocol,
                         public AbstractUI<ArdourWebsocketsUIRequest>
{
public:
	ArdourWebsockets (ARDOUR::Session&);
	virtual ~ArdourWebsockets ();

	static void* request_factory (uint32_t);

	int set_active (bool);

	ARDOUR::Session& ardour_session ()
	{
		return *session;
	}
	ArdourMixer& mixer_component ()
	{
		return _mixer;
	}
	ArdourTransport& transport_component ()
	{
		return _transport;
	}
	WebsocketsServer& server_component ()
	{
		return _server;
	}
	WebsocketsDispatcher& dispatcher_component ()
	{
		return _dispatcher;
	}

	/* ControlProtocol */
	void stripable_selection_changed () {}

protected:
	/* BaseUI */
	void thread_init ();

	/* AbstractUI */
	void do_request (ArdourWebsocketsUIRequest*);

private:
	ArdourMixer                    _mixer;
	ArdourTransport                _transport;
	WebsocketsServer               _server;
	ArdourFeedback                 _feedback;
	WebsocketsDispatcher           _dispatcher;
	std::vector<SurfaceComponent*> _components;

	int start ();
	int stop ();
};

} // namespace ArdourSurface

#endif // _ardour_surface_websockets_h_
