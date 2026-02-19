/*
 * Copyright (C) 2026
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

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <vector>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include "pbd/error.h"
#include "pbd/id.h"
#include "pbd/controllable.h"

#include "ardour/audio_track.h"
#include "ardour/dB.h"
#include "ardour/internal_send.h"
#include "ardour/midi_track.h"
#include "ardour/processor.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/session_event.h"
#include "ardour/tempo.h"
#include "ardour/track.h"

#include "mcp_http_server.h"

namespace pt = boost::property_tree;

using namespace ArdourSurface;

namespace {

static std::string
json_escape (const std::string& s)
{
	std::ostringstream o;

	for (std::string::const_iterator it = s.begin (); it != s.end (); ++it) {
		if (*it == '"' || *it == '\\' || ('\x00' <= *it && *it <= '\x1f')) {
			o << "\\u" << std::hex << std::setw (4) << std::setfill ('0') << static_cast<int> (*it);
		} else {
			o << *it;
		}
	}

	return o.str ();
}

static bool
is_number_literal (const std::string& s)
{
	if (s.empty ()) {
		return false;
	}

	char* endptr = 0;
	std::strtod (s.c_str (), &endptr);
	return endptr && *endptr == '\0';
}

static std::string
jsonrpc_id (const pt::ptree& root)
{
	boost::optional<const pt::ptree&> id_node = root.get_child_optional ("id");
	if (!id_node) {
		return "null";
	}

	if (!id_node->empty ()) {
		return "null";
	}

	std::string id = id_node->data ();
	if (id.empty () || id == "null") {
		return "null";
	}

	if (id == "true" || id == "false" || is_number_literal (id)) {
		return id;
	}

	return std::string ("\"") + json_escape (id) + "\"";
}

static bool
has_jsonrpc_id (const pt::ptree& root)
{
	return root.get_child_optional ("id").is_initialized ();
}

static std::string
jsonrpc_result (const std::string& id, const std::string& result_json)
{
	return std::string ("{\"jsonrpc\":\"2.0\",\"id\":") + id + ",\"result\":" + result_json + "}";
}

static std::string
jsonrpc_error (const std::string& id, int code, const std::string& message)
{
	std::ostringstream ss;
	ss << "{\"jsonrpc\":\"2.0\",\"id\":" << id << ",\"error\":{\"code\":" << code << ",\"message\":\""
	   << json_escape (message) << "\"}}";
	return ss.str ();
}

static std::string
transport_state_string (ARDOUR::Session& session)
{
	if (session.transport_locating ()) {
		return "locating";
	}

	if (session.transport_rolling ()) {
		return "rolling";
	}

	return "stopped";
}

static std::string
transport_state_json (ARDOUR::Session& session)
{
	std::ostringstream ss;
	ss << "{\"rolling\":" << (session.transport_rolling () ? "true" : "false")
	   << ",\"speed\":" << session.transport_speed ()
	   << ",\"sample\":" << session.transport_sample ()
	   << ",\"state\":\"" << transport_state_string (session) << "\"}";
	return ss.str ();
}

static double
transport_tempo_bpm (ARDOUR::Session& session)
{
	try {
		Temporal::TempoMap::SharedPtr tmap (Temporal::TempoMap::fetch ());
		return tmap->metric_at (Temporal::timepos_t (session.transport_sample ())).tempo ().quarter_notes_per_minute ();
	} catch (...) {
		return 120.0;
	}
}

static std::string
session_info_json (ARDOUR::Session& session)
{
	std::ostringstream ss;
	ss << "{\"sessionName\":\"" << json_escape (session.name ()) << "\""
	   << ",\"sampleRate\":" << session.nominal_sample_rate ()
	   << ",\"tempoBpm\":" << transport_tempo_bpm (session)
	   << ",\"transport\":" << transport_state_json (session)
	   << "}";
	return ss.str ();
}

static std::string
route_type_string (const std::shared_ptr<ARDOUR::Route>& route)
{
	if (!route) {
		return "route";
	}

	if (std::dynamic_pointer_cast<ARDOUR::MidiTrack> (route)) {
		return "midi_track";
	}
	if (std::dynamic_pointer_cast<ARDOUR::AudioTrack> (route)) {
		return "audio_track";
	}
	if (route->is_track ()) {
		return "track";
	}
	return "bus";
}

static std::string
tracks_list_json (ARDOUR::Session& session, bool include_hidden)
{
	std::shared_ptr<ARDOUR::RouteList> tracks = session.get_tracks ();
	std::vector<std::shared_ptr<ARDOUR::Route> > sorted;

	if (tracks) {
		sorted.assign (tracks->begin (), tracks->end ());
	}

	std::sort (
		sorted.begin (),
		sorted.end (),
		[] (const std::shared_ptr<ARDOUR::Route>& a, const std::shared_ptr<ARDOUR::Route>& b) {
			if (!a) {
				return false;
			}
			if (!b) {
				return true;
			}
			return a->presentation_info ().order () < b->presentation_info ().order ();
		});

	std::ostringstream ss;
	ss << "{\"tracks\":[";

	bool first = true;
	for (std::vector<std::shared_ptr<ARDOUR::Route> >::const_iterator it = sorted.begin (); it != sorted.end (); ++it) {
		const std::shared_ptr<ARDOUR::Route>& route = *it;
		if (!route) {
			continue;
		}

		const bool hidden = route->is_hidden ();
		if (hidden && !include_hidden) {
			continue;
		}

		std::string type = route_type_string (route);

		if (!first) {
			ss << ",";
		}
		first = false;

		ss << "{\"id\":\"" << json_escape (route->id ().to_s ()) << "\""
		   << ",\"name\":\"" << json_escape (route->name ()) << "\""
		   << ",\"type\":\"" << type << "\""
		   << ",\"trackNumber\":" << route->track_number ()
		   << ",\"presentationOrder\":" << route->presentation_info ().order ()
		   << ",\"hidden\":" << (hidden ? "true" : "false")
		   << "}";
	}

	ss << "]}";
	return ss.str ();
}

static bool
valid_fader_position (double p)
{
	return std::isfinite (p) && p >= 0.0 && p <= 1.0;
}

static bool
valid_fader_db (double d)
{
	return std::isfinite (d);
}

static std::string
track_fader_json (const std::shared_ptr<ARDOUR::Route>& route)
{
	std::shared_ptr<ARDOUR::AutomationControl> gain = route ? route->gain_control () : std::shared_ptr<ARDOUR::AutomationControl> ();
	const double position = gain ? gain->internal_to_interface (gain->get_value ()) : 0.0;
	double db = -193.0;

	if (gain && gain->get_value () > 0.0) {
		db = fast_coefficient_to_dB (gain->get_value ());
		if (!std::isfinite (db)) {
			db = -193.0;
		}
	}

	std::ostringstream ss;
	ss << "{\"id\":\"" << json_escape (route->id ().to_s ()) << "\""
	   << ",\"name\":\"" << json_escape (route->name ()) << "\""
	   << ",\"position\":" << position
	   << ",\"db\":" << db
	   << "}";
	return ss.str ();
}

static std::string
send_list_json (const std::shared_ptr<ARDOUR::Route>& route)
{
	std::ostringstream ss;
	ss << "[";

	bool first = true;
	for (uint32_t i = 0;; ++i) {
		std::shared_ptr<ARDOUR::Processor> p = route->nth_send (i);
		if (!p) {
			break;
		}

		std::shared_ptr<ARDOUR::AutomationControl> gain = route->send_level_controllable (i);
		const double position = gain ? gain->internal_to_interface (gain->get_value ()) : 0.0;
		double db = -193.0;
		if (gain && gain->get_value () > 0.0) {
			db = fast_coefficient_to_dB (gain->get_value ());
			if (!std::isfinite (db)) {
				db = -193.0;
			}
		}

		if (!first) {
			ss << ",";
		}
		first = false;

		ss << "{\"index\":" << i
		   << ",\"name\":\"" << json_escape (route->send_name (i)) << "\""
		   << ",\"active\":" << (p->active () ? "true" : "false")
		   << ",\"position\":" << position
		   << ",\"db\":" << db;

		std::shared_ptr<ARDOUR::InternalSend> isend = std::dynamic_pointer_cast<ARDOUR::InternalSend> (p);
		if (isend) {
			std::shared_ptr<ARDOUR::Route> target = isend->target_route ();
			if (target) {
				ss << ",\"targetRouteId\":\"" << json_escape (target->id ().to_s ()) << "\""
				   << ",\"targetRouteName\":\"" << json_escape (target->name ()) << "\"";
			}
		}

		ss << "}";
	}

	ss << "]";
	return ss.str ();
}

static std::string
plugin_list_json (const std::shared_ptr<ARDOUR::Route>& route)
{
	std::ostringstream ss;
	ss << "[";

	bool first = true;
	for (uint32_t i = 0;; ++i) {
		std::shared_ptr<ARDOUR::Processor> p = route->nth_plugin (i);
		if (!p) {
			break;
		}
		if (!p->display_to_user ()) {
			continue;
		}

		if (!first) {
			ss << ",";
		}
		first = false;

		ss << "{\"index\":" << i
		   << ",\"name\":\"" << json_escape (p->name ()) << "\""
		   << ",\"displayName\":\"" << json_escape (p->display_name ()) << "\""
		   << ",\"active\":" << (p->active () ? "true" : "false")
		   << ",\"enabled\":" << (p->enabled () ? "true" : "false")
		   << "}";
	}

	ss << "]";
	return ss.str ();
}

static std::string
track_info_json (const std::shared_ptr<ARDOUR::Route>& route)
{
	const bool hidden = route->is_hidden ();
	const std::string type = route_type_string (route);

	std::shared_ptr<ARDOUR::AutomationControl> gain = route->gain_control ();
	std::shared_ptr<ARDOUR::AutomationControl> pan = route->pan_azimuth_control ();
	std::shared_ptr<ARDOUR::AutomationControl> mute = route->mute_control ();
	std::shared_ptr<ARDOUR::AutomationControl> solo = route->solo_control ();
	std::shared_ptr<ARDOUR::Track> track = std::dynamic_pointer_cast<ARDOUR::Track> (route);
	std::shared_ptr<ARDOUR::AutomationControl> rec_enable = track ? track->rec_enable_control () : std::shared_ptr<ARDOUR::AutomationControl> ();
	std::shared_ptr<ARDOUR::AutomationControl> rec_safe = track ? track->rec_safe_control () : std::shared_ptr<ARDOUR::AutomationControl> ();

	std::ostringstream ss;
	ss << "{\"id\":\"" << json_escape (route->id ().to_s ()) << "\""
	   << ",\"name\":\"" << json_escape (route->name ()) << "\""
	   << ",\"type\":\"" << type << "\""
	   << ",\"trackNumber\":" << route->track_number ()
	   << ",\"presentationOrder\":" << route->presentation_info ().order ()
	   << ",\"hidden\":" << (hidden ? "true" : "false");

	if (gain) {
		double db = -193.0;
		if (gain->get_value () > 0.0) {
			db = fast_coefficient_to_dB (gain->get_value ());
			if (!std::isfinite (db)) {
				db = -193.0;
			}
		}

		ss << ",\"fader\":{\"position\":" << gain->internal_to_interface (gain->get_value ())
		   << ",\"db\":" << db << "}";
	} else {
		ss << ",\"fader\":null";
	}

	if (pan) {
		ss << ",\"pan\":{\"position\":" << pan->internal_to_interface (pan->get_value ()) << "}";
	} else {
		ss << ",\"pan\":null";
	}

	ss << ",\"mute\":";
	if (mute) {
		ss << (mute->get_value () > 0.5 ? "true" : "false");
	} else {
		ss << "null";
	}

	ss << ",\"solo\":";
	if (solo) {
		ss << (solo->get_value () > 0.5 ? "true" : "false");
	} else {
		ss << "null";
	}

	ss << ",\"recEnabled\":";
	if (rec_enable) {
		ss << (rec_enable->get_value () > 0.5 ? "true" : "false");
	} else {
		ss << "null";
	}

	ss << ",\"recSafe\":";
	if (rec_safe) {
		ss << (rec_safe->get_value () > 0.5 ? "true" : "false");
	} else {
		ss << "null";
	}

	ss << ",\"sends\":" << send_list_json (route);
	ss << ",\"plugins\":" << plugin_list_json (route);

	ss << "}";
	return ss.str ();
}

} // namespace

MCPHttpServer::MCPHttpServer (ARDOUR::Session& session, uint16_t port)
	: _session (session)
	, _port (port)
	, _context (0)
	, _running (false)
{
	memset (_protocols, 0, sizeof (_protocols));
	memset (&_info, 0, sizeof (_info));
}

MCPHttpServer::~MCPHttpServer ()
{
	stop ();
}

int
MCPHttpServer::start ()
{
	if (_context) {
		return 0;
	}

	_protocols[0].name = "mcp-http";
	_protocols[0].callback = MCPHttpServer::lws_callback;
	_protocols[0].per_session_data_size = 0;
	_protocols[0].rx_buffer_size = 0;
	_protocols[0].id = 0;
	_protocols[0].user = 0;
#if LWS_LIBRARY_VERSION_MAJOR >= 3
	_protocols[0].tx_packet_size = 0;
#endif

	_info.port = _port;
	_info.protocols = _protocols;
	_info.gid = -1;
	_info.uid = -1;
	_info.user = this;

	_context = lws_create_context (&_info);
	if (!_context) {
		PBD::error << "MCPHttp: could not create libwebsockets context" << endmsg;
		return -1;
	}

	_running = true;
	_service_thread = std::thread (&MCPHttpServer::run, this);

	return 0;
}

int
MCPHttpServer::stop ()
{
	if (!_context) {
		return 0;
	}

	_running = false;
	lws_cancel_service (_context);

	if (_service_thread.joinable ()) {
		_service_thread.join ();
	}

	lws_context_destroy (_context);
	_context = 0;
	_clients.clear ();

	return 0;
}

void
MCPHttpServer::run ()
{
	/* Session transport requests allocate SessionEvent objects from a per-thread pool. */
	ARDOUR::SessionEvent::create_per_thread_pool ("MCPHttp events", 256);

	while (_running) {
		lws_service (_context, 100);
	}
}

MCPHttpServer::ClientContext&
MCPHttpServer::client (struct lws* wsi)
{
	ClientMap::iterator it = _clients.find (wsi);
	if (it == _clients.end ()) {
		ClientContext ctx;
		ctx.sse = false;
		ctx.mcp_post = false;
		ctx.have_response = false;
		it = _clients.emplace (wsi, ctx).first;
	}

	return it->second;
}

void
MCPHttpServer::erase_client (struct lws* wsi)
{
	ClientMap::iterator it = _clients.find (wsi);
	if (it != _clients.end ()) {
		_clients.erase (it);
	}
}

int
MCPHttpServer::handle_http (struct lws* wsi, ClientContext& ctx)
{
	ctx.sse = false;
	ctx.mcp_post = false;
	ctx.have_response = false;
	ctx.request_body.clear ();
	ctx.response_body.clear ();
	ctx.sse_queue.clear ();

	char uri[1024];
	std::string path;

	if (lws_hdr_copy (wsi, uri, sizeof (uri), WSI_TOKEN_GET_URI) > 0) {
		path = uri;

		/* Support both legacy SSE path and streamable HTTP endpoint path. */
		if (path == "/mcp" || path == "/mcp/sse") {
			if (send_sse_headers (wsi)) {
				return 1;
			}

			ctx.sse = true;
			ctx.sse_queue.push_back ("event: ready\ndata: {\"jsonrpc\":\"2.0\",\"method\":\"notifications/ready\",\"params\":{\"server\":\"ardour-mcp-http\"}}\n\n");
			lws_callback_on_writable (wsi);
			return 0;
		}

		if (path == "/mcp/messages") {
			return send_http_status (wsi, 405);
		}

		return send_http_status (wsi, 404);
	}

	if (lws_hdr_copy (wsi, uri, sizeof (uri), WSI_TOKEN_POST_URI) > 0) {
		path = uri;

		if (path == "/mcp" || path == "/mcp/messages") {
			ctx.mcp_post = true;
			return 0;
		}

		if (path == "/mcp/sse") {
			return send_http_status (wsi, 405);
		}

		return send_http_status (wsi, 404);
	}

	return send_http_status (wsi, 400);
}

int
MCPHttpServer::handle_http_body (struct lws* /*wsi*/, ClientContext& ctx, void* in, size_t len)
{
	if (!ctx.mcp_post) {
		return 0;
	}

	ctx.request_body.append (static_cast<char*> (in), len);
	return 0;
}

int
MCPHttpServer::handle_http_body_completion (struct lws* wsi, ClientContext& ctx)
{
	if (!ctx.mcp_post) {
		return send_http_status (wsi, 400);
	}

	ctx.response_body = dispatch_jsonrpc (ctx.request_body);

	if (ctx.response_body.empty ()) {
		return send_http_status (wsi, 202);
	}

	ctx.have_response = true;

	if (send_json_headers (wsi)) {
		return 1;
	}

	lws_callback_on_writable (wsi);
	return 0;
}

int
MCPHttpServer::handle_http_writeable (struct lws* wsi, ClientContext& ctx)
{
	if (ctx.sse) {
		return write_sse_message (wsi, ctx);
	}

	if (ctx.have_response) {
		return write_json_response (wsi, ctx);
	}

	return 0;
}

int
MCPHttpServer::send_http_status (struct lws* wsi, unsigned int status)
{
	lws_return_http_status (wsi, status, 0);
	return -1;
}

int
MCPHttpServer::send_json_headers (struct lws* wsi)
{
	unsigned char out_buf[1024];
	unsigned char* start = out_buf;
	unsigned char* p = start;
	unsigned char* end = &out_buf[sizeof (out_buf) - 1];

#if LWS_LIBRARY_VERSION_MAJOR >= 3
	if (   lws_add_http_common_headers (wsi, 200, "application/json", LWS_ILLEGAL_HTTP_CONTENT_LEN, &p, end)
	    || lws_add_http_header_by_token (wsi, WSI_TOKEN_HTTP_CACHE_CONTROL, reinterpret_cast<const unsigned char*> ("no-store"), 8, &p, end)
	    || lws_finalize_write_http_header (wsi, start, &p, end)) {
		return 1;
	}
#else
	if (   lws_add_http_header_status (wsi, 200, &p, end)
	    || lws_add_http_header_by_token (wsi, WSI_TOKEN_HTTP_CONTENT_TYPE, reinterpret_cast<const unsigned char*> ("application/json"), 16, &p, end)
	    || lws_add_http_header_by_token (wsi, WSI_TOKEN_CONNECTION, reinterpret_cast<const unsigned char*> ("close"), 5, &p, end)
	    || lws_add_http_header_by_token (wsi, WSI_TOKEN_HTTP_CACHE_CONTROL, reinterpret_cast<const unsigned char*> ("no-store"), 8, &p, end)
	    || lws_finalize_http_header (wsi, &p, end)) {
		return 1;
	}

	int len = p - start;
	if (lws_write (wsi, start, len, LWS_WRITE_HTTP_HEADERS) != len) {
		return 1;
	}
#endif

	return 0;
}

int
MCPHttpServer::send_sse_headers (struct lws* wsi)
{
	unsigned char out_buf[1024];
	unsigned char* start = out_buf;
	unsigned char* p = start;
	unsigned char* end = &out_buf[sizeof (out_buf) - 1];

#if LWS_LIBRARY_VERSION_MAJOR >= 3
	if (   lws_add_http_common_headers (wsi, 200, "text/event-stream", LWS_ILLEGAL_HTTP_CONTENT_LEN, &p, end)
	    || lws_add_http_header_by_token (wsi, WSI_TOKEN_HTTP_CACHE_CONTROL, reinterpret_cast<const unsigned char*> ("no-cache"), 8, &p, end)
	    || lws_add_http_header_by_token (wsi, WSI_TOKEN_CONNECTION, reinterpret_cast<const unsigned char*> ("keep-alive"), 10, &p, end)
	    || lws_finalize_write_http_header (wsi, start, &p, end)) {
		return 1;
	}
#else
	if (   lws_add_http_header_status (wsi, 200, &p, end)
	    || lws_add_http_header_by_token (wsi, WSI_TOKEN_HTTP_CONTENT_TYPE, reinterpret_cast<const unsigned char*> ("text/event-stream"), 17, &p, end)
	    || lws_add_http_header_by_token (wsi, WSI_TOKEN_CONNECTION, reinterpret_cast<const unsigned char*> ("keep-alive"), 10, &p, end)
	    || lws_add_http_header_by_token (wsi, WSI_TOKEN_HTTP_CACHE_CONTROL, reinterpret_cast<const unsigned char*> ("no-cache"), 8, &p, end)
	    || lws_finalize_http_header (wsi, &p, end)) {
		return 1;
	}

	int len = p - start;
	if (lws_write (wsi, start, len, LWS_WRITE_HTTP_HEADERS) != len) {
		return 1;
	}
#endif

	return 0;
}

int
MCPHttpServer::write_json_response (struct lws* wsi, ClientContext& ctx)
{
	std::vector<unsigned char> body (ctx.response_body.begin (), ctx.response_body.end ());
	if (!body.empty () && lws_write (wsi, body.data (), body.size (), LWS_WRITE_HTTP) != (int) body.size ()) {
		return 1;
	}

	ctx.have_response = false;
	ctx.mcp_post = false;
	ctx.request_body.clear ();
	ctx.response_body.clear ();

	if (lws_http_transaction_completed (wsi)) {
		return -1;
	}

	return -1;
}

int
MCPHttpServer::write_sse_message (struct lws* wsi, ClientContext& ctx)
{
	if (ctx.sse_queue.empty ()) {
		return 0;
	}

	std::string message = ctx.sse_queue.front ();
	ctx.sse_queue.pop_front ();

	std::vector<unsigned char> data (message.begin (), message.end ());
	if (!data.empty () && lws_write (wsi, data.data (), data.size (), LWS_WRITE_HTTP) != (int) data.size ()) {
		return 1;
	}

	if (!ctx.sse_queue.empty ()) {
		lws_callback_on_writable (wsi);
	}

	return 0;
}

std::string
MCPHttpServer::dispatch_jsonrpc (const std::string& payload) const
{
	pt::ptree root;
	std::istringstream is (payload);

	try {
		pt::read_json (is, root);
	} catch (...) {
		return jsonrpc_error ("null", -32700, "Parse error");
	}

	std::string method = root.get<std::string> ("method", "");
	std::string id = jsonrpc_id (root);
	const bool has_id = has_jsonrpc_id (root);

	if (method.empty ()) {
		return jsonrpc_error (id, -32600, "Invalid Request");
	}

	if (method == "initialize") {
		return jsonrpc_result (
			id,
			"{\"protocolVersion\":\"2025-03-26\","
			"\"capabilities\":{\"tools\":{\"listChanged\":false}},"
			"\"serverInfo\":{\"name\":\"ardour-mcp-http\",\"version\":\"0.1.0\"}}");
	}

	if (method == "notifications/initialized" && !has_id) {
		/* JSON-RPC notification: no response body. */
		return std::string ();
	}

	if (method == "ping" || method == "notifications/initialized") {
		return jsonrpc_result (id, "{}");
	}

	if (method == "tools/list") {
		return jsonrpc_result (
			id,
			"{\"tools\":["
			"{\"name\":\"hello_world\",\"title\":\"Hello World\",\"description\":\"Return a greeting from Ardour.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"}},\"additionalProperties\":false}},"
			"{\"name\":\"session/get_info\",\"title\":\"Session Info\",\"description\":\"Return basic session and transport info.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}},"
			"{\"name\":\"transport/get_state\",\"title\":\"Transport State\",\"description\":\"Return transport state.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}},"
			"{\"name\":\"transport/play\",\"title\":\"Transport Play\",\"description\":\"Start transport playback.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}},"
			"{\"name\":\"transport/stop\",\"title\":\"Transport Stop\",\"description\":\"Stop transport playback.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}},"
			"{\"name\":\"tracks/list\",\"title\":\"Tracks List\",\"description\":\"List session tracks.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"includeHidden\":{\"type\":\"boolean\"}},\"additionalProperties\":false}},"
			"{\"name\":\"track/get_info\",\"title\":\"Get Track Info\",\"description\":\"Get one track info (fader, pan, rec, mute, solo, sends, plugins).\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"}},\"required\":[\"id\"],\"additionalProperties\":false}},"
			"{\"name\":\"track/get_fader\",\"title\":\"Get Track Fader\",\"description\":\"Get current track fader as normalized position and dB.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"}},\"required\":[\"id\"],\"additionalProperties\":false}},"
			"{\"name\":\"track/set_fader\",\"title\":\"Set Track Fader\",\"description\":\"Set track fader by normalized position (0.0 to 1.0) or dB.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"position\":{\"type\":\"number\",\"minimum\":0,\"maximum\":1},\"db\":{\"type\":\"number\"}},\"required\":[\"id\"],\"oneOf\":[{\"required\":[\"position\"]},{\"required\":[\"db\"]}],\"additionalProperties\":false}}"
			"]}");
	}

	if (method == "tools/call") {
		std::string tool_name = root.get<std::string> ("params.name", "");
		if (tool_name == "hello_world") {
			std::string caller = root.get<std::string> ("params.arguments.name", "");
			std::string text = "Hello from Ardour";
			if (!caller.empty ()) {
				text += ", " + caller;
			}
			text += " (session: " + _session.name () + ")";

			return jsonrpc_result (
				id,
				std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"") + json_escape (text) + "\"}]}");
		}

		if (tool_name == "session/get_info") {
			std::string structured = session_info_json (_session);
			return jsonrpc_result (
				id,
				std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Session info\"}],\"structuredContent\":") + structured + "}");
		}

		if (tool_name == "transport/get_state") {
			std::string structured = transport_state_json (_session);
			return jsonrpc_result (
				id,
				std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Transport state\"}],\"structuredContent\":") + structured + "}");
		}

		if (tool_name == "transport/play") {
			_session.request_roll ();
			std::string structured = transport_state_json (_session);
			return jsonrpc_result (
				id,
				std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Transport play requested\"}],\"structuredContent\":") + structured + "}");
		}

		if (tool_name == "transport/stop") {
			_session.request_stop ();
			std::string structured = transport_state_json (_session);
			return jsonrpc_result (
				id,
				std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Transport stop requested\"}],\"structuredContent\":") + structured + "}");
		}

			if (tool_name == "tracks/list") {
				const bool include_hidden = root.get<bool> ("params.arguments.includeHidden", false);
				std::string structured = tracks_list_json (_session, include_hidden);
				return jsonrpc_result (
					id,
					std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Tracks listed\"}],\"structuredContent\":") + structured + "}");
			}

			if (tool_name == "track/get_info") {
				const std::string route_id = root.get<std::string> ("params.arguments.id", "");

				if (route_id.empty ()) {
					return jsonrpc_error (id, -32602, "Missing track id");
				}

				const std::shared_ptr<ARDOUR::Route> route = _session.route_by_id (PBD::ID (route_id));
				if (!route || !route->is_track ()) {
					return jsonrpc_error (id, -32602, "Track not found");
				}

				std::string structured = track_info_json (route);
				return jsonrpc_result (
					id,
					std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Track info\"}],\"structuredContent\":") + structured + "}");
			}

			if (tool_name == "track/get_fader") {
				const std::string route_id = root.get<std::string> ("params.arguments.id", "");

				if (route_id.empty ()) {
					return jsonrpc_error (id, -32602, "Missing track id");
				}

				const std::shared_ptr<ARDOUR::Route> route = _session.route_by_id (PBD::ID (route_id));
				if (!route || !route->is_track ()) {
					return jsonrpc_error (id, -32602, "Track not found");
				}

				std::shared_ptr<ARDOUR::AutomationControl> gain = route->gain_control ();
				if (!gain) {
					return jsonrpc_error (id, -32602, "Track has no gain control");
				}

				std::string structured = track_fader_json (route);
				return jsonrpc_result (
					id,
					std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Track fader state\"}],\"structuredContent\":") + structured + "}");
			}

			if (tool_name == "track/set_fader") {
				const std::string route_id = root.get<std::string> ("params.arguments.id", "");
				const boost::optional<double> position = root.get_optional<double> ("params.arguments.position");
				const boost::optional<double> db = root.get_optional<double> ("params.arguments.db");

			if (route_id.empty ()) {
				return jsonrpc_error (id, -32602, "Missing track id");
			}
			if (!position && !db) {
				return jsonrpc_error (id, -32602, "Provide one of: position (0.0 to 1.0) or db");
			}
			if (position && db) {
				return jsonrpc_error (id, -32602, "Provide only one of: position or db");
			}

			const std::shared_ptr<ARDOUR::Route> route = _session.route_by_id (PBD::ID (route_id));
			if (!route || !route->is_track ()) {
				return jsonrpc_error (id, -32602, "Track not found");
			}

			std::shared_ptr<ARDOUR::AutomationControl> gain = route->gain_control ();
			if (!gain) {
				return jsonrpc_error (id, -32602, "Track has no gain control");
			}

			double internal_gain = gain->get_value ();
			if (position) {
				if (!valid_fader_position (*position)) {
					return jsonrpc_error (id, -32602, "Invalid fader position (expected 0.0 to 1.0)");
				}

				/* Match OSC behavior for /fader: convert surface position to internal gain value. */
				internal_gain = gain->interface_to_internal (*position);
			} else {
				if (!valid_fader_db (*db)) {
					return jsonrpc_error (id, -32602, "Invalid dB value");
				}

				/* Match OSC behavior for /gain: map dB to coefficient, then clamp to control bounds. */
				internal_gain = (*db <= -192.0) ? 0.0 : dB_to_coefficient (*db);
				internal_gain = std::max (gain->lower (), std::min (gain->upper (), internal_gain));
			}

			gain->set_value (internal_gain, PBD::Controllable::NoGroup);

			std::string structured = track_fader_json (route);
			return jsonrpc_result (
				id,
				std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Track fader updated\"}],\"structuredContent\":") + structured + "}");
		}

		return jsonrpc_error (id, -32602, "Unknown tool name");
	}

	return jsonrpc_error (id, -32601, "Method not found");
}

int
MCPHttpServer::callback (struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len)
{
	ClientContext& ctx = client (wsi);
	int rc = 0;

	switch (reason) {
		case LWS_CALLBACK_HTTP:
			rc = handle_http (wsi, ctx);
			break;
		case LWS_CALLBACK_HTTP_BODY:
			rc = handle_http_body (wsi, ctx, in, len);
			break;
		case LWS_CALLBACK_HTTP_BODY_COMPLETION:
			rc = handle_http_body_completion (wsi, ctx);
			break;
		case LWS_CALLBACK_HTTP_WRITEABLE:
			rc = handle_http_writeable (wsi, ctx);
			break;
		case LWS_CALLBACK_CLOSED:
#ifdef LWS_CALLBACK_CLOSED_HTTP
		case LWS_CALLBACK_CLOSED_HTTP:
#endif
#ifdef LWS_CALLBACK_WSI_DESTROY
		case LWS_CALLBACK_WSI_DESTROY:
#endif
			erase_client (wsi);
			rc = 0;
			break;
#if ((LWS_LIBRARY_VERSION_MAJOR * 1000000) + (LWS_LIBRARY_VERSION_MINOR * 1000)) >= 2001000
		default:
			rc = lws_callback_http_dummy (wsi, reason, user, in, len);
			break;
#else
		default:
			rc = 0;
			break;
#endif
	}

	return rc;
}

int
MCPHttpServer::lws_callback (struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len)
{
	void* ctx_userdata = lws_context_user (lws_get_context (wsi));
	MCPHttpServer* server = static_cast<MCPHttpServer*> (ctx_userdata);
	if (!server) {
		return 0;
	}

	return server->callback (wsi, reason, user, in, len);
}
