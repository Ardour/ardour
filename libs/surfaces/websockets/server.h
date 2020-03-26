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

#ifndef _ardour_surface_websockets_server_h_
#define _ardour_surface_websockets_server_h_

#include <boost/unordered_map.hpp>
#include <glibmm.h>
#include <libwebsockets.h>

#if LWS_LIBRARY_VERSION_MAJOR < 3
// <libwebsockets.h> includes <uv.h> which in turn includes
// /usr/include/asm-generic/param.h on linux which defines HZ
// and conflicts with libs/ardour/ardour/parameter_descriptor.h
#undef HZ
#else
// also libwebsockets >=3 already includes integration with the glib event loop
// but ubuntu default repositories are stuck at version 2, hold until requiring
// version 3 in order to keep things simpler for the end user
#endif

#include "client.h"
#include "component.h"
#include "message.h"
#include "state.h"

#define WEBSOCKET_LISTEN_PORT 9000

struct LwsPollFdGlibSource {
	struct lws_pollfd             lws_pfd;
	Glib::RefPtr<Glib::IOChannel> g_channel;
	Glib::RefPtr<Glib::IOSource>  rg_iosrc;
	Glib::RefPtr<Glib::IOSource>  wg_iosrc;
};

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
	struct lws_protocols             _lws_proto[2];
	struct lws_context_creation_info _lws_info;
	struct lws_context*              _lws_context;

	Glib::RefPtr<Glib::IOChannel> _channel;

	typedef boost::unordered_map<lws_sockfd_type, LwsPollFdGlibSource> LwsPollFdGlibSourceMap;
	LwsPollFdGlibSourceMap                                             _fd_ctx;

	typedef boost::unordered_map<Client, ClientContext> ClientContextMap;
	ClientContextMap                                    _client_ctx;

	void add_poll_fd (struct lws_pollargs*);
	void mod_poll_fd (struct lws_pollargs*);
	void del_poll_fd (struct lws_pollargs*);

	void add_client (Client);
	void del_client (Client);
	void recv_client (Client, void* buf, size_t len);
	void write_client (Client);
	void reject_http_client (Client);

	bool io_handler (Glib::IOCondition, lws_sockfd_type);

	Glib::IOCondition events_to_ioc (int);
	int               ioc_to_events (Glib::IOCondition);

	static int lws_callback (struct lws*, enum lws_callback_reasons, void*, void*, size_t);
};

#endif // websockets_server_h
