/*
 * Copyright (C) 2020-2021 Luciano Iam <oss@lucianoiam.com>
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

#ifndef _ardour_surface_websockets_server_h_
#define _ardour_surface_websockets_server_h_

#include <boost/unordered_map.hpp>
#include <glibmm.h>
#include <libwebsockets.h>

#if LWS_LIBRARY_VERSION_MAJOR < 3
// older <libwebsockets.h> includes <uv.h> which in turn includes
// /usr/include/asm-generic/param.h on linux which defines HZ
// and conflicts with libs/ardour/ardour/parameter_descriptor.h
#undef HZ
#endif

#include "client.h"
#include "component.h"
#include "message.h"
#include "state.h"
#include "resources.h"

// TO DO: make this configurable
#define WEBSOCKET_LISTEN_PORT 3818

namespace ArdourSurface {

class WebsocketsServer : public SurfaceComponent
{
public:
	WebsocketsServer (ArdourSurface::ArdourWebsockets&);
	virtual ~WebsocketsServer (){};

	int start ();
	int stop ();

	void update_client (Client, const NodeState&, bool);
	void update_all_clients (const NodeState&, bool);

private:
#if LWS_LIBRARY_VERSION_MAJOR < 3
	struct lws_protocol_vhost_options _lws_vhost_opt;
#endif
	struct lws_protocols              _lws_proto[2];
	struct lws_http_mount             _lws_mnt_root;
	struct lws_http_mount             _lws_mnt_user;
	struct lws_context_creation_info  _lws_info;
	struct lws_context*               _lws_context;

	typedef boost::unordered_map<Client, ClientContext> ClientContextMap;
	ClientContextMap                                    _client_ctx;

	ServerResources _resources;

	int add_client (Client);
	int del_client (Client);
	int recv_client (Client, void*, size_t);
	int write_client (Client);
	int send_availsurf_hdr (Client);
	int send_availsurf_body (Client);

	void request_write (Client);

	static int lws_callback (struct lws*, enum lws_callback_reasons, void*, void*, size_t);

	/* Glib event loop integration that requires LWS_WITH_EXTERNAL_POLL */

	struct LwsPollFdGlibSource {
		struct lws_pollfd             lws_pfd;
		Glib::RefPtr<Glib::IOChannel> g_channel;
		Glib::RefPtr<Glib::IOSource>  rg_iosrc;
		Glib::RefPtr<Glib::IOSource>  wg_iosrc;
	};

	Glib::RefPtr<Glib::IOChannel> _channel;

	typedef boost::unordered_map<lws_sockfd_type, LwsPollFdGlibSource> LwsPollFdGlibSourceMap;
	LwsPollFdGlibSourceMap _fd_ctx;

	bool _fd_callbacks;

	bool fd_callbacks () { return _fd_callbacks; }

	int add_poll_fd (struct lws_pollargs*);
	int mod_poll_fd (struct lws_pollargs*);
	int del_poll_fd (struct lws_pollargs*);

	bool io_handler (Glib::IOCondition, lws_sockfd_type);

	Glib::IOCondition events_to_ioc (int);
	int               ioc_to_events (Glib::IOCondition);

	/* Glib event loop integration that does NOT require LWS_WITH_EXTERNAL_POLL
	   but needs a secondary thread for notifying the server when there is
	   pending data for writing. Unfortunately libwesockets' own approach to
	   Glib integration cannot be copied because it relies on file descriptors
	   that are hidden by the 'lws' opaque type. See feedback.cc . */

	GSource* _g_source;

	static gboolean glib_idle_callback (void *);

public:
	bool read_blocks_event_loop () { return _g_source != 0; }

};

} // namespace ArdourSurface

#endif // _ardour_surface_websockets_server_h_
