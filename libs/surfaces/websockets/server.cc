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

/* libwebsockets version of LWS_LIBRARY_VERSION_NUMBER also appends
   LWS_LIBRARY_VERSION_PATCH which can contain non-numeric characters
   rendering it unsuitable for numeric version checks  */
#define LWS_LIBRARY_VERSION_NUM (LWS_LIBRARY_VERSION_MAJOR * 1000000) + \
								(LWS_LIBRARY_VERSION_MINOR * 1000)

#define MAX_INDEX_SIZE	65536

using namespace Glib;
using namespace ArdourSurface;

WebsocketsServer::WebsocketsServer (ArdourSurface::ArdourWebsockets& surface)
    : SurfaceComponent (surface)
    , _lws_context (0)
    , _fd_callbacks (false)
    , _g_source (0)
{
	/* keep references to all config for libwebsockets 2 */
	lws_protocols proto;
	memset (&proto, 0, sizeof (lws_protocols));
	proto.name                  = "lws-ardour";
	proto.callback              = WebsocketsServer::lws_callback;
	proto.rx_buffer_size        = 0;
	proto.id                    = 0;
	proto.user                  = 0;
#if LWS_LIBRARY_VERSION_MAJOR >= 3
	proto.tx_packet_size = 0;
#endif
	
	_lws_proto[0] = proto;
	memset (&_lws_proto[1], 0, sizeof (lws_protocols));

	/* '/' is served by a static index.html file in the surface data directory
	 * inside it there is a 'builtin' subdirectory that contains all built-in
	 * surfaces so there is no need to create a dedicated mount point for them
	 * list of surfaces is available as a dynamically generated json file
	 */
	memset (&_lws_mnt_root, 0, sizeof (lws_http_mount));
	_lws_mnt_root.mountpoint       = "/";
	_lws_mnt_root.mountpoint_len   = strlen (_lws_mnt_root.mountpoint);
	_lws_mnt_root.origin           = _resources.index_dir ().c_str ();
	_lws_mnt_root.origin_protocol  = LWSMPRO_FILE;
	_lws_mnt_root.def              = "index.html";
	/* do not send caching headers if NDEBUG is set, this is useful while
	 * developing web surfaces. Ideally this would exist as a configurable
	 * option in the TO DO surface settings UI */
#ifdef NDEBUG
	_lws_mnt_root.cache_max_age    = 3600;
	_lws_mnt_root.cache_reusable   = 1;
	_lws_mnt_root.cache_revalidate = 1;
#endif

	/* user defined surfaces in the user config directory */
	memcpy (&_lws_mnt_user, &_lws_mnt_root, sizeof (lws_http_mount));
	_lws_mnt_user.mountpoint       = "/user";
	_lws_mnt_user.mountpoint_len   = strlen (_lws_mnt_user.mountpoint);
	_lws_mnt_user.origin           = _resources.user_dir ().c_str ();

	_lws_mnt_root.mount_next = &_lws_mnt_user;

	memset (&_lws_info, 0, sizeof (lws_context_creation_info));
	_lws_info.port      = WEBSOCKET_LISTEN_PORT;
	_lws_info.protocols = _lws_proto;
	_lws_info.mounts    = &_lws_mnt_root;
	_lws_info.uid       = -1;
	_lws_info.gid       = -1;
	_lws_info.user      = this;

#if LWS_LIBRARY_VERSION_MAJOR < 3
	/* older libwebsockets does not define mime type for svg files */
	memset (&_lws_vhost_opt, 0, sizeof (lws_protocol_vhost_options));
	_lws_vhost_opt.name           = ".svg";
	_lws_vhost_opt.value          = "image/svg+xml";
	_lws_mnt_root.extra_mimetypes = &_lws_vhost_opt;
	_lws_mnt_user.extra_mimetypes = &_lws_vhost_opt;
#endif
}

int
WebsocketsServer::start ()
{
#ifndef NDEBUG
	lws_set_log_level (LLL_ERR | LLL_WARN /*| LLL_DEBUG*/, 0);
#endif

	if (_lws_context) {
		stop ();
	}

	/* Event loop integration method depends on how libwebsockets is configured
	 * for Ardour build environment and how libwebsockets is compiled for the
	 * system running Ardour. */

#ifdef LWS_WITH_GLIB
	void *foreign_loops[1];
	foreign_loops[0] = main_loop ()->gobj ();
	_lws_info.foreign_loops = foreign_loops;
	_lws_info.options = LWS_SERVER_OPTION_GLIB;
	_lws_context = lws_create_context (&_lws_info);
#endif

	if (_lws_context) {
		/* Keep in mind _lws_context can be != 0 even when the user's
		   libwebsockets does not support LWS_SERVER_OPTION_GLIB !
		   This is by libwebsockets design */ 
		PBD::info << "ArdourWebsockets: using event loop integration method 1" << endmsg;
	} else {
		/* More compatible approach */
		_fd_callbacks = true;
		_lws_info.options = 0;
		_lws_context = lws_create_context (&_lws_info);

		if (!_lws_context) {
			PBD::error << "ArdourWebsockets: could not create the libwebsockets context" << endmsg;
			return -1;
		}

		if (!_fd_ctx.empty ()) {
			// LWS_CALLBACK_ADD_POLL_FD was called, LWS_WITH_EXTERNAL_POLL is available
			PBD::info << "ArdourWebsockets: using event loop integration method 2" << endmsg;
		} else {
			// Neither LWS_WITH_EXTERNAL_POLL or LWS_WITH_GLIB are available
			PBD::info << "ArdourWebsockets: using event loop integration method 3" << endmsg;
			_g_source = g_idle_source_new();
			g_source_set_callback (_g_source, WebsocketsServer::glib_idle_callback, _lws_context, 0);
			g_source_attach (_g_source, g_main_loop_get_context (main_loop ()->gobj ()));
		}
	}

	PBD::info << "ArdourWebsockets: listening on: http://"
	          << lws_canonical_hostname (_lws_context)
	          << ":"
	          << std::dec << (int) WEBSOCKET_LISTEN_PORT
	          << "/"
	          << endmsg;

	return 0;
}

int
WebsocketsServer::stop ()
{
	if (!_fd_ctx.empty ()) { // Method 2
		for (LwsPollFdGlibSourceMap::iterator it = _fd_ctx.begin (); it != _fd_ctx.end (); ++it) {
			it->second.rg_iosrc->destroy ();

			if (it->second.wg_iosrc) {
				it->second.wg_iosrc->destroy ();
			}
		}

		_fd_ctx.clear ();
	}

	if (_g_source) { // Method 3
		g_source_destroy (_g_source);
		lws_cancel_service (_lws_context);
	}

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
		request_write (wsi);
	}
}

void
WebsocketsServer::update_all_clients (const NodeState& state, bool force)
{
	for (ClientContextMap::iterator it = _client_ctx.begin (); it != _client_ctx.end (); ++it) {
		update_client (it->second.wsi (), state, force);
	}
}

int
WebsocketsServer::add_client (Client wsi)
{
	_client_ctx.emplace (wsi, ClientContext (wsi));
	dispatcher ().update_all_nodes (wsi); // send all state
	return 0;
}

int
WebsocketsServer::del_client (Client wsi)
{
	ClientContextMap::iterator it = _client_ctx.find (wsi);

	if (it != _client_ctx.end ()) {
		_client_ctx.erase (it);
	}

	return 0;
}

int
WebsocketsServer::recv_client (Client wsi, void* buf, size_t len)
{
	NodeStateMessage msg (buf, len);
	if (!msg.is_valid ()) {
		return 1;
	}

#ifdef PRINT_TRAFFIC
	std::cerr << "RX " << msg.state ().debug_str () << std::endl;
#endif

	ClientContextMap::iterator it = _client_ctx.find (wsi);
	if (it == _client_ctx.end ()) {
		return 1;
	}

	/* avoid echo */
	it->second.update_state (msg.state ());

	dispatcher ().dispatch (wsi, msg);

	return 0;
}

int
WebsocketsServer::write_client (Client wsi)
{
	ClientContextMap::iterator it = _client_ctx.find (wsi);
	if (it == _client_ctx.end ()) {
		return 1;
	}

	ClientOutputBuffer& pending = it->second.output_buf ();
	if (pending.empty ()) {
		return 0;
	}

	/* one lws_write() call per LWS_CALLBACK_SERVER_WRITEABLE callback */

	NodeStateMessage msg = pending.front ();
	pending.pop_front ();

	unsigned char out_buf[1024];
	int len = msg.serialize (out_buf + LWS_PRE, 1024 - LWS_PRE);

	if (len > 0) {
#ifdef PRINT_TRAFFIC
		std::cerr << "TX " << msg.state ().debug_str () << std::endl;
#endif
		if (lws_write (wsi, out_buf + LWS_PRE, len, LWS_WRITE_TEXT) != len) {
			return 1;
		}
	} else {
		PBD::error << "ArdourWebsockets: cannot serialize message" << endmsg;
	}

	if (!pending.empty ()) {
		request_write (wsi);
	}

	return 0;
}

int
WebsocketsServer::send_availsurf_hdr (Client wsi)
{
	char url[1024];

	if (lws_hdr_copy (wsi, url, 1024, WSI_TOKEN_GET_URI) < 0) {
		return 1;
	}

	if (strcmp (url, "/surfaces.json") != 0) {
		lws_return_http_status (wsi, 404, 0);
		return 1;
	}

	unsigned char out_buf[1024],
		*start = out_buf,
		*p = start,
		*end = &out_buf[sizeof(out_buf) - 1]; 

#if LWS_LIBRARY_VERSION_MAJOR >= 3
	if (   lws_add_http_common_headers (wsi, HTTP_STATUS_OK, "application/json", LWS_ILLEGAL_HTTP_CONTENT_LEN, &p, end)
	    || lws_add_http_header_by_token (wsi, WSI_TOKEN_HTTP_CACHE_CONTROL, reinterpret_cast<const unsigned char*> ("no-store"), 8, &p, end)
		 ) {
		return 1;
	}

	if (lws_finalize_write_http_header (wsi, start, &p, end) != 0) {
		return 1;
	}
#else
	if (   lws_add_http_header_status (wsi, HTTP_STATUS_OK, &p, end)
	    || lws_add_http_header_by_token (wsi, WSI_TOKEN_HTTP_CONTENT_TYPE, reinterpret_cast<const unsigned char*> ("application/json"), 16, &p, end)
	    || lws_add_http_header_by_token (wsi, WSI_TOKEN_CONNECTION, reinterpret_cast<const unsigned char*> ("close"), 5, &p, end)
	    || lws_add_http_header_by_token (wsi, WSI_TOKEN_HTTP_CACHE_CONTROL, reinterpret_cast<const unsigned char*> ("no-store"), 8, &p, end)
	    || lws_finalize_http_header (wsi, &p, end)
	   ) {
		return 1;
	}

	int len = p - start;

	if (lws_write (wsi, start, len, LWS_WRITE_HTTP_HEADERS) != len) {
		return 1;
	}
#endif

	request_write (wsi);

	return 0;
}

int
WebsocketsServer::send_availsurf_body (Client wsi)
{
	std::string index = _resources.scan ();

	char body[MAX_INDEX_SIZE];
#if LWS_LIBRARY_VERSION_MAJOR >= 3
	lws_strncpy (body, index.c_str (), sizeof(body));
#else
	memset (body, 0, sizeof (body));
	strncpy (body, index.c_str (), sizeof(body) - 1);
#endif
	int len = strlen (body);

	/* lws_write() expects a writable buffer */
	if (lws_write (wsi, reinterpret_cast<unsigned char*> (body), len, LWS_WRITE_HTTP) != len) {
		return 1;
	}

	/* lws_http_transaction_completed() returns 1 if the HTTP connection must close now
	   Returns 0 and resets connection to wait for new HTTP header / transaction if possible */
	if (lws_http_transaction_completed (wsi) != 1) {
		return -1;
	}

	return -1;	// end connection
}

void
WebsocketsServer::request_write (Client wsi)
{
	lws_callback_on_writable (wsi);

	if (read_blocks_event_loop ()) {
		// cancel lws_service() in the idle callback to write pending data asap
		lws_cancel_service (_lws_context);
	}
}

int
WebsocketsServer::lws_callback (struct lws* wsi, enum lws_callback_reasons reason,
                                void* user, void* in, size_t len)
{
	void*             ctx_userdata = lws_context_user (lws_get_context (wsi));
	WebsocketsServer* server       = static_cast<WebsocketsServer*> (ctx_userdata);
	int rc;

	switch (reason) {
		case LWS_CALLBACK_ESTABLISHED:
			rc = server->add_client (wsi);
			break;

		case LWS_CALLBACK_CLOSED:
			rc = server->del_client (wsi);
			break;

		case LWS_CALLBACK_RECEIVE:
			rc = server->recv_client (wsi, in, len);
			break;

		case LWS_CALLBACK_SERVER_WRITEABLE:
			rc = server->write_client (wsi);
			break;

		/* will be called only if the requested url is not fulfilled
		   by the any of the mount configurations (root, user) */
		case LWS_CALLBACK_HTTP:
			rc = server->send_availsurf_hdr (wsi);
			break;

		case LWS_CALLBACK_HTTP_WRITEABLE:
			rc = server->send_availsurf_body (wsi);
			break;

		/* fd callbacks must be skipped for integration method 1 */
		case LWS_CALLBACK_ADD_POLL_FD:
			if (server->fd_callbacks()) {
				rc = server->add_poll_fd (static_cast<struct lws_pollargs*> (in));
			} else {
				rc = 0;
			}
			break;

		case LWS_CALLBACK_CHANGE_MODE_POLL_FD:
			if (server->fd_callbacks()) {
				rc = server->mod_poll_fd (static_cast<struct lws_pollargs*> (in));
			} else {
				rc = 0;
			}
			break;

		case LWS_CALLBACK_DEL_POLL_FD:
			if (server->fd_callbacks()) {
				rc = server->del_poll_fd (static_cast<struct lws_pollargs*> (in));
			} else {
				rc = 0;
			}
			break;

#if LWS_LIBRARY_VERSION_NUM >= 2001000
		// lws_callback_http_dummy is not available on lws < 2.1.0
		default:
			rc = lws_callback_http_dummy (wsi, reason, user, in, len);
			break;
#else
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
			/* do nothing but keep connection alive */
			rc = 0;
			break;

		default:
#ifndef NDEBUG
			/* see libwebsockets.h lws_callback_reasons */
			std::cerr << "LWS: unhandled callback " << reason << std::endl;
#endif
			rc = -1;
			break;
#endif // LWS_LIBRARY_VERSION_NUM >= 2001000
	}

	return rc;
}

int
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

	return 0;
}

int
WebsocketsServer::mod_poll_fd (struct lws_pollargs* pa)
{
	LwsPollFdGlibSourceMap::iterator it = _fd_ctx.find (pa->fd);
	if (it == _fd_ctx.end ()) {
		return 1;
	}

	it->second.lws_pfd.events = pa->events;

	if (pa->events & LWS_POLLOUT) {
		/* libwebsockets needs to write but cannot find a way to update
		 * an existing glib::iosource event flags using glibmm alone,
		 * create another iosource and set to IO_OUT, it will be destroyed
		 * after clearing POLLOUT (see 'else' body below)
		 */

		if (it->second.wg_iosrc) {
			/* already polling for write */
			return 0;
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

	return 0;
}

int
WebsocketsServer::del_poll_fd (struct lws_pollargs* pa)
{
	LwsPollFdGlibSourceMap::iterator it = _fd_ctx.find (pa->fd);
	if (it == _fd_ctx.end ()) {
		return 1;
	}

	it->second.rg_iosrc->destroy ();

	if (it->second.wg_iosrc) {
		it->second.wg_iosrc->destroy ();
	}

	_fd_ctx.erase (it);

	return 0;
}

bool
WebsocketsServer::io_handler (Glib::IOCondition ioc, lws_sockfd_type fd)
{
	/* IO_IN=1, IO_PRI=2, IO_OUT=4, IO_ERR=8, IO_HUP=16 */

	LwsPollFdGlibSourceMap::iterator it = _fd_ctx.find (fd);
	if (it == _fd_ctx.end ()) {
		return false;
	}

	struct lws_pollfd* lws_pfd = &it->second.lws_pfd;
	lws_pfd->revents           = ioc_to_events (ioc);

	lws_service_fd (_lws_context, lws_pfd);

	return ioc & (Glib::IO_IN | Glib::IO_OUT);
}

IOCondition
WebsocketsServer::events_to_ioc (int events)
{
	IOCondition ioc = Glib::IOCondition (0);

	if (events & LWS_POLLIN) {
		ioc |= Glib::IO_IN;
	}

	if (events & LWS_POLLOUT) {
		ioc |= Glib::IO_OUT;
	}

	if (events & LWS_POLLHUP) {
		ioc |= Glib::IO_HUP;
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

gboolean
WebsocketsServer::glib_idle_callback (void *data)
{
	struct lws_context *lws_ctx = static_cast<struct lws_context *>(data);
	lws_service (lws_ctx, 0);
	return TRUE;
}
