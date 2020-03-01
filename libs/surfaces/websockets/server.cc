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
 * You should have received a copy of the/GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef NDEBUG
#include <iostream>
#endif

#include "dispatcher.h"
#include "server.h"

/* backport from libwebsockets 3.0,
 * allow to compile on GNU/Linux with libwebsockets 2.x
 */
#ifndef PLATFORM_WINDOWS
#ifndef LWS_POLLHUP
#define LWS_POLLHUP (POLLHUP | POLLERR)
#endif
#ifndef LWS_POLLIN
#define LWS_POLLIN (POLLIN)
#endif
#ifndef LWS_POLLOUT
#define LWS_POLLOUT (POLLOUT)
#endif
#endif

using namespace Glib;

WebsocketsServer::WebsocketsServer (ArdourSurface::ArdourWebsockets& surface)
    : SurfaceComponent (surface)
    , _lws_context (0)
{
	/* keep references to all config for libwebsockets 2 */
	lws_protocols proto;
	memset (&proto, 0, sizeof (lws_protocols));
	proto.name                  = "lws-ardour";
	proto.callback              = WebsocketsServer::lws_callback;
	proto.per_session_data_size = 0;
	proto.rx_buffer_size        = 0;
	proto.id                    = 0;
	proto.user                  = 0;
#if LWS_LIBRARY_VERSION_MAJOR >= 3
	proto.tx_packet_size = 0;
#endif
	_lws_proto[0] = proto;
	memset (&_lws_proto[1], 0, sizeof (lws_protocols));

	memset (&_lws_info, 0, sizeof (lws_context_creation_info));
	_lws_info.port      = WEBSOCKET_LISTEN_PORT;
	_lws_info.protocols = _lws_proto;
	_lws_info.uid       = -1;
	_lws_info.gid       = -1;
	_lws_info.user      = this;
}

int
WebsocketsServer::start ()
{
	_lws_context = lws_create_context (&_lws_info);

	if (!_lws_context) {
		PBD::error << "ArdourWebsockets: could not create libwebsockets context" << endmsg;
		return -1;
	}

	/* add_poll_fd() should have been called once during lws_create_context()
	 * if _fd_ctx is empty then LWS_CALLBACK_ADD_POLL_FD was not called
	 * this means libwesockets was not compiled with LWS_WITH_EXTERNAL_POLL
	 * - macos homebrew libwebsockets: disabled (3.2.2 as of Feb 2020)
	 * - linux ubuntu libwebsockets-dev: enabled (2.0.3 as of Feb 2020) but
	 *   #if defined(LWS_WITH_EXTERNAL_POLL) check is not reliable -- constant
	 *   missing from /usr/include/lws_config.h
	 */

	if (_fd_ctx.empty ()) {
		PBD::error << "ArdourWebsockets: check your libwebsockets was compiled"
		              " with LWS_WITH_EXTERNAL_POLL enabled"
		           << endmsg;
		return -1;
	}

	return 0;
}

int
WebsocketsServer::stop ()
{
	for (LwsPollFdGlibSourceMap::iterator it = _fd_ctx.begin (); it != _fd_ctx.end (); ++it) {
		it->second.rg_iosrc->destroy ();

		if (it->second.wg_iosrc) {
			it->second.wg_iosrc->destroy ();
		}
	}

	_fd_ctx.clear ();

	if (_lws_context) {
		lws_context_destroy (_lws_context);
		_lws_context = 0;
	}

	return 0;
}

void
WebsocketsServer::update_client (Client wsi, const NodeState& state, bool force)
{
	ClientContextMap::iterator it = _client_ctx.find (wsi);
	if (it == _client_ctx.end ()) {
		return;
	}

	if (force || !it->second.has_state (state)) {
		/* write to client only if state was updated */
		it->second.update_state (state);
		it->second.output_buf ().push_back (NodeStateMessage (state));
		lws_callback_on_writable (wsi);
	}
}

void
WebsocketsServer::update_all_clients (const NodeState& state, bool force)
{
	for (ClientContextMap::iterator it = _client_ctx.begin (); it != _client_ctx.end (); ++it) {
		update_client (it->second.wsi (), state, force);
	}
}

void
WebsocketsServer::add_poll_fd (struct lws_pollargs* pa)
{
	/* fd can be SOCKET or int depending platform */
	lws_sockfd_type fd = pa->fd;

#ifdef PLATFORM_WINDOWS
	RefPtr<IOChannel> g_channel = IOChannel::create_from_win32_socket (fd);
#else
	RefPtr<IOChannel> g_channel = IOChannel::create_from_fd (fd);
#endif
	RefPtr<IOSource> rg_iosrc (IOSource::create (g_channel, events_to_ioc (pa->events)));
	rg_iosrc->connect (sigc::bind (sigc::mem_fun (*this, &WebsocketsServer::io_handler), fd));
	rg_iosrc->attach (main_loop ()->get_context ());

	struct lws_pollfd lws_pfd;
	lws_pfd.fd      = pa->fd;
	lws_pfd.events  = pa->events;
	lws_pfd.revents = 0;

	LwsPollFdGlibSource ctx;
	ctx.lws_pfd   = lws_pfd;
	ctx.g_channel = g_channel;
	ctx.rg_iosrc  = rg_iosrc;
	ctx.wg_iosrc  = Glib::RefPtr<Glib::IOSource> (0);

	_fd_ctx[fd] = ctx;
}

void
WebsocketsServer::mod_poll_fd (struct lws_pollargs* pa)
{
	LwsPollFdGlibSourceMap::iterator it = _fd_ctx.find (pa->fd);
	if (it == _fd_ctx.end ()) {
		return;
	}

	it->second.lws_pfd.events = pa->events;

	if (pa->events & LWS_POLLOUT) {
		/* libwebsockets wants to write but cannot find a way to update
		 * an existing glib::iosource event flags using glibmm,
		 * create another iosource and set to IO_OUT, it will be destroyed
		 * after clearing POLLOUT (see 'else' body below)
		 */

		if (it->second.wg_iosrc) {
			/* already polling for write */
			return;
		}

		RefPtr<IOSource> wg_iosrc = it->second.g_channel->create_watch (Glib::IO_OUT);
		wg_iosrc->connect (sigc::bind (sigc::mem_fun (*this, &WebsocketsServer::io_handler), pa->fd));
		wg_iosrc->attach (main_loop ()->get_context ());
		it->second.wg_iosrc = wg_iosrc;
	} else {
		if (it->second.wg_iosrc) {
			it->second.wg_iosrc->destroy ();
			it->second.wg_iosrc = Glib::RefPtr<Glib::IOSource> (0);
		}
	}
}

void
WebsocketsServer::del_poll_fd (struct lws_pollargs* pa)
{
	LwsPollFdGlibSourceMap::iterator it = _fd_ctx.find (pa->fd);
	if (it == _fd_ctx.end ()) {
		return;
	}

	it->second.rg_iosrc->destroy ();

	if (it->second.wg_iosrc) {
		it->second.wg_iosrc->destroy ();
	}

	_fd_ctx.erase (it);
}

void
WebsocketsServer::add_client (Client wsi)
{
	_client_ctx.emplace (wsi, ClientContext (wsi));
	dispatcher ().update_all_nodes (wsi); // send all state
}

void
WebsocketsServer::del_client (Client wsi)
{
	ClientContextMap::iterator it = _client_ctx.find (wsi);
	if (it != _client_ctx.end ()) {
		_client_ctx.erase (it);
	}
}

void
WebsocketsServer::recv_client (Client wsi, void* buf, size_t len)
{
	NodeStateMessage msg (buf, len);
	if (!msg.is_valid ()) {
		return;
	}

#ifndef NDEBUG
	std::cerr << "RX " << msg.state ().debug_str () << std::endl;
#endif

	ClientContextMap::iterator it = _client_ctx.find (wsi);
	if (it == _client_ctx.end ()) {
		return;
	}

	/* avoid echo */
	it->second.update_state (msg.state ());

	dispatcher ().dispatch (wsi, msg);
}

void
WebsocketsServer::write_client (Client wsi)
{
	ClientContextMap::iterator it = _client_ctx.find (wsi);
	if (it == _client_ctx.end ()) {
		return;
	}

	ClientOutputBuffer& pending = it->second.output_buf ();
	if (pending.empty ()) {
		return;
	}

	/* one lws_write() call per LWS_CALLBACK_SERVER_WRITEABLE callback */

	NodeStateMessage msg = pending.front ();
	pending.pop_front ();

	unsigned char out_buf[1024];
	size_t        len = msg.serialize (out_buf + LWS_PRE, 1024 - LWS_PRE);

	if (len > 0) {
#ifndef NDEBUG
		std::cerr << "TX " << msg.state ().debug_str () << std::endl;
#endif
		lws_write (wsi, out_buf + LWS_PRE, len, LWS_WRITE_TEXT);
	} else {
		PBD::error << "ArdourWebsockets: cannot serialize message" << endmsg;
	}

	if (!pending.empty ()) {
		lws_callback_on_writable (wsi);
	}
}

void
WebsocketsServer::reject_http_client (Client wsi)
{
	const char *html_body = "<p>This URL is not meant to be accessed via HTTP; for example using"
		" a web browser. Refer to Ardour documentation for further information.</p>";
	lws_return_http_status (wsi, 404, html_body);
}

bool
WebsocketsServer::io_handler (Glib::IOCondition ioc, lws_sockfd_type fd)
{
	/* IO_IN=1, IO_PRI=2, IO_ERR=8, IO_HUP=16 */
	//printf ("io_handler ioc = %d\n", ioc);

	LwsPollFdGlibSourceMap::iterator it = _fd_ctx.find (fd);
	if (it == _fd_ctx.end ()) {
		return false;
	}

	struct lws_pollfd* lws_pfd = &it->second.lws_pfd;
	lws_pfd->revents           = ioc_to_events (ioc);

	if (lws_service_fd (_lws_context, lws_pfd) < 0) {
		return false;
	}

	return ioc & (Glib::IO_IN | Glib::IO_OUT);
}

IOCondition
WebsocketsServer::events_to_ioc (int events)
{
	IOCondition ioc;

	if (events & LWS_POLLIN) {
		ioc = Glib::IO_IN;
	} else if (events & LWS_POLLOUT) {
		ioc = Glib::IO_OUT;
	} else if (events & LWS_POLLHUP) {
		ioc = Glib::IO_HUP;
	}

	return ioc;
}

int
WebsocketsServer::ioc_to_events (IOCondition ioc)
{
	int events = 0;

	if (ioc & Glib::IO_IN) {
		events |= LWS_POLLIN;
	}

	if (ioc & Glib::IO_OUT) {
		events |= LWS_POLLOUT;
	}

	if (ioc & (Glib::IO_HUP | Glib::IO_ERR)) {
		events |= LWS_POLLHUP;
	}

	return events;
}

int
WebsocketsServer::lws_callback (struct lws* wsi, enum lws_callback_reasons reason,
                                void* user, void* in, size_t len)
{
	void*             ctx_userdata = lws_context_user (lws_get_context (wsi));
	WebsocketsServer* server       = static_cast<WebsocketsServer*> (ctx_userdata);

	switch (reason) {
		case LWS_CALLBACK_ADD_POLL_FD:
			server->add_poll_fd (static_cast<struct lws_pollargs*> (in));
			break;

		case LWS_CALLBACK_CHANGE_MODE_POLL_FD:
			server->mod_poll_fd (static_cast<struct lws_pollargs*> (in));
			break;

		case LWS_CALLBACK_DEL_POLL_FD:
			server->del_poll_fd (static_cast<struct lws_pollargs*> (in));
			break;

		case LWS_CALLBACK_ESTABLISHED:
			server->add_client (wsi);
			break;

		case LWS_CALLBACK_CLOSED:
			server->del_client (wsi);
			break;

		case LWS_CALLBACK_RECEIVE:
			server->recv_client (wsi, in, len);
			break;

		case LWS_CALLBACK_SERVER_WRITEABLE:
			server->write_client (wsi);
			break;

		case LWS_CALLBACK_HTTP:
			server->reject_http_client (wsi);
			return 1;
			break;

		case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:
		case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
		case LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED:
		case LWS_CALLBACK_PROTOCOL_INIT:
		case LWS_CALLBACK_PROTOCOL_DESTROY:
		case LWS_CALLBACK_WSI_CREATE:
		case LWS_CALLBACK_WSI_DESTROY:
		case LWS_CALLBACK_LOCK_POLL:
		case LWS_CALLBACK_UNLOCK_POLL:
		case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
		case LWS_CALLBACK_FILTER_HTTP_CONNECTION:
#if LWS_LIBRARY_VERSION_MAJOR >= 3
		case LWS_CALLBACK_HTTP_BIND_PROTOCOL:
		case LWS_CALLBACK_ADD_HEADERS:
#if LWS_LIBRARY_VERSION_MINOR >= 1
		case LWS_CALLBACK_HTTP_CONFIRM_UPGRADE:
#endif
#endif
			break;

		/* TODO: handle HTTP connections.
		 * Serve static ctrl-surface pages, JS, CSS etc.
		 */

		default:
#ifndef NDEBUG
			/* see libwebsockets.h lws_callback_reasons */
			std::cerr << "LWS: unhandled callback " << reason << std::endl;
#endif
			return -1;
			break;
	}

	return 0;
}
