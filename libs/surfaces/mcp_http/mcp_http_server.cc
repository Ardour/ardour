/*
 * Copyright (C) 2026 Frank Povazanj <frank.povazanj@gmail.com>
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
#include <chrono>
#include <cctype>
#include <climits>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>
#include <condition_variable>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include "pbd/error.h"
#include "pbd/basename.h"
#include "pbd/id.h"
#include "pbd/controllable.h"
#include "pbd/enumwriter.h"
#include "pbd/event_loop.h"
#include "pbd/memento_command.h"
#include "pbd/pthread_utils.h"
#include "pbd/stateful_diff_command.h"
#include "pbd/xml++.h"

#include "ardour/audioregion.h"
#include "ardour/audio_track.h"
#include "ardour/amp.h"
#include "ardour/dB.h"
#include "ardour/internal_send.h"
#include "ardour/location.h"
#include "ardour/midi_model.h"
#include "ardour/midi_region.h"
#include "ardour/midi_track.h"
#include "ardour/midi_source.h"
#include "ardour/plugin.h"
#include "ardour/plugin_manager.h"
#include "ardour/plugin_insert.h"
#include "ardour/playlist.h"
#include "ardour/presentation_info.h"
#include "ardour/processor.h"
#include "ardour/rc_configuration.h"
#include "ardour/region.h"
#include "ardour/region_factory.h"
#include "ardour/route.h"
#include "ardour/selection.h"
#include "ardour/session.h"
#include "ardour/session_event.h"
#include "ardour/source.h"
#include "ardour/stripable.h"
#include "ardour/tempo.h"
#include "ardour/track.h"

#include "mcp_http_server.h"

namespace pt = boost::property_tree;

using namespace ArdourSurface;

namespace {

class ScopedInstrumentPromptDisable {
public:
	ScopedInstrumentPromptDisable ()
		: _config (ARDOUR::Config)
		, _ask_replace (false)
		, _ask_setup (false)
		, _active (false)
	{
		if (!_config) {
			return;
		}

		_ask_replace = _config->get_ask_replace_instrument ();
		_ask_setup = _config->get_ask_setup_instrument ();
		_config->set_ask_replace_instrument (false);
		_config->set_ask_setup_instrument (false);
		_active = true;
	}

	~ScopedInstrumentPromptDisable ()
	{
		if (!_active || !_config) {
			return;
		}
		_config->set_ask_replace_instrument (_ask_replace);
		_config->set_ask_setup_instrument (_ask_setup);
	}

private:
	ARDOUR::RCConfiguration* _config;
	bool _ask_replace;
	bool _ask_setup;
	bool _active;
};

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

static std::string
canonical_tool_name (std::string tool_name)
{
	/* Some MCP clients only support function-safe identifiers, so accept
	 * underscore and dotted aliases in addition to slash-delimited names.
	 */
	std::replace (tool_name.begin (), tool_name.end (), '.', '/');

	if (tool_name.find ('/') != std::string::npos) {
		return tool_name;
	}

	static const char* known_groups[] = {
		"session",
		"transport",
		"markers",
		"tracks",
		"buses",
		"track",
		"region",
		"plugin",
		"midi_region",
		"midi_note"
	};

	for (size_t i = 0; i < (sizeof (known_groups) / sizeof (known_groups[0])); ++i) {
		const std::string group (known_groups[i]);
		const std::string prefix = group + "_";
		if (tool_name.size () <= prefix.size ()) {
			continue;
		}
		if (tool_name.compare (0, prefix.size (), prefix) == 0) {
			tool_name[group.size ()] = '/';
			return tool_name;
		}
	}

	return tool_name;
}

static bool
is_decimal_pbd_id_string (const std::string& s)
{
	if (s.empty ()) {
		return false;
	}

	for (std::string::const_iterator i = s.begin (); i != s.end (); ++i) {
		if (!std::isdigit ((unsigned char) *i)) {
			return false;
		}
	}

	return true;
}

static ARDOUR::Location*
location_by_mcp_id (ARDOUR::Locations& locations, const std::string& id)
{
	/* Defensive guard:
	 * PBD::ID(string) does not fail-closed on parse errors, so reject
	 * non-decimal MCP IDs before constructing an ID object.
	 */
	if (!is_decimal_pbd_id_string (id)) {
		return 0;
	}

	return locations.get_location_by_id (PBD::ID (id));
}

static std::shared_ptr<ARDOUR::Region>
region_by_mcp_id (const std::string& id)
{
	if (!is_decimal_pbd_id_string (id)) {
		return std::shared_ptr<ARDOUR::Region> ();
	}

	return ARDOUR::RegionFactory::region_by_id (PBD::ID (id));
}

static std::shared_ptr<ARDOUR::Route>
route_by_mcp_id (ARDOUR::Session& session, const std::string& id)
{
	if (!is_decimal_pbd_id_string (id)) {
		return std::shared_ptr<ARDOUR::Route> ();
	}

	return session.route_by_id (PBD::ID (id));
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

static int
clamp_debug_level (int level)
{
	if (level < 0) {
		return 0;
	}
	if (level > 2) {
		return 2;
	}
	return level;
}

static std::string
tool_result_with_structured_text_fallback (const std::string& result_json)
{
	/* Compatibility policy: whenever structuredContent is present, mirror it
	 * as serialized JSON in content[0].text for clients that ignore structure.
	 *
	 * This helper assumes the tool result shape used in this server:
	 * {"content":[...],"structuredContent":<json>}
	 */
	static const std::string key = "\"structuredContent\":";
	const std::string::size_type key_pos = result_json.find (key);
	if (key_pos == std::string::npos) {
		return result_json;
	}

	std::string::size_type obj_end = result_json.find_last_not_of (" \t\r\n");
	if (obj_end == std::string::npos || result_json[obj_end] != '}') {
		return result_json;
	}

	std::string::size_type value_start = key_pos + key.size ();
	while (value_start < result_json.size () && std::isspace ((unsigned char) result_json[value_start])) {
		++value_start;
	}
	if (value_start >= obj_end) {
		return result_json;
	}

	const std::string structured_json = result_json.substr (value_start, obj_end - value_start);
	return std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"")
		+ json_escape (structured_json)
		+ "\"}],\"structuredContent\":"
		+ structured_json
		+ "}";
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
	const std::string normalized_result_json = tool_result_with_structured_text_fallback (result_json);
	return std::string ("{\"jsonrpc\":\"2.0\",\"id\":") + id + ",\"result\":" + normalized_result_json + "}";
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

static const char*
record_state_string (ARDOUR::RecordState state)
{
	switch (state) {
	case ARDOUR::Disabled:
		return "disabled";
	case ARDOUR::Enabled:
		return "enabled";
	case ARDOUR::Recording:
		return "recording";
	default:
		return "unknown";
	}
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
quick_snapshot_name_localtime ()
{
	const std::time_t now = std::time (0);
	std::tm tm_now;
#ifdef _WIN32
	localtime_s (&tm_now, &now);
#else
	localtime_r (&now, &tm_now);
#endif

	char buf[64];
	if (std::strftime (buf, sizeof (buf), "%Y-%m-%dT%H.%M.%S", &tm_now) == 0) {
		return "snapshot";
	}
	return std::string (buf);
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
	std::shared_ptr<ARDOUR::RouteList const> routes = session.get_routes ();
	ARDOUR::RouteList sorted;

	if (routes) {
		sorted = *routes;
		sorted.sort (ARDOUR::Stripable::Sorter ());
	}

	std::ostringstream ss;
	ss << "{\"tracks\":[";

	bool first = true;
	for (ARDOUR::RouteList::const_iterator it = sorted.begin (); it != sorted.end (); ++it) {
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

static std::string
tracks_list_text (ARDOUR::Session& session, bool include_hidden)
{
	std::shared_ptr<ARDOUR::RouteList const> routes = session.get_routes ();
	ARDOUR::RouteList sorted;

	if (routes) {
		sorted = *routes;
		sorted.sort (ARDOUR::Stripable::Sorter ());
	}

	std::ostringstream ss;
	int count = 0;
	for (ARDOUR::RouteList::const_iterator it = sorted.begin (); it != sorted.end (); ++it) {
		const std::shared_ptr<ARDOUR::Route>& route = *it;
		if (!route) {
			continue;
		}
		if (route->is_hidden () && !include_hidden) {
			continue;
		}
		++count;
	}

	ss << "Tracks (" << count << "):";
	for (ARDOUR::RouteList::const_iterator it = sorted.begin (); it != sorted.end (); ++it) {
		const std::shared_ptr<ARDOUR::Route>& route = *it;
		if (!route) {
			continue;
		}
		if (route->is_hidden () && !include_hidden) {
			continue;
		}

		ss << "\n- " << route->name ()
		   << " (" << route_type_string (route)
		   << ", id " << route->id ().to_s ()
		   << ")";
	}

	return ss.str ();
}

static std::string
route_list_json (const ARDOUR::RouteList& routes)
{
	std::ostringstream ss;
	ss << "{\"count\":" << routes.size () << ",\"routes\":[";

	bool first = true;
	for (ARDOUR::RouteList::const_iterator it = routes.begin (); it != routes.end (); ++it) {
		const std::shared_ptr<ARDOUR::Route>& route = *it;
		if (!route) {
			continue;
		}

		if (!first) {
			ss << ",";
		}
		first = false;

		ss << "{\"id\":\"" << json_escape (route->id ().to_s ()) << "\""
		   << ",\"name\":\"" << json_escape (route->name ()) << "\""
		   << ",\"type\":\"" << route_type_string (route) << "\""
		   << ",\"trackNumber\":" << route->track_number ()
		   << ",\"presentationOrder\":" << route->presentation_info ().order ()
		   << ",\"hidden\":" << (route->is_hidden () ? "true" : "false")
		   << "}";
	}

	ss << "]}";
	return ss.str ();
}

static std::string
marker_type_json (ARDOUR::Location::Flags flags)
{
	static const struct TypeName {
		const char* enum_name;
		const char* wire_name;
	} names[] = {
		{"IsMark", "mark"},
		{"IsHidden", "hidden"},
		{"IsCueMarker", "cue"},
		{"IsCDMarker", "cd"},
		{"IsXrun", "xrun"},
		{"IsSection", "section"},
		{"IsScene", "scene"},
		{"IsRangeMarker", "range"},
		{"IsSessionRange", "session_range"},
		{"IsAutoLoop", "auto_loop"},
		{"IsAutoPunch", "auto_punch"},
		{"IsClockOrigin", "clock_origin"},
		{"IsSkip", "skip"}
	};

	const std::string flags_text = enum_2_string (flags);
	std::ostringstream ss;
	ss << "[";

	bool first = true;
	size_t start = 0;
	while (start < flags_text.size ()) {
		size_t comma = flags_text.find (',', start);
		if (comma == std::string::npos) {
			comma = flags_text.size ();
		}

		size_t token_begin = flags_text.find_first_not_of (" \t", start);
		size_t token_end = comma;
		while (token_end > start && (flags_text[token_end - 1] == ' ' || flags_text[token_end - 1] == '\t')) {
			--token_end;
		}
		if (token_begin == std::string::npos || token_begin >= token_end) {
			start = comma + 1;
			continue;
		}

		std::string token = flags_text.substr (token_begin, token_end - token_begin);
		for (size_t i = 0; i < (sizeof (names) / sizeof (names[0])); ++i) {
			if (token == names[i].enum_name) {
				token = names[i].wire_name;
				break;
			}
		}

		if (!first) {
			ss << ",";
		}
		first = false;
		ss << "\"" << json_escape (token) << "\"";

		start = comma + 1;
	}

	ss << "]";
	return ss.str ();
}

static std::string
bbt_json_at_sample (samplepos_t sample)
{
	Temporal::BBT_Time bbt = Temporal::TempoMap::use ()->bbt_at (Temporal::timepos_t (sample));

	std::ostringstream text;
	text << bbt.bars << "|" << bbt.beats << "|" << bbt.ticks;

	std::ostringstream ss;
	ss << "{\"bars\":" << bbt.bars
	   << ",\"beats\":" << bbt.beats
	   << ",\"ticks\":" << bbt.ticks
	   << ",\"text\":\"" << text.str () << "\"}";
	return ss.str ();
}

static std::string
markers_list_json (ARDOUR::Session& session)
{
	/* Ensure this thread has a current tempo-map pointer for any beat->audio conversions. */
	Temporal::TempoMap::fetch ();

	ARDOUR::Locations* locations = session.locations ();
	if (!locations) {
		return "{\"markers\":[]}";
	}

	struct MarkerSnapshot {
		std::string name;
		samplepos_t entry_start_sample;
		samplepos_t entry_end_sample;
		ARDOUR::Location::Flags flags;
		int32_t cue_id;
		bool have_cue;
		bool hidden;
		std::string location_id;
		std::string location_name;
		samplepos_t location_start_sample;
		samplepos_t location_end_sample;
		bool synthetic;
		bool boundary_start;
	};

	std::vector<MarkerSnapshot> markers;
	const ARDOUR::Locations::LocationList location_list = locations->list ();
	for (ARDOUR::Locations::LocationList::const_iterator it = location_list.begin (); it != location_list.end (); ++it) {
		ARDOUR::Location* loc = *it;
		if (!loc) {
			continue;
		}

		const ARDOUR::Location::Flags flags = loc->flags ();
		const std::string name = loc->name ();
		const samplepos_t start_sample = loc->start_sample ();
		const samplepos_t end_sample = loc->end_sample ();
		const std::string location_id = loc->id ().to_s ();
		const bool have_cue = loc->is_cue_marker ();
		const int32_t cue_id = have_cue ? loc->cue_id () : 0;

		if (loc->is_session_range ()) {
			/* Match OSC behavior: expose session bounds as synthetic "start"/"end" markers. */
			MarkerSnapshot start_marker;
			start_marker.name = "start";
			start_marker.entry_start_sample = start_sample;
			start_marker.entry_end_sample = start_sample;
			start_marker.flags = flags;
			start_marker.cue_id = 0;
			start_marker.have_cue = false;
			start_marker.hidden = false;
			start_marker.location_id = location_id;
			start_marker.location_name = name;
			start_marker.location_start_sample = start_sample;
			start_marker.location_end_sample = end_sample;
			start_marker.synthetic = true;
			start_marker.boundary_start = true;
			markers.push_back (start_marker);

			MarkerSnapshot end_marker;
			end_marker.name = "end";
			end_marker.entry_start_sample = end_sample;
			end_marker.entry_end_sample = end_sample;
			end_marker.flags = flags;
			end_marker.cue_id = 0;
			end_marker.have_cue = false;
			end_marker.hidden = false;
			end_marker.location_id = location_id;
			end_marker.location_name = name;
			end_marker.location_start_sample = start_sample;
			end_marker.location_end_sample = end_sample;
			end_marker.synthetic = true;
			end_marker.boundary_start = false;
			markers.push_back (end_marker);
			continue;
		}

		if (!(loc->is_mark () || loc->is_range_marker () || loc->is_auto_loop () || loc->is_auto_punch ())) {
			continue;
		}

		MarkerSnapshot marker;
		marker.name = name;
		marker.entry_start_sample = start_sample;
		marker.entry_end_sample = end_sample;
		marker.flags = flags;
		marker.cue_id = cue_id;
		marker.have_cue = have_cue;
		marker.hidden = loc->is_hidden ();
		marker.location_id = location_id;
		marker.location_name = name;
		marker.location_start_sample = start_sample;
		marker.location_end_sample = end_sample;
		marker.synthetic = false;
		marker.boundary_start = true;
		markers.push_back (marker);
	}

	std::sort (
		markers.begin (),
		markers.end (),
		[] (const MarkerSnapshot& a, const MarkerSnapshot& b) {
			if (a.entry_start_sample == b.entry_start_sample) {
				return a.name < b.name;
			}
			return a.entry_start_sample < b.entry_start_sample;
		});

	std::ostringstream ss;
	ss << "{\"markers\":[";

	bool first = true;
	for (size_t i = 0; i < markers.size (); ++i) {
		const MarkerSnapshot& marker = markers[i];
		if (!first) {
			ss << ",";
		}
		first = false;

		const bool is_mark = (marker.flags & ARDOUR::Location::IsMark) != 0;
		const bool is_cue = (marker.flags & ARDOUR::Location::IsCueMarker) != 0;
		const bool is_cd = (marker.flags & ARDOUR::Location::IsCDMarker) != 0;
		const bool is_xrun = (marker.flags & ARDOUR::Location::IsXrun) != 0;
		const bool is_section = (marker.flags & ARDOUR::Location::IsSection) != 0;
		const bool is_scene = (marker.flags & ARDOUR::Location::IsScene) != 0;
		const bool is_range_marker = (marker.flags & ARDOUR::Location::IsRangeMarker) != 0;
		const bool is_session_range = (marker.flags & ARDOUR::Location::IsSessionRange) != 0;
		const bool is_auto_loop = (marker.flags & ARDOUR::Location::IsAutoLoop) != 0;
		const bool is_auto_punch = (marker.flags & ARDOUR::Location::IsAutoPunch) != 0;
		const bool is_clock_origin = (marker.flags & ARDOUR::Location::IsClockOrigin) != 0;
		const bool is_skip = (marker.flags & ARDOUR::Location::IsSkip) != 0;
		const bool is_range = is_session_range || is_range_marker || is_auto_loop || is_auto_punch || is_cd;
		const samplepos_t entry_end_sample = std::max (marker.entry_start_sample, marker.entry_end_sample);
		const int64_t distance_from_start = (int64_t) marker.entry_start_sample - (int64_t) marker.location_start_sample;
		const std::string marker_bbt = bbt_json_at_sample (marker.entry_start_sample);
		const std::string marker_end_bbt = bbt_json_at_sample (entry_end_sample);
		const std::string location_start_bbt = bbt_json_at_sample (marker.location_start_sample);
		const std::string location_end_bbt = bbt_json_at_sample (marker.location_end_sample);

		ss << "{\"name\":\"" << json_escape (marker.name) << "\""
		   << ",\"label\":\"" << json_escape (marker.name) << "\""
		   << ",\"source\":\"" << (marker.synthetic ? "session_range_boundary" : "location") << "\""
		   << ",\"boundary\":\"" << (marker.synthetic ? (marker.boundary_start ? "start" : "end") : (is_range ? "range" : "point")) << "\""
		   << ",\"sortIndex\":" << i
		   << ",\"isSynthetic\":" << (marker.synthetic ? "true" : "false");

		ss << ",\"locationId\":\"" << json_escape (marker.location_id) << "\"";

		ss << ",\"locationName\":\"" << json_escape (marker.location_name) << "\""
		   << ",\"locationStartSample\":" << marker.location_start_sample
		   << ",\"locationEndSample\":" << marker.location_end_sample
		   << ",\"locationStartBbt\":" << location_start_bbt
		   << ",\"locationEndBbt\":" << location_end_bbt
		   << ",\"distanceFromLocationStartSamples\":" << distance_from_start
		   << ",\"startSample\":" << marker.entry_start_sample
		   << ",\"endSample\":" << entry_end_sample
		   << ",\"bbt\":" << marker_bbt
		   << ",\"endBbt\":" << marker_end_bbt
		   << ",\"lengthSamples\":" << (entry_end_sample - marker.entry_start_sample)
		   << ",\"isHidden\":" << (marker.hidden ? "true" : "false")
		   << ",\"isRange\":" << (is_range ? "true" : "false")
		   << ",\"flagBits\":" << (uint32_t) marker.flags
		   << ",\"isMark\":" << (is_mark ? "true" : "false")
		   << ",\"isCue\":" << (is_cue ? "true" : "false")
		   << ",\"isCD\":" << (is_cd ? "true" : "false")
		   << ",\"isXrun\":" << (is_xrun ? "true" : "false")
		   << ",\"isSection\":" << (is_section ? "true" : "false")
		   << ",\"isScene\":" << (is_scene ? "true" : "false")
		   << ",\"isRangeMarker\":" << (is_range_marker ? "true" : "false")
		   << ",\"isSessionRange\":" << (is_session_range ? "true" : "false")
		   << ",\"isAutoLoop\":" << (is_auto_loop ? "true" : "false")
		   << ",\"isAutoPunch\":" << (is_auto_punch ? "true" : "false")
		   << ",\"isClockOrigin\":" << (is_clock_origin ? "true" : "false")
		   << ",\"isSkip\":" << (is_skip ? "true" : "false")
		   << ",\"types\":" << marker_type_json (marker.flags);

		if (is_cue && marker.have_cue) {
			ss << ",\"cueId\":" << marker.cue_id;
		} else {
			ss << ",\"cueId\":null";
		}

		ss << "}";
	}

	ss << "]}";
	return ss.str ();
}

static std::string
marker_added_json (const ARDOUR::Location& location, bool used_default_name, const std::string& requested_name)
{
	const samplepos_t sample = location.start_sample ();
	const ARDOUR::Location::Flags flags = location.flags ();
	const std::string bbt = bbt_json_at_sample (sample);

	std::ostringstream ss;
	ss << "{\"locationId\":\"" << json_escape (location.id ().to_s ()) << "\""
	   << ",\"name\":\"" << json_escape (location.name ()) << "\""
	   << ",\"startSample\":" << sample
	   << ",\"endSample\":" << sample
	   << ",\"bbt\":" << bbt
	   << ",\"types\":" << marker_type_json (flags)
	   << ",\"usedDefaultName\":" << (used_default_name ? "true" : "false");

	if (requested_name.empty ()) {
		ss << ",\"requestedName\":null";
	} else {
		ss << ",\"requestedName\":\"" << json_escape (requested_name) << "\"";
	}

	ss << "}";
	return ss.str ();
}

static std::string
range_added_json (const ARDOUR::Location& location, bool used_default_name, const std::string& requested_name)
{
	const samplepos_t start_sample = location.start_sample ();
	const samplepos_t end_sample = std::max (start_sample, location.end_sample ());
	const ARDOUR::Location::Flags flags = location.flags ();
	const std::string start_bbt = bbt_json_at_sample (start_sample);
	const std::string end_bbt = bbt_json_at_sample (end_sample);

	std::ostringstream ss;
	ss << "{\"locationId\":\"" << json_escape (location.id ().to_s ()) << "\""
	   << ",\"name\":\"" << json_escape (location.name ()) << "\""
	   << ",\"startSample\":" << start_sample
	   << ",\"endSample\":" << end_sample
	   << ",\"startBbt\":" << start_bbt
	   << ",\"endBbt\":" << end_bbt
	   << ",\"lengthSamples\":" << (end_sample - start_sample)
	   << ",\"types\":" << marker_type_json (flags)
	   << ",\"usedDefaultName\":" << (used_default_name ? "true" : "false");

	if (requested_name.empty ()) {
		ss << ",\"requestedName\":null";
	} else {
		ss << ",\"requestedName\":\"" << json_escape (requested_name) << "\"";
	}

	ss << "}";
	return ss.str ();
}

static std::string
special_range_json (const ARDOUR::Location& location, const std::string& mode)
{
	const samplepos_t start_sample = location.start_sample ();
	const samplepos_t end_sample = std::max (start_sample, location.end_sample ());
	const ARDOUR::Location::Flags flags = location.flags ();
	const std::string start_bbt = bbt_json_at_sample (start_sample);
	const std::string end_bbt = bbt_json_at_sample (end_sample);
	const bool is_hidden = location.is_hidden ();

	std::ostringstream ss;
	ss << "{\"mode\":\"" << json_escape (mode) << "\""
	   << ",\"locationId\":\"" << json_escape (location.id ().to_s ()) << "\""
	   << ",\"name\":\"" << json_escape (location.name ()) << "\""
	   << ",\"startSample\":" << start_sample
	   << ",\"endSample\":" << end_sample
	   << ",\"startBbt\":" << start_bbt
	   << ",\"endBbt\":" << end_bbt
	   << ",\"lengthSamples\":" << (end_sample - start_sample)
	   << ",\"isHidden\":" << (is_hidden ? "true" : "false")
	   << ",\"types\":" << marker_type_json (flags)
	   << "}";
	return ss.str ();
}

static bool
parse_bbt_target_sample (int bar, double beat, samplepos_t& target_sample, std::string& error)
{
	error.clear ();

	if (bar < 1 || !std::isfinite (beat) || beat < 1.0) {
		error = "Invalid bar/beat (expected: bar>=1, beat>=1.0)";
		return false;
	}

	int32_t whole_beats = (int32_t) std::floor (beat);
	double fractional = beat - (double) whole_beats;

	if (whole_beats < 1 || fractional < 0.0) {
		error = "Invalid beat value";
		return false;
	}

	int32_t ticks = (int32_t) std::llround (fractional * (double) Temporal::ticks_per_beat);
	if (ticks >= Temporal::ticks_per_beat) {
		ticks = 0;
		++whole_beats;
	}

	Temporal::BBT_Argument bbt ((int32_t) bar, whole_beats, ticks);
	target_sample = Temporal::TempoMap::use ()->sample_at (bbt);
	return true;
}

static std::string
marker_deleted_json (
	const std::string& location_id,
	const std::string& name,
	samplepos_t start_sample,
	samplepos_t end_sample,
	ARDOUR::Location::Flags flags)
{
	const samplepos_t safe_end_sample = std::max (start_sample, end_sample);
	const std::string start_bbt = bbt_json_at_sample (start_sample);
	const std::string end_bbt = bbt_json_at_sample (safe_end_sample);

	std::ostringstream ss;
	ss << "{\"locationId\":\"" << json_escape (location_id) << "\""
	   << ",\"name\":\"" << json_escape (name) << "\""
	   << ",\"startSample\":" << start_sample
	   << ",\"endSample\":" << safe_end_sample
	   << ",\"bbt\":" << start_bbt
	   << ",\"endBbt\":" << end_bbt
	   << ",\"lengthSamples\":" << (safe_end_sample - start_sample)
	   << ",\"types\":" << marker_type_json (flags)
	   << "}";
	return ss.str ();
}

static std::string
marker_renamed_json (
	const std::string& location_id,
	const std::string& old_name,
	const std::string& new_name,
	samplepos_t start_sample,
	samplepos_t end_sample,
	ARDOUR::Location::Flags flags)
{
	const samplepos_t safe_end_sample = std::max (start_sample, end_sample);
	const std::string start_bbt = bbt_json_at_sample (start_sample);
	const std::string end_bbt = bbt_json_at_sample (safe_end_sample);

	std::ostringstream ss;
	ss << "{\"locationId\":\"" << json_escape (location_id) << "\""
	   << ",\"oldName\":\"" << json_escape (old_name) << "\""
	   << ",\"name\":\"" << json_escape (new_name) << "\""
	   << ",\"startSample\":" << start_sample
	   << ",\"endSample\":" << safe_end_sample
	   << ",\"bbt\":" << start_bbt
	   << ",\"endBbt\":" << end_bbt
	   << ",\"lengthSamples\":" << (safe_end_sample - start_sample)
	   << ",\"types\":" << marker_type_json (flags)
	   << "}";
	return ss.str ();
}

static bool
is_marker_or_range_location (const ARDOUR::Location& location)
{
	return location.is_mark () || location.is_range_marker ();
}

static ARDOUR::Location*
resolve_marker_location (
	ARDOUR::Locations& locations,
	const std::string& location_id,
	const std::string& name,
	const boost::optional<int64_t>& sample_opt,
	std::string& error)
{
	error.clear ();

	if (!location_id.empty ()) {
		ARDOUR::Location* by_id = location_by_mcp_id (locations, location_id);
		if (!by_id) {
			error = "Marker locationId not found";
			return 0;
		}
		if (!is_marker_or_range_location (*by_id)) {
			error = "Location is not a marker/range";
			return 0;
		}
		return by_id;
	}

	std::vector<ARDOUR::Location*> candidates;
	const ARDOUR::Locations::LocationList list = locations.list ();
	for (ARDOUR::Locations::LocationList::const_iterator it = list.begin (); it != list.end (); ++it) {
		ARDOUR::Location* loc = *it;
		if (!loc || !is_marker_or_range_location (*loc)) {
			continue;
		}
		if (loc->name () != name) {
			continue;
		}
		if (sample_opt && loc->start_sample () != (samplepos_t) *sample_opt) {
			continue;
		}
		candidates.push_back (loc);
	}

	if (candidates.empty ()) {
		error = "Marker not found by name/sample";
		return 0;
	}
	if (candidates.size () > 1) {
		error = "Ambiguous marker name; provide locationId or sample";
		return 0;
	}

	return candidates[0];
}

static bool
parse_optional_bbt_target_sample (
	const pt::ptree& root,
	const std::string& args_path,
	samplepos_t& target_sample,
	bool& have_target,
	std::string& error)
{
	have_target = false;
	error.clear ();

	const boost::optional<int> bar_opt = root.get_optional<int> (args_path + ".bar");
	const boost::optional<double> beat_opt = root.get_optional<double> (args_path + ".beat");

	if ((bar_opt && !beat_opt) || (!bar_opt && beat_opt)) {
		error = "Provide both bar and beat, or neither";
		return false;
	}

	if (!bar_opt && !beat_opt) {
		return true;
	}

	const int bar = *bar_opt;
	const double beat = *beat_opt;
	if (!parse_bbt_target_sample (bar, beat, target_sample, error)) {
		return false;
	}

	have_target = true;
	return true;
}

static bool
parse_optional_timeline_boundary_sample (
	const pt::ptree& root,
	const std::string& args_path,
	const std::string& sample_key,
	const std::string& bar_key,
	const std::string& beat_key,
	samplepos_t& target_sample,
	bool& have_target,
	std::string& error)
{
	have_target = false;
	error.clear ();

	const boost::optional<int64_t> sample_opt = root.get_optional<int64_t> (args_path + "." + sample_key);
	const boost::optional<int> bar_opt = root.get_optional<int> (args_path + "." + bar_key);
	const boost::optional<double> beat_opt = root.get_optional<double> (args_path + "." + beat_key);

	if ((bar_opt && !beat_opt) || (!bar_opt && beat_opt)) {
		error = std::string ("Provide both ") + bar_key + " and " + beat_key + ", or neither";
		return false;
	}

	if (sample_opt && (bar_opt || beat_opt)) {
		error = std::string ("Provide either ") + sample_key + " or " + bar_key + "+" + beat_key + ", not both";
		return false;
	}

	if (!sample_opt && !bar_opt && !beat_opt) {
		return true;
	}

	if (sample_opt) {
		if (*sample_opt < 0) {
			error = std::string ("Invalid ") + sample_key + " (expected >= 0)";
			return false;
		}
		target_sample = (samplepos_t) *sample_opt;
		have_target = true;
		return true;
	}

	if (!parse_bbt_target_sample (*bar_opt, *beat_opt, target_sample, error)) {
		error = std::string ("Invalid ") + bar_key + "/" + beat_key + ": " + error;
		return false;
	}

	have_target = true;
	return true;
}

static bool
resolve_region_argument_or_selected_at_playhead (
	ARDOUR::Session& session,
	const pt::ptree& root,
	const std::string& args_path,
	std::shared_ptr<ARDOUR::Region>& region,
	std::string& resolved_via,
	std::string& error)
{
	error.clear ();
	resolved_via.clear ();
	region.reset ();

	const std::string region_id = root.get<std::string> (args_path + ".regionId", "");
	if (!region_id.empty ()) {
		region = region_by_mcp_id (region_id);
		if (!region) {
			error = "regionId not found";
			return false;
		}
		resolved_via = "regionId";
		return true;
	}

	const std::shared_ptr<ARDOUR::Stripable> selected_stripable = session.selection ().first_selected_stripable ();
	if (!selected_stripable) {
		error = "Missing regionId and no selected track";
		return false;
	}

	const std::shared_ptr<ARDOUR::Route> selected_route = std::dynamic_pointer_cast<ARDOUR::Route> (selected_stripable);
	const std::shared_ptr<ARDOUR::Track> selected_track = std::dynamic_pointer_cast<ARDOUR::Track> (selected_route);
	if (!selected_track) {
		error = "Missing regionId and selected stripable is not a track";
		return false;
	}

	const std::shared_ptr<ARDOUR::Playlist> selected_playlist = selected_track->playlist ();
	if (!selected_playlist) {
		error = "Missing regionId and selected track has no playlist";
		return false;
	}

	const samplepos_t playhead_sample = session.transport_sample ();
	region = selected_playlist->top_unmuted_region_at (Temporal::timepos_t (playhead_sample));
	if (!region) {
		region = selected_playlist->top_region_at (Temporal::timepos_t (playhead_sample));
	}
	if (!region) {
		error = "Missing regionId and no region at playhead on selected track";
		return false;
	}

	resolved_via = "selectedTrackAtPlayhead";
	return true;
}

static bool
parse_range_endpoints (
	const pt::ptree& root,
	const std::string& args_path,
	samplepos_t& start_sample,
	samplepos_t& end_sample,
	std::string& error)
{
	error.clear ();
	start_sample = 0;
	end_sample = 0;

	const boost::optional<int64_t> start_sample_opt = root.get_optional<int64_t> (args_path + ".startSample");
	const boost::optional<int64_t> end_sample_opt = root.get_optional<int64_t> (args_path + ".endSample");
	const boost::optional<int> start_bar_opt = root.get_optional<int> (args_path + ".startBar");
	const boost::optional<double> start_beat_opt = root.get_optional<double> (args_path + ".startBeat");
	const boost::optional<int> end_bar_opt = root.get_optional<int> (args_path + ".endBar");
	const boost::optional<double> end_beat_opt = root.get_optional<double> (args_path + ".endBeat");

	if ((start_bar_opt && !start_beat_opt) || (!start_bar_opt && start_beat_opt)) {
		error = "Provide both startBar and startBeat, or neither";
		return false;
	}
	if ((end_bar_opt && !end_beat_opt) || (!end_bar_opt && end_beat_opt)) {
		error = "Provide both endBar and endBeat, or neither";
		return false;
	}
	if ((start_sample_opt && !end_sample_opt) || (!start_sample_opt && end_sample_opt)) {
		error = "Provide both startSample and endSample, or neither";
		return false;
	}

	const bool have_samples = start_sample_opt && end_sample_opt;
	const bool have_bbt = start_bar_opt && start_beat_opt && end_bar_opt && end_beat_opt;

	if (have_samples && (start_bar_opt || start_beat_opt || end_bar_opt || end_beat_opt)) {
		error = "Provide either sample pair or bar+beat pair, not both";
		return false;
	}
	if (!have_samples && !have_bbt) {
		error = "Missing range endpoints (provide sample pair or bar+beat pair)";
		return false;
	}

	if (have_samples) {
		if (*start_sample_opt < 0 || *end_sample_opt < 0) {
			error = "Invalid sample (expected >= 0)";
			return false;
		}
		start_sample = (samplepos_t) *start_sample_opt;
		end_sample = (samplepos_t) *end_sample_opt;
	} else {
		std::string bbt_error;
		if (!parse_bbt_target_sample (*start_bar_opt, *start_beat_opt, start_sample, bbt_error)) {
			error = std::string ("Invalid start ") + bbt_error;
			return false;
		}
		if (!parse_bbt_target_sample (*end_bar_opt, *end_beat_opt, end_sample, bbt_error)) {
			error = std::string ("Invalid end ") + bbt_error;
			return false;
		}
	}

	if (end_sample < start_sample) {
		error = "Invalid range: end before start";
		return false;
	}

	return true;
}

static std::string
midi_region_json (const std::shared_ptr<ARDOUR::Region>& region, const std::shared_ptr<ARDOUR::Track>& track)
{
	const samplepos_t start_sample = region->position_sample ();
	const samplepos_t end_sample = start_sample + region->length_samples ();

	std::ostringstream ss;
	ss << "{\"regionId\":\"" << json_escape (region->id ().to_s ()) << "\""
	   << ",\"name\":\"" << json_escape (region->name ()) << "\""
	   << ",\"type\":\"midi\""
	   << ",\"trackId\":\"" << json_escape (track->id ().to_s ()) << "\""
	   << ",\"trackName\":\"" << json_escape (track->name ()) << "\"";

	const std::shared_ptr<ARDOUR::Playlist> playlist = region->playlist ();
	if (playlist) {
		ss << ",\"playlistId\":\"" << json_escape (playlist->id ().to_s ()) << "\"";
	} else {
		ss << ",\"playlistId\":null";
	}

	ss << ",\"startSample\":" << start_sample
	   << ",\"endSample\":" << end_sample
	   << ",\"lengthSamples\":" << region->length_samples ()
	   << ",\"startBbt\":" << bbt_json_at_sample (start_sample)
	   << ",\"endBbt\":" << bbt_json_at_sample (end_sample)
	   << ",\"locked\":" << (region->locked () ? "true" : "false")
	   << ",\"positionLocked\":" << (region->position_locked () ? "true" : "false")
	   << "}";

	return ss.str ();
}

struct MidiJsonEventDef {
	int bar;
	double beat;
	int tick;
	int note;
	int velocity;
	int channel;
	std::string type;
};

struct MidiJsonNoteDef {
	double start_quarters;
	double length_quarters;
	int note;
	int velocity;
	int channel;
};

static bool
parse_time_signature (const std::string& sig, int& numerator, int& denominator)
{
	const std::string::size_type slash = sig.find ('/');
	if (slash == std::string::npos || slash == 0 || (slash + 1) >= sig.size ()) {
		return false;
	}

	const std::string num = sig.substr (0, slash);
	const std::string den = sig.substr (slash + 1);
	char* endptr = 0;
	long n = std::strtol (num.c_str (), &endptr, 10);
	if (!endptr || *endptr != '\0') {
		return false;
	}
	endptr = 0;
	long d = std::strtol (den.c_str (), &endptr, 10);
	if (!endptr || *endptr != '\0') {
		return false;
	}

	if (n <= 0 || d <= 0 || n > 64 || d > 64) {
		return false;
	}

	numerator = (int) n;
	denominator = (int) d;
	return true;
}

static std::string
json_string_array (const std::vector<std::string>& values)
{
	std::ostringstream ss;
	ss << "[";
	for (size_t i = 0; i < values.size (); ++i) {
		if (i > 0) {
			ss << ",";
		}
		ss << "\"" << json_escape (values[i]) << "\"";
	}
	ss << "]";
	return ss.str ();
}

static double
midi_json_event_quarters (const MidiJsonEventDef& e, int numerator, int denominator, int ticks_per_quarter)
{
	const double quarters_per_bar = (double) numerator * (4.0 / (double) denominator);
	const double quarters_per_beat_unit = 4.0 / (double) denominator;
	return ((double) (e.bar - 1) * quarters_per_bar)
		+ ((e.beat - 1.0) * quarters_per_beat_unit)
		+ ((double) e.tick / (double) ticks_per_quarter);
}

static double
beats_to_double (const Temporal::Beats& beats)
{
	return beats.get_beats () + (beats.get_ticks () / (double) Temporal::ticks_per_beat);
}

static bool
quarters_to_midi_json_time (
	double quarters,
	int time_sig_num,
	int time_sig_den,
	int ticks_per_quarter,
	int& bar_out,
	int& beat_out,
	int& tick_out)
{
	if (!std::isfinite (quarters) || quarters < 0.0 || ticks_per_quarter <= 0 || time_sig_num <= 0 || time_sig_den <= 0) {
		return false;
	}

	const double quarters_per_bar = (double) time_sig_num * (4.0 / (double) time_sig_den);
	const double quarters_per_beat_unit = 4.0 / (double) time_sig_den;
	if (!std::isfinite (quarters_per_bar) || quarters_per_bar <= 0.0 || !std::isfinite (quarters_per_beat_unit) || quarters_per_beat_unit <= 0.0) {
		return false;
	}

	const int ticks_per_beat_unit = std::max (1, (int) std::llround (quarters_per_beat_unit * (double) ticks_per_quarter));

	int bar_index = (int) std::floor (quarters / quarters_per_bar);
	double in_bar = quarters - ((double) bar_index * quarters_per_bar);
	if (in_bar < 0.0) {
		in_bar = 0.0;
	}

	int beat_index = (int) std::floor (in_bar / quarters_per_beat_unit);
	if (beat_index < 0) {
		beat_index = 0;
	}
	if (beat_index >= time_sig_num) {
		beat_index = time_sig_num - 1;
	}

	double in_beat = in_bar - ((double) beat_index * quarters_per_beat_unit);
	if (in_beat < 0.0) {
		in_beat = 0.0;
	}
	if (in_beat > quarters_per_beat_unit) {
		in_beat = quarters_per_beat_unit;
	}

	int tick = (int) std::llround (in_beat * (double) ticks_per_quarter);
	if (tick >= ticks_per_beat_unit) {
		tick -= ticks_per_beat_unit;
		++beat_index;
	}
	if (beat_index >= time_sig_num) {
		beat_index = 0;
		++bar_index;
	}

	bar_out = bar_index + 1;
	beat_out = beat_index + 1;
	tick_out = std::max (0, tick);
	return true;
}

static bool
parse_midi_json_events (
	const pt::ptree& midi_root,
	std::vector<MidiJsonEventDef>& expanded_events,
	int& channel,
	bool& is_drum_mode,
	int& ticks_per_quarter,
	int& time_sig_num,
	int& time_sig_den,
	std::vector<std::string>& warnings,
	std::string& error)
{
	expanded_events.clear ();
	warnings.clear ();
	error.clear ();

	const int64_t channel_in = midi_root.get<int64_t> ("channel", 9);
	if (channel_in < 0 || channel_in > 15) {
		error = "Invalid midi.channel (expected 0..15)";
		return false;
	}
	channel = (int) channel_in;

	is_drum_mode = midi_root.get<bool> ("is_drum_mode", true);

	const int64_t tpq_in = midi_root.get<int64_t> ("ticks_per_quarter", 480);
	if (tpq_in <= 0 || tpq_in > 96000) {
		error = "Invalid midi.ticks_per_quarter (expected 1..96000)";
		return false;
	}
	ticks_per_quarter = (int) tpq_in;

	const std::string time_signature = midi_root.get<std::string> ("time_signature", "4/4");
	if (!parse_time_signature (time_signature, time_sig_num, time_sig_den)) {
		error = "Invalid midi.time_signature (expected format N/D)";
		return false;
	}

	const boost::optional<const pt::ptree&> midi_events = midi_root.get_child_optional ("midi_events");
	if (!midi_events) {
		error = "Missing midi.midi_events (expected an array of event objects)";
		return false;
	}

	std::map<int, std::vector<MidiJsonEventDef> > events_by_bar;
	for (pt::ptree::const_iterator it = midi_events->begin (); it != midi_events->end (); ++it) {
		const pt::ptree& ev = it->second;

		if (ev.get_optional<std::string> ("comment")) {
			continue;
		}

		const boost::optional<int64_t> bar_opt = ev.get_optional<int64_t> ("bar");
		if (!bar_opt || *bar_opt < 1 || *bar_opt > 1000000) {
			error = "Each midi event must provide bar >= 1";
			return false;
		}
		const int bar = (int) *bar_opt;

		const boost::optional<int64_t> repeat_opt = ev.get_optional<int64_t> ("repeat");
		if (repeat_opt) {
			if (*repeat_opt < 0 || *repeat_opt > 1000000) {
				error = "Invalid repeat value (expected >= 0)";
				return false;
			}

			const int source_bar = (*repeat_opt == 0) ? (bar - 1) : (int) *repeat_opt;
			std::map<int, std::vector<MidiJsonEventDef> >::const_iterator src = events_by_bar.find (source_bar);
			if (src == events_by_bar.end ()) {
				std::ostringstream w;
				w << "Repeat skipped at bar " << bar << ": source bar " << source_bar << " not found";
				warnings.push_back (w.str ());
				continue;
			}

			for (size_t i = 0; i < src->second.size (); ++i) {
				MidiJsonEventDef copy = src->second[i];
				copy.bar = bar;
				expanded_events.push_back (copy);
				events_by_bar[bar].push_back (copy);
			}
			continue;
		}

		const boost::optional<double> beat_opt = ev.get_optional<double> ("b");
		if (!beat_opt || !std::isfinite (*beat_opt) || *beat_opt < 1.0) {
			error = "Each normal midi event must provide b >= 1";
			return false;
		}

		const boost::optional<int64_t> note_opt = ev.get_optional<int64_t> ("n");
		if (!note_opt || *note_opt < 0 || *note_opt > 127) {
			error = "Each normal midi event must provide n (0..127)";
			return false;
		}

		const int64_t tick_in = ev.get<int64_t> ("t", 0);
		if (tick_in < 0) {
			error = "Invalid midi event t (expected >= 0)";
			return false;
		}

		const int64_t velocity_in = ev.get<int64_t> ("v", 64);
		if (velocity_in < 0 || velocity_in > 127) {
			error = "Invalid midi event v (expected 0..127)";
			return false;
		}

		const boost::optional<int64_t> event_channel_opt = ev.get_optional<int64_t> ("channel");
		int event_channel = channel;
		if (event_channel_opt) {
			if (*event_channel_opt < 0 || *event_channel_opt > 15) {
				error = "Invalid midi event channel (expected 0..15)";
				return false;
			}
			event_channel = (int) *event_channel_opt;
		}

		std::string type = ev.get<std::string> ("type", "note_on");
		std::transform (type.begin (), type.end (), type.begin (), ::tolower);

		MidiJsonEventDef out;
		out.bar = bar;
		out.beat = *beat_opt;
		out.tick = (int) tick_in;
		out.note = (int) *note_opt;
		out.velocity = (int) velocity_in;
		out.channel = event_channel;
		out.type = type;

		expanded_events.push_back (out);
		events_by_bar[bar].push_back (out);
	}

	return true;
}

static bool
build_midi_json_note_defs (
	const std::vector<MidiJsonEventDef>& expanded_events,
	bool is_drum_mode,
	int ticks_per_quarter,
	int time_sig_num,
	int time_sig_den,
	std::vector<MidiJsonNoteDef>& notes,
	std::vector<std::string>& warnings,
	std::string& error)
{
	notes.clear ();
	error.clear ();

	struct TimedEvent {
		double quarters;
		size_t ordinal;
		MidiJsonEventDef ev;
	};

	std::vector<TimedEvent> events;
	events.reserve (expanded_events.size ());
	for (size_t i = 0; i < expanded_events.size (); ++i) {
		const double q = midi_json_event_quarters (expanded_events[i], time_sig_num, time_sig_den, ticks_per_quarter);
		if (!std::isfinite (q) || q < 0.0) {
			error = "Invalid event timing produced by midi_events";
			return false;
		}

		TimedEvent te;
		te.quarters = q;
		te.ordinal = i;
		te.ev = expanded_events[i];
		events.push_back (te);
	}

	std::sort (
		events.begin (),
		events.end (),
		[] (const TimedEvent& a, const TimedEvent& b) {
			if (a.quarters != b.quarters) {
				return a.quarters < b.quarters;
			}
			return a.ordinal < b.ordinal;
		});

	if (is_drum_mode) {
		const double default_length = 0.0;

		for (size_t i = 0; i < events.size (); ++i) {
			MidiJsonNoteDef n;
			n.start_quarters = events[i].quarters;
			n.length_quarters = default_length;
			n.note = events[i].ev.note;
			n.velocity = events[i].ev.velocity;
			n.channel = events[i].ev.channel;
			notes.push_back (n);
		}
		return true;
	}

	struct PendingOn {
		double quarters;
		int velocity;
	};
	std::map<int, std::vector<PendingOn> > active_by_note;

	for (size_t i = 0; i < events.size (); ++i) {
		const MidiJsonEventDef& ev = events[i].ev;
		const bool is_off = (ev.type == "note_off") || (ev.velocity == 0);
		const bool is_on = (ev.type.empty () || ev.type == "note_on");
		const int note_key = (ev.channel * 128) + ev.note;

		if (is_off) {
			std::vector<PendingOn>& stack = active_by_note[note_key];
			if (stack.empty ()) {
				std::ostringstream w;
				w << "Unmatched note_off skipped for note " << ev.note << " on channel " << (ev.channel + 1) << " at bar " << ev.bar;
				warnings.push_back (w.str ());
				continue;
			}

			const PendingOn on = stack.back ();
			stack.pop_back ();

			const double len = events[i].quarters - on.quarters;
			if (!std::isfinite (len) || len <= 0.0) {
				std::ostringstream w;
				w << "Non-positive note length skipped for note " << ev.note << " at bar " << ev.bar;
				warnings.push_back (w.str ());
				continue;
			}

			MidiJsonNoteDef n;
			n.start_quarters = on.quarters;
			n.length_quarters = len;
			n.note = ev.note;
			n.velocity = on.velocity;
			n.channel = ev.channel;
			notes.push_back (n);
			continue;
		}

		if (!is_on) {
			std::ostringstream w;
			w << "Unknown event type '" << ev.type << "' skipped at bar " << ev.bar;
			warnings.push_back (w.str ());
			continue;
		}

		PendingOn on;
		on.quarters = events[i].quarters;
		on.velocity = ev.velocity;
		active_by_note[note_key].push_back (on);
	}

	for (std::map<int, std::vector<PendingOn> >::const_iterator it = active_by_note.begin (); it != active_by_note.end (); ++it) {
		if (!it->second.empty ()) {
			const int channel = it->first / 128;
			const int note = it->first % 128;
			std::ostringstream w;
			w << "Unclosed note_on skipped for note " << note << " on channel " << (channel + 1)
			  << " (" << it->second.size () << " pending)";
			warnings.push_back (w.str ());
		}
	}

	return true;
}

static bool
create_midi_region_on_track (
	ARDOUR::Session& session,
	const std::shared_ptr<ARDOUR::Track>& track,
	samplepos_t start_sample,
	samplepos_t end_sample,
	const std::string& requested_name,
	std::shared_ptr<ARDOUR::Region>& out_region,
	std::string& error)
{
	error.clear ();
	out_region.reset ();

	const std::shared_ptr<ARDOUR::MidiTrack> midi_track = std::dynamic_pointer_cast<ARDOUR::MidiTrack> (track);
	if (!midi_track) {
		error = "trackId is not a MIDI track";
		return false;
	}

	const std::shared_ptr<ARDOUR::Playlist> playlist = midi_track->playlist ();
	if (!playlist) {
		error = "Track has no playlist";
		return false;
	}

	const Temporal::timepos_t start_pos (start_sample);
	const Temporal::timepos_t end_pos (end_sample);
	const Temporal::timecnt_t region_length = start_pos.distance (end_pos);
	if (region_length.samples () <= 0) {
		error = "Invalid region length";
		return false;
	}

	std::shared_ptr<ARDOUR::MidiSource> midi_src;
	try {
		midi_src = session.create_midi_source_by_stealing_name (track);
	} catch (...) {
		midi_src.reset ();
	}
	if (!midi_src) {
		error = "Failed to create MIDI source";
		return false;
	}

	ARDOUR::SourceList srcs;
	srcs.push_back (std::dynamic_pointer_cast<ARDOUR::Source> (midi_src));
	if (srcs.empty () || !srcs.front ()) {
		error = "Failed to resolve MIDI source";
		return false;
	}

	const Temporal::timecnt_t source_start (Temporal::BeatTime); /* zero beats */

	PBD::PropertyList whole_file_props;
	whole_file_props.add (ARDOUR::Properties::start, source_start);
	whole_file_props.add (ARDOUR::Properties::length, region_length);
	whole_file_props.add (ARDOUR::Properties::automatic, true);
	whole_file_props.add (ARDOUR::Properties::whole_file, true);
	whole_file_props.add (ARDOUR::Properties::name, PBD::basename_nosuffix (midi_src->name ()));
	whole_file_props.add (ARDOUR::Properties::opaque, session.config.get_draw_opaque_midi_regions ());

	std::shared_ptr<ARDOUR::Region> whole_file_region = ARDOUR::RegionFactory::create (srcs, whole_file_props);
	if (!whole_file_region) {
		error = "Failed to create whole-file MIDI region";
		return false;
	}

	PBD::PropertyList playlist_region_props;
	if (requested_name.empty ()) {
		playlist_region_props.add (ARDOUR::Properties::name, whole_file_region->name ());
	} else {
		playlist_region_props.add (ARDOUR::Properties::name, requested_name);
	}

	std::shared_ptr<ARDOUR::Region> region = ARDOUR::RegionFactory::create (whole_file_region, playlist_region_props);
	if (!region) {
		error = "Failed to create MIDI playlist region";
		return false;
	}

	session.begin_reversible_command ("add midi region");
	playlist->clear_changes ();
	playlist->clear_owned_changes ();
	region->set_position (start_pos);
	playlist->add_region (region, start_pos, 1.0, false);
	playlist->rdiff_and_add_command (&session);
	session.commit_reversible_command ();

	out_region = region;
	return true;
}

static std::string
midi_region_brief_json (const std::shared_ptr<ARDOUR::Region>& region)
{
	const samplepos_t start_sample = region->position_sample ();
	const samplepos_t end_sample = start_sample + region->length_samples ();

	std::ostringstream ss;
	ss << "{\"regionId\":\"" << json_escape (region->id ().to_s ()) << "\""
	   << ",\"name\":\"" << json_escape (region->name ()) << "\""
	   << ",\"type\":\"midi\"";

	const std::shared_ptr<ARDOUR::Playlist> playlist = region->playlist ();
	if (playlist) {
		ss << ",\"playlistId\":\"" << json_escape (playlist->id ().to_s ()) << "\"";
	} else {
		ss << ",\"playlistId\":null";
	}

	ss << ",\"startSample\":" << start_sample
	   << ",\"endSample\":" << end_sample
	   << ",\"lengthSamples\":" << region->length_samples ()
	   << ",\"startBbt\":" << bbt_json_at_sample (start_sample)
	   << ",\"endBbt\":" << bbt_json_at_sample (end_sample)
	   << "}";
	return ss.str ();
}

static std::string
midi_note_json (
	const std::shared_ptr<ARDOUR::Region>& region,
	const std::shared_ptr<Evoral::Note<Temporal::Beats> >& note,
	const std::string& position_origin)
{
	const Temporal::Beats start_source_beats = note->time ();
	const Temporal::Beats length_beats = note->length ();
	const Temporal::Beats end_source_beats = start_source_beats + length_beats;
	const Temporal::Beats start_region_beats = region->source_beats_to_region_time (start_source_beats).beats ();
	const Temporal::Beats end_region_beats = region->source_beats_to_region_time (end_source_beats).beats ();
	const double start_source_beats_double = start_source_beats.get_beats () + (start_source_beats.get_ticks () / (double) Temporal::ticks_per_beat);
	const double start_region_beats_double = start_region_beats.get_beats () + (start_region_beats.get_ticks () / (double) Temporal::ticks_per_beat);
	const double length_beats_double = length_beats.get_beats () + (length_beats.get_ticks () / (double) Temporal::ticks_per_beat);
	const double end_source_beats_double = end_source_beats.get_beats () + (end_source_beats.get_ticks () / (double) Temporal::ticks_per_beat);
	const double end_region_beats_double = end_region_beats.get_beats () + (end_region_beats.get_ticks () / (double) Temporal::ticks_per_beat);

	const samplepos_t start_sample = region->source_beats_to_absolute_time (start_source_beats).samples ();
	const samplepos_t end_sample = region->source_beats_to_absolute_time (end_source_beats).samples ();

	std::ostringstream ss;
	ss << "{\"regionId\":\"" << json_escape (region->id ().to_s ()) << "\""
	   << ",\"regionName\":\"" << json_escape (region->name ()) << "\""
	   << ",\"noteId\":" << note->id ()
	   << ",\"note\":" << (int) note->note ()
	   << ",\"velocity\":" << (int) note->velocity ()
	   << ",\"channel\":" << ((int) note->channel () + 1)
	   << ",\"channelRaw\":" << (int) note->channel ()
	   << ",\"startRegionBeats\":" << start_region_beats_double
	   << ",\"startSourceBeats\":" << start_source_beats_double
	   << ",\"lengthBeats\":" << length_beats_double
	   << ",\"endRegionBeats\":" << end_region_beats_double
	   << ",\"endSourceBeats\":" << end_source_beats_double
	   << ",\"startSample\":" << start_sample
	   << ",\"endSample\":" << end_sample
	   << ",\"startBbt\":" << bbt_json_at_sample (start_sample)
	   << ",\"endBbt\":" << bbt_json_at_sample (end_sample)
	   << ",\"positionOrigin\":\"" << json_escape (position_origin) << "\""
	   << "}";

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
	if (std::isnan (d)) {
		return false;
	}

	if (std::isinf (d)) {
		return d < 0.0;
	}

	/* Match OSC silence floor and enforce explicit MCP dB bounds. */
	return d >= -193.0 && d <= 6.0;
}

static bool
db_is_silence_floor (double db)
{
	return !std::isfinite (db) || db <= -192.0;
}

static double
db_to_gain_with_floor (double db)
{
	return db_is_silence_floor (db) ? 0.0 : dB_to_coefficient (db);
}

static double
normalized_db_value (double db)
{
	if (db_is_silence_floor (db)) {
		return -193.0;
	}

	/* Avoid scientific-notation near-zero noise in JSON output. */
	return (std::fabs (db) < 1e-6) ? 0.0 : db;
}

static std::string
track_fader_json (const std::shared_ptr<ARDOUR::Route>& route)
{
	std::shared_ptr<ARDOUR::AutomationControl> gain = route ? route->gain_control () : std::shared_ptr<ARDOUR::AutomationControl> ();
	const double position = gain ? gain->internal_to_interface (gain->get_value ()) : 0.0;
	double db = -193.0;

	if (gain && gain->get_value () > 0.0) {
		db = accurate_coefficient_to_dB (gain->get_value ());
		if (!std::isfinite (db)) {
			db = -193.0;
		}
	}

	std::ostringstream ss;
	ss << "{\"id\":\"" << json_escape (route->id ().to_s ()) << "\""
	   << ",\"name\":\"" << json_escape (route->name ()) << "\""
	   << ",\"position\":" << position
	   << ",\"db\":" << normalized_db_value (db)
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
			db = accurate_coefficient_to_dB (gain->get_value ());
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
			   << ",\"preFader\":" << (p->get_pre_fader () ? "true" : "false")
			   << ",\"postFader\":" << (p->get_pre_fader () ? "false" : "true")
			   << ",\"position\":" << position
			   << ",\"db\":" << normalized_db_value (db);

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
send_level_json (const std::shared_ptr<ARDOUR::Route>& route, uint32_t send_index)
{
	std::shared_ptr<ARDOUR::Processor> p = route ? route->nth_send (send_index) : std::shared_ptr<ARDOUR::Processor> ();
	std::shared_ptr<ARDOUR::AutomationControl> gain = route ? route->send_level_controllable (send_index) : std::shared_ptr<ARDOUR::AutomationControl> ();
	const double position = gain ? gain->internal_to_interface (gain->get_value ()) : 0.0;
	double db = -193.0;

	if (gain && gain->get_value () > 0.0) {
		db = accurate_coefficient_to_dB (gain->get_value ());
		if (!std::isfinite (db)) {
			db = -193.0;
		}
	}

	std::ostringstream ss;
	ss << "{\"id\":\"" << json_escape (route->id ().to_s ()) << "\""
	   << ",\"routeName\":\"" << json_escape (route->name ()) << "\""
	   << ",\"sendIndex\":" << send_index
	   << ",\"name\":\"" << json_escape (route->send_name (send_index)) << "\""
	   << ",\"active\":" << ((p && p->active ()) ? "true" : "false")
	   << ",\"preFader\":" << ((p && p->get_pre_fader ()) ? "true" : "false")
	   << ",\"postFader\":" << ((p && p->get_pre_fader ()) ? "false" : "true")
	   << ",\"position\":" << position
	   << ",\"db\":" << normalized_db_value (db);

	std::shared_ptr<ARDOUR::InternalSend> isend = std::dynamic_pointer_cast<ARDOUR::InternalSend> (p);
	if (isend) {
		std::shared_ptr<ARDOUR::Route> target = isend->target_route ();
		if (target) {
			ss << ",\"targetRouteId\":\"" << json_escape (target->id ().to_s ()) << "\""
			   << ",\"targetRouteName\":\"" << json_escape (target->name ()) << "\"";
		}
	}

	ss << "}";
	return ss.str ();
}

static bool
find_internal_send_index (
	const std::shared_ptr<ARDOUR::Route>& source_route,
	const std::shared_ptr<ARDOUR::Route>& target_route,
	uint32_t& send_index_out)
{
	if (!source_route || !target_route) {
		return false;
	}

	for (uint32_t i = 0;; ++i) {
		std::shared_ptr<ARDOUR::Processor> p = source_route->nth_send (i);
		if (!p) {
			break;
		}

		std::shared_ptr<ARDOUR::InternalSend> isend = std::dynamic_pointer_cast<ARDOUR::InternalSend> (p);
		if (!isend) {
			continue;
		}

		std::shared_ptr<ARDOUR::Route> target = isend->target_route ();
		if (target && target->id () == target_route->id ()) {
			send_index_out = i;
			return true;
		}
	}

	return false;
}

static bool
recreate_aux_send_with_position (
	const std::shared_ptr<ARDOUR::Route>& source_route,
	uint32_t send_index,
	bool post_fader,
	uint32_t& resolved_send_index,
	std::string& error)
{
	if (!source_route) {
		error = "Route not found";
		return false;
	}

	std::shared_ptr<ARDOUR::Processor> send_proc = source_route->nth_send (send_index);
	if (!send_proc) {
		error = "Send not found";
		return false;
	}

	std::shared_ptr<ARDOUR::InternalSend> isend = std::dynamic_pointer_cast<ARDOUR::InternalSend> (send_proc);
	if (!isend) {
		error = "Only internal aux sends are supported";
		return false;
	}

	std::shared_ptr<ARDOUR::Route> target_route = isend->target_route ();
	if (!target_route) {
		error = "Send target route not found";
		return false;
	}

	if (((!send_proc->get_pre_fader ()) && post_fader) || (send_proc->get_pre_fader () && !post_fader)) {
		resolved_send_index = send_index;
		return true;
	}

	std::shared_ptr<ARDOUR::AutomationControl> send_gain = source_route->send_level_controllable (send_index);
	std::shared_ptr<ARDOUR::AutomationControl> send_enable = source_route->send_enable_controllable (send_index);
	std::shared_ptr<ARDOUR::AutomationControl> send_pan = source_route->send_pan_azimuth_controllable (send_index);
	std::shared_ptr<ARDOUR::AutomationControl> send_pan_enable = source_route->send_pan_azimuth_enable_controllable (send_index);

	const bool have_gain = !!send_gain;
	const bool have_enable = !!send_enable;
	const bool have_pan = !!send_pan;
	const bool have_pan_enable = !!send_pan_enable;
	const bool processor_enabled = send_proc->enabled ();
	const double gain_value = have_gain ? send_gain->get_value () : 0.0;
	const double enable_value = have_enable ? send_enable->get_value () : 0.0;
	const double pan_value = have_pan ? send_pan->get_value () : 0.0;
	const double pan_enable_value = have_pan_enable ? send_pan_enable->get_value () : 0.0;

	if (source_route->remove_processor (send_proc) != 0) {
		error = "Failed to remove existing send";
		return false;
	}

	std::shared_ptr<ARDOUR::Processor> before = source_route->before_processor_for_placement (post_fader ? ARDOUR::PostFader : ARDOUR::PreFader);
	if (source_route->add_aux_send (target_route, before) != 0) {
		error = "Failed to re-create send at requested position";
		return false;
	}

	if (!find_internal_send_index (source_route, target_route, resolved_send_index)) {
		error = "Send not found after re-create";
		return false;
	}

	if (have_gain) {
		if (std::shared_ptr<ARDOUR::AutomationControl> c = source_route->send_level_controllable (resolved_send_index)) {
			c->set_value (gain_value, PBD::Controllable::NoGroup);
		}
	}
	if (have_enable) {
		if (std::shared_ptr<ARDOUR::AutomationControl> c = source_route->send_enable_controllable (resolved_send_index)) {
			c->set_value (enable_value, PBD::Controllable::NoGroup);
		}
	} else {
		if (std::shared_ptr<ARDOUR::Processor> p = source_route->nth_send (resolved_send_index)) {
			p->enable (processor_enabled);
		}
	}
	if (have_pan) {
		if (std::shared_ptr<ARDOUR::AutomationControl> c = source_route->send_pan_azimuth_controllable (resolved_send_index)) {
			c->set_value (pan_value, PBD::Controllable::NoGroup);
		}
	}
	if (have_pan_enable) {
		if (std::shared_ptr<ARDOUR::AutomationControl> c = source_route->send_pan_azimuth_enable_controllable (resolved_send_index)) {
			c->set_value (pan_enable_value, PBD::Controllable::NoGroup);
		}
	}

	return true;
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
		   << ",\"preFader\":" << (p->get_pre_fader () ? "true" : "false")
		   << ",\"postFader\":" << (p->get_pre_fader () ? "false" : "true")
		   << ",\"active\":" << (p->active () ? "true" : "false")
		   << ",\"enabled\":" << (p->enabled () ? "true" : "false")
		   << "}";
	}

	ss << "]";
	return ss.str ();
}

static int
plugin_index_for_processor (
	const std::shared_ptr<ARDOUR::Route>& route,
	const std::shared_ptr<ARDOUR::Processor>& processor)
{
	if (!route || !processor) {
		return -1;
	}

	for (uint32_t i = 0;; ++i) {
		std::shared_ptr<ARDOUR::Processor> p = route->nth_plugin (i);
		if (!p) {
			break;
		}
		if (p == processor) {
			return (int) i;
		}
	}

	return -1;
}

static bool
wait_for_plugin_index (
	const std::shared_ptr<ARDOUR::Route>& route,
	const std::shared_ptr<ARDOUR::Processor>& processor,
	int target_index,
	int timeout_ms,
	int& resolved_index)
{
	const std::chrono::steady_clock::time_point deadline =
		std::chrono::steady_clock::now () + std::chrono::milliseconds (timeout_ms);

	for (;;) {
		resolved_index = plugin_index_for_processor (route, processor);
		if (resolved_index == target_index) {
			return true;
		}
		if (std::chrono::steady_clock::now () >= deadline) {
			return false;
		}
		std::this_thread::sleep_for (std::chrono::milliseconds (2));
	}
}

static bool
wait_for_plugin_post_fader (
	const std::shared_ptr<ARDOUR::Route>& route,
	const std::shared_ptr<ARDOUR::Processor>& processor,
	bool target_post_fader,
	int timeout_ms,
	int& resolved_index,
	bool& resolved_post_fader)
{
	const std::chrono::steady_clock::time_point deadline =
		std::chrono::steady_clock::now () + std::chrono::milliseconds (timeout_ms);

	for (;;) {
		resolved_index = plugin_index_for_processor (route, processor);
		resolved_post_fader = !processor->get_pre_fader ();
		if (resolved_index >= 0 && resolved_post_fader == target_post_fader) {
			return true;
		}
		if (std::chrono::steady_clock::now () >= deadline) {
			return false;
		}
		std::this_thread::sleep_for (std::chrono::milliseconds (2));
	}
}

static std::string
lower_ascii (std::string s)
{
	std::transform (
		s.begin (),
		s.end (),
		s.begin (),
		[] (unsigned char c) -> unsigned char { return (unsigned char) std::tolower (c); });
	return s;
}

static std::string
plugin_type_token (ARDOUR::PluginType type)
{
	switch (type) {
	case ARDOUR::AudioUnit:
		return "audiounit";
	case ARDOUR::LADSPA:
		return "ladspa";
	case ARDOUR::LV2:
		return "lv2";
	case ARDOUR::Windows_VST:
		return "windows_vst";
	case ARDOUR::LXVST:
		return "lxvst";
	case ARDOUR::MacVST:
		return "macvst";
	case ARDOUR::Lua:
		return "lua";
	case ARDOUR::VST3:
		return "vst3";
	default:
		return "unknown";
	}
}

static bool
parse_plugin_type_token (const std::string& value, ARDOUR::PluginType& type)
{
	const std::string token = lower_ascii (value);
	if (token == "audiounit" || token == "audio-unit" || token == "audio_unit" || token == "au") {
		type = ARDOUR::AudioUnit;
		return true;
	}
	if (token == "ladspa") {
		type = ARDOUR::LADSPA;
		return true;
	}
	if (token == "lv2") {
		type = ARDOUR::LV2;
		return true;
	}
	if (token == "windows_vst" || token == "windows_vst2" || token == "winvst" || token == "vst2_windows") {
		type = ARDOUR::Windows_VST;
		return true;
	}
	if (token == "lxvst" || token == "linux_vst" || token == "linux_vst2" || token == "vst2_linux") {
		type = ARDOUR::LXVST;
		return true;
	}
	if (token == "macvst" || token == "mac_vst" || token == "mac_vst2" || token == "vst2_mac") {
		type = ARDOUR::MacVST;
		return true;
	}
	if (token == "lua") {
		type = ARDOUR::Lua;
		return true;
	}
	if (token == "vst3") {
		type = ARDOUR::VST3;
		return true;
	}
	return false;
}

static std::string
plugin_status_string (ARDOUR::PluginManager::PluginStatusType status)
{
	switch (status) {
	case ARDOUR::PluginManager::Normal:
		return "normal";
	case ARDOUR::PluginManager::Favorite:
		return "favorite";
	case ARDOUR::PluginManager::Hidden:
		return "hidden";
	case ARDOUR::PluginManager::Concealed:
		return "concealed";
	default:
		return "unknown";
	}
}

static std::string
plugin_catalog_id (const ARDOUR::PluginInfoPtr& info)
{
	return plugin_type_token (info->type) + ":" + info->unique_id;
}

static bool
plugin_matches_search (const ARDOUR::PluginInfoPtr& info, const std::string& search_lower)
{
	if (search_lower.empty ()) {
		return true;
	}

	const std::string haystack = lower_ascii (
		info->name + " " + info->creator + " " + info->category + " " + info->unique_id + " " + info->path);
	return haystack.find (search_lower) != std::string::npos;
}

static void
append_plugin_catalog_entries_json (
	std::ostringstream& out,
	const ARDOUR::PluginInfoList& infos,
	ARDOUR::PluginManager& manager,
	const std::string& search_lower,
	const boost::optional<ARDOUR::PluginType>& type_filter,
	bool include_hidden,
	bool include_internal,
	bool& first_entry,
	size_t& count)
{
	for (ARDOUR::PluginInfoList::const_iterator i = infos.begin (); i != infos.end (); ++i) {
		const ARDOUR::PluginInfoPtr& info = *i;
		if (!info) {
			continue;
		}

		if (type_filter && info->type != *type_filter) {
			continue;
		}
		if (!include_internal && info->is_internal ()) {
			continue;
		}

		const ARDOUR::PluginManager::PluginStatusType status = manager.get_status (info);
		const bool hidden = status == ARDOUR::PluginManager::Hidden || status == ARDOUR::PluginManager::Concealed;
		if (!include_hidden && hidden) {
			continue;
		}
		if (!plugin_matches_search (info, search_lower)) {
			continue;
		}

		if (!first_entry) {
			out << ",";
		}
		first_entry = false;
		++count;

		out << "{\"pluginId\":\"" << json_escape (plugin_catalog_id (info)) << "\""
		    << ",\"type\":\"" << json_escape (plugin_type_token (info->type)) << "\""
		    << ",\"typeLabel\":\"" << json_escape (ARDOUR::PluginManager::plugin_type_name (info->type, false)) << "\""
		    << ",\"name\":\"" << json_escape (info->name) << "\""
		    << ",\"category\":\"" << json_escape (info->category) << "\""
		    << ",\"creator\":\"" << json_escape (info->creator) << "\""
		    << ",\"uniqueId\":\"" << json_escape (info->unique_id) << "\""
		    << ",\"path\":\"" << json_escape (info->path) << "\""
		    << ",\"status\":\"" << json_escape (plugin_status_string (status)) << "\""
		    << ",\"favorite\":" << (status == ARDOUR::PluginManager::Favorite ? "true" : "false")
		    << ",\"hidden\":" << (status == ARDOUR::PluginManager::Hidden ? "true" : "false")
		    << ",\"concealed\":" << (status == ARDOUR::PluginManager::Concealed ? "true" : "false")
		    << ",\"isInternal\":" << (info->is_internal () ? "true" : "false")
		    << ",\"isEffect\":" << (info->is_effect () ? "true" : "false")
		    << ",\"isInstrument\":" << (info->is_instrument () ? "true" : "false")
		    << ",\"isUtility\":" << (info->is_utility () ? "true" : "false")
		    << ",\"isAnalyzer\":" << (info->is_analyzer () ? "true" : "false")
		    << ",\"needsMidiInput\":" << (info->needs_midi_input () ? "true" : "false")
		    << ",\"nInputs\":{\"audio\":" << info->n_inputs.n_audio () << ",\"midi\":" << info->n_inputs.n_midi () << "}"
		    << ",\"nOutputs\":{\"audio\":" << info->n_outputs.n_audio () << ",\"midi\":" << info->n_outputs.n_midi () << "}"
		    << "}";
	}
}

static void
append_all_plugin_infos (std::vector<ARDOUR::PluginInfoPtr>& infos, const ARDOUR::PluginInfoList& list)
{
	for (ARDOUR::PluginInfoList::const_iterator i = list.begin (); i != list.end (); ++i) {
		if (*i) {
			infos.push_back (*i);
		}
	}
}

static bool
resolve_plugin_info_for_add (
	ARDOUR::PluginManager& manager,
	const std::string& plugin_id,
	const std::string& type_token,
	const std::string& unique_id,
	ARDOUR::PluginInfoPtr& resolved,
	std::string& error)
{
	resolved.reset ();
	error.clear ();

	boost::optional<ARDOUR::PluginType> requested_type;
	std::string requested_unique_id;

	if (!plugin_id.empty ()) {
		const std::string::size_type sep = plugin_id.find (':');
		if (sep == std::string::npos || sep == 0 || sep + 1 >= plugin_id.size ()) {
			error = "Invalid pluginId (expected type:uniqueId)";
			return false;
		}

		ARDOUR::PluginType parsed_type;
		if (!parse_plugin_type_token (plugin_id.substr (0, sep), parsed_type)) {
			error = "Invalid pluginId type token";
			return false;
		}
		requested_type = parsed_type;
		requested_unique_id = plugin_id.substr (sep + 1);
	} else {
		if (!type_token.empty ()) {
			ARDOUR::PluginType parsed_type;
			if (!parse_plugin_type_token (type_token, parsed_type)) {
				error = "Invalid plugin type";
				return false;
			}
			requested_type = parsed_type;
		}

		if (!unique_id.empty ()) {
			requested_unique_id = unique_id;
		}
	}

	if (requested_unique_id.empty ()) {
		error = "Missing plugin identifier (provide pluginId or uniqueId)";
		return false;
	}

	std::vector<ARDOUR::PluginInfoPtr> all;
	append_all_plugin_infos (all, manager.windows_vst_plugin_info ());
	append_all_plugin_infos (all, manager.lxvst_plugin_info ());
	append_all_plugin_infos (all, manager.mac_vst_plugin_info ());
	append_all_plugin_infos (all, manager.vst3_plugin_info ());
	append_all_plugin_infos (all, manager.au_plugin_info ());
	append_all_plugin_infos (all, manager.ladspa_plugin_info ());
	append_all_plugin_infos (all, manager.lv2_plugin_info ());
	append_all_plugin_infos (all, manager.lua_plugin_info ());

	size_t matches = 0;
	for (size_t i = 0; i < all.size (); ++i) {
		const ARDOUR::PluginInfoPtr& info = all[i];
		if (!info || info->unique_id != requested_unique_id) {
			continue;
		}
		if (requested_type && info->type != *requested_type) {
			continue;
		}
		resolved = info;
		++matches;
	}

	if (matches == 0) {
		error = "Plugin not found";
		return false;
	}
	if (matches > 1 && !requested_type) {
		error = "Ambiguous uniqueId across plugin types; provide plugin type";
		resolved.reset ();
		return false;
	}

	return true;
}

static std::string
variant_type_string (ARDOUR::Variant::Type type)
{
	switch (type) {
		case ARDOUR::Variant::BEATS: return "BEATS";
		case ARDOUR::Variant::BOOL: return "BOOL";
		case ARDOUR::Variant::DOUBLE: return "DOUBLE";
		case ARDOUR::Variant::FLOAT: return "FLOAT";
		case ARDOUR::Variant::INT: return "INT";
		case ARDOUR::Variant::LONG: return "LONG";
		case ARDOUR::Variant::NOTHING: return "NOTHING";
		case ARDOUR::Variant::PATH: return "PATH";
		case ARDOUR::Variant::STRING: return "STRING";
		case ARDOUR::Variant::URI: return "URI";
		default: return "UNKNOWN";
	}
}

static std::string
plugin_descriptor_json (const std::shared_ptr<ARDOUR::Route>& route, int plugin_index, std::string* error_message)
{
	std::shared_ptr<ARDOUR::Processor> proc = route->nth_plugin (plugin_index);
	if (!proc) {
		if (error_message) {
			*error_message = "Plugin not found";
		}
		return std::string ();
	}

	std::shared_ptr<ARDOUR::PluginInsert> pi = std::dynamic_pointer_cast<ARDOUR::PluginInsert> (proc);
	if (!pi) {
		if (error_message) {
			*error_message = "Processor is not a plugin";
		}
		return std::string ();
	}

	std::shared_ptr<ARDOUR::Plugin> pip = pi->plugin ();
	if (!pip) {
		if (error_message) {
			*error_message = "Plugin instance unavailable";
		}
		return std::string ();
	}

	std::ostringstream ss;
	ss << "{\"plugin\":{\"index\":" << plugin_index
	   << ",\"name\":\"" << json_escape (proc->name ()) << "\""
	   << ",\"displayName\":\"" << json_escape (proc->display_name ()) << "\""
	   << ",\"enabled\":" << (proc->enabled () ? "true" : "false")
	   << ",\"active\":" << (proc->active () ? "true" : "false")
	   << ",\"maker\":\"" << json_escape (pip->maker ()) << "\""
	   << ",\"label\":\"" << json_escape (pip->label ()) << "\""
	   << ",\"uniqueId\":\"" << json_escape (pip->unique_id ()) << "\""
	   << "},\"parameters\":[";

	bool ok = false;
	bool first = true;
	for (uint32_t ppi = 0; ppi < pip->parameter_count (); ++ppi) {
		const uint32_t controlid = pip->nth_parameter (ppi, ok);
		if (!ok) {
			continue;
		}

		ARDOUR::ParameterDescriptor pd;
		if (pip->get_parameter_descriptor (controlid, pd) != 0) {
			continue;
		}

		if (!first) {
			ss << ",";
		}
		first = false;

		int flags = 0;
		flags |= pd.enumeration ? 1 : 0;
		flags |= pd.integer_step ? 2 : 0;
		flags |= pd.logarithmic ? 4 : 0;
		flags |= pd.sr_dependent ? 32 : 0;
		flags |= pd.toggled ? 64 : 0;
		flags |= pip->parameter_is_input (controlid) ? 0x80 : 0;
		std::string param_desc = pip->describe_parameter (Evoral::Parameter (ARDOUR::PluginAutomation, 0, controlid));
		flags |= (param_desc == "hidden") ? 0x100 : 0;

		ss << "{\"index\":" << ppi
		   << ",\"number\":" << (ppi + 1)
		   << ",\"controlId\":" << controlid
		   << ",\"label\":\"" << json_escape (pd.label) << "\""
		   << ",\"flags\":" << flags
		   << ",\"datatype\":\"" << variant_type_string (pd.datatype) << "\""
		   << ",\"lower\":" << pd.lower
		   << ",\"upper\":" << pd.upper
		   << ",\"printFmt\":\"" << json_escape (pd.print_fmt) << "\""
		   << ",\"isInput\":" << (pip->parameter_is_input (controlid) ? "true" : "false")
		   << ",\"isOutput\":" << (pip->parameter_is_output (controlid) ? "true" : "false")
		   << ",\"isControl\":" << (pip->parameter_is_control (controlid) ? "true" : "false")
		   << ",\"isHidden\":" << ((flags & 0x100) ? "true" : "false");

		ss << ",\"scalePoints\":[";
		bool first_scale = true;
		if (pd.scale_points) {
			for (ARDOUR::ScalePoints::const_iterator i = pd.scale_points->begin (); i != pd.scale_points->end (); ++i) {
				if (!first_scale) {
					ss << ",";
				}
				first_scale = false;
				ss << "{\"value\":" << i->second
				   << ",\"label\":\"" << json_escape (i->first) << "\"}";
			}
		}
		ss << "]";

		std::shared_ptr<ARDOUR::AutomationControl> c = pi->automation_control (Evoral::Parameter (ARDOUR::PluginAutomation, 0, controlid));
		if (c) {
			ss << ",\"currentValue\":" << c->get_value ()
			   << ",\"currentInterface\":" << c->internal_to_interface (c->get_value ());
		} else {
			ss << ",\"currentValue\":0"
			   << ",\"currentInterface\":0";
		}

		ss << "}";
	}

	ss << "]}";
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
				db = accurate_coefficient_to_dB (gain->get_value ());
			if (!std::isfinite (db)) {
				db = -193.0;
			}
		}

		ss << ",\"fader\":{\"position\":" << gain->internal_to_interface (gain->get_value ())
		   << ",\"db\":" << normalized_db_value (db) << "}";
	} else {
		ss << ",\"fader\":null";
	}

		if (pan) {
			ss << ",\"pan\":{\"position\":" << pan->internal_to_interface (pan->get_value ())
			   << ",\"convention\":\"0.0=right, 0.5=center, 1.0=left\"}";
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

static std::string
playlist_regions_json (const std::shared_ptr<ARDOUR::Playlist>& playlist, bool include_hidden)
{
	std::vector<std::shared_ptr<ARDOUR::Region> > regions;
	const ARDOUR::RegionList& all_regions = playlist->region_list_property ().rlist ();
	for (ARDOUR::RegionList::const_iterator it = all_regions.begin (); it != all_regions.end (); ++it) {
		if (!*it) {
			continue;
		}
		if (!include_hidden && (*it)->hidden ()) {
			continue;
		}
		regions.push_back (*it);
	}

	std::sort (
		regions.begin (),
		regions.end (),
		[] (const std::shared_ptr<ARDOUR::Region>& a, const std::shared_ptr<ARDOUR::Region>& b) {
			if (a->position_sample () != b->position_sample ()) {
				return a->position_sample () < b->position_sample ();
			}
			if (a->length_samples () != b->length_samples ()) {
				return a->length_samples () < b->length_samples ();
			}
			return a->id () < b->id ();
		});

	std::ostringstream ss;
	ss << "[";
	for (size_t i = 0; i < regions.size (); ++i) {
		const std::shared_ptr<ARDOUR::Region>& region = regions[i];
		if (i > 0) {
			ss << ",";
		}

		const samplepos_t start_sample = region->position_sample ();
		const samplepos_t end_sample = start_sample + region->length_samples ();

		ss << "{\"regionId\":\"" << json_escape (region->id ().to_s ()) << "\""
		   << ",\"name\":\"" << json_escape (region->name ()) << "\""
		   << ",\"type\":\"" << region->data_type ().to_string () << "\""
		   << ",\"startSample\":" << start_sample
		   << ",\"endSample\":" << end_sample
		   << ",\"lengthSamples\":" << region->length_samples ()
		   << ",\"startBbt\":" << bbt_json_at_sample (start_sample)
		   << ",\"endBbt\":" << bbt_json_at_sample (end_sample)
		   << ",\"hidden\":" << (region->hidden () ? "true" : "false")
		   << ",\"muted\":" << (region->muted () ? "true" : "false")
		   << ",\"locked\":" << (region->locked () ? "true" : "false")
		   << ",\"positionLocked\":" << (region->position_locked () ? "true" : "false")
		   << "}";
	}
	ss << "]";
	return ss.str ();
}

static bool
resolve_audio_region_argument_or_selected_at_playhead (
	ARDOUR::Session& session,
	const pt::ptree& root,
	const std::string& args_path,
	std::shared_ptr<ARDOUR::Region>& region,
	std::shared_ptr<ARDOUR::AudioRegion>& audio_region,
	std::string& resolved_via,
	std::string& error)
{
	if (!resolve_region_argument_or_selected_at_playhead (session, root, args_path, region, resolved_via, error)) {
		return false;
	}

	audio_region = std::dynamic_pointer_cast<ARDOUR::AudioRegion> (region);
	if (!audio_region) {
		error = "Region is not an audio region";
		return false;
	}

	return true;
}

static double
safe_region_gain_db (double linear_gain)
{
	const double magnitude = std::fabs (linear_gain);
	if (magnitude <= 0.0 || !std::isfinite (magnitude)) {
		return -193.0;
	}

	double db = accurate_coefficient_to_dB (magnitude);
	if (!std::isfinite (db)) {
		db = -193.0;
	}
	return db;
}

static std::string
region_info_json (
	const std::shared_ptr<ARDOUR::Region>& region,
	const std::string& resolved_via,
	bool include_analysis,
	const boost::optional<double>& maximum_amplitude,
	const boost::optional<double>& rms)
{
	const samplepos_t start_sample = region->position_sample ();
	const samplepos_t end_sample = start_sample + region->length_samples ();
	const std::shared_ptr<ARDOUR::Playlist> playlist = region->playlist ();
	const std::shared_ptr<ARDOUR::AudioRegion> audio_region = std::dynamic_pointer_cast<ARDOUR::AudioRegion> (region);

	std::ostringstream ss;
	ss << "{\"regionId\":\"" << json_escape (region->id ().to_s ()) << "\""
	   << ",\"name\":\"" << json_escape (region->name ()) << "\""
	   << ",\"type\":\"" << json_escape (region->data_type ().to_string ()) << "\""
	   << ",\"resolvedVia\":\"" << json_escape (resolved_via) << "\"";

	if (playlist) {
		ss << ",\"playlistId\":\"" << json_escape (playlist->id ().to_s ()) << "\"";
	} else {
		ss << ",\"playlistId\":null";
	}

	ss << ",\"startSample\":" << start_sample
	   << ",\"endSample\":" << end_sample
	   << ",\"lengthSamples\":" << region->length_samples ()
	   << ",\"startBbt\":" << bbt_json_at_sample (start_sample)
	   << ",\"endBbt\":" << bbt_json_at_sample (end_sample)
	   << ",\"hidden\":" << (region->hidden () ? "true" : "false")
	   << ",\"muted\":" << (region->muted () ? "true" : "false")
	   << ",\"locked\":" << (region->locked () ? "true" : "false")
	   << ",\"positionLocked\":" << (region->position_locked () ? "true" : "false")
	   << ",\"isAudio\":" << (audio_region ? "true" : "false")
	   << ",\"includeAnalysis\":" << (include_analysis ? "true" : "false");

	if (audio_region) {
		const double scale_amplitude = audio_region->scale_amplitude ();
		const double magnitude = std::fabs (scale_amplitude);
		ss << ",\"audio\":{\"scaleAmplitude\":" << scale_amplitude
		   << ",\"gainLinearAbs\":" << magnitude
		   << ",\"gainDb\":" << safe_region_gain_db (scale_amplitude)
		   << ",\"polarityInverted\":" << (scale_amplitude < 0.0 ? "true" : "false");

		if (include_analysis) {
			ss << ",\"maximumAmplitude\":";
			if (maximum_amplitude && std::isfinite (*maximum_amplitude) && *maximum_amplitude >= 0.0) {
				ss << *maximum_amplitude;
			} else {
				ss << "null";
			}
			ss << ",\"rms\":";
			if (rms && std::isfinite (*rms) && *rms >= 0.0) {
				ss << *rms;
			} else {
				ss << "null";
			}
		}

		ss << "}";
	} else {
		ss << ",\"audio\":null";
	}

	ss << "}";
	return ss.str ();
}

} // namespace

MCPHttpServer::MCPHttpServer (ARDOUR::Session& session, uint16_t port, int debug_level, PBD::EventLoop* event_loop)
	: _session (session)
	, _port (port)
	, _debug_level (clamp_debug_level (debug_level))
	, _event_loop (event_loop)
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
MCPHttpServer::set_debug_level (int level)
{
	_debug_level.store (clamp_debug_level (level));
}

int
MCPHttpServer::debug_level () const
{
	return _debug_level.load ();
}

void
MCPHttpServer::run ()
{
	if (_event_loop) {
		PBD::EventLoop::set_event_loop_for_thread (_event_loop);
	}

	PBD::notify_event_loops_about_thread_creation (pthread_self (), "MCPHttp", 2048);

	/* Session transport requests allocate SessionEvent objects from a per-thread pool. */
	ARDOUR::SessionEvent::create_per_thread_pool ("MCPHttp events", 256);
	Temporal::TempoMap::fetch ();

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
	ctx.mcp_post = false;
	ctx.have_response = false;
	ctx.request_body.clear ();
	ctx.response_body.clear ();

	char uri[1024];
	std::string path;

	if (lws_hdr_copy (wsi, uri, sizeof (uri), WSI_TOKEN_GET_URI) > 0) {
		path = uri;

		/* HTTP-only MCP endpoint: POST /mcp */
		if (path == "/mcp") {
			return send_http_status (wsi, 405);
		}

		return send_http_status (wsi, 404);
	}

	if (lws_hdr_copy (wsi, uri, sizeof (uri), WSI_TOKEN_POST_URI) > 0) {
		path = uri;

		if (path == "/mcp") {
			ctx.mcp_post = true;
			return 0;
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

std::string
MCPHttpServer::dispatch_jsonrpc (const std::string& payload) const
{
	pt::ptree root;
	std::istringstream is (payload);
	const int dbg = debug_level ();

	try {
		pt::read_json (is, root);
	} catch (...) {
		if (dbg >= 1) {
			PBD::warning << "MCPHttp: JSON parse error" << endmsg;
		}
		return jsonrpc_error ("null", -32700, "Parse error");
	}

	std::string method = root.get<std::string> ("method", "");
	std::string id = jsonrpc_id (root);
	const bool has_id = has_jsonrpc_id (root);

	if (dbg >= 2) {
		PBD::info << "MCPHttp: request payload: " << payload << endmsg;
	}
	if (dbg >= 1) {
		if (method == "tools/call") {
			const std::string requested_tool = canonical_tool_name (root.get<std::string> ("params.name", ""));
			PBD::info << "MCPHttp: tools/call " << requested_tool << endmsg;
		} else {
			PBD::info << "MCPHttp: " << method << endmsg;
		}
	}

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
			"{\"name\":\"session_get_info\",\"title\":\"Session Info\",\"description\":\"Return basic session and transport info.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}},"
			"{\"name\":\"session_save\",\"title\":\"Save Session\",\"description\":\"Save current session state, optionally to a snapshot name.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"snapshotName\":{\"type\":\"string\"},\"switchToSnapshot\":{\"type\":\"boolean\"}},\"additionalProperties\":false}},"
			"{\"name\":\"session_undo\",\"title\":\"Undo\",\"description\":\"Undo one or more operations from session history.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"count\":{\"type\":\"integer\",\"minimum\":1}},\"additionalProperties\":false}},"
			"{\"name\":\"session_redo\",\"title\":\"Redo\",\"description\":\"Redo one or more operations from session history.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"count\":{\"type\":\"integer\",\"minimum\":1}},\"additionalProperties\":false}},"
			"{\"name\":\"session_rename\",\"title\":\"Rename Session\",\"description\":\"Rename current session.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"newName\":{\"type\":\"string\"}},\"required\":[\"newName\"],\"additionalProperties\":false}},"
			"{\"name\":\"session_quick_snapshot\",\"title\":\"Quick Snapshot\",\"description\":\"Trigger quick snapshot action; optionally switch to the new snapshot.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"switchToSnapshot\":{\"type\":\"boolean\"}},\"additionalProperties\":false}},"
			"{\"name\":\"session_store_mixer_scene\",\"title\":\"Store Mixer Scene\",\"description\":\"Store mixer scene by index.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"index\":{\"type\":\"integer\",\"minimum\":0}},\"required\":[\"index\"],\"additionalProperties\":false}},"
			"{\"name\":\"session_recall_mixer_scene\",\"title\":\"Recall Mixer Scene\",\"description\":\"Recall mixer scene by index.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"index\":{\"type\":\"integer\",\"minimum\":0}},\"required\":[\"index\"],\"additionalProperties\":false}},"
				"{\"name\":\"transport_get_state\",\"title\":\"Transport State\",\"description\":\"Return transport state (rolling, speed, sample, state).\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"required\":[\"rolling\",\"speed\",\"sample\",\"state\"],\"properties\":{\"rolling\":{\"type\":\"boolean\"},\"speed\":{\"type\":\"number\"},\"sample\":{\"type\":\"integer\"},\"state\":{\"type\":\"string\"}},\"additionalProperties\":true}},"
				"{\"name\":\"transport_locate\",\"title\":\"Transport Locate\",\"description\":\"Move playhead to an absolute target position. Provide either sample, or bar+beat (fractional beat allowed).\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"sample\":{\"type\":\"integer\",\"minimum\":0},\"bar\":{\"type\":\"integer\",\"minimum\":1},\"beat\":{\"type\":\"number\",\"minimum\":1}},\"oneOf\":[{\"required\":[\"sample\"]},{\"required\":[\"bar\",\"beat\"]}],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"required\":[\"requestedSample\",\"requestedBbt\",\"transport\"],\"properties\":{\"requestedSample\":{\"type\":\"integer\"},\"requestedBbt\":{\"type\":\"object\"},\"transport\":{\"type\":\"object\"}},\"additionalProperties\":true}},"
				"{\"name\":\"transport_goto_start\",\"title\":\"Transport Go To Start\",\"description\":\"Move playhead to session start. Optional andRoll starts playback after locate.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"andRoll\":{\"type\":\"boolean\"}},\"additionalProperties\":false}},"
				"{\"name\":\"transport_goto_end\",\"title\":\"Transport Go To End\",\"description\":\"Move playhead to session end.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}},"
				"{\"name\":\"transport_prev_marker\",\"title\":\"Transport Previous Marker\",\"description\":\"Move playhead to the previous visible marker/range boundary.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}},"
				"{\"name\":\"transport_next_marker\",\"title\":\"Transport Next Marker\",\"description\":\"Move playhead to the next visible marker/range boundary.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}},"
				"{\"name\":\"transport_loop_toggle\",\"title\":\"Transport Loop Toggle\",\"description\":\"Toggle loop playback, or set explicit loop state. Shows loop range if available.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"enabled\":{\"type\":\"boolean\"}},\"additionalProperties\":false}},"
				"{\"name\":\"transport_loop_location\",\"title\":\"Transport Loop Location\",\"description\":\"Set loop range. Provide either startSample+endSample, or startBar+startBeat+endBar+endBeat.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"startSample\":{\"type\":\"integer\",\"minimum\":0},\"endSample\":{\"type\":\"integer\",\"minimum\":0},\"startBar\":{\"type\":\"integer\",\"minimum\":1},\"startBeat\":{\"type\":\"number\",\"minimum\":1},\"endBar\":{\"type\":\"integer\",\"minimum\":1},\"endBeat\":{\"type\":\"number\",\"minimum\":1}},\"oneOf\":[{\"required\":[\"startSample\",\"endSample\"]},{\"required\":[\"startBar\",\"startBeat\",\"endBar\",\"endBeat\"]}],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"properties\":{\"kind\":{\"type\":\"string\"},\"locationId\":{\"type\":\"string\"},\"name\":{\"type\":\"string\"},\"startSample\":{\"type\":\"integer\"},\"endSample\":{\"type\":\"integer\"},\"startBbt\":{\"type\":\"object\"},\"endBbt\":{\"type\":\"object\"},\"lengthSamples\":{\"type\":\"integer\"},\"hidden\":{\"type\":\"boolean\"}},\"additionalProperties\":true}},"
				"{\"name\":\"transport_set_record_enable\",\"title\":\"Set Global Record Enable\",\"description\":\"Set session record-enable (global rec arm) state.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"enabled\":{\"type\":\"boolean\"}},\"required\":[\"enabled\"],\"additionalProperties\":false}},"
				"{\"name\":\"transport_set_speed\",\"title\":\"Set Transport Speed\",\"description\":\"Set transport speed (as in OSC set_transport_speed).\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"speed\":{\"type\":\"number\"}},\"required\":[\"speed\"],\"additionalProperties\":false}},"
				"{\"name\":\"transport_play\",\"title\":\"Transport Play\",\"description\":\"Start transport playback.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}},"
				"{\"name\":\"transport_stop\",\"title\":\"Transport Stop\",\"description\":\"Stop transport playback.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}},"
				"{\"name\":\"markers_list\",\"title\":\"Markers List\",\"description\":\"List all session markers regardless of marker subtype.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}},"
				"{\"name\":\"markers_add\",\"title\":\"Add Marker\",\"description\":\"Add one marker at current playhead/audible position or a specific bar+beat. If name is omitted or empty, Ardour assigns a default name. Optional type: mark, section (arrangement), scene.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"},\"type\":{\"type\":\"string\",\"enum\":[\"mark\",\"section\",\"scene\",\"arrangement\"]},\"bar\":{\"type\":\"integer\",\"minimum\":1},\"beat\":{\"type\":\"number\",\"minimum\":1}},\"additionalProperties\":false}},"
				"{\"name\":\"markers_add_range\",\"title\":\"Add Range Marker\",\"description\":\"Add one range marker. Provide either startSample+endSample, or startBar+startBeat+endBar+endBeat.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"},\"startSample\":{\"type\":\"integer\",\"minimum\":0},\"endSample\":{\"type\":\"integer\",\"minimum\":0},\"startBar\":{\"type\":\"integer\",\"minimum\":1},\"startBeat\":{\"type\":\"number\",\"minimum\":1},\"endBar\":{\"type\":\"integer\",\"minimum\":1},\"endBeat\":{\"type\":\"number\",\"minimum\":1}},\"oneOf\":[{\"required\":[\"startSample\",\"endSample\"]},{\"required\":[\"startBar\",\"startBeat\",\"endBar\",\"endBeat\"]}],\"additionalProperties\":false}},"
				"{\"name\":\"markers_set_auto_loop\",\"title\":\"Set Auto Loop Range\",\"description\":\"Set auto-loop range. Provide either startSample+endSample, or startBar+startBeat+endBar+endBeat.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"startSample\":{\"type\":\"integer\",\"minimum\":0},\"endSample\":{\"type\":\"integer\",\"minimum\":0},\"startBar\":{\"type\":\"integer\",\"minimum\":1},\"startBeat\":{\"type\":\"number\",\"minimum\":1},\"endBar\":{\"type\":\"integer\",\"minimum\":1},\"endBeat\":{\"type\":\"number\",\"minimum\":1}},\"oneOf\":[{\"required\":[\"startSample\",\"endSample\"]},{\"required\":[\"startBar\",\"startBeat\",\"endBar\",\"endBeat\"]}],\"additionalProperties\":false}},"
				"{\"name\":\"markers_hide_auto_loop\",\"title\":\"Hide Auto Loop Range\",\"description\":\"Hide or show the current auto-loop range without changing its endpoints.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"hidden\":{\"type\":\"boolean\"}},\"additionalProperties\":false}},"
				"{\"name\":\"markers_set_auto_punch\",\"title\":\"Set Auto Punch Range\",\"description\":\"Set auto-punch range. Provide either startSample+endSample, or startBar+startBeat+endBar+endBeat.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"startSample\":{\"type\":\"integer\",\"minimum\":0},\"endSample\":{\"type\":\"integer\",\"minimum\":0},\"startBar\":{\"type\":\"integer\",\"minimum\":1},\"startBeat\":{\"type\":\"number\",\"minimum\":1},\"endBar\":{\"type\":\"integer\",\"minimum\":1},\"endBeat\":{\"type\":\"number\",\"minimum\":1}},\"oneOf\":[{\"required\":[\"startSample\",\"endSample\"]},{\"required\":[\"startBar\",\"startBeat\",\"endBar\",\"endBeat\"]}],\"additionalProperties\":false}},"
				"{\"name\":\"markers_hide_auto_punch\",\"title\":\"Hide Auto Punch Range\",\"description\":\"Hide or show the current auto-punch range without changing its endpoints.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"hidden\":{\"type\":\"boolean\"}},\"additionalProperties\":false}},"
				"{\"name\":\"markers_delete\",\"title\":\"Delete Marker\",\"description\":\"Delete one marker/range by locationId (preferred), or by name with optional sample to disambiguate duplicate names.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"locationId\":{\"type\":\"string\"},\"name\":{\"type\":\"string\"},\"sample\":{\"type\":\"integer\",\"minimum\":0}},\"anyOf\":[{\"required\":[\"locationId\"]},{\"required\":[\"name\"]}],\"additionalProperties\":false}},"
				"{\"name\":\"markers_rename\",\"title\":\"Rename Marker\",\"description\":\"Rename one marker/range by locationId (preferred), or by name with optional sample to disambiguate duplicate names.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"locationId\":{\"type\":\"string\"},\"name\":{\"type\":\"string\"},\"sample\":{\"type\":\"integer\",\"minimum\":0},\"newName\":{\"type\":\"string\"}},\"required\":[\"newName\"],\"anyOf\":[{\"required\":[\"locationId\"]},{\"required\":[\"name\"]}],\"additionalProperties\":false}},"
				"{\"name\":\"tracks_list\",\"title\":\"Tracks List\",\"description\":\"List session tracks and buses. includeHidden=true includes hidden routes.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"includeHidden\":{\"type\":\"boolean\"}},\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"required\":[\"tracks\"],\"properties\":{\"tracks\":{\"type\":\"array\"}},\"additionalProperties\":true}},"
				"{\"name\":\"tracks_add\",\"title\":\"Add Track\",\"description\":\"Add one or more tracks (audio or MIDI). insert can be end/before/after; when before/after and relativeToId is omitted, selected route is used if available.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"type\":{\"type\":\"string\",\"enum\":[\"audio\",\"midi\"]},\"count\":{\"type\":\"integer\",\"minimum\":1},\"name\":{\"type\":\"string\"},\"inputChannels\":{\"type\":\"integer\",\"minimum\":1},\"outputChannels\":{\"type\":\"integer\",\"minimum\":1},\"strictIo\":{\"type\":\"boolean\"},\"insert\":{\"type\":\"string\",\"enum\":[\"end\",\"before\",\"after\"]},\"relativeToId\":{\"type\":\"string\"}},\"additionalProperties\":false}},"
				"{\"name\":\"buses_add\",\"title\":\"Add Bus\",\"description\":\"Add one or more buses (audio or MIDI). insert can be end/before/after; when before/after and relativeToId is omitted, selected route is used if available.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"type\":{\"type\":\"string\",\"enum\":[\"audio\",\"midi\"]},\"count\":{\"type\":\"integer\",\"minimum\":1},\"name\":{\"type\":\"string\"},\"inputChannels\":{\"type\":\"integer\",\"minimum\":1},\"outputChannels\":{\"type\":\"integer\",\"minimum\":1},\"strictIo\":{\"type\":\"boolean\"},\"insert\":{\"type\":\"string\",\"enum\":[\"end\",\"before\",\"after\"]},\"relativeToId\":{\"type\":\"string\"}},\"additionalProperties\":false}},"
					"{\"name\":\"track_get_info\",\"title\":\"Get Route Info\",\"description\":\"Get one route info (fader, pan, rec, mute, solo, sends, plugins). Pan uses Ardour convention: 0.0=right, 0.5=center, 1.0=left.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"}},\"required\":[\"id\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"required\":[\"id\",\"name\",\"type\"],\"properties\":{\"id\":{\"type\":\"string\"},\"name\":{\"type\":\"string\"},\"type\":{\"type\":\"string\"},\"sends\":{\"type\":\"array\"},\"plugins\":{\"type\":\"array\"}},\"additionalProperties\":true}},"
						"{\"name\":\"track_get_regions\",\"title\":\"Get Track Regions\",\"description\":\"List regions in one track playlist. includeHidden=true includes hidden regions.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"includeHidden\":{\"type\":\"boolean\"}},\"required\":[\"id\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"required\":[\"id\",\"regions\"],\"properties\":{\"id\":{\"type\":\"string\"},\"regions\":{\"type\":\"array\"}},\"additionalProperties\":true}},"
						"{\"name\":\"region_get_info\",\"title\":\"Get Region Info\",\"description\":\"Get one region info. If regionId is omitted, resolves to selected track's top region at playhead. includeAnalysis=true adds audio peak/RMS analysis.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"regionId\":{\"type\":\"string\"},\"includeAnalysis\":{\"type\":\"boolean\"}},\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"required\":[\"regionId\",\"type\",\"startSample\",\"endSample\",\"isAudio\"],\"properties\":{\"regionId\":{\"type\":\"string\"},\"resolvedVia\":{\"type\":\"string\"},\"type\":{\"type\":\"string\"},\"isAudio\":{\"type\":\"boolean\"},\"audio\":{\"type\":[\"object\",\"null\"]}},\"additionalProperties\":true}},"
						"{\"name\":\"region_set_gain\",\"title\":\"Set Region Gain\",\"description\":\"Set one audio region gain using linear gain or dB. Optional invertPolarity toggles polarity.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"regionId\":{\"type\":\"string\"},\"linear\":{\"type\":\"number\",\"minimum\":0},\"db\":{\"type\":\"number\"},\"invertPolarity\":{\"type\":\"boolean\"}},\"oneOf\":[{\"required\":[\"linear\"]},{\"required\":[\"db\"]}],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"required\":[\"regionId\",\"updated\",\"gain\"],\"properties\":{\"regionId\":{\"type\":\"string\"},\"updated\":{\"type\":\"boolean\"},\"gain\":{\"type\":\"object\"}},\"additionalProperties\":true}},"
						"{\"name\":\"region_normalize\",\"title\":\"Normalize Region\",\"description\":\"Normalize one audio region to targetDb (default 0.0 dBFS).\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"regionId\":{\"type\":\"string\"},\"targetDb\":{\"type\":\"number\"}},\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"required\":[\"regionId\",\"targetDb\",\"peakAmplitude\",\"updated\",\"gain\"],\"properties\":{\"regionId\":{\"type\":\"string\"},\"targetDb\":{\"type\":\"number\"},\"peakAmplitude\":{\"type\":\"number\"},\"updated\":{\"type\":\"boolean\"},\"gain\":{\"type\":\"object\"}},\"additionalProperties\":true}},"
						"{\"name\":\"region_split\",\"title\":\"Split Region\",\"description\":\"Split one region (audio or MIDI) at sample, bar+beat, or current playhead if no split point is provided. If regionId is omitted, uses selected track's top region at playhead.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"regionId\":{\"type\":\"string\"},\"sample\":{\"type\":\"integer\",\"minimum\":0},\"bar\":{\"type\":\"integer\",\"minimum\":1},\"beat\":{\"type\":\"number\",\"minimum\":1}},\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"required\":[\"regionId\",\"split\",\"splitSample\",\"createdRegions\"],\"properties\":{\"regionId\":{\"type\":\"string\"},\"split\":{\"type\":\"boolean\"},\"splitSample\":{\"type\":\"integer\"},\"createdRegions\":{\"type\":\"array\"}},\"additionalProperties\":true}},"
						"{\"name\":\"region_resize\",\"title\":\"Resize Region\",\"description\":\"Resize one region (audio or MIDI) by setting start and/or end timeline boundary using sample or bar+beat. If regionId is omitted, uses selected track's top region at playhead.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"regionId\":{\"type\":\"string\"},\"startSample\":{\"type\":\"integer\",\"minimum\":0},\"startBar\":{\"type\":\"integer\",\"minimum\":1},\"startBeat\":{\"type\":\"number\",\"minimum\":1},\"endSample\":{\"type\":\"integer\",\"minimum\":0},\"endBar\":{\"type\":\"integer\",\"minimum\":1},\"endBeat\":{\"type\":\"number\",\"minimum\":1}},\"anyOf\":[{\"required\":[\"startSample\"]},{\"required\":[\"startBar\",\"startBeat\"]},{\"required\":[\"endSample\"]},{\"required\":[\"endBar\",\"endBeat\"]}],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"required\":[\"regionId\",\"startSample\",\"endSample\"],\"properties\":{\"regionId\":{\"type\":\"string\"},\"resolvedVia\":{\"type\":\"string\"},\"startSample\":{\"type\":\"integer\"},\"endSample\":{\"type\":\"integer\"}},\"additionalProperties\":true}},"
						"{\"name\":\"region_copy\",\"title\":\"Copy Region\",\"description\":\"Copy one region (audio or MIDI) to an absolute position or by delta. deltaBeats can be fractional. Optional trackId copies to another track. If regionId is omitted, uses selected track's top region at playhead.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"regionId\":{\"type\":\"string\"},\"trackId\":{\"type\":\"string\"},\"sample\":{\"type\":\"integer\",\"minimum\":0},\"bar\":{\"type\":\"integer\",\"minimum\":1},\"beat\":{\"type\":\"number\",\"minimum\":1},\"deltaSamples\":{\"type\":\"integer\"},\"deltaBeats\":{\"type\":\"number\"}},\"oneOf\":[{\"required\":[\"sample\"]},{\"required\":[\"bar\",\"beat\"]},{\"required\":[\"deltaSamples\"]},{\"required\":[\"deltaBeats\"]}],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"required\":[\"regionId\",\"sourceRegionId\",\"startSample\",\"endSample\"],\"properties\":{\"regionId\":{\"type\":\"string\"},\"sourceRegionId\":{\"type\":\"string\"},\"resolvedVia\":{\"type\":\"string\"},\"startSample\":{\"type\":\"integer\"},\"endSample\":{\"type\":\"integer\"},\"crossTrack\":{\"type\":\"boolean\"}},\"additionalProperties\":true}},"
						"{\"name\":\"region_move\",\"title\":\"Move Region\",\"description\":\"Move one region (audio or MIDI) to an absolute position or by delta. deltaBeats can be fractional. Optional trackId moves to another track. If regionId is omitted, uses selected track's top region at playhead.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"regionId\":{\"type\":\"string\"},\"trackId\":{\"type\":\"string\"},\"sample\":{\"type\":\"integer\",\"minimum\":0},\"bar\":{\"type\":\"integer\",\"minimum\":1},\"beat\":{\"type\":\"number\",\"minimum\":1},\"deltaSamples\":{\"type\":\"integer\"},\"deltaBeats\":{\"type\":\"number\"}},\"oneOf\":[{\"required\":[\"sample\"]},{\"required\":[\"bar\",\"beat\"]},{\"required\":[\"deltaSamples\"]},{\"required\":[\"deltaBeats\"]}],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"required\":[\"regionId\",\"sourceRegionId\",\"startSample\",\"endSample\"],\"properties\":{\"regionId\":{\"type\":\"string\"},\"sourceRegionId\":{\"type\":\"string\"},\"resolvedVia\":{\"type\":\"string\"},\"startSample\":{\"type\":\"integer\"},\"endSample\":{\"type\":\"integer\"},\"crossTrack\":{\"type\":\"boolean\"}},\"additionalProperties\":true}},"
						"{\"name\":\"plugin_get_description\",\"title\":\"Plugin Description\",\"description\":\"Get plugin descriptor and parameter metadata for a route plugin.\","
						"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"pluginIndex\":{\"type\":\"integer\",\"minimum\":0}},\"required\":[\"id\",\"pluginIndex\"],\"additionalProperties\":false}},"
					"{\"name\":\"plugin_set_parameter\",\"title\":\"Set Plugin Parameter\",\"description\":\"Set one plugin parameter by parameterIndex or controlId, using either internal value or interface value.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"pluginIndex\":{\"type\":\"integer\",\"minimum\":0},\"parameterIndex\":{\"type\":\"integer\",\"minimum\":0},\"controlId\":{\"type\":\"integer\",\"minimum\":0},\"value\":{\"type\":\"number\"},\"interface\":{\"type\":\"number\"}},\"required\":[\"id\",\"pluginIndex\"],\"allOf\":[{\"oneOf\":[{\"required\":[\"parameterIndex\"]},{\"required\":[\"controlId\"]}]},{\"oneOf\":[{\"required\":[\"value\"]},{\"required\":[\"interface\"]}]}],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"required\":[\"id\",\"pluginIndex\",\"parameterIndex\",\"controlId\",\"value\",\"interface\"],\"properties\":{\"id\":{\"type\":\"string\"},\"pluginIndex\":{\"type\":\"integer\"},\"parameterIndex\":{\"type\":\"integer\"},\"controlId\":{\"type\":\"integer\"},\"value\":{\"type\":\"number\"},\"interface\":{\"type\":\"number\"}},\"additionalProperties\":true}},"
					"{\"name\":\"plugin_set_enabled\",\"title\":\"Set Plugin Enabled\",\"description\":\"Enable or disable one route plugin by index.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"pluginIndex\":{\"type\":\"integer\",\"minimum\":0},\"enabled\":{\"type\":\"boolean\"}},\"required\":[\"id\",\"pluginIndex\",\"enabled\"],\"additionalProperties\":false}},"
					"{\"name\":\"plugin_remove\",\"title\":\"Remove Plugin\",\"description\":\"Remove one route plugin by index.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"pluginIndex\":{\"type\":\"integer\",\"minimum\":0}},\"required\":[\"id\",\"pluginIndex\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"required\":[\"removed\",\"id\",\"pluginIndex\",\"plugins\"],\"properties\":{\"removed\":{\"type\":\"boolean\"},\"id\":{\"type\":\"string\"},\"pluginIndex\":{\"type\":\"integer\"},\"plugins\":{\"type\":\"array\"}},\"additionalProperties\":true}},"
					"{\"name\":\"plugin_reorder\",\"title\":\"Reorder Plugin\",\"description\":\"Move one route plugin from fromIndex to toIndex.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"fromIndex\":{\"type\":\"integer\",\"minimum\":0},\"toIndex\":{\"type\":\"integer\",\"minimum\":0}},\"required\":[\"id\",\"fromIndex\",\"toIndex\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"required\":[\"moved\",\"id\",\"fromIndex\",\"toIndex\",\"plugins\"],\"properties\":{\"moved\":{\"type\":\"boolean\"},\"id\":{\"type\":\"string\"},\"fromIndex\":{\"type\":\"integer\"},\"toIndex\":{\"type\":\"integer\"},\"plugins\":{\"type\":\"array\"}},\"additionalProperties\":true}},"
					"{\"name\":\"plugin_set_position\",\"title\":\"Set Plugin Position\",\"description\":\"Move one route plugin between pre-fader and post-fader sections.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"pluginIndex\":{\"type\":\"integer\",\"minimum\":0},\"postFader\":{\"type\":\"boolean\"}},\"required\":[\"id\",\"pluginIndex\",\"postFader\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"required\":[\"moved\",\"id\",\"pluginIndex\",\"preFader\",\"postFader\",\"plugins\"],\"properties\":{\"moved\":{\"type\":\"boolean\"},\"id\":{\"type\":\"string\"},\"pluginIndex\":{\"type\":\"integer\"},\"preFader\":{\"type\":\"boolean\"},\"postFader\":{\"type\":\"boolean\"},\"plugins\":{\"type\":\"array\"}},\"additionalProperties\":true}},"
					"{\"name\":\"plugin_list_available\",\"title\":\"List Available Plugins\",\"description\":\"List discovered plugins from Ardour's plugin manager. Returns stable pluginId values suitable for plugin_add.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"search\":{\"type\":\"string\"},\"type\":{\"type\":\"string\"},\"includeHidden\":{\"type\":\"boolean\"},\"includeInternal\":{\"type\":\"boolean\"}},\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"required\":[\"count\",\"plugins\"],\"properties\":{\"count\":{\"type\":\"integer\"},\"plugins\":{\"type\":\"array\"}},\"additionalProperties\":true}},"
					"{\"name\":\"plugin_add\",\"title\":\"Add Plugin\",\"description\":\"Add one plugin to a route by pluginId (preferred) or uniqueId+type. position inserts at plugin index.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"pluginId\":{\"type\":\"string\"},\"uniqueId\":{\"type\":\"string\"},\"type\":{\"type\":\"string\"},\"position\":{\"type\":\"integer\",\"minimum\":0},\"enabled\":{\"type\":\"boolean\"}},\"required\":[\"id\"],\"allOf\":[{\"oneOf\":[{\"required\":[\"pluginId\"]},{\"required\":[\"uniqueId\"]}]}],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"required\":[\"id\",\"pluginIndex\",\"plugins\"],\"properties\":{\"id\":{\"type\":\"string\"},\"pluginIndex\":{\"type\":\"integer\"},\"plugins\":{\"type\":\"array\"}},\"additionalProperties\":true}},"
					"{\"name\":\"track_get_fader\",\"title\":\"Get Track Fader\",\"description\":\"Get current track fader as normalized position and dB.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"}},\"required\":[\"id\"],\"additionalProperties\":false}},"
				"{\"name\":\"track_select\",\"title\":\"Select Track\",\"description\":\"Select a route in Ardour.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"}},\"required\":[\"id\"],\"additionalProperties\":false}},"
				"{\"name\":\"track_rename\",\"title\":\"Rename Track\",\"description\":\"Rename one route by ID.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"newName\":{\"type\":\"string\"}},\"required\":[\"id\",\"newName\"],\"additionalProperties\":false}},"
				"{\"name\":\"track_set_mute\",\"title\":\"Set Track Mute\",\"description\":\"Set route mute state.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"value\":{\"type\":\"boolean\"}},\"required\":[\"id\",\"value\"],\"additionalProperties\":false}},"
					"{\"name\":\"track_set_solo\",\"title\":\"Set Track Solo\",\"description\":\"Set route solo state.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"value\":{\"type\":\"boolean\"}},\"required\":[\"id\",\"value\"],\"additionalProperties\":false}},"
					"{\"name\":\"track_set_rec_enable\",\"title\":\"Set Record Enable\",\"description\":\"Set track record-enable state.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"value\":{\"type\":\"boolean\"}},\"required\":[\"id\",\"value\"],\"additionalProperties\":false}},"
					"{\"name\":\"track_set_rec_safe\",\"title\":\"Set Record Safe\",\"description\":\"Set track record-safe state.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"value\":{\"type\":\"boolean\"}},\"required\":[\"id\",\"value\"],\"additionalProperties\":false}},"
					"{\"name\":\"track_set_pan\",\"title\":\"Set Pan\",\"description\":\"Set route pan position (Ardour convention: 0.0=right, 0.5=center, 1.0=left).\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"position\":{\"type\":\"number\",\"minimum\":0,\"maximum\":1}},\"required\":[\"id\",\"position\"],\"additionalProperties\":false}},"
						"{\"name\":\"track_set_send_level\",\"title\":\"Set Send Level\",\"description\":\"Set route send level by send index using normalized position (0.0 to 1.0) or dB. For dB mode, valid range is -193.0 to +6.0, and -193.0 is silence floor.\","
						"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"sendIndex\":{\"type\":\"integer\",\"minimum\":0},\"position\":{\"type\":\"number\",\"minimum\":0,\"maximum\":1},\"db\":{\"type\":\"number\",\"minimum\":-193.0,\"maximum\":6.0}},\"required\":[\"id\",\"sendIndex\"],\"oneOf\":[{\"required\":[\"position\"]},{\"required\":[\"db\"]}],\"additionalProperties\":false}},"
						"{\"name\":\"track_add_send\",\"title\":\"Add Send\",\"description\":\"Add one internal aux send from a source route to a target route. Optional level and enabled state are applied to the resolved send. For dB mode, valid range is -193.0 to +6.0, and -193.0 is silence floor.\","
						"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"targetId\":{\"type\":\"string\"},\"position\":{\"type\":\"number\",\"minimum\":0,\"maximum\":1},\"db\":{\"type\":\"number\",\"minimum\":-193.0,\"maximum\":6.0},\"enabled\":{\"type\":\"boolean\"},\"postFader\":{\"type\":\"boolean\"}},\"required\":[\"id\",\"targetId\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"required\":[\"created\",\"send\",\"sends\"],\"properties\":{\"created\":{\"type\":\"boolean\"},\"send\":{\"type\":\"object\"},\"sends\":{\"type\":\"array\"}},\"additionalProperties\":true}},"
					"{\"name\":\"track_set_send_position\",\"title\":\"Set Send Position\",\"description\":\"Set send pre/post-fader position for an existing internal aux send.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"sendIndex\":{\"type\":\"integer\",\"minimum\":0},\"postFader\":{\"type\":\"boolean\"}},\"required\":[\"id\",\"sendIndex\",\"postFader\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"required\":[\"id\",\"routeName\",\"sendIndex\",\"preFader\",\"postFader\"],\"properties\":{\"id\":{\"type\":\"string\"},\"routeName\":{\"type\":\"string\"},\"sendIndex\":{\"type\":\"integer\"},\"preFader\":{\"type\":\"boolean\"},\"postFader\":{\"type\":\"boolean\"}},\"additionalProperties\":true}},"
					"{\"name\":\"track_remove_send\",\"title\":\"Remove Send\",\"description\":\"Remove one route send by send index. Optional targetId can be used as a safety check for internal sends.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"sendIndex\":{\"type\":\"integer\",\"minimum\":0},\"targetId\":{\"type\":\"string\"}},\"required\":[\"id\",\"sendIndex\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"required\":[\"removed\",\"routeId\",\"routeName\",\"sendIndex\",\"sends\"],\"properties\":{\"removed\":{\"type\":\"boolean\"},\"routeId\":{\"type\":\"string\"},\"routeName\":{\"type\":\"string\"},\"sendIndex\":{\"type\":\"integer\"},\"sends\":{\"type\":\"array\"}},\"additionalProperties\":true}},"
						"{\"name\":\"track_set_fader\",\"title\":\"Set Track Fader\",\"description\":\"Set track fader by normalized position (0.0 to 1.0) or dB. For dB mode, valid range is -193.0 to +6.0, and -193.0 is silence floor.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"position\":{\"type\":\"number\",\"minimum\":0,\"maximum\":1},\"db\":{\"type\":\"number\",\"minimum\":-193.0,\"maximum\":6.0}},\"required\":[\"id\"],\"oneOf\":[{\"required\":[\"position\"]},{\"required\":[\"db\"]}],\"additionalProperties\":false}},"
						"{\"name\":\"midi_region_add\",\"title\":\"Add MIDI Region\",\"description\":\"Create an empty MIDI region on a MIDI track. Provide either startSample+endSample, or startBar+startBeat+endBar+endBeat.\","
						"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"trackId\":{\"type\":\"string\"},\"name\":{\"type\":\"string\"},\"startSample\":{\"type\":\"integer\",\"minimum\":0},\"endSample\":{\"type\":\"integer\",\"minimum\":0},\"startBar\":{\"type\":\"integer\",\"minimum\":1},\"startBeat\":{\"type\":\"number\",\"minimum\":1},\"endBar\":{\"type\":\"integer\",\"minimum\":1},\"endBeat\":{\"type\":\"number\",\"minimum\":1}},\"required\":[\"trackId\"],\"oneOf\":[{\"required\":[\"startSample\",\"endSample\"]},{\"required\":[\"startBar\",\"startBeat\",\"endBar\",\"endBeat\"]}],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"required\":[\"created\"],\"properties\":{\"created\":{\"type\":\"object\"},\"usedDefaultName\":{\"type\":\"boolean\"}},\"additionalProperties\":true}},"
					"{\"name\":\"midi_note_add\",\"title\":\"Add MIDI Note\",\"description\":\"Add one MIDI note to an existing MIDI region at region beats, sample, or bar+beat position.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"regionId\":{\"type\":\"string\"},\"note\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":127},\"velocity\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":127},\"channel\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":16},\"lengthBeats\":{\"type\":\"number\",\"exclusiveMinimum\":0},\"regionBeat\":{\"type\":\"number\",\"minimum\":0},\"sample\":{\"type\":\"integer\",\"minimum\":0},\"bar\":{\"type\":\"integer\",\"minimum\":1},\"beat\":{\"type\":\"number\",\"minimum\":1}},\"required\":[\"regionId\",\"note\",\"lengthBeats\"],\"oneOf\":[{\"required\":[\"regionBeat\"]},{\"required\":[\"sample\"]},{\"required\":[\"bar\",\"beat\"]}],\"additionalProperties\":false}},"
					"{\"name\":\"midi_note_list\",\"title\":\"List MIDI Notes\",\"description\":\"List all MIDI notes in a MIDI region.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"regionId\":{\"type\":\"string\"}},\"required\":[\"regionId\"],\"additionalProperties\":false}},"
					"{\"name\":\"midi_note_edit\",\"title\":\"Edit MIDI Notes\",\"description\":\"Edit or delete MIDI notes by noteId in a MIDI region.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"regionId\":{\"type\":\"string\"},\"edits\":{\"type\":\"array\",\"minItems\":1,\"items\":{\"type\":\"object\",\"properties\":{\"noteId\":{\"type\":\"integer\"},\"delete\":{\"type\":\"boolean\"},\"note\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":127},\"deltaSemitones\":{\"type\":\"integer\"},\"velocity\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":127},\"deltaVelocity\":{\"type\":\"integer\"},\"channel\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":16},\"startBeats\":{\"type\":\"number\",\"minimum\":0},\"deltaBeats\":{\"type\":\"number\"},\"lengthBeats\":{\"type\":\"number\",\"exclusiveMinimum\":0}},\"required\":[\"noteId\"],\"additionalProperties\":false}}},\"required\":[\"regionId\",\"edits\"],\"additionalProperties\":false}},"
						"{\"name\":\"midi_note_import_json\",\"title\":\"Import MIDI JSON\",\"description\":\"Import MIDI JSON into an existing MIDI region (regionId), or create/populate a new MIDI region on a track (trackId plus range endpoints). midi.midi_events is required. Event positions (bar/b) are relative to region start: bar 1 beat 1 = region start (not absolute session time). b may be fractional (e.g. 1.5 = off-beat 8th note in 4/4), and t is tick offset within the beat using ticks_per_quarter. In drum mode (is_drum_mode=true), note_off events are optional; hits may be provided as note_on only.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"midi\":{\"type\":\"object\",\"properties\":{\"channel\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":15},\"is_drum_mode\":{\"type\":\"boolean\"},\"ticks_per_quarter\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":96000},\"time_signature\":{\"type\":\"string\"},\"midi_events\":{\"type\":\"array\",\"minItems\":1,\"items\":{\"type\":\"object\",\"properties\":{\"bar\":{\"type\":\"integer\",\"minimum\":1},\"repeat\":{\"type\":\"integer\",\"minimum\":0},\"b\":{\"type\":\"number\",\"minimum\":1},\"t\":{\"type\":\"integer\",\"minimum\":0},\"n\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":127},\"v\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":127},\"type\":{\"type\":\"string\",\"enum\":[\"note_on\",\"note_off\"]},\"channel\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":15},\"comment\":{\"type\":\"string\"}},\"oneOf\":[{\"required\":[\"comment\"]},{\"required\":[\"bar\",\"repeat\"]},{\"required\":[\"bar\",\"b\",\"n\"]}],\"additionalProperties\":false}}},\"required\":[\"midi_events\"],\"additionalProperties\":false},\"regionId\":{\"type\":\"string\"},\"trackId\":{\"type\":\"string\"},\"name\":{\"type\":\"string\"},\"startSample\":{\"type\":\"integer\",\"minimum\":0},\"endSample\":{\"type\":\"integer\",\"minimum\":0},\"startBar\":{\"type\":\"integer\",\"minimum\":1},\"startBeat\":{\"type\":\"number\",\"minimum\":1},\"endBar\":{\"type\":\"integer\",\"minimum\":1},\"endBeat\":{\"type\":\"number\",\"minimum\":1}},\"required\":[\"midi\"],\"oneOf\":[{\"required\":[\"regionId\"]},{\"required\":[\"trackId\",\"startSample\",\"endSample\"]},{\"required\":[\"trackId\",\"startBar\",\"startBeat\",\"endBar\",\"endBeat\"]}],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"required\":[\"createdRegion\",\"region\",\"summary\"],\"properties\":{\"createdRegion\":{\"type\":\"boolean\"},\"region\":{\"type\":\"object\"},\"summary\":{\"type\":\"object\"},\"warnings\":{\"type\":\"array\"}},\"additionalProperties\":true}},"
					"{\"name\":\"midi_note_get_json\",\"title\":\"Export MIDI JSON\",\"description\":\"Export all notes from a MIDI region as import-compatible MIDI JSON (standard mode with note_on/note_off events).\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"regionId\":{\"type\":\"string\"},\"ticksPerQuarter\":{\"type\":\"integer\",\"minimum\":1},\"timeSignature\":{\"type\":\"string\"}},\"required\":[\"regionId\"],\"additionalProperties\":false},\"outputSchema\":{\"type\":\"object\",\"required\":[\"region\",\"midi\"],\"properties\":{\"region\":{\"type\":\"object\"},\"midi\":{\"type\":\"object\",\"required\":[\"channel\",\"is_drum_mode\",\"time_signature\",\"ticks_per_quarter\",\"midi_events\"],\"properties\":{\"channel\":{\"type\":\"integer\"},\"is_drum_mode\":{\"type\":\"boolean\"},\"time_signature\":{\"type\":\"string\"},\"ticks_per_quarter\":{\"type\":\"integer\"},\"midi_events\":{\"type\":\"array\"}},\"additionalProperties\":true},\"warnings\":{\"type\":\"array\"}},\"additionalProperties\":true}}"
					"]}");
			}

	if (method == "tools/call") {
		std::string tool_name = canonical_tool_name (root.get<std::string> ("params.name", ""));
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

		if (tool_name == "session/save") {
			const std::string snapshot_name = root.get<std::string> ("params.arguments.snapshotName", "");
			const bool switch_to_snapshot = root.get<bool> ("params.arguments.switchToSnapshot", false);

			const int rc = _session.save_state (snapshot_name, false, switch_to_snapshot);
			if (rc != 0) {
				return jsonrpc_error (id, -32000, "Failed to save session state");
			}

			std::ostringstream structured;
			structured << "{\"saved\":true";
			if (snapshot_name.empty ()) {
				structured << ",\"requestedSnapshotName\":null";
			} else {
				structured << ",\"requestedSnapshotName\":\"" << json_escape (snapshot_name) << "\"";
			}
			structured << ",\"switchToSnapshot\":" << (switch_to_snapshot ? "true" : "false")
				<< ",\"currentSnapshotName\":\"" << json_escape (_session.snap_name ()) << "\""
				<< ",\"session\":" << session_info_json (_session)
				<< "}";

			return jsonrpc_result (
				id,
				std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Session saved\"}],\"structuredContent\":") + structured.str () + "}");
		}

		if (tool_name == "session/undo") {
			const int64_t count_in = root.get<int64_t> ("params.arguments.count", 1);
			if (count_in < 1 || count_in > 1024) {
				return jsonrpc_error (id, -32602, "Invalid count (expected 1..1024)");
			}
			const uint32_t count = (uint32_t) count_in;

			const uint32_t undo_before = _session.undo_depth ();
			const uint32_t redo_before = _session.redo_depth ();
			_session.undo (count);
			const uint32_t undo_after = _session.undo_depth ();
			const uint32_t redo_after = _session.redo_depth ();

			std::ostringstream structured;
			structured << "{\"countRequested\":" << count
				<< ",\"undoDepthBefore\":" << undo_before
				<< ",\"redoDepthBefore\":" << redo_before
				<< ",\"undoDepthAfter\":" << undo_after
				<< ",\"redoDepthAfter\":" << redo_after
				<< ",\"nextUndo\":\"" << json_escape (_session.next_undo ()) << "\""
				<< ",\"nextRedo\":\"" << json_escape (_session.next_redo ()) << "\""
				<< "}";

			return jsonrpc_result (
				id,
				std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Undo applied\"}],\"structuredContent\":") + structured.str () + "}");
		}

		if (tool_name == "session/redo") {
			const int64_t count_in = root.get<int64_t> ("params.arguments.count", 1);
			if (count_in < 1 || count_in > 1024) {
				return jsonrpc_error (id, -32602, "Invalid count (expected 1..1024)");
			}
			const uint32_t count = (uint32_t) count_in;

			const uint32_t undo_before = _session.undo_depth ();
			const uint32_t redo_before = _session.redo_depth ();
			_session.redo (count);
			const uint32_t undo_after = _session.undo_depth ();
			const uint32_t redo_after = _session.redo_depth ();

			std::ostringstream structured;
			structured << "{\"countRequested\":" << count
				<< ",\"undoDepthBefore\":" << undo_before
				<< ",\"redoDepthBefore\":" << redo_before
				<< ",\"undoDepthAfter\":" << undo_after
				<< ",\"redoDepthAfter\":" << redo_after
				<< ",\"nextUndo\":\"" << json_escape (_session.next_undo ()) << "\""
				<< ",\"nextRedo\":\"" << json_escape (_session.next_redo ()) << "\""
				<< "}";

			return jsonrpc_result (
				id,
				std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Redo applied\"}],\"structuredContent\":") + structured.str () + "}");
		}

		if (tool_name == "session/rename") {
			const std::string new_name = root.get<std::string> ("params.arguments.newName", "");
			if (new_name.empty ()) {
				return jsonrpc_error (id, -32602, "Missing newName");
			}

			const std::string illegal = ARDOUR::Session::session_name_is_legal (new_name);
			if (!illegal.empty ()) {
				return jsonrpc_error (id, -32602, std::string ("Session name contains illegal character: ") + illegal);
			}

			const std::string old_name = _session.name ();
			const int rc = _session.rename (new_name);
			if (rc == -1) {
				return jsonrpc_error (id, -32000, "Session name already exists");
			}
			if (rc != 0) {
				return jsonrpc_error (id, -32000, "Renaming session failed");
			}

			std::ostringstream structured;
			structured << "{\"oldName\":\"" << json_escape (old_name) << "\""
				<< ",\"newName\":\"" << json_escape (_session.name ()) << "\""
				<< ",\"session\":" << session_info_json (_session)
				<< "}";

			return jsonrpc_result (
				id,
				std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Session renamed\"}],\"structuredContent\":") + structured.str () + "}");
		}

		if (tool_name == "session/quick_snapshot") {
			const bool switch_to_snapshot = root.get<bool> ("params.arguments.switchToSnapshot", false);
			const std::string before_snapshot = _session.snap_name ();
			const std::string snapshot_name = quick_snapshot_name_localtime ();

			/* Match ARDOUR_UI::quick_snapshot_session behavior without invoking GUI actions. */
			if (switch_to_snapshot && _session.dirty ()) {
				if (_session.save_state ("") != 0) {
					return jsonrpc_error (id, -32000, "Failed to save current session before quick snapshot");
				}
			}
			if (_session.save_state (snapshot_name, false, switch_to_snapshot) != 0) {
				return jsonrpc_error (id, -32000, "Quick snapshot save failed");
			}

			std::ostringstream structured;
			structured << "{\"switchToSnapshot\":" << (switch_to_snapshot ? "true" : "false")
				<< ",\"snapshotName\":\"" << json_escape (snapshot_name) << "\""
				<< ",\"snapshotNameBefore\":\"" << json_escape (before_snapshot) << "\""
				<< ",\"snapshotNameAfter\":\"" << json_escape (_session.snap_name ()) << "\""
				<< ",\"session\":" << session_info_json (_session)
				<< "}";

			return jsonrpc_result (
				id,
				std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Quick snapshot requested\"}],\"structuredContent\":") + structured.str () + "}");
		}

		if (tool_name == "session/store_mixer_scene") {
			const int64_t index_in = root.get<int64_t> ("params.arguments.index", -1);
			if (index_in < 0 || index_in > 1024) {
				return jsonrpc_error (id, -32602, "Invalid index (expected 0..1024)");
			}
			const size_t index = (size_t) index_in;
			_session.store_nth_mixer_scene (index);

			std::ostringstream structured;
			structured << "{\"index\":" << index
				<< ",\"stored\":true}";
			return jsonrpc_result (
				id,
				std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Mixer scene stored\"}],\"structuredContent\":") + structured.str () + "}");
		}

		if (tool_name == "session/recall_mixer_scene") {
			const int64_t index_in = root.get<int64_t> ("params.arguments.index", -1);
			if (index_in < 0 || index_in > 1024) {
				return jsonrpc_error (id, -32602, "Invalid index (expected 0..1024)");
			}
			const size_t index = (size_t) index_in;
			const bool ok = _session.apply_nth_mixer_scene (index);
			if (!ok) {
				return jsonrpc_error (id, -32000, "Mixer scene recall failed");
			}

			std::ostringstream structured;
			structured << "{\"index\":" << index
				<< ",\"recalled\":true}";
			return jsonrpc_result (
				id,
				std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Mixer scene recalled\"}],\"structuredContent\":") + structured.str () + "}");
		}

			if (tool_name == "transport/get_state") {
				std::string structured = transport_state_json (_session);
				return jsonrpc_result (
					id,
					std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Transport state\"}],\"structuredContent\":") + structured + "}");
			}

			if (tool_name == "transport/locate") {
				const boost::optional<int64_t> sample_opt = root.get_optional<int64_t> ("params.arguments.sample");
				samplepos_t target_sample = 0;
				bool have_bbt_target = false;
				std::string bbt_error;
				if (!parse_optional_bbt_target_sample (root, "params.arguments", target_sample, have_bbt_target, bbt_error)) {
					return jsonrpc_error (id, -32602, bbt_error);
				}

				if (sample_opt && have_bbt_target) {
					return jsonrpc_error (id, -32602, "Provide either sample or bar+beat, not both");
				}
				if (!sample_opt && !have_bbt_target) {
					return jsonrpc_error (id, -32602, "Missing target position (provide sample or bar+beat)");
				}

				if (sample_opt) {
					if (*sample_opt < 0) {
						return jsonrpc_error (id, -32602, "Invalid sample (expected >= 0)");
					}
					target_sample = (samplepos_t) *sample_opt;
				}

				_session.request_locate (target_sample, false, ARDOUR::RollIfAppropriate);

				std::ostringstream structured;
				structured << "{\"requestedSample\":" << target_sample
					<< ",\"requestedBbt\":" << bbt_json_at_sample (target_sample)
					<< ",\"transport\":" << transport_state_json (_session)
					<< "}";

				return jsonrpc_result (
					id,
					std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Transport locate requested\"}],\"structuredContent\":") + structured.str () + "}");
			}

			if (tool_name == "transport/goto_start") {
				const bool and_roll = root.get<bool> ("params.arguments.andRoll", false);
				_session.goto_start (and_roll);
				std::string structured = transport_state_json (_session);
				return jsonrpc_result (
					id,
					std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Transport moved to start\"}],\"structuredContent\":") + structured + "}");
			}

			if (tool_name == "transport/goto_end") {
				_session.goto_end ();
				std::string structured = transport_state_json (_session);
				return jsonrpc_result (
					id,
					std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Transport moved to end\"}],\"structuredContent\":") + structured + "}");
			}

			if (tool_name == "transport/prev_marker") {
				ARDOUR::Locations* locations = _session.locations ();
				if (!locations) {
					return jsonrpc_error (id, -32602, "Session locations unavailable");
				}

				const samplepos_t now_sample = _session.transport_sample ();
				Temporal::timepos_t pos = locations->first_mark_before_flagged (
					Temporal::timepos_t (now_sample),
					true,
					ARDOUR::Location::Flags (0),
					ARDOUR::Location::Flags (0),
					ARDOUR::Location::Flags (0));

				/* Match Editor behavior while rolling: skip the current/very-near mark. */
				if (pos != Temporal::timepos_t::max (Temporal::AudioTime) && _session.transport_rolling ()) {
					if ((now_sample - pos.samples ()) < (_session.sample_rate () / 2)) {
						Temporal::timepos_t prior = locations->first_mark_before (pos, true);
						if (prior != Temporal::timepos_t::max (Temporal::AudioTime)) {
							pos = prior;
						}
					}
				}

				std::ostringstream structured;
				if (pos == Temporal::timepos_t::max (Temporal::AudioTime)) {
					structured << "{\"moved\":false,\"transport\":" << transport_state_json (_session) << "}";
					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"No previous marker\"}],\"structuredContent\":") + structured.str () + "}");
				}

				_session.request_locate (pos.samples ());

				structured << "{\"moved\":true"
					<< ",\"targetSample\":" << pos.samples ()
					<< ",\"targetBbt\":" << bbt_json_at_sample (pos.samples ())
					<< ",\"transport\":" << transport_state_json (_session)
					<< "}";
				return jsonrpc_result (
					id,
					std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Moved to previous marker\"}],\"structuredContent\":") + structured.str () + "}");
			}

			if (tool_name == "transport/next_marker") {
				ARDOUR::Locations* locations = _session.locations ();
				if (!locations) {
					return jsonrpc_error (id, -32602, "Session locations unavailable");
				}

				Temporal::timepos_t pos = locations->first_mark_after_flagged (
					Temporal::timepos_t (_session.transport_sample () + 1),
					true,
					ARDOUR::Location::Flags (0),
					ARDOUR::Location::Flags (0),
					ARDOUR::Location::Flags (0));

				std::ostringstream structured;
				if (pos == Temporal::timepos_t::max (Temporal::AudioTime)) {
					structured << "{\"moved\":false,\"transport\":" << transport_state_json (_session) << "}";
					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"No next marker\"}],\"structuredContent\":") + structured.str () + "}");
				}

				_session.request_locate (pos.samples ());

				structured << "{\"moved\":true"
					<< ",\"targetSample\":" << pos.samples ()
					<< ",\"targetBbt\":" << bbt_json_at_sample (pos.samples ())
					<< ",\"transport\":" << transport_state_json (_session)
					<< "}";
				return jsonrpc_result (
					id,
					std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Moved to next marker\"}],\"structuredContent\":") + structured.str () + "}");
			}

			if (tool_name == "transport/loop_toggle") {
				ARDOUR::Locations* locations = _session.locations ();
				if (!locations) {
					return jsonrpc_error (id, -32602, "Session locations unavailable");
				}

				ARDOUR::Location* loop_loc = locations->auto_loop_location ();
				if (!loop_loc) {
					return jsonrpc_error (id, -32602, "Auto-loop range not set");
				}

				const boost::optional<bool> enabled_opt = root.get_optional<bool> ("params.arguments.enabled");
				const bool requested_enabled = enabled_opt ? *enabled_opt : !_session.get_play_loop ();
				const bool loop_is_mode = ARDOUR::Config->get_loop_is_mode ();
				const bool was_rolling = _session.transport_rolling ();

				if (_session.get_play_loop () != requested_enabled) {
					if (requested_enabled) {
						/* Match BasicUI/OSC semantics: loop-is-mode does not force roll. */
						if (loop_is_mode) {
							_session.request_play_loop (true, false);
						} else {
							_session.request_play_loop (true, true);
							/* Ensure transport-state UI updates if SetLoop and roll requests race. */
							if (!was_rolling) {
								_session.request_roll ();
							}
						}
					} else {
						_session.request_play_loop (false);
					}
				}

				/* Match BasicUI: loop toggle should unhide the loop range. */
				loop_loc->set_hidden (false, 0);

				std::ostringstream structured;
				structured << "{\"requestedEnabled\":" << (requested_enabled ? "true" : "false")
					<< ",\"loopIsMode\":" << (loop_is_mode ? "true" : "false")
					<< ",\"rollingBefore\":" << (was_rolling ? "true" : "false")
					<< ",\"playLoop\":" << (_session.get_play_loop () ? "true" : "false")
					<< ",\"transport\":" << transport_state_json (_session)
					<< ",\"loop\":" << special_range_json (*loop_loc, "auto_loop")
					<< "}";

				return jsonrpc_result (
					id,
					std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Loop playback updated\"}],\"structuredContent\":") + structured.str () + "}");
			}

			if (tool_name == "transport/set_record_enable") {
				const bool requested_enabled = root.get<bool> ("params.arguments.enabled");
				const ARDOUR::RecordState status_before = _session.record_status ();

				if (requested_enabled) {
					_session.maybe_enable_record ();
				} else {
					_session.disable_record (false, true);
				}

				const ARDOUR::RecordState status_after = _session.record_status ();

				std::ostringstream structured;
				structured << "{\"requestedEnabled\":" << (requested_enabled ? "true" : "false")
					<< ",\"recordEnabled\":" << (_session.get_record_enabled () ? "true" : "false")
					<< ",\"recordStatusBefore\":\"" << record_state_string (status_before) << "\""
					<< ",\"recordStatusAfter\":\"" << record_state_string (status_after) << "\""
					<< ",\"transport\":" << transport_state_json (_session)
					<< "}";

				return jsonrpc_result (
					id,
					std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Global record-enable updated\"}],\"structuredContent\":") + structured.str () + "}");
			}

			if (tool_name == "transport/set_speed") {
				const boost::optional<double> speed_opt = root.get_optional<double> ("params.arguments.speed");
				if (!speed_opt || !std::isfinite (*speed_opt)) {
					return jsonrpc_error (id, -32602, "Missing or invalid speed");
				}

				const double speed = *speed_opt;
				/* Match OSC/BasicUI set_transport_speed behavior. */
				_session.request_roll (ARDOUR::TRS_UI);
				_session.request_transport_speed (speed, ARDOUR::TRS_UI);

				std::ostringstream structured;
				structured << "{\"requestedSpeed\":" << speed
					<< ",\"actualSpeed\":" << _session.actual_speed ()
					<< ",\"transport\":" << transport_state_json (_session)
					<< "}";
				return jsonrpc_result (
					id,
					std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Transport speed updated\"}],\"structuredContent\":") + structured.str () + "}");
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

			if (tool_name == "markers/list") {
				std::string structured = markers_list_json (_session);
				return jsonrpc_result (
					id,
					std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Markers listed\"}],\"structuredContent\":") + structured + "}");
			}

				if (tool_name == "markers/add") {
					const boost::optional<std::string> marker_name_opt = root.get_optional<std::string> ("params.arguments.name");
					const boost::optional<std::string> marker_type_opt = root.get_optional<std::string> ("params.arguments.type");
					const std::string requested_name = marker_name_opt ? *marker_name_opt : std::string ();
					const std::string marker_type = marker_type_opt ? *marker_type_opt : std::string ("mark");

					ARDOUR::Location::Flags marker_flags = ARDOUR::Location::IsMark;
					std::string default_name_base = "mark";

					if (marker_type == "mark") {
						marker_flags = ARDOUR::Location::IsMark;
						default_name_base = "mark";
					} else if (marker_type == "section" || marker_type == "arrangement") {
						marker_flags = (ARDOUR::Location::Flags) ((uint32_t) ARDOUR::Location::IsMark | (uint32_t) ARDOUR::Location::IsSection);
						default_name_base = "section";
					} else if (marker_type == "scene") {
						marker_flags = (ARDOUR::Location::Flags) ((uint32_t) ARDOUR::Location::IsMark | (uint32_t) ARDOUR::Location::IsScene);
						default_name_base = "scene";
					} else {
						return jsonrpc_error (id, -32602, "Invalid marker type (expected: mark, section, scene, arrangement)");
					}

					samplepos_t target_sample = _session.audible_sample ();
					bool have_bbt_target = false;
					std::string bbt_error;
					if (!parse_optional_bbt_target_sample (root, "params.arguments", target_sample, have_bbt_target, bbt_error)) {
						return jsonrpc_error (id, -32602, bbt_error);
					}

					ARDOUR::Locations* locations = _session.locations ();
					if (!locations) {
						return jsonrpc_error (id, -32602, "Session locations unavailable");
					}

					std::string marker_name = requested_name;
					if (marker_name.empty ()) {
						locations->next_available_name (marker_name, default_name_base);
					}
					const bool used_default_name = requested_name.empty ();

					const Temporal::timepos_t where (target_sample);
					ARDOUR::Location* location = new ARDOUR::Location (_session, where, where, marker_name, marker_flags);

					/* Match BasicUI::add_marker/OSC behavior: add an undoable marker operation. */
					_session.begin_reversible_command ("add marker");
					XMLNode& before = locations->get_state ();
					locations->add (location, true);
					XMLNode& after = locations->get_state ();
					_session.add_command (new MementoCommand<ARDOUR::Locations> (*locations, &before, &after));
					_session.commit_reversible_command ();

					std::string structured = marker_added_json (*location, used_default_name, requested_name);
					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Marker added\"}],\"structuredContent\":") + structured + "}");
				}

				if (tool_name == "markers/add_range") {
					const boost::optional<std::string> marker_name_opt = root.get_optional<std::string> ("params.arguments.name");
					const std::string requested_name = marker_name_opt ? *marker_name_opt : std::string ();
					samplepos_t start_sample = 0;
					samplepos_t end_sample = 0;
					std::string range_error;
					if (!parse_range_endpoints (root, "params.arguments", start_sample, end_sample, range_error)) {
						return jsonrpc_error (id, -32602, range_error);
					}

					ARDOUR::Locations* locations = _session.locations ();
					if (!locations) {
						return jsonrpc_error (id, -32602, "Session locations unavailable");
					}

					std::string marker_name = requested_name;
					if (marker_name.empty ()) {
						locations->next_available_name (marker_name, "range");
					}
					const bool used_default_name = requested_name.empty ();

					const Temporal::timepos_t start_where (start_sample);
					const Temporal::timepos_t end_where (end_sample);
					ARDOUR::Location* location = new ARDOUR::Location (_session, start_where, end_where, marker_name, ARDOUR::Location::IsRangeMarker);

					_session.begin_reversible_command ("add range marker");
					XMLNode& before = locations->get_state ();
					locations->add (location, true);
					XMLNode& after = locations->get_state ();
					_session.add_command (new MementoCommand<ARDOUR::Locations> (*locations, &before, &after));
					_session.commit_reversible_command ();

					std::string structured = range_added_json (*location, used_default_name, requested_name);
					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Range marker added\"}],\"structuredContent\":") + structured + "}");
				}

				if (tool_name == "markers/set_auto_loop" || tool_name == "transport/loop_location") {
					samplepos_t start_sample = 0;
					samplepos_t end_sample = 0;
					std::string range_error;
					if (!parse_range_endpoints (root, "params.arguments", start_sample, end_sample, range_error)) {
						return jsonrpc_error (id, -32602, range_error);
					}
					if (end_sample <= start_sample) {
						return jsonrpc_error (id, -32602, "Auto-loop range must have positive length");
					}

					ARDOUR::Locations* locations = _session.locations ();
					if (!locations) {
						return jsonrpc_error (id, -32602, "Session locations unavailable");
					}

					ARDOUR::Location* loop_loc = locations->auto_loop_location ();
					if (loop_loc &&
					    loop_loc->start_sample () == start_sample &&
					    loop_loc->end_sample () == end_sample &&
					    !loop_loc->is_hidden ()) {
						std::string structured = special_range_json (*loop_loc, "auto_loop");
						const std::string unchanged_text = (tool_name == "transport/loop_location") ? "Loop location unchanged" : "Auto-loop range unchanged";
						return jsonrpc_result (
							id,
							std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"") + json_escape (unchanged_text) + "\"}],\"structuredContent\":" + structured + "}");
					}

					_session.begin_reversible_command ("set auto loop range");

					if (!loop_loc) {
						const Temporal::timepos_t start_where (start_sample);
						const Temporal::timepos_t end_where (end_sample);
						ARDOUR::Location* loc = new ARDOUR::Location (_session, start_where, end_where, "Loop", ARDOUR::Location::IsAutoLoop);
						XMLNode& before = locations->get_state ();
						locations->add (loc, true);
						_session.set_auto_loop_location (loc);
						XMLNode& after = locations->get_state ();
						_session.add_command (new MementoCommand<ARDOUR::Locations> (*locations, &before, &after));
						loop_loc = loc;
					} else {
						XMLNode& before = loop_loc->get_state ();
						loop_loc->set_hidden (false, 0);
						loop_loc->set (Temporal::timepos_t (start_sample), Temporal::timepos_t (end_sample));
						XMLNode& after = loop_loc->get_state ();
						_session.add_command (new MementoCommand<ARDOUR::Location> (*loop_loc, &before, &after));
					}

					_session.commit_reversible_command ();

					std::string structured = special_range_json (*loop_loc, "auto_loop");
					const std::string updated_text = (tool_name == "transport/loop_location") ? "Loop location updated" : "Auto-loop range updated";
					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"") + json_escape (updated_text) + "\"}],\"structuredContent\":" + structured + "}");
				}

				if (tool_name == "markers/hide_auto_loop") {
					const bool hidden = root.get<bool> ("params.arguments.hidden", true);

					ARDOUR::Locations* locations = _session.locations ();
					if (!locations) {
						return jsonrpc_error (id, -32602, "Session locations unavailable");
					}

					ARDOUR::Location* loop_loc = locations->auto_loop_location ();
					if (!loop_loc) {
						return jsonrpc_error (id, -32602, "Auto-loop range not set");
					}

					if (loop_loc->is_hidden () == hidden) {
						std::string structured = special_range_json (*loop_loc, "auto_loop");
						return jsonrpc_result (
							id,
							std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Auto-loop range visibility unchanged\"}],\"structuredContent\":") + structured + "}");
					}

					_session.begin_reversible_command (hidden ? "hide auto loop range" : "show auto loop range");
					XMLNode& before = loop_loc->get_state ();
					loop_loc->set_hidden (hidden, 0);
					XMLNode& after = loop_loc->get_state ();
					_session.add_command (new MementoCommand<ARDOUR::Location> (*loop_loc, &before, &after));
					_session.commit_reversible_command ();

					std::string structured = special_range_json (*loop_loc, "auto_loop");
					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Auto-loop range visibility updated\"}],\"structuredContent\":") + structured + "}");
				}

				if (tool_name == "markers/set_auto_punch") {
					samplepos_t start_sample = 0;
					samplepos_t end_sample = 0;
					std::string range_error;
					if (!parse_range_endpoints (root, "params.arguments", start_sample, end_sample, range_error)) {
						return jsonrpc_error (id, -32602, range_error);
					}
					if (end_sample <= start_sample) {
						return jsonrpc_error (id, -32602, "Auto-punch range must have positive length");
					}

					ARDOUR::Locations* locations = _session.locations ();
					if (!locations) {
						return jsonrpc_error (id, -32602, "Session locations unavailable");
					}

					ARDOUR::Location* punch_loc = locations->auto_punch_location ();
					if (punch_loc &&
					    punch_loc->start_sample () == start_sample &&
					    punch_loc->end_sample () == end_sample &&
					    !punch_loc->is_hidden ()) {
						std::string structured = special_range_json (*punch_loc, "auto_punch");
						return jsonrpc_result (
							id,
							std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Auto-punch range unchanged\"}],\"structuredContent\":") + structured + "}");
					}

					_session.begin_reversible_command ("set auto punch range");

					if (!punch_loc) {
						const Temporal::timepos_t start_where (start_sample);
						const Temporal::timepos_t end_where (end_sample);
						ARDOUR::Location* loc = new ARDOUR::Location (_session, start_where, end_where, "Punch", ARDOUR::Location::IsAutoPunch);
						XMLNode& before = locations->get_state ();
						locations->add (loc, true);
						_session.set_auto_punch_location (loc);
						XMLNode& after = locations->get_state ();
						_session.add_command (new MementoCommand<ARDOUR::Locations> (*locations, &before, &after));
						punch_loc = loc;
					} else {
						XMLNode& before = punch_loc->get_state ();
						punch_loc->set_hidden (false, 0);
						punch_loc->set (Temporal::timepos_t (start_sample), Temporal::timepos_t (end_sample));
						XMLNode& after = punch_loc->get_state ();
						_session.add_command (new MementoCommand<ARDOUR::Location> (*punch_loc, &before, &after));
					}

					_session.commit_reversible_command ();

					std::string structured = special_range_json (*punch_loc, "auto_punch");
					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Auto-punch range updated\"}],\"structuredContent\":") + structured + "}");
				}

				if (tool_name == "markers/hide_auto_punch") {
					const bool hidden = root.get<bool> ("params.arguments.hidden", true);

					ARDOUR::Locations* locations = _session.locations ();
					if (!locations) {
						return jsonrpc_error (id, -32602, "Session locations unavailable");
					}

					ARDOUR::Location* punch_loc = locations->auto_punch_location ();
					if (!punch_loc) {
						return jsonrpc_error (id, -32602, "Auto-punch range not set");
					}

					if (punch_loc->is_hidden () == hidden) {
						std::string structured = special_range_json (*punch_loc, "auto_punch");
						return jsonrpc_result (
							id,
							std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Auto-punch range visibility unchanged\"}],\"structuredContent\":") + structured + "}");
					}

					_session.begin_reversible_command (hidden ? "hide auto punch range" : "show auto punch range");
					XMLNode& before = punch_loc->get_state ();
					punch_loc->set_hidden (hidden, 0);
					XMLNode& after = punch_loc->get_state ();
					_session.add_command (new MementoCommand<ARDOUR::Location> (*punch_loc, &before, &after));
					_session.commit_reversible_command ();

					std::string structured = special_range_json (*punch_loc, "auto_punch");
					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Auto-punch range visibility updated\"}],\"structuredContent\":") + structured + "}");
				}

				if (tool_name == "markers/delete") {
					const boost::optional<std::string> location_id_opt = root.get_optional<std::string> ("params.arguments.locationId");
					const boost::optional<std::string> name_opt = root.get_optional<std::string> ("params.arguments.name");
					const boost::optional<int64_t> sample_opt = root.get_optional<int64_t> ("params.arguments.sample");
					const std::string location_id = location_id_opt ? *location_id_opt : std::string ();
					const std::string name = name_opt ? *name_opt : std::string ();

					if (location_id.empty () && name.empty ()) {
						return jsonrpc_error (id, -32602, "Provide one of: locationId or name");
					}
					if (sample_opt && *sample_opt < 0) {
						return jsonrpc_error (id, -32602, "Invalid sample (expected >= 0)");
					}

					ARDOUR::Locations* locations = _session.locations ();
					if (!locations) {
						return jsonrpc_error (id, -32602, "Session locations unavailable");
					}

					std::string resolve_error;
					ARDOUR::Location* target = resolve_marker_location (*locations, location_id, name, sample_opt, resolve_error);
					if (!target) {
						return jsonrpc_error (id, -32602, resolve_error);
					}

						const std::string removed_id = target->id ().to_s ();
						const std::string removed_name = target->name ();
						const samplepos_t removed_start_sample = target->start_sample ();
						const samplepos_t removed_end_sample = target->end_sample ();
						const ARDOUR::Location::Flags removed_flags = target->flags ();

					_session.begin_reversible_command ("delete marker");
					XMLNode& before = locations->get_state ();
					locations->remove (target);
					XMLNode& after = locations->get_state ();
					_session.add_command (new MementoCommand<ARDOUR::Locations> (*locations, &before, &after));
					_session.commit_reversible_command ();

						std::string structured = marker_deleted_json (removed_id, removed_name, removed_start_sample, removed_end_sample, removed_flags);
						return jsonrpc_result (
							id,
							std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Marker/range deleted\"}],\"structuredContent\":") + structured + "}");
				}

				if (tool_name == "markers/rename") {
					const boost::optional<std::string> location_id_opt = root.get_optional<std::string> ("params.arguments.locationId");
					const boost::optional<std::string> name_opt = root.get_optional<std::string> ("params.arguments.name");
					const boost::optional<int64_t> sample_opt = root.get_optional<int64_t> ("params.arguments.sample");
					const std::string new_name = root.get<std::string> ("params.arguments.newName", "");
					const std::string location_id = location_id_opt ? *location_id_opt : std::string ();
					const std::string name = name_opt ? *name_opt : std::string ();

					if (new_name.empty ()) {
						return jsonrpc_error (id, -32602, "Missing newName");
					}
					if (location_id.empty () && name.empty ()) {
						return jsonrpc_error (id, -32602, "Provide one of: locationId or name");
					}
					if (sample_opt && *sample_opt < 0) {
						return jsonrpc_error (id, -32602, "Invalid sample (expected >= 0)");
					}

					ARDOUR::Locations* locations = _session.locations ();
					if (!locations) {
						return jsonrpc_error (id, -32602, "Session locations unavailable");
					}

					std::string resolve_error;
					ARDOUR::Location* target = resolve_marker_location (*locations, location_id, name, sample_opt, resolve_error);
					if (!target) {
						return jsonrpc_error (id, -32602, resolve_error);
					}

					const std::string renamed_id = target->id ().to_s ();
					const std::string old_name = target->name ();
					const samplepos_t marker_start_sample = target->start_sample ();
					const samplepos_t marker_end_sample = target->end_sample ();
					const ARDOUR::Location::Flags marker_flags = target->flags ();

					_session.begin_reversible_command ("rename marker");
					XMLNode& before = locations->get_state ();
					target->set_name (new_name);
					XMLNode& after = locations->get_state ();
					_session.add_command (new MementoCommand<ARDOUR::Locations> (*locations, &before, &after));
					_session.commit_reversible_command ();

					std::string structured = marker_renamed_json (renamed_id, old_name, target->name (), marker_start_sample, marker_end_sample, marker_flags);
					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Marker/range renamed\"}],\"structuredContent\":") + structured + "}");
				}

					if (tool_name == "tracks/list") {
						const bool include_hidden = root.get<bool> ("params.arguments.includeHidden", false);
						std::string structured = tracks_list_json (_session, include_hidden);
						std::string text = tracks_list_text (_session, include_hidden);
						return jsonrpc_result (
							id,
							std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"") + json_escape (text) + "\"}],\"structuredContent\":" + structured + "}");
			}

				if (tool_name == "tracks/add" || tool_name == "buses/add") {
					const bool add_tracks = (tool_name == "tracks/add");
					const std::string route_kind = root.get<std::string> ("params.arguments.type", "audio");
					const int64_t count_in = root.get<int64_t> ("params.arguments.count", 1);
					const std::string name_template = root.get<std::string> ("params.arguments.name", "");
					const bool has_input_channels = root.get_child_optional ("params.arguments.inputChannels").is_initialized ();
					const bool has_output_channels = root.get_child_optional ("params.arguments.outputChannels").is_initialized ();
					const int64_t input_channels_in = root.get<int64_t> ("params.arguments.inputChannels", 2);
					const int64_t output_channels_in = root.get<int64_t> ("params.arguments.outputChannels", 2);
					const bool strict_io = root.get<bool> ("params.arguments.strictIo", false);
					const std::string insert_mode = root.get<std::string> ("params.arguments.insert", "end");
					const std::string relative_to_id = root.get<std::string> ("params.arguments.relativeToId", "");

					if (count_in < 1 || count_in > 256) {
						return jsonrpc_error (id, -32602, "Invalid count (expected 1..256)");
					}
					const uint32_t count = (uint32_t) count_in;

					if (insert_mode != "end" && insert_mode != "before" && insert_mode != "after") {
						return jsonrpc_error (id, -32602, "Invalid insert mode (expected: end, before, after)");
					}

					ARDOUR::PresentationInfo::order_t insert_order = ARDOUR::PresentationInfo::max_order;
					std::shared_ptr<ARDOUR::Route> relative_route;
					bool relative_from_selection = false;
					ARDOUR::PresentationInfo::order_t relative_order = 0;

					if (insert_mode != "end") {
						if (!relative_to_id.empty ()) {
							relative_route = route_by_mcp_id (_session, relative_to_id);
							if (!relative_route) {
								return jsonrpc_error (id, -32602, "relativeToId route not found");
							}
						} else {
							relative_route = std::dynamic_pointer_cast<ARDOUR::Route> (_session.selection ().first_selected_stripable ());
							relative_from_selection = true;
							if (!relative_route) {
								return jsonrpc_error (id, -32602, "No selected route; provide relativeToId or select a route");
							}
						}

						relative_order = relative_route->presentation_info ().order ();
						insert_order = relative_order + (insert_mode == "after" ? 1 : 0);
					}

					ARDOUR::RouteList created_routes;
					int resolved_input_channels = 0;
					int resolved_output_channels = 0;

					if (route_kind == "audio") {
						if (input_channels_in < 1 || input_channels_in > 1024 || output_channels_in < 1 || output_channels_in > 1024) {
							return jsonrpc_error (id, -32602, "Invalid audio channel count (expected 1..1024)");
						}

						const int input_channels = (int) input_channels_in;
						int output_channels = (int) output_channels_in;
						resolved_input_channels = input_channels;
						resolved_output_channels = output_channels;

						if (add_tracks && input_channels == 1 && output_channels == 1 && !strict_io) {
							/* Match UI mono-track behavior (mono in, stereo out) so pan is available. */
							output_channels = 2;
							resolved_output_channels = output_channels;
						}

						if (add_tracks) {
							std::list<std::shared_ptr<ARDOUR::AudioTrack> > tracks = _session.new_audio_track (
								input_channels,
								output_channels,
								std::shared_ptr<ARDOUR::RouteGroup> (),
								count,
								name_template,
								insert_order,
								ARDOUR::Normal,
								true,
								false);
							for (std::list<std::shared_ptr<ARDOUR::AudioTrack> >::const_iterator it = tracks.begin (); it != tracks.end (); ++it) {
								if (*it) {
									created_routes.push_back (*it);
								}
							}
						} else {
							created_routes = _session.new_audio_route (
								input_channels,
								output_channels,
								std::shared_ptr<ARDOUR::RouteGroup> (),
								count,
								name_template,
								ARDOUR::PresentationInfo::AudioBus,
								insert_order);
						}
					} else if (route_kind == "midi") {
						if (add_tracks) {
							const ARDOUR::ChanCount midi_io (ARDOUR::DataType::MIDI, 1);
							std::list<std::shared_ptr<ARDOUR::MidiTrack> > tracks = _session.new_midi_track (
								midi_io,
								midi_io,
								strict_io,
								std::shared_ptr<ARDOUR::PluginInfo> (),
								static_cast<ARDOUR::Plugin::PresetRecord*> (0),
								std::shared_ptr<ARDOUR::RouteGroup> (),
								count,
								name_template,
								insert_order,
								ARDOUR::Normal,
								true,
								false);
							for (std::list<std::shared_ptr<ARDOUR::MidiTrack> >::const_iterator it = tracks.begin (); it != tracks.end (); ++it) {
								if (*it) {
									created_routes.push_back (*it);
								}
							}
						} else {
							created_routes = _session.new_midi_route (
								std::shared_ptr<ARDOUR::RouteGroup> (),
								count,
								name_template,
								strict_io,
								std::shared_ptr<ARDOUR::PluginInfo> (),
								static_cast<ARDOUR::Plugin::PresetRecord*> (0),
								ARDOUR::PresentationInfo::MidiBus,
								insert_order);
						}
					} else {
						return jsonrpc_error (id, -32602, "Invalid type (expected: audio or midi)");
					}

					if (created_routes.empty ()) {
						return jsonrpc_error (id, -32000, add_tracks ? "Failed to add track(s)" : "Failed to add bus(es)");
					}

					std::ostringstream structured;
					structured << "{\"kind\":\"" << (add_tracks ? "track" : "bus") << "\""
						<< ",\"type\":\"" << json_escape (route_kind) << "\"";
					if (route_kind == "audio") {
						structured << ",\"io\":{"
							<< "\"requestedInputChannels\":" << (has_input_channels ? std::to_string ((int) input_channels_in) : "null")
							<< ",\"requestedOutputChannels\":" << (has_output_channels ? std::to_string ((int) output_channels_in) : "null")
							<< ",\"resolvedInputChannels\":" << resolved_input_channels
							<< ",\"resolvedOutputChannels\":" << resolved_output_channels
							<< "}";
					}
					structured
						<< ",\"insert\":{\"mode\":\"" << json_escape (insert_mode) << "\"";
					if (insert_mode == "end") {
						structured << ",\"order\":\"end\""
							<< ",\"relativeToId\":null"
							<< ",\"relativeToName\":null"
							<< ",\"relativeFromSelection\":false";
					} else {
						structured << ",\"order\":" << insert_order
							<< ",\"relativeToId\":\"" << json_escape (relative_route->id ().to_s ()) << "\""
							<< ",\"relativeToName\":\"" << json_escape (relative_route->name ()) << "\""
							<< ",\"relativeOrder\":" << relative_order
							<< ",\"relativeFromSelection\":" << (relative_from_selection ? "true" : "false");
					}
					structured << "}"
						<< ",\"created\":" << route_list_json (created_routes)
						<< ",\"transport\":" << transport_state_json (_session)
						<< "}";
					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"") + (add_tracks ? "Track(s) added" : "Bus(es) added") + "\"}],\"structuredContent\":" + structured.str () + "}");
				}

					if (tool_name == "midi_region/add") {
					const std::string track_id = root.get<std::string> ("params.arguments.trackId", "");
					const std::string requested_name = root.get<std::string> ("params.arguments.name", "");

					if (track_id.empty ()) {
						return jsonrpc_error (id, -32602, "Missing trackId");
					}

					const std::shared_ptr<ARDOUR::Route> route = route_by_mcp_id (_session, track_id);
					const std::shared_ptr<ARDOUR::Track> track = std::dynamic_pointer_cast<ARDOUR::Track> (route);
					const std::shared_ptr<ARDOUR::MidiTrack> midi_track = std::dynamic_pointer_cast<ARDOUR::MidiTrack> (track);
					if (!midi_track) {
						return jsonrpc_error (id, -32602, "trackId is not a MIDI track");
					}

					samplepos_t start_sample = 0;
					samplepos_t end_sample = 0;
					std::string range_error;
					if (!parse_range_endpoints (root, "params.arguments", start_sample, end_sample, range_error)) {
						return jsonrpc_error (id, -32602, range_error);
					}
					if (end_sample <= start_sample) {
						return jsonrpc_error (id, -32602, "Invalid range: end must be greater than start");
					}

					const std::shared_ptr<ARDOUR::Playlist> playlist = midi_track->playlist ();
					if (!playlist) {
						return jsonrpc_error (id, -32602, "Track has no playlist");
					}

					const Temporal::timepos_t start_pos (start_sample);
					const Temporal::timepos_t end_pos (end_sample);
					const Temporal::timecnt_t region_length = start_pos.distance (end_pos);
					if (region_length.samples () <= 0) {
						return jsonrpc_error (id, -32602, "Invalid region length");
					}

					std::shared_ptr<ARDOUR::MidiSource> midi_src;
					try {
						midi_src = _session.create_midi_source_by_stealing_name (track);
					} catch (...) {
						midi_src.reset ();
					}
					if (!midi_src) {
						return jsonrpc_error (id, -32000, "Failed to create MIDI source");
					}

					ARDOUR::SourceList srcs;
					srcs.push_back (std::dynamic_pointer_cast<ARDOUR::Source> (midi_src));
					if (srcs.empty () || !srcs.front ()) {
						return jsonrpc_error (id, -32000, "Failed to resolve MIDI source");
					}

					const Temporal::timecnt_t source_start (Temporal::BeatTime); /* zero beats */

					PBD::PropertyList whole_file_props;
					whole_file_props.add (ARDOUR::Properties::start, source_start);
					whole_file_props.add (ARDOUR::Properties::length, region_length);
					whole_file_props.add (ARDOUR::Properties::automatic, true);
					whole_file_props.add (ARDOUR::Properties::whole_file, true);
					whole_file_props.add (ARDOUR::Properties::name, PBD::basename_nosuffix (midi_src->name ()));
					whole_file_props.add (ARDOUR::Properties::opaque, _session.config.get_draw_opaque_midi_regions ());

					std::shared_ptr<ARDOUR::Region> whole_file_region = ARDOUR::RegionFactory::create (srcs, whole_file_props);
					if (!whole_file_region) {
						return jsonrpc_error (id, -32000, "Failed to create whole-file MIDI region");
					}

					PBD::PropertyList playlist_region_props;
					if (requested_name.empty ()) {
						playlist_region_props.add (ARDOUR::Properties::name, whole_file_region->name ());
					} else {
						playlist_region_props.add (ARDOUR::Properties::name, requested_name);
					}

					std::shared_ptr<ARDOUR::Region> region = ARDOUR::RegionFactory::create (whole_file_region, playlist_region_props);
					if (!region) {
						return jsonrpc_error (id, -32000, "Failed to create MIDI playlist region");
					}

					_session.begin_reversible_command ("add midi region");
					playlist->clear_changes ();
					playlist->clear_owned_changes ();
					region->set_position (start_pos);
					playlist->add_region (region, start_pos, 1.0, false);
					playlist->rdiff_and_add_command (&_session);
					_session.commit_reversible_command ();

					std::ostringstream structured;
					structured << "{\"created\":" << midi_region_json (region, track)
						<< ",\"usedDefaultName\":" << (requested_name.empty () ? "true" : "false");
					if (requested_name.empty ()) {
						structured << ",\"requestedName\":null";
					} else {
						structured << ",\"requestedName\":\"" << json_escape (requested_name) << "\"";
					}
					structured << "}";

						return jsonrpc_result (
							id,
							std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"MIDI region added\"}],\"structuredContent\":")
								+ structured.str () + "}");
					}

					if (tool_name == "midi_note/add") {
						const std::string region_id = root.get<std::string> ("params.arguments.regionId", "");
						if (region_id.empty ()) {
							return jsonrpc_error (id, -32602, "Missing regionId");
						}

						const boost::optional<int64_t> note_opt = root.get_optional<int64_t> ("params.arguments.note");
						if (!note_opt || *note_opt < 0 || *note_opt > 127) {
							return jsonrpc_error (id, -32602, "Invalid note (expected 0..127)");
						}

						const boost::optional<double> length_beats_opt = root.get_optional<double> ("params.arguments.lengthBeats");
						if (!length_beats_opt || !std::isfinite (*length_beats_opt) || *length_beats_opt <= 0.0) {
							return jsonrpc_error (id, -32602, "Invalid lengthBeats (expected > 0)");
						}

						const int64_t velocity_in = root.get<int64_t> ("params.arguments.velocity", 100);
						if (velocity_in < 0 || velocity_in > 127) {
							return jsonrpc_error (id, -32602, "Invalid velocity (expected 0..127)");
						}

						const int64_t channel_in = root.get<int64_t> ("params.arguments.channel", 1);
						if (channel_in < 1 || channel_in > 16) {
							return jsonrpc_error (id, -32602, "Invalid channel (expected 1..16)");
						}

						const boost::optional<double> region_beat_opt = root.get_optional<double> ("params.arguments.regionBeat");
						const boost::optional<int64_t> sample_opt = root.get_optional<int64_t> ("params.arguments.sample");

						samplepos_t bbt_sample = 0;
						bool have_bbt_target = false;
						std::string bbt_error;
						if (!parse_optional_bbt_target_sample (root, "params.arguments", bbt_sample, have_bbt_target, bbt_error)) {
							return jsonrpc_error (id, -32602, bbt_error);
						}

						if (region_beat_opt && (!std::isfinite (*region_beat_opt) || *region_beat_opt < 0.0)) {
							return jsonrpc_error (id, -32602, "Invalid regionBeat (expected >= 0)");
						}

						if (sample_opt && *sample_opt < 0) {
							return jsonrpc_error (id, -32602, "Invalid sample (expected >= 0)");
						}

						int position_arg_count = 0;
						if (region_beat_opt) {
							++position_arg_count;
						}
						if (sample_opt) {
							++position_arg_count;
						}
						if (have_bbt_target) {
							++position_arg_count;
						}
						if (position_arg_count != 1) {
							return jsonrpc_error (id, -32602, "Provide exactly one position: regionBeat, sample, or bar+beat");
						}

						const std::shared_ptr<ARDOUR::Region> region = region_by_mcp_id (region_id);
						const std::shared_ptr<ARDOUR::MidiRegion> midi_region = std::dynamic_pointer_cast<ARDOUR::MidiRegion> (region);
						if (!midi_region) {
							return jsonrpc_error (id, -32602, "regionId is not a MIDI region");
						}

						const std::shared_ptr<ARDOUR::MidiModel> model = midi_region->model ();
						if (!model) {
							return jsonrpc_error (id, -32000, "MIDI region model not available");
						}

						Temporal::Beats start_source_beats;
						std::string position_origin;
						if (region_beat_opt) {
							start_source_beats = midi_region->region_beats_to_source_beats (Temporal::Beats::from_double (*region_beat_opt));
							position_origin = "regionBeat";
						} else {
							samplepos_t target_sample = 0;
							if (sample_opt) {
								target_sample = (samplepos_t) *sample_opt;
								position_origin = "sample";
							} else {
								target_sample = bbt_sample;
								position_origin = "bbt";
							}
							start_source_beats = midi_region->absolute_time_to_source_beats (Temporal::timepos_t (target_sample));
						}

						if (start_source_beats < Temporal::Beats ()) {
							return jsonrpc_error (id, -32602, "Target position is before source start");
						}

						const Temporal::Beats length_beats = Temporal::Beats::from_double (*length_beats_opt);
						if (length_beats < Temporal::Beats::one_tick ()) {
							return jsonrpc_error (id, -32602, "lengthBeats too small (minimum is one tick)");
						}

						std::shared_ptr<Evoral::Note<Temporal::Beats> > note (
							new Evoral::Note<Temporal::Beats> (
								(uint8_t) (channel_in - 1),
								start_source_beats,
								length_beats,
								(uint8_t) *note_opt,
								(uint8_t) velocity_in));

						ARDOUR::MidiModel::NoteDiffCommand* cmd = model->new_note_diff_command ("add midi note");
						cmd->add (note);
						model->apply_diff_command_as_commit (_session, cmd);

						const std::shared_ptr<Evoral::Note<Temporal::Beats> > inserted = model->find_note (note->id ());
						if (!inserted) {
							return jsonrpc_error (id, -32000, "MIDI note insertion was rejected by overlap policy");
						}

						std::ostringstream structured;
						structured << "{\"added\":" << midi_note_json (region, inserted, position_origin)
							<< ",\"requested\":{"
							<< "\"note\":" << *note_opt
							<< ",\"velocity\":" << velocity_in
							<< ",\"channel\":" << channel_in
							<< ",\"lengthBeats\":" << *length_beats_opt;
						if (region_beat_opt) {
							structured << ",\"regionBeat\":" << *region_beat_opt
								<< ",\"sample\":null"
								<< ",\"bar\":null"
								<< ",\"beat\":null";
						} else if (sample_opt) {
							structured << ",\"regionBeat\":null"
								<< ",\"sample\":" << *sample_opt
								<< ",\"bar\":null"
								<< ",\"beat\":null";
						} else {
							const int req_bar = root.get<int> ("params.arguments.bar", 0);
							const double req_beat = root.get<double> ("params.arguments.beat", 0.0);
							structured << ",\"regionBeat\":null"
								<< ",\"sample\":null"
								<< ",\"bar\":" << req_bar
								<< ",\"beat\":" << req_beat;
						}
						structured << "}}";

						return jsonrpc_result (
							id,
							std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"MIDI note added\"}],\"structuredContent\":")
								+ structured.str () + "}");
					}

					if (tool_name == "midi_note/list") {
						const std::string region_id = root.get<std::string> ("params.arguments.regionId", "");
						if (region_id.empty ()) {
							return jsonrpc_error (id, -32602, "Missing regionId");
						}

						const std::shared_ptr<ARDOUR::Region> region = region_by_mcp_id (region_id);
						if (!region) {
							return jsonrpc_error (id, -32602, "regionId not found");
						}
						const std::shared_ptr<ARDOUR::MidiRegion> midi_region = std::dynamic_pointer_cast<ARDOUR::MidiRegion> (region);
						if (!midi_region) {
							return jsonrpc_error (id, -32602, "regionId is not a MIDI region");
						}

						const std::shared_ptr<ARDOUR::MidiModel> model = midi_region->model ();
						if (!model) {
							return jsonrpc_error (id, -32000, "MIDI region model not available");
						}

						std::vector<std::string> notes_json;
						const ARDOUR::MidiModel::Notes& notes = model->notes ();
						for (ARDOUR::MidiModel::Notes::const_iterator it = notes.begin (); it != notes.end (); ++it) {
							const std::shared_ptr<Evoral::Note<Temporal::Beats> >& note = *it;
							if (!note) {
								continue;
							}
							notes_json.push_back (midi_note_json (region, note, "list"));
						}

						std::ostringstream structured;
						structured << "{\"region\":" << midi_region_brief_json (region)
							<< ",\"count\":" << notes_json.size ()
							<< ",\"notes\":[";
						for (size_t i = 0; i < notes_json.size (); ++i) {
							if (i > 0) {
								structured << ",";
							}
							structured << notes_json[i];
						}
						structured << "]}";

						return jsonrpc_result (
							id,
							std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"MIDI notes listed\"}],\"structuredContent\":")
								+ structured.str () + "}");
					}

					if (tool_name == "midi_note/edit") {
						const std::string region_id = root.get<std::string> ("params.arguments.regionId", "");
						if (region_id.empty ()) {
							return jsonrpc_error (id, -32602, "Missing regionId");
						}

						const std::shared_ptr<ARDOUR::Region> region = region_by_mcp_id (region_id);
						if (!region) {
							return jsonrpc_error (id, -32602, "regionId not found");
						}
						const std::shared_ptr<ARDOUR::MidiRegion> midi_region = std::dynamic_pointer_cast<ARDOUR::MidiRegion> (region);
						if (!midi_region) {
							return jsonrpc_error (id, -32602, "regionId is not a MIDI region");
						}

						const std::shared_ptr<ARDOUR::MidiModel> model = midi_region->model ();
						if (!model) {
							return jsonrpc_error (id, -32000, "MIDI region model not available");
						}

						boost::optional<pt::ptree&> edits_opt = root.get_child_optional ("params.arguments.edits");
						if (!edits_opt) {
							return jsonrpc_error (id, -32602, "Missing edits array");
						}
						if (edits_opt->empty ()) {
							return jsonrpc_error (id, -32602, "edits must contain at least one item");
						}

						struct PlannedNoteEdit {
							Evoral::event_id_t note_id;
							std::shared_ptr<Evoral::Note<Temporal::Beats> > note;
							bool remove;
							bool change_note;
							bool change_velocity;
							bool change_channel;
							bool change_start;
							bool change_length;
							uint8_t note_value;
							uint8_t velocity_value;
							uint8_t channel_value;
							Temporal::Beats start_value;
							Temporal::Beats length_value;
						};

						std::vector<PlannedNoteEdit> planned;
						std::vector<Evoral::event_id_t> not_found_ids;
						std::vector<std::string> warnings;

						size_t requested_count = 0;
						size_t unchanged_count = 0;
						size_t invalid_count = 0;

						for (pt::ptree::const_iterator it = edits_opt->begin (); it != edits_opt->end (); ++it) {
							++requested_count;
							const pt::ptree& edit = it->second;
							const size_t edit_index = requested_count;

							const boost::optional<int64_t> note_id_opt = edit.get_optional<int64_t> ("noteId");
							if (!note_id_opt) {
								return jsonrpc_error (id, -32602, "Each edit item must include noteId");
							}
							if (*note_id_opt < INT32_MIN || *note_id_opt > INT32_MAX) {
								std::ostringstream w;
								w << "Edit " << edit_index << " skipped: noteId out of range";
								warnings.push_back (w.str ());
								++invalid_count;
								continue;
							}

							const Evoral::event_id_t note_id = (Evoral::event_id_t) *note_id_opt;
							const std::shared_ptr<Evoral::Note<Temporal::Beats> > note = model->find_note (note_id);
							if (!note) {
								not_found_ids.push_back (note_id);
								continue;
							}

							const bool remove_note = edit.get<bool> ("delete", false);

							const boost::optional<int64_t> note_value_opt = edit.get_optional<int64_t> ("note");
							const boost::optional<int64_t> delta_semitones_opt = edit.get_optional<int64_t> ("deltaSemitones");
							if (note_value_opt && delta_semitones_opt) {
								return jsonrpc_error (id, -32602, "Each edit item may include only one of: note or deltaSemitones");
							}

							const boost::optional<int64_t> velocity_value_opt = edit.get_optional<int64_t> ("velocity");
							const boost::optional<int64_t> delta_velocity_opt = edit.get_optional<int64_t> ("deltaVelocity");
							if (velocity_value_opt && delta_velocity_opt) {
								return jsonrpc_error (id, -32602, "Each edit item may include only one of: velocity or deltaVelocity");
							}

							const boost::optional<double> start_beats_opt = edit.get_optional<double> ("startBeats");
							const boost::optional<double> delta_beats_opt = edit.get_optional<double> ("deltaBeats");
							if (start_beats_opt && delta_beats_opt) {
								return jsonrpc_error (id, -32602, "Each edit item may include only one of: startBeats or deltaBeats");
							}

							const boost::optional<int64_t> channel_opt = edit.get_optional<int64_t> ("channel");
							const boost::optional<double> length_beats_opt = edit.get_optional<double> ("lengthBeats");

							if (start_beats_opt && (!std::isfinite (*start_beats_opt) || *start_beats_opt < 0.0)) {
								return jsonrpc_error (id, -32602, "Invalid startBeats (expected finite >= 0)");
							}
							if (delta_beats_opt && !std::isfinite (*delta_beats_opt)) {
								return jsonrpc_error (id, -32602, "Invalid deltaBeats (expected finite number)");
							}
							if (length_beats_opt && (!std::isfinite (*length_beats_opt) || *length_beats_opt <= 0.0)) {
								return jsonrpc_error (id, -32602, "Invalid lengthBeats (expected > 0)");
							}

							PlannedNoteEdit planned_edit;
							planned_edit.note_id = note_id;
							planned_edit.note = note;
							planned_edit.remove = remove_note;
							planned_edit.change_note = false;
							planned_edit.change_velocity = false;
							planned_edit.change_channel = false;
							planned_edit.change_start = false;
							planned_edit.change_length = false;
							planned_edit.note_value = note->note ();
							planned_edit.velocity_value = note->velocity ();
							planned_edit.channel_value = note->channel ();
							planned_edit.start_value = note->time ();
							planned_edit.length_value = note->length ();

							if (remove_note) {
								planned.push_back (planned_edit);
								continue;
							}

							bool invalid_edit = false;
							std::string invalid_reason;

							int note_number = (int) note->note ();
							if (note_value_opt) {
								note_number = (int) *note_value_opt;
							} else if (delta_semitones_opt) {
								note_number += (int) *delta_semitones_opt;
							}
							if (note_number < 0 || note_number > 127) {
								invalid_edit = true;
								invalid_reason = "note out of range after edit";
							}

							int velocity = (int) note->velocity ();
							if (!invalid_edit) {
								if (velocity_value_opt) {
									velocity = (int) *velocity_value_opt;
								} else if (delta_velocity_opt) {
									velocity += (int) *delta_velocity_opt;
								}
								if (velocity < 0 || velocity > 127) {
									invalid_edit = true;
									invalid_reason = "velocity out of range after edit";
								}
							}

							int channel = (int) note->channel ();
							if (!invalid_edit && channel_opt) {
								channel = (int) *channel_opt - 1;
								if (channel < 0 || channel > 15) {
									invalid_edit = true;
									invalid_reason = "channel out of range (expected 1..16)";
								}
							}

							Temporal::Beats start_source = note->time ();
							if (!invalid_edit && (start_beats_opt || delta_beats_opt)) {
								const Temporal::Beats current_region_beats = midi_region->source_beats_to_region_time (note->time ()).beats ();
								double target_region_beats = beats_to_double (current_region_beats);
								if (start_beats_opt) {
									target_region_beats = *start_beats_opt;
								} else {
									target_region_beats += *delta_beats_opt;
								}

								if (!std::isfinite (target_region_beats) || target_region_beats < 0.0) {
									invalid_edit = true;
									invalid_reason = "start position out of range after edit";
								} else {
									start_source = midi_region->region_beats_to_source_beats (Temporal::Beats::from_double (target_region_beats));
									if (start_source < Temporal::Beats ()) {
										invalid_edit = true;
										invalid_reason = "start position maps before source start";
									}
								}
							}

							Temporal::Beats length_source = note->length ();
							if (!invalid_edit && length_beats_opt) {
								length_source = Temporal::Beats::from_double (*length_beats_opt);
								if (length_source < Temporal::Beats::one_tick ()) {
									invalid_edit = true;
									invalid_reason = "lengthBeats too small (minimum is one tick)";
								}
							}

							if (invalid_edit) {
								std::ostringstream w;
								w << "Edit " << edit_index << " for noteId " << note_id << " skipped: " << invalid_reason;
								warnings.push_back (w.str ());
								++invalid_count;
								continue;
							}

							planned_edit.note_value = (uint8_t) note_number;
							planned_edit.velocity_value = (uint8_t) velocity;
							planned_edit.channel_value = (uint8_t) channel;
							planned_edit.start_value = start_source;
							planned_edit.length_value = length_source;

							planned_edit.change_note = (planned_edit.note_value != note->note ());
							planned_edit.change_velocity = (planned_edit.velocity_value != note->velocity ());
							planned_edit.change_channel = (planned_edit.channel_value != note->channel ());
							planned_edit.change_start = (planned_edit.start_value != note->time ());
							planned_edit.change_length = (planned_edit.length_value != note->length ());

							if (!planned_edit.change_note
								&& !planned_edit.change_velocity
								&& !planned_edit.change_channel
								&& !planned_edit.change_start
								&& !planned_edit.change_length) {
								++unchanged_count;
								continue;
							}

							planned.push_back (planned_edit);
						}

						const size_t queued_count = planned.size ();
						if (queued_count > 0) {
							ARDOUR::MidiModel::NoteDiffCommand* cmd = model->new_note_diff_command ("edit midi notes");
							for (size_t i = 0; i < planned.size (); ++i) {
								const PlannedNoteEdit& e = planned[i];
								if (e.remove) {
									cmd->remove (e.note);
									continue;
								}
								if (e.change_note) {
									cmd->change (e.note, ARDOUR::MidiModel::NoteDiffCommand::NoteNumber, e.note_value);
								}
								if (e.change_velocity) {
									cmd->change (e.note, ARDOUR::MidiModel::NoteDiffCommand::Velocity, e.velocity_value);
								}
								if (e.change_channel) {
									cmd->change (e.note, ARDOUR::MidiModel::NoteDiffCommand::Channel, e.channel_value);
								}
								if (e.change_start) {
									cmd->change (e.note, ARDOUR::MidiModel::NoteDiffCommand::StartTime, e.start_value);
								}
								if (e.change_length) {
									cmd->change (e.note, ARDOUR::MidiModel::NoteDiffCommand::Length, e.length_value);
								}
							}
							model->apply_diff_command_as_commit (_session, cmd);
						}

						size_t changed_count = 0;
						size_t deleted_count = 0;
						size_t rejected_count = 0;
						std::vector<std::string> changed_notes_json;
						std::vector<Evoral::event_id_t> deleted_ids;

						for (size_t i = 0; i < planned.size (); ++i) {
							const PlannedNoteEdit& e = planned[i];
							const std::shared_ptr<Evoral::Note<Temporal::Beats> > after = model->find_note (e.note_id);

							if (e.remove) {
								if (after) {
									++rejected_count;
									std::ostringstream w;
									w << "Delete for noteId " << e.note_id << " was rejected";
									warnings.push_back (w.str ());
								} else {
									++deleted_count;
									deleted_ids.push_back (e.note_id);
								}
								continue;
							}

							if (!after) {
								++rejected_count;
								std::ostringstream w;
								w << "Edit for noteId " << e.note_id << " was rejected (note no longer present)";
								warnings.push_back (w.str ());
								continue;
							}

							bool mismatch = false;
							if (e.change_note && after->note () != e.note_value) {
								mismatch = true;
							}
							if (e.change_velocity && after->velocity () != e.velocity_value) {
								mismatch = true;
							}
							if (e.change_channel && after->channel () != e.channel_value) {
								mismatch = true;
							}
							if (e.change_start && after->time () != e.start_value) {
								mismatch = true;
							}
							if (e.change_length && after->length () != e.length_value) {
								mismatch = true;
							}

							if (mismatch) {
								++rejected_count;
								std::ostringstream w;
								w << "Edit for noteId " << e.note_id << " did not fully apply";
								warnings.push_back (w.str ());
								continue;
							}

							++changed_count;
							changed_notes_json.push_back (midi_note_json (region, after, "edit"));
						}

						std::ostringstream structured;
						structured << "{\"region\":" << midi_region_brief_json (region)
							<< ",\"summary\":{"
							<< "\"requested\":" << requested_count
							<< ",\"queued\":" << queued_count
							<< ",\"changed\":" << changed_count
							<< ",\"deleted\":" << deleted_count
							<< ",\"unchanged\":" << unchanged_count
							<< ",\"notFound\":" << not_found_ids.size ()
							<< ",\"invalid\":" << invalid_count
							<< ",\"rejected\":" << rejected_count
							<< "}"
							<< ",\"changed\":[";
						for (size_t i = 0; i < changed_notes_json.size (); ++i) {
							if (i > 0) {
								structured << ",";
							}
							structured << changed_notes_json[i];
						}
						structured << "],\"deletedNoteIds\":[";
						for (size_t i = 0; i < deleted_ids.size (); ++i) {
							if (i > 0) {
								structured << ",";
							}
							structured << deleted_ids[i];
						}
						structured << "],\"notFoundNoteIds\":[";
						for (size_t i = 0; i < not_found_ids.size (); ++i) {
							if (i > 0) {
								structured << ",";
							}
							structured << not_found_ids[i];
						}
						structured << "],\"warnings\":" << json_string_array (warnings) << "}";

						return jsonrpc_result (
							id,
							std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"MIDI notes edited\"}],\"structuredContent\":")
								+ structured.str () + "}");
					}

					if (tool_name == "midi_note/import_json") {
						boost::optional<pt::ptree&> midi_opt = root.get_child_optional ("params.arguments.midi");
						if (!midi_opt) {
							return jsonrpc_error (id, -32602, "Missing midi object");
						}

						const std::string region_id = root.get<std::string> ("params.arguments.regionId", "");
						const std::string track_id_arg = root.get<std::string> ("params.arguments.trackId", "");
						const std::string requested_name = root.get<std::string> ("params.arguments.name", "");

						bool created_region = false;
						std::shared_ptr<ARDOUR::Track> target_track;
						std::shared_ptr<ARDOUR::Region> target_region;

						if (!region_id.empty ()) {
							target_region = region_by_mcp_id (region_id);
							if (!target_region) {
								return jsonrpc_error (id, -32602, "regionId not found");
							}

							if (!track_id_arg.empty ()) {
								std::shared_ptr<ARDOUR::Route> r = route_by_mcp_id (_session, track_id_arg);
								target_track = std::dynamic_pointer_cast<ARDOUR::Track> (r);
							}
						} else {
							if (track_id_arg.empty ()) {
								return jsonrpc_error (id, -32602, "Provide regionId, or trackId with range endpoints");
							}

							std::shared_ptr<ARDOUR::Route> route = route_by_mcp_id (_session, track_id_arg);
							target_track = std::dynamic_pointer_cast<ARDOUR::Track> (route);
							const std::shared_ptr<ARDOUR::MidiTrack> midi_track = std::dynamic_pointer_cast<ARDOUR::MidiTrack> (target_track);
							if (!midi_track) {
								return jsonrpc_error (id, -32602, "trackId is not a MIDI track");
							}

							samplepos_t start_sample = 0;
							samplepos_t end_sample = 0;
							std::string range_error;
							if (!parse_range_endpoints (root, "params.arguments", start_sample, end_sample, range_error)) {
								return jsonrpc_error (id, -32602, range_error);
							}
							if (end_sample <= start_sample) {
								return jsonrpc_error (id, -32602, "Invalid range: end must be greater than start");
							}

							std::string create_error;
							if (!create_midi_region_on_track (_session, target_track, start_sample, end_sample, requested_name, target_region, create_error)) {
								return jsonrpc_error (id, -32000, create_error);
							}
							created_region = true;
						}

						const std::shared_ptr<ARDOUR::MidiRegion> midi_region = std::dynamic_pointer_cast<ARDOUR::MidiRegion> (target_region);
						if (!midi_region) {
							return jsonrpc_error (id, -32602, "Target region is not a MIDI region");
						}

						const std::shared_ptr<ARDOUR::MidiModel> model = midi_region->model ();
						if (!model) {
							return jsonrpc_error (id, -32000, "MIDI region model not available");
						}

						std::vector<MidiJsonEventDef> expanded_events;
						std::vector<MidiJsonNoteDef> note_defs;
						std::vector<std::string> warnings;
						int channel = 9;
						bool is_drum_mode = true;
						int ticks_per_quarter = 480;
						int time_sig_num = 4;
						int time_sig_den = 4;
						std::string parse_error;

						if (!parse_midi_json_events (*midi_opt, expanded_events, channel, is_drum_mode, ticks_per_quarter, time_sig_num, time_sig_den, warnings, parse_error)) {
							return jsonrpc_error (id, -32602, parse_error);
						}
						if (!build_midi_json_note_defs (expanded_events, is_drum_mode, ticks_per_quarter, time_sig_num, time_sig_den, note_defs, warnings, parse_error)) {
							return jsonrpc_error (id, -32602, parse_error);
						}

						ARDOUR::MidiModel::NoteDiffCommand* cmd = model->new_note_diff_command ("import midi json");
						std::vector<std::shared_ptr<Evoral::Note<Temporal::Beats> > > requested_notes;
						requested_notes.reserve (note_defs.size ());

						bool uses_per_event_channels = false;
						for (size_t i = 0; i < expanded_events.size (); ++i) {
							if (expanded_events[i].channel != channel) {
								uses_per_event_channels = true;
								break;
							}
						}

						for (size_t i = 0; i < note_defs.size (); ++i) {
							if (note_defs[i].note < 0 || note_defs[i].note > 127 || note_defs[i].velocity < 0 || note_defs[i].velocity > 127 || note_defs[i].channel < 0 || note_defs[i].channel > 15) {
								std::ostringstream w;
								w << "Skipped invalid note/velocity/channel values at index " << i;
								warnings.push_back (w.str ());
								continue;
							}

							const Temporal::Beats start_source_beats = midi_region->region_beats_to_source_beats (Temporal::Beats::from_double (note_defs[i].start_quarters));
							if (start_source_beats < Temporal::Beats ()) {
								std::ostringstream w;
								w << "Skipped note before source start at index " << i;
								warnings.push_back (w.str ());
								continue;
							}

							const Temporal::Beats length_beats = Temporal::Beats::from_double (note_defs[i].length_quarters);
							if (length_beats < Temporal::Beats ()) {
								std::ostringstream w;
								w << "Skipped note with negative length at index " << i;
								warnings.push_back (w.str ());
								continue;
							}
							if (!is_drum_mode && length_beats < Temporal::Beats::one_tick ()) {
								std::ostringstream w;
								w << "Skipped note shorter than one tick at index " << i;
								warnings.push_back (w.str ());
								continue;
							}

							std::shared_ptr<Evoral::Note<Temporal::Beats> > note (
								new Evoral::Note<Temporal::Beats> (
									(uint8_t) note_defs[i].channel,
									start_source_beats,
									length_beats,
									(uint8_t) note_defs[i].note,
									(uint8_t) note_defs[i].velocity));
							cmd->add (note);
							requested_notes.push_back (note);
						}

						if (!requested_notes.empty ()) {
							model->apply_diff_command_as_commit (_session, cmd);
						} else {
							delete cmd;
						}

						size_t inserted_count = 0;
						for (size_t i = 0; i < requested_notes.size (); ++i) {
							if (model->find_note (requested_notes[i]->id ())) {
								++inserted_count;
							}
						}

						const size_t rejected_count = (note_defs.size () >= inserted_count) ? (note_defs.size () - inserted_count) : 0;

						std::ostringstream structured;
						structured << "{\"createdRegion\":" << (created_region ? "true" : "false")
							<< ",\"region\":" << midi_region_brief_json (target_region)
							<< ",\"summary\":{"
							<< "\"channel\":" << (channel + 1)
							<< ",\"defaultChannel\":" << (channel + 1)
							<< ",\"usesPerEventChannels\":" << (uses_per_event_channels ? "true" : "false")
							<< ",\"isDrumMode\":" << (is_drum_mode ? "true" : "false")
							<< ",\"ticksPerQuarter\":" << ticks_per_quarter
							<< ",\"timeSignature\":{\"numerator\":" << time_sig_num << ",\"denominator\":" << time_sig_den << "}"
							<< ",\"eventsExpanded\":" << expanded_events.size ()
							<< ",\"notesRequested\":" << note_defs.size ()
							<< ",\"notesAttempted\":" << requested_notes.size ()
							<< ",\"notesInserted\":" << inserted_count
							<< ",\"notesRejected\":" << rejected_count
							<< "}"
							<< ",\"warnings\":" << json_string_array (warnings);
						if (target_track) {
							structured << ",\"trackId\":\"" << json_escape (target_track->id ().to_s ()) << "\""
								<< ",\"trackName\":\"" << json_escape (target_track->name ()) << "\"";
						} else {
							structured << ",\"trackId\":null"
								<< ",\"trackName\":null";
						}
						if (requested_name.empty ()) {
							structured << ",\"requestedName\":null";
						} else {
							structured << ",\"requestedName\":\"" << json_escape (requested_name) << "\"";
						}
						structured << "}";

						return jsonrpc_result (
							id,
							std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"MIDI JSON imported\"}],\"structuredContent\":")
								+ structured.str () + "}");
					}

					if (tool_name == "midi_note/get_json") {
						const std::string region_id = root.get<std::string> ("params.arguments.regionId", "");
						if (region_id.empty ()) {
							return jsonrpc_error (id, -32602, "Missing regionId");
						}

						const int64_t ticks_per_quarter_in = root.get<int64_t> ("params.arguments.ticksPerQuarter", 480);
						if (ticks_per_quarter_in <= 0 || ticks_per_quarter_in > 96000) {
							return jsonrpc_error (id, -32602, "Invalid ticksPerQuarter (expected 1..96000)");
						}
						const int ticks_per_quarter = (int) ticks_per_quarter_in;

						const std::string time_signature = root.get<std::string> ("params.arguments.timeSignature", "4/4");
						int time_sig_num = 4;
						int time_sig_den = 4;
						if (!parse_time_signature (time_signature, time_sig_num, time_sig_den)) {
							return jsonrpc_error (id, -32602, "Invalid timeSignature (expected format N/D)");
						}

						const std::shared_ptr<ARDOUR::Region> region = region_by_mcp_id (region_id);
						if (!region) {
							return jsonrpc_error (id, -32602, "regionId not found");
						}
						const std::shared_ptr<ARDOUR::MidiRegion> midi_region = std::dynamic_pointer_cast<ARDOUR::MidiRegion> (region);
						if (!midi_region) {
							return jsonrpc_error (id, -32602, "regionId is not a MIDI region");
						}

						const std::shared_ptr<ARDOUR::MidiModel> model = midi_region->model ();
						if (!model) {
							return jsonrpc_error (id, -32000, "MIDI region model not available");
						}

						struct ExportEvent {
							double quarters;
							size_t ordinal;
							int note;
							int velocity;
							int channel;
							const char* type;
						};

						std::vector<ExportEvent> events;
						std::vector<std::string> warnings;
						std::map<int, size_t> channel_counts;
						size_t notes_seen = 0;
						size_t notes_exported = 0;

						const ARDOUR::MidiModel::Notes& notes = model->notes ();
						for (ARDOUR::MidiModel::Notes::const_iterator it = notes.begin (); it != notes.end (); ++it) {
							const std::shared_ptr<Evoral::Note<Temporal::Beats> >& note = *it;
							if (!note) {
								continue;
							}
							++notes_seen;

							const int note_num = (int) note->note ();
							const int velocity = (int) note->velocity ();
							const int channel = (int) note->channel ();
							if (note_num < 0 || note_num > 127 || velocity < 0 || velocity > 127 || channel < 0 || channel > 15) {
								std::ostringstream w;
								w << "Skipped note with invalid note/velocity/channel (noteId " << note->id () << ")";
								warnings.push_back (w.str ());
								continue;
							}

							const Temporal::Beats start_source_beats = note->time ();
							const Temporal::Beats end_source_beats = start_source_beats + note->length ();
							if (end_source_beats < start_source_beats) {
								std::ostringstream w;
								w << "Skipped note with negative length (noteId " << note->id () << ")";
								warnings.push_back (w.str ());
								continue;
							}

							const Temporal::Beats start_region_beats = midi_region->source_beats_to_region_time (start_source_beats).beats ();
							const Temporal::Beats end_region_beats = midi_region->source_beats_to_region_time (end_source_beats).beats ();
							const double start_quarters = beats_to_double (start_region_beats);
							const double end_quarters = beats_to_double (end_region_beats);
							if (!std::isfinite (start_quarters) || !std::isfinite (end_quarters) || end_quarters < start_quarters) {
								std::ostringstream w;
								w << "Skipped note with invalid export timing (noteId " << note->id () << ")";
								warnings.push_back (w.str ());
								continue;
							}

							const size_t base_ordinal = events.size ();

							ExportEvent on;
							on.quarters = start_quarters;
							on.ordinal = base_ordinal;
							on.note = note_num;
							on.velocity = velocity;
							on.channel = channel;
							on.type = "note_on";
							events.push_back (on);

							ExportEvent off;
							off.quarters = end_quarters;
							off.ordinal = base_ordinal + 1;
							off.note = note_num;
							off.velocity = 0;
							off.channel = channel;
							off.type = "note_off";
							events.push_back (off);

							channel_counts[channel] += 1;
							++notes_exported;
						}

						std::sort (
							events.begin (),
							events.end (),
							[] (const ExportEvent& a, const ExportEvent& b) {
								if (a.quarters != b.quarters) {
									return a.quarters < b.quarters;
								}
								if (a.channel == b.channel && a.note == b.note && a.ordinal != b.ordinal) {
									return a.ordinal < b.ordinal;
								}
								const int a_type_order = (std::strcmp (a.type, "note_off") == 0) ? 0 : 1;
								const int b_type_order = (std::strcmp (b.type, "note_off") == 0) ? 0 : 1;
								if (a_type_order != b_type_order) {
									return a_type_order < b_type_order;
								}
								if (a.channel != b.channel) {
									return a.channel < b.channel;
								}
								if (a.note != b.note) {
									return a.note < b.note;
								}
								return a.ordinal < b.ordinal;
							});

						int default_channel = 9;
						size_t best_count = 0;
						for (std::map<int, size_t>::const_iterator it = channel_counts.begin (); it != channel_counts.end (); ++it) {
							if (it->second > best_count) {
								best_count = it->second;
								default_channel = it->first;
							}
						}

						std::ostringstream events_json;
						events_json << "[";
						bool first_event = true;
						size_t events_exported = 0;
						for (size_t i = 0; i < events.size (); ++i) {
							int bar = 0;
							int beat = 0;
							int tick = 0;
							if (!quarters_to_midi_json_time (events[i].quarters, time_sig_num, time_sig_den, ticks_per_quarter, bar, beat, tick)) {
								std::ostringstream w;
								w << "Skipped event with unrepresentable timing at event index " << i;
								warnings.push_back (w.str ());
								continue;
							}

							if (!first_event) {
								events_json << ",";
							}
							first_event = false;
							events_json << "{\"bar\":" << bar
								<< ",\"b\":" << beat
								<< ",\"t\":" << tick
								<< ",\"n\":" << events[i].note
								<< ",\"v\":" << events[i].velocity
								<< ",\"type\":\"" << events[i].type << "\"";
							if (events[i].channel != default_channel) {
								events_json << ",\"channel\":" << events[i].channel;
							}
							events_json << "}";
							++events_exported;
						}
						events_json << "]";

						std::ostringstream midi_json;
						midi_json << "{\"channel\":" << default_channel
							<< ",\"is_drum_mode\":false"
							<< ",\"time_signature\":\"" << json_escape (time_signature) << "\""
							<< ",\"ticks_per_quarter\":" << ticks_per_quarter
							<< ",\"midi_events\":" << events_json.str ()
							<< "}";

						std::ostringstream structured;
						structured << "{\"region\":" << midi_region_brief_json (region)
							<< ",\"midi\":" << midi_json.str ()
							<< ",\"summary\":{"
							<< "\"notesInRegion\":" << notes_seen
							<< ",\"notesExported\":" << notes_exported
							<< ",\"eventsExported\":" << events_exported
							<< ",\"defaultChannel\":" << default_channel
							<< "}"
							<< ",\"warnings\":" << json_string_array (warnings)
							<< "}";

						return jsonrpc_result (
							id,
							std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"MIDI JSON exported\"}],\"structuredContent\":")
								+ structured.str () + "}");
					}

					if (tool_name == "track/get_info") {
					const std::string route_id = root.get<std::string> ("params.arguments.id", "");

				if (route_id.empty ()) {
					return jsonrpc_error (id, -32602, "Missing track id");
				}

				const std::shared_ptr<ARDOUR::Route> route = route_by_mcp_id (_session, route_id);
				if (!route) {
					return jsonrpc_error (id, -32602, "Route not found");
				}

					std::string structured = track_info_json (route);
					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Route info\"}],\"structuredContent\":") + structured + "}");
					}

					if (tool_name == "track/get_regions") {
					const std::string route_id = root.get<std::string> ("params.arguments.id", "");
					const bool include_hidden = root.get<bool> ("params.arguments.includeHidden", false);

					if (route_id.empty ()) {
						return jsonrpc_error (id, -32602, "Missing track id");
					}

					const std::shared_ptr<ARDOUR::Route> route = route_by_mcp_id (_session, route_id);
					if (!route) {
						return jsonrpc_error (id, -32602, "Route not found");
					}

					const std::shared_ptr<ARDOUR::Track> track = std::dynamic_pointer_cast<ARDOUR::Track> (route);
					if (!track) {
						return jsonrpc_error (id, -32602, "Route is not a track");
					}

					const std::shared_ptr<ARDOUR::Playlist> playlist = track->playlist ();
					if (!playlist) {
						return jsonrpc_error (id, -32000, "Track has no playlist");
					}

					const std::string regions = playlist_regions_json (playlist, include_hidden);
					std::ostringstream structured;
					structured << "{\"id\":\"" << json_escape (route->id ().to_s ()) << "\""
						<< ",\"name\":\"" << json_escape (route->name ()) << "\""
						<< ",\"type\":\"" << json_escape (route_type_string (route)) << "\""
						<< ",\"playlistId\":\"" << json_escape (playlist->id ().to_s ()) << "\""
						<< ",\"includeHidden\":" << (include_hidden ? "true" : "false")
						<< ",\"regions\":" << regions
						<< "}";

					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Track regions listed\"}],\"structuredContent\":")
							+ structured.str () + "}");
					}

					if (tool_name == "region/get_info") {
						const bool include_analysis = root.get<bool> ("params.arguments.includeAnalysis", false);

						std::shared_ptr<ARDOUR::Region> region;
						std::string region_resolved_via;
						std::string region_error;
						if (!resolve_region_argument_or_selected_at_playhead (_session, root, "params.arguments", region, region_resolved_via, region_error)) {
							return jsonrpc_error (id, -32602, region_error);
						}

						boost::optional<double> analyzed_maximum_amplitude;
						boost::optional<double> analyzed_rms;
						if (include_analysis) {
							const std::shared_ptr<ARDOUR::AudioRegion> audio_region = std::dynamic_pointer_cast<ARDOUR::AudioRegion> (region);
							if (audio_region) {
								const double max_amp = audio_region->maximum_amplitude ();
								if (std::isfinite (max_amp) && max_amp >= 0.0) {
									analyzed_maximum_amplitude = max_amp;
								}

								const double rms = audio_region->rms ();
								if (std::isfinite (rms) && rms >= 0.0) {
									analyzed_rms = rms;
								}
							}
						}

						const std::string structured = region_info_json (
							region,
							region_resolved_via,
							include_analysis,
							analyzed_maximum_amplitude,
							analyzed_rms);

						return jsonrpc_result (
							id,
							std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Region info\"}],\"structuredContent\":")
								+ structured + "}");
					}

					if (tool_name == "region/set_gain") {
						const boost::optional<double> linear = root.get_optional<double> ("params.arguments.linear");
						const boost::optional<double> db = root.get_optional<double> ("params.arguments.db");
						const bool invert_polarity = root.get<bool> ("params.arguments.invertPolarity", false);

						if (!linear && !db) {
							return jsonrpc_error (id, -32602, "Provide one of: linear or db");
						}
						if (linear && db) {
							return jsonrpc_error (id, -32602, "Provide only one of: linear or db");
						}

						if (linear) {
							if (!std::isfinite (*linear) || *linear < 0.0) {
								return jsonrpc_error (id, -32602, "Invalid linear gain (expected finite >= 0)");
							}
						}
						if (db) {
							if (!std::isfinite (*db)) {
								return jsonrpc_error (id, -32602, "Invalid dB value");
							}
						}

						std::shared_ptr<ARDOUR::Region> region;
						std::shared_ptr<ARDOUR::AudioRegion> audio_region;
						std::string region_resolved_via;
						std::string region_error;
						if (!resolve_audio_region_argument_or_selected_at_playhead (
								_session,
								root,
								"params.arguments",
								region,
								audio_region,
								region_resolved_via,
								region_error)) {
							return jsonrpc_error (id, -32602, region_error);
						}

						const double previous_gain = audio_region->scale_amplitude ();
						const double previous_db = safe_region_gain_db (previous_gain);
						double new_magnitude = std::fabs (previous_gain);

						if (linear) {
							new_magnitude = *linear;
						} else {
							new_magnitude = (*db <= -192.0) ? 0.0 : dB_to_coefficient (*db);
						}

						if (!std::isfinite (new_magnitude) || new_magnitude < 0.0) {
							return jsonrpc_error (id, -32602, "Invalid mapped region gain");
						}

						double sign = (previous_gain < 0.0) ? -1.0 : 1.0;
						if (invert_polarity) {
							sign *= -1.0;
						}
						const double new_gain = (new_magnitude == 0.0) ? 0.0 : (new_magnitude * sign);
						const bool updated = (new_gain != previous_gain);

						if (updated) {
							_session.begin_reversible_command ("set region gain");
							region->clear_changes ();
							audio_region->set_scale_amplitude ((ARDOUR::gain_t) new_gain);
							_session.add_command (new PBD::StatefulDiffCommand (region));
							_session.commit_reversible_command ();
						}

						std::ostringstream structured;
						structured << "{\"regionId\":\"" << json_escape (region->id ().to_s ()) << "\""
							<< ",\"name\":\"" << json_escape (region->name ()) << "\""
							<< ",\"resolvedVia\":\"" << json_escape (region_resolved_via) << "\""
							<< ",\"updated\":" << (updated ? "true" : "false")
							<< ",\"requested\":{"
							<< "\"linear\":";
						if (linear) {
							structured << *linear;
						} else {
							structured << "null";
						}
						structured << ",\"db\":";
						if (db) {
							structured << *db;
						} else {
							structured << "null";
						}
						structured << ",\"invertPolarity\":" << (invert_polarity ? "true" : "false")
							<< "},\"gain\":{"
							<< "\"previousLinear\":" << previous_gain
							<< ",\"previousDb\":" << previous_db
							<< ",\"linear\":" << audio_region->scale_amplitude ()
							<< ",\"db\":" << safe_region_gain_db (audio_region->scale_amplitude ())
							<< ",\"polarityInverted\":" << (audio_region->scale_amplitude () < 0.0 ? "true" : "false")
							<< "}}";

						return jsonrpc_result (
							id,
							std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"")
								+ (updated ? "Region gain updated" : "Region gain unchanged")
								+ "\"}],\"structuredContent\":" + structured.str () + "}");
					}

					if (tool_name == "region/normalize") {
						const double target_db = root.get<double> ("params.arguments.targetDb", 0.0);
						if (!std::isfinite (target_db)) {
							return jsonrpc_error (id, -32602, "Invalid targetDb");
						}

						std::shared_ptr<ARDOUR::Region> region;
						std::shared_ptr<ARDOUR::AudioRegion> audio_region;
						std::string region_resolved_via;
						std::string region_error;
						if (!resolve_audio_region_argument_or_selected_at_playhead (
								_session,
								root,
								"params.arguments",
								region,
								audio_region,
								region_resolved_via,
								region_error)) {
							return jsonrpc_error (id, -32602, region_error);
						}

						const double peak_amplitude = audio_region->maximum_amplitude ();
						if (!std::isfinite (peak_amplitude) || peak_amplitude < 0.0) {
							return jsonrpc_error (id, -32000, "Failed to analyze region peak amplitude");
						}

						const double previous_gain = audio_region->scale_amplitude ();
						const double previous_db = safe_region_gain_db (previous_gain);

						_session.begin_reversible_command ("normalize region");
						region->clear_changes ();
						audio_region->normalize ((float) peak_amplitude, (float) target_db);
						const double normalized_gain = audio_region->scale_amplitude ();
						const bool updated = (normalized_gain != previous_gain);
						_session.add_command (new PBD::StatefulDiffCommand (region));
						_session.commit_reversible_command ();

						std::ostringstream structured;
						structured << "{\"regionId\":\"" << json_escape (region->id ().to_s ()) << "\""
							<< ",\"name\":\"" << json_escape (region->name ()) << "\""
							<< ",\"resolvedVia\":\"" << json_escape (region_resolved_via) << "\""
							<< ",\"targetDb\":" << target_db
							<< ",\"peakAmplitude\":" << peak_amplitude
							<< ",\"updated\":" << (updated ? "true" : "false")
							<< ",\"gain\":{"
							<< "\"previousLinear\":" << previous_gain
							<< ",\"previousDb\":" << previous_db
							<< ",\"linear\":" << audio_region->scale_amplitude ()
							<< ",\"db\":" << safe_region_gain_db (audio_region->scale_amplitude ())
							<< ",\"polarityInverted\":" << (audio_region->scale_amplitude () < 0.0 ? "true" : "false")
							<< "}}";

						return jsonrpc_result (
							id,
							std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"")
								+ (updated ? "Region normalized" : "Region unchanged")
								+ "\"}],\"structuredContent\":" + structured.str () + "}");
					}

					if (tool_name == "region/split") {
						Temporal::TempoMap::fetch ();

						const boost::optional<int64_t> sample_opt = root.get_optional<int64_t> ("params.arguments.sample");
						if (sample_opt && *sample_opt < 0) {
							return jsonrpc_error (id, -32602, "Invalid sample (expected >= 0)");
						}

						samplepos_t bbt_sample = 0;
						bool have_bbt_target = false;
						std::string bbt_error;
						if (!parse_optional_bbt_target_sample (root, "params.arguments", bbt_sample, have_bbt_target, bbt_error)) {
							return jsonrpc_error (id, -32602, bbt_error);
						}

						if (sample_opt && have_bbt_target) {
							return jsonrpc_error (id, -32602, "Provide either sample or bar+beat, not both");
						}

						std::shared_ptr<ARDOUR::Region> region;
						std::string region_resolved_via;
						std::string region_error;
						if (!resolve_region_argument_or_selected_at_playhead (_session, root, "params.arguments", region, region_resolved_via, region_error)) {
							return jsonrpc_error (id, -32602, region_error);
						}

						const std::shared_ptr<ARDOUR::Playlist> playlist = region->playlist ();
						if (!playlist) {
							return jsonrpc_error (id, -32000, "Region has no playlist");
						}
						if (region->locked ()) {
							return jsonrpc_error (id, -32602, "Region is locked");
						}

						samplepos_t split_sample = _session.transport_sample ();
						std::string split_origin = "playhead";
						if (sample_opt) {
							split_sample = (samplepos_t) *sample_opt;
							split_origin = "sample";
						} else if (have_bbt_target) {
							split_sample = bbt_sample;
							split_origin = "bbt";
						}

						const samplepos_t region_start_sample = region->position_sample ();
						const samplepos_t region_end_sample = region_start_sample + region->length_samples ();
						if (split_sample <= region_start_sample || split_sample >= region_end_sample) {
							std::ostringstream structured;
							structured << "{\"regionId\":\"" << json_escape (region->id ().to_s ()) << "\""
								<< ",\"name\":\"" << json_escape (region->name ()) << "\""
								<< ",\"type\":\"" << json_escape (region->data_type ().to_string ()) << "\""
								<< ",\"playlistId\":\"" << json_escape (playlist->id ().to_s ()) << "\""
								<< ",\"resolvedVia\":\"" << json_escape (region_resolved_via) << "\""
								<< ",\"split\":false"
								<< ",\"reason\":\"split point outside region body\""
								<< ",\"splitOrigin\":\"" << json_escape (split_origin) << "\""
								<< ",\"splitSample\":" << split_sample
								<< ",\"splitBbt\":" << bbt_json_at_sample (split_sample)
								<< ",\"regionStartSample\":" << region_start_sample
								<< ",\"regionEndSample\":" << region_end_sample
								<< ",\"regionStartBbt\":" << bbt_json_at_sample (region_start_sample)
								<< ",\"regionEndBbt\":" << bbt_json_at_sample (region_end_sample)
								<< ",\"createdRegions\":[]}";

							return jsonrpc_result (
								id,
								std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Region unchanged\"}],\"structuredContent\":")
									+ structured.str () + "}");
						}

						std::vector<std::string> playlist_region_ids_before;
						{
							const ARDOUR::RegionList& existing = playlist->region_list_property ().rlist ();
							for (ARDOUR::RegionList::const_iterator i = existing.begin (); i != existing.end (); ++i) {
								if (*i) {
									playlist_region_ids_before.push_back ((*i)->id ().to_s ());
								}
							}
						}

						_session.begin_reversible_command ("split region");
						playlist->clear_changes ();
						playlist->split_region (region, Temporal::timepos_t (split_sample));
						_session.add_command (new PBD::StatefulDiffCommand (playlist));
						_session.commit_reversible_command ();

						std::vector<std::shared_ptr<ARDOUR::Region> > created_regions;
						bool source_region_remaining = false;
						{
							const ARDOUR::RegionList& after = playlist->region_list_property ().rlist ();
							for (ARDOUR::RegionList::const_iterator i = after.begin (); i != after.end (); ++i) {
								if (!*i) {
									continue;
								}

								const std::string candidate_id = (*i)->id ().to_s ();
								if (candidate_id == region->id ().to_s ()) {
									source_region_remaining = true;
								}

								if (std::find (playlist_region_ids_before.begin (), playlist_region_ids_before.end (), candidate_id) == playlist_region_ids_before.end ()) {
									created_regions.push_back (*i);
								}
							}
						}

						std::sort (
							created_regions.begin (),
							created_regions.end (),
							[] (const std::shared_ptr<ARDOUR::Region>& a, const std::shared_ptr<ARDOUR::Region>& b) {
								if (a->position_sample () != b->position_sample ()) {
									return a->position_sample () < b->position_sample ();
								}
								return a->id () < b->id ();
							});

						const bool split_performed = (created_regions.size () >= 2) && !source_region_remaining;
						std::ostringstream structured;
						structured << "{\"regionId\":\"" << json_escape (region->id ().to_s ()) << "\""
							<< ",\"name\":\"" << json_escape (region->name ()) << "\""
							<< ",\"type\":\"" << json_escape (region->data_type ().to_string ()) << "\""
							<< ",\"playlistId\":\"" << json_escape (playlist->id ().to_s ()) << "\""
							<< ",\"resolvedVia\":\"" << json_escape (region_resolved_via) << "\""
							<< ",\"split\":" << (split_performed ? "true" : "false")
							<< ",\"splitOrigin\":\"" << json_escape (split_origin) << "\""
							<< ",\"splitSample\":" << split_sample
							<< ",\"splitBbt\":" << bbt_json_at_sample (split_sample)
							<< ",\"regionStartSample\":" << region_start_sample
							<< ",\"regionEndSample\":" << region_end_sample
							<< ",\"regionStartBbt\":" << bbt_json_at_sample (region_start_sample)
							<< ",\"regionEndBbt\":" << bbt_json_at_sample (region_end_sample)
							<< ",\"sourceRegionRemoved\":" << (source_region_remaining ? "false" : "true")
							<< ",\"createdRegions\":[";

						for (size_t i = 0; i < created_regions.size (); ++i) {
							if (i > 0) {
								structured << ",";
							}
							const samplepos_t created_start_sample = created_regions[i]->position_sample ();
							const samplepos_t created_end_sample = created_start_sample + created_regions[i]->length_samples ();
							structured << "{\"regionId\":\"" << json_escape (created_regions[i]->id ().to_s ()) << "\""
								<< ",\"name\":\"" << json_escape (created_regions[i]->name ()) << "\""
								<< ",\"type\":\"" << json_escape (created_regions[i]->data_type ().to_string ()) << "\""
								<< ",\"startSample\":" << created_start_sample
								<< ",\"endSample\":" << created_end_sample
								<< ",\"lengthSamples\":" << created_regions[i]->length_samples ()
								<< ",\"startBbt\":" << bbt_json_at_sample (created_start_sample)
								<< ",\"endBbt\":" << bbt_json_at_sample (created_end_sample)
								<< "}";
						}
						structured << "]}";

						return jsonrpc_result (
							id,
							std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"")
								+ (split_performed ? "Region split" : "Region unchanged")
								+ "\"}],\"structuredContent\":" + structured.str () + "}");
					}

					if (tool_name == "region/resize") {
						Temporal::TempoMap::fetch ();

						samplepos_t requested_start_boundary = 0;
						samplepos_t requested_end_boundary = 0;
						bool have_start_boundary = false;
						bool have_end_boundary = false;
						std::string boundary_error;

						if (!parse_optional_timeline_boundary_sample (
								root, "params.arguments",
								"startSample", "startBar", "startBeat",
								requested_start_boundary, have_start_boundary, boundary_error)) {
							return jsonrpc_error (id, -32602, boundary_error);
						}
						if (!parse_optional_timeline_boundary_sample (
								root, "params.arguments",
								"endSample", "endBar", "endBeat",
								requested_end_boundary, have_end_boundary, boundary_error)) {
							return jsonrpc_error (id, -32602, boundary_error);
						}
						if (!have_start_boundary && !have_end_boundary) {
							return jsonrpc_error (id, -32602, "Provide at least one boundary (startSample/startBar+startBeat and/or endSample/endBar+endBeat)");
						}

						std::shared_ptr<ARDOUR::Region> region;
						std::string region_resolved_via;
						std::string region_error;
						if (!resolve_region_argument_or_selected_at_playhead (_session, root, "params.arguments", region, region_resolved_via, region_error)) {
							return jsonrpc_error (id, -32602, region_error);
						}
						const std::shared_ptr<ARDOUR::Playlist> playlist = region->playlist ();
						if (!playlist) {
							return jsonrpc_error (id, -32000, "Region has no playlist");
						}
						if (region->locked ()) {
							return jsonrpc_error (id, -32602, "Region is locked");
						}

						const samplepos_t previous_start_sample = region->position_sample ();
						const samplepos_t previous_end_sample = previous_start_sample + region->length_samples ();
						const samplepos_t requested_start_sample = have_start_boundary ? requested_start_boundary : previous_start_sample;
						const samplepos_t requested_end_sample = have_end_boundary ? requested_end_boundary : previous_end_sample;

						if (requested_end_sample <= requested_start_sample) {
							return jsonrpc_error (id, -32602, "Invalid region bounds: end must be greater than start");
						}

						const ARDOUR::Trimmable::CanTrim can_trim = region->can_trim ();
						const int trim_flags = (int) can_trim;
						if (requested_start_sample < previous_start_sample && !(trim_flags & ARDOUR::Trimmable::FrontTrimEarlier)) {
							return jsonrpc_error (id, -32602, "Region front cannot be trimmed earlier");
						}
						if (requested_start_sample > previous_start_sample && !(trim_flags & ARDOUR::Trimmable::FrontTrimLater)) {
							return jsonrpc_error (id, -32602, "Region front cannot be trimmed later");
						}
						if (requested_end_sample < previous_end_sample && !(trim_flags & ARDOUR::Trimmable::EndTrimEarlier)) {
							return jsonrpc_error (id, -32602, "Region end cannot be trimmed earlier");
						}
						if (requested_end_sample > previous_end_sample && !(trim_flags & ARDOUR::Trimmable::EndTrimLater)) {
							return jsonrpc_error (id, -32602, "Region end cannot be trimmed later");
						}

						if (requested_start_sample != previous_start_sample || requested_end_sample != previous_end_sample) {
							_session.begin_reversible_command ("resize region");
							region->clear_changes ();
							region->trim_to (
								Temporal::timepos_t (requested_start_sample),
								Temporal::timepos_t (requested_start_sample).distance (Temporal::timepos_t (requested_end_sample)));
							_session.add_command (new PBD::StatefulDiffCommand (region));
							_session.commit_reversible_command ();
						}

						const samplepos_t resized_start_sample = region->position_sample ();
						const samplepos_t resized_end_sample = resized_start_sample + region->length_samples ();
						const bool resized = (resized_start_sample != previous_start_sample) || (resized_end_sample != previous_end_sample);

						std::ostringstream structured;
						structured << "{\"regionId\":\"" << json_escape (region->id ().to_s ()) << "\""
							<< ",\"name\":\"" << json_escape (region->name ()) << "\""
							<< ",\"type\":\"" << json_escape (region->data_type ().to_string ()) << "\""
							<< ",\"playlistId\":\"" << json_escape (playlist->id ().to_s ()) << "\""
							<< ",\"resolvedVia\":\"" << json_escape (region_resolved_via) << "\""
							<< ",\"resized\":" << (resized ? "true" : "false")
							<< ",\"previousStartSample\":" << previous_start_sample
							<< ",\"previousEndSample\":" << previous_end_sample
							<< ",\"requestedStartSample\":" << requested_start_sample
							<< ",\"requestedEndSample\":" << requested_end_sample
							<< ",\"startSample\":" << resized_start_sample
							<< ",\"endSample\":" << resized_end_sample
							<< ",\"lengthSamples\":" << region->length_samples ()
							<< ",\"previousStartBbt\":" << bbt_json_at_sample (previous_start_sample)
							<< ",\"previousEndBbt\":" << bbt_json_at_sample (previous_end_sample)
							<< ",\"requestedStartBbt\":" << bbt_json_at_sample (requested_start_sample)
							<< ",\"requestedEndBbt\":" << bbt_json_at_sample (requested_end_sample)
							<< ",\"startBbt\":" << bbt_json_at_sample (resized_start_sample)
							<< ",\"endBbt\":" << bbt_json_at_sample (resized_end_sample)
							<< ",\"requested\":{";

						const boost::optional<int64_t> req_start_sample_opt = root.get_optional<int64_t> ("params.arguments.startSample");
						const boost::optional<int> req_start_bar_opt = root.get_optional<int> ("params.arguments.startBar");
						const boost::optional<double> req_start_beat_opt = root.get_optional<double> ("params.arguments.startBeat");
						const boost::optional<int64_t> req_end_sample_opt = root.get_optional<int64_t> ("params.arguments.endSample");
						const boost::optional<int> req_end_bar_opt = root.get_optional<int> ("params.arguments.endBar");
						const boost::optional<double> req_end_beat_opt = root.get_optional<double> ("params.arguments.endBeat");

						structured << "\"start\":{"
							<< "\"sample\":";
						if (req_start_sample_opt) {
							structured << *req_start_sample_opt;
						} else {
							structured << "null";
						}
						structured << ",\"bar\":";
						if (req_start_bar_opt) {
							structured << *req_start_bar_opt;
						} else {
							structured << "null";
						}
						structured << ",\"beat\":";
						if (req_start_beat_opt) {
							structured << *req_start_beat_opt;
						} else {
							structured << "null";
						}
						structured << "},\"end\":{"
							<< "\"sample\":";
						if (req_end_sample_opt) {
							structured << *req_end_sample_opt;
						} else {
							structured << "null";
						}
						structured << ",\"bar\":";
						if (req_end_bar_opt) {
							structured << *req_end_bar_opt;
						} else {
							structured << "null";
						}
						structured << ",\"beat\":";
						if (req_end_beat_opt) {
							structured << *req_end_beat_opt;
						} else {
							structured << "null";
						}
						structured << "}}}";

						return jsonrpc_result (
							id,
							std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"")
								+ (resized ? "Region resized" : "Region unchanged")
								+ "\"}],\"structuredContent\":" + structured.str () + "}");
					}

					if (tool_name == "region/copy") {
						Temporal::TempoMap::fetch ();

						const std::string target_track_id = root.get<std::string> ("params.arguments.trackId", "");

						const boost::optional<int64_t> sample_opt = root.get_optional<int64_t> ("params.arguments.sample");
						const boost::optional<int64_t> delta_samples_opt = root.get_optional<int64_t> ("params.arguments.deltaSamples");
						const boost::optional<double> delta_beats_opt = root.get_optional<double> ("params.arguments.deltaBeats");

						samplepos_t bbt_sample = 0;
						bool have_bbt_target = false;
						std::string bbt_error;
						if (!parse_optional_bbt_target_sample (root, "params.arguments", bbt_sample, have_bbt_target, bbt_error)) {
							return jsonrpc_error (id, -32602, bbt_error);
						}

						if (sample_opt && *sample_opt < 0) {
							return jsonrpc_error (id, -32602, "Invalid sample (expected >= 0)");
						}
						if (delta_beats_opt && !std::isfinite (*delta_beats_opt)) {
							return jsonrpc_error (id, -32602, "Invalid deltaBeats (expected finite number)");
						}

						int target_mode_count = 0;
						if (sample_opt) {
							++target_mode_count;
						}
						if (have_bbt_target) {
							++target_mode_count;
						}
						if (delta_samples_opt) {
							++target_mode_count;
						}
						if (delta_beats_opt) {
							++target_mode_count;
						}
						if (target_mode_count != 1) {
							return jsonrpc_error (id, -32602, "Provide exactly one target: sample, bar+beat, deltaSamples, or deltaBeats");
						}

						std::shared_ptr<ARDOUR::Region> region;
						std::string region_resolved_via;
						std::string region_error;
						if (!resolve_region_argument_or_selected_at_playhead (_session, root, "params.arguments", region, region_resolved_via, region_error)) {
							return jsonrpc_error (id, -32602, region_error);
						}

						const std::shared_ptr<ARDOUR::Playlist> source_playlist = region->playlist ();
						if (!source_playlist) {
							return jsonrpc_error (id, -32000, "Region has no playlist");
						}

						std::shared_ptr<ARDOUR::Track> target_track;
						std::shared_ptr<ARDOUR::Playlist> target_playlist = source_playlist;
						if (!target_track_id.empty ()) {
							const std::shared_ptr<ARDOUR::Route> route = route_by_mcp_id (_session, target_track_id);
							target_track = std::dynamic_pointer_cast<ARDOUR::Track> (route);
							if (!target_track) {
								return jsonrpc_error (id, -32602, "trackId is not a track");
							}

							target_playlist = target_track->playlist ();
							if (!target_playlist) {
								return jsonrpc_error (id, -32000, "Destination track has no playlist");
							}
						}

						if (target_playlist->data_type () != region->data_type ()) {
							return jsonrpc_error (id, -32602, "Region type does not match destination track type");
						}

						const samplepos_t source_start_sample = region->position_sample ();
						samplepos_t requested_start_sample = source_start_sample;
						std::string copy_origin = "none";

						if (sample_opt) {
							requested_start_sample = (samplepos_t) *sample_opt;
							copy_origin = "sample";
						} else if (have_bbt_target) {
							requested_start_sample = bbt_sample;
							copy_origin = "bbt";
						} else if (delta_samples_opt) {
							const int64_t current = (int64_t) source_start_sample;
							const int64_t delta = *delta_samples_opt;
							if ((delta > 0 && current > (LLONG_MAX - delta)) || (delta < 0 && current < (LLONG_MIN - delta))) {
								return jsonrpc_error (id, -32602, "deltaSamples overflow");
							}
							const int64_t target = current + delta;
							if (target < 0) {
								return jsonrpc_error (id, -32602, "Resulting position is before session start");
							}
							requested_start_sample = (samplepos_t) target;
							copy_origin = "deltaSamples";
						} else {
							const Temporal::Beats current_quarters = Temporal::TempoMap::use ()->quarters_at (Temporal::timepos_t (source_start_sample));
							const Temporal::Beats target_quarters = current_quarters + Temporal::Beats::from_double (*delta_beats_opt);
							if (target_quarters < Temporal::Beats ()) {
								return jsonrpc_error (id, -32602, "Resulting position is before session start");
							}
							requested_start_sample = Temporal::TempoMap::use ()->sample_at (target_quarters);
							copy_origin = "deltaBeats";
						}

						const bool cross_track = target_playlist != source_playlist;
						const std::shared_ptr<ARDOUR::Region> region_copy = ARDOUR::RegionFactory::create (region, true);
						if (!region_copy) {
							return jsonrpc_error (id, -32000, "Failed to create region copy");
						}

						std::vector<std::string> target_region_ids_before;
						{
							const ARDOUR::RegionList& existing = target_playlist->region_list_property ().rlist ();
							for (ARDOUR::RegionList::const_iterator i = existing.begin (); i != existing.end (); ++i) {
								target_region_ids_before.push_back ((*i)->id ().to_s ());
							}
						}

						_session.begin_reversible_command ("copy region");
						target_playlist->clear_changes ();
						target_playlist->clear_owned_changes ();
						target_playlist->add_region (region_copy, Temporal::timepos_t (requested_start_sample), 1.0, false);
						target_playlist->rdiff_and_add_command (&_session);
						_session.commit_reversible_command ();

						std::shared_ptr<ARDOUR::Region> inserted_region;
						{
							const ARDOUR::RegionList& after = target_playlist->region_list_property ().rlist ();
							for (ARDOUR::RegionList::const_iterator i = after.begin (); i != after.end (); ++i) {
								const std::string candidate_id = (*i)->id ().to_s ();
								if (std::find (target_region_ids_before.begin (), target_region_ids_before.end (), candidate_id) == target_region_ids_before.end ()) {
									inserted_region = *i;
									break;
								}
							}
						}

						const std::shared_ptr<ARDOUR::Region> copied_region = inserted_region ? inserted_region : region_copy;
						const samplepos_t copied_start_sample = copied_region->position_sample ();
						const samplepos_t copied_end_sample = copied_start_sample + copied_region->length_samples ();

						std::ostringstream structured;
						structured << "{\"regionId\":\"" << json_escape (copied_region->id ().to_s ()) << "\""
							<< ",\"sourceRegionId\":\"" << json_escape (region->id ().to_s ()) << "\""
							<< ",\"name\":\"" << json_escape (copied_region->name ()) << "\""
							<< ",\"type\":\"" << json_escape (copied_region->data_type ().to_string ()) << "\""
							<< ",\"playlistId\":\"" << json_escape (target_playlist->id ().to_s ()) << "\""
							<< ",\"sourcePlaylistId\":\"" << json_escape (source_playlist->id ().to_s ()) << "\""
							<< ",\"resolvedVia\":\"" << json_escape (region_resolved_via) << "\""
							<< ",\"trackId\":";
						if (target_track) {
							structured << "\"" << json_escape (target_track->id ().to_s ()) << "\"";
						} else {
							structured << "null";
						}
						structured << ",\"lengthSamples\":" << copied_region->length_samples ()
							<< ",\"crossTrack\":" << (cross_track ? "true" : "false")
							<< ",\"copyOrigin\":\"" << json_escape (copy_origin) << "\""
							<< ",\"copied\":true"
							<< ",\"sourceStartSample\":" << source_start_sample
							<< ",\"requestedStartSample\":" << requested_start_sample
							<< ",\"startSample\":" << copied_start_sample
							<< ",\"endSample\":" << copied_end_sample
							<< ",\"sourceStartBbt\":" << bbt_json_at_sample (source_start_sample)
							<< ",\"requestedStartBbt\":" << bbt_json_at_sample (requested_start_sample)
							<< ",\"startBbt\":" << bbt_json_at_sample (copied_start_sample)
							<< ",\"endBbt\":" << bbt_json_at_sample (copied_end_sample)
							<< ",\"requested\":{";
						if (sample_opt) {
							structured << "\"sample\":" << *sample_opt
								<< ",\"bar\":null,\"beat\":null,\"deltaSamples\":null,\"deltaBeats\":null";
						} else if (have_bbt_target) {
							const int req_bar = root.get<int> ("params.arguments.bar", 0);
							const double req_beat = root.get<double> ("params.arguments.beat", 0.0);
							structured << "\"sample\":null"
								<< ",\"bar\":" << req_bar
								<< ",\"beat\":" << req_beat
								<< ",\"deltaSamples\":null,\"deltaBeats\":null";
						} else if (delta_samples_opt) {
							structured << "\"sample\":null,\"bar\":null,\"beat\":null"
								<< ",\"deltaSamples\":" << *delta_samples_opt
								<< ",\"deltaBeats\":null";
						} else {
							structured << "\"sample\":null,\"bar\":null,\"beat\":null,\"deltaSamples\":null"
								<< ",\"deltaBeats\":" << *delta_beats_opt;
						}
						structured << ",\"trackId\":";
						if (!target_track_id.empty ()) {
							structured << "\"" << json_escape (target_track_id) << "\"";
						} else {
							structured << "null";
						}
						structured << "}}";

						return jsonrpc_result (
							id,
							std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Region copied\"}],\"structuredContent\":")
								+ structured.str () + "}");
					}

					if (tool_name == "region/move") {
						Temporal::TempoMap::fetch ();

						const std::string target_track_id = root.get<std::string> ("params.arguments.trackId", "");

						const boost::optional<int64_t> sample_opt = root.get_optional<int64_t> ("params.arguments.sample");
						const boost::optional<int64_t> delta_samples_opt = root.get_optional<int64_t> ("params.arguments.deltaSamples");
						const boost::optional<double> delta_beats_opt = root.get_optional<double> ("params.arguments.deltaBeats");

						samplepos_t bbt_sample = 0;
						bool have_bbt_target = false;
						std::string bbt_error;
						if (!parse_optional_bbt_target_sample (root, "params.arguments", bbt_sample, have_bbt_target, bbt_error)) {
							return jsonrpc_error (id, -32602, bbt_error);
						}

						if (sample_opt && *sample_opt < 0) {
							return jsonrpc_error (id, -32602, "Invalid sample (expected >= 0)");
						}
						if (delta_beats_opt && !std::isfinite (*delta_beats_opt)) {
							return jsonrpc_error (id, -32602, "Invalid deltaBeats (expected finite number)");
						}

						int target_mode_count = 0;
						if (sample_opt) {
							++target_mode_count;
						}
						if (have_bbt_target) {
							++target_mode_count;
						}
						if (delta_samples_opt) {
							++target_mode_count;
						}
						if (delta_beats_opt) {
							++target_mode_count;
						}
						if (target_mode_count != 1) {
							return jsonrpc_error (id, -32602, "Provide exactly one target: sample, bar+beat, deltaSamples, or deltaBeats");
						}

						std::shared_ptr<ARDOUR::Region> region;
						std::string region_resolved_via;
						std::string region_error;
						if (!resolve_region_argument_or_selected_at_playhead (_session, root, "params.arguments", region, region_resolved_via, region_error)) {
							return jsonrpc_error (id, -32602, region_error);
						}

						const std::shared_ptr<ARDOUR::Playlist> source_playlist = region->playlist ();
						if (!source_playlist) {
							return jsonrpc_error (id, -32000, "Region has no playlist");
						}
						if (!region->can_move ()) {
							return jsonrpc_error (id, -32602, "Region is locked or position-locked");
						}

						std::shared_ptr<ARDOUR::Track> target_track;
						std::shared_ptr<ARDOUR::Playlist> target_playlist = source_playlist;
						if (!target_track_id.empty ()) {
							const std::shared_ptr<ARDOUR::Route> route = route_by_mcp_id (_session, target_track_id);
							target_track = std::dynamic_pointer_cast<ARDOUR::Track> (route);
							if (!target_track) {
								return jsonrpc_error (id, -32602, "trackId is not a track");
							}

							target_playlist = target_track->playlist ();
							if (!target_playlist) {
								return jsonrpc_error (id, -32000, "Destination track has no playlist");
							}
						}

						if (target_playlist->data_type () != region->data_type ()) {
							return jsonrpc_error (id, -32602, "Region type does not match destination track type");
						}

						const samplepos_t previous_start_sample = region->position_sample ();
						samplepos_t requested_start_sample = previous_start_sample;
						std::string move_origin = "none";

						if (sample_opt) {
							requested_start_sample = (samplepos_t) *sample_opt;
							move_origin = "sample";
						} else if (have_bbt_target) {
							requested_start_sample = bbt_sample;
							move_origin = "bbt";
						} else if (delta_samples_opt) {
							const int64_t current = (int64_t) previous_start_sample;
							const int64_t delta = *delta_samples_opt;
							if ((delta > 0 && current > (LLONG_MAX - delta)) || (delta < 0 && current < (LLONG_MIN - delta))) {
								return jsonrpc_error (id, -32602, "deltaSamples overflow");
							}
							const int64_t target = current + delta;
							if (target < 0) {
								return jsonrpc_error (id, -32602, "Resulting position is before session start");
							}
							requested_start_sample = (samplepos_t) target;
							move_origin = "deltaSamples";
						} else {
							const Temporal::Beats current_quarters = Temporal::TempoMap::use ()->quarters_at (Temporal::timepos_t (previous_start_sample));
							const Temporal::Beats target_quarters = current_quarters + Temporal::Beats::from_double (*delta_beats_opt);
							if (target_quarters < Temporal::Beats ()) {
								return jsonrpc_error (id, -32602, "Resulting position is before session start");
							}
							requested_start_sample = Temporal::TempoMap::use ()->sample_at (target_quarters);
							move_origin = "deltaBeats";
						}

						const bool cross_playlist_move = target_playlist != source_playlist;
						std::shared_ptr<ARDOUR::Region> moved_region = region;
						std::shared_ptr<ARDOUR::Playlist> moved_playlist = source_playlist;

						if (cross_playlist_move) {
							const std::shared_ptr<ARDOUR::Region> region_copy = ARDOUR::RegionFactory::create (region, true);
							if (!region_copy) {
								return jsonrpc_error (id, -32000, "Failed to create region copy for cross-track move");
							}

							std::vector<std::string> target_region_ids_before;
							{
								const ARDOUR::RegionList& existing = target_playlist->region_list_property ().rlist ();
								for (ARDOUR::RegionList::const_iterator i = existing.begin (); i != existing.end (); ++i) {
									target_region_ids_before.push_back ((*i)->id ().to_s ());
								}
							}

							_session.begin_reversible_command ("move region");
							source_playlist->clear_changes ();
							source_playlist->clear_owned_changes ();
							target_playlist->clear_changes ();
							target_playlist->clear_owned_changes ();
							target_playlist->add_region (region_copy, Temporal::timepos_t (requested_start_sample), 1.0, false);
							source_playlist->remove_region (region);
							target_playlist->rdiff_and_add_command (&_session);
							source_playlist->rdiff_and_add_command (&_session);
							_session.commit_reversible_command ();

							std::shared_ptr<ARDOUR::Region> inserted_region;
							{
								const ARDOUR::RegionList& after = target_playlist->region_list_property ().rlist ();
								for (ARDOUR::RegionList::const_iterator i = after.begin (); i != after.end (); ++i) {
									const std::string candidate_id = (*i)->id ().to_s ();
									if (std::find (target_region_ids_before.begin (), target_region_ids_before.end (), candidate_id) == target_region_ids_before.end ()) {
										inserted_region = *i;
										break;
									}
								}
							}

							moved_region = inserted_region ? inserted_region : region_copy;
							/* RegionFactory::create() auto-generates a new name.
							 * For cross-track move we preserve the original name.
							 */
							moved_region->set_name (region->name ());
							moved_playlist = target_playlist;
						} else if (requested_start_sample != previous_start_sample) {
							_session.begin_reversible_command ("move region");
							region->clear_changes ();
							region->set_position (Temporal::timepos_t (requested_start_sample));
							_session.add_command (new PBD::StatefulDiffCommand (region));
							_session.commit_reversible_command ();
						}

						const samplepos_t moved_start_sample = moved_region->position_sample ();
						const samplepos_t moved_end_sample = moved_start_sample + moved_region->length_samples ();
						const bool moved = cross_playlist_move || moved_start_sample != previous_start_sample;

						std::ostringstream structured;
						structured << "{\"regionId\":\"" << json_escape (moved_region->id ().to_s ()) << "\""
							<< ",\"sourceRegionId\":\"" << json_escape (region->id ().to_s ()) << "\""
							<< ",\"name\":\"" << json_escape (moved_region->name ()) << "\""
							<< ",\"type\":\"" << json_escape (moved_region->data_type ().to_string ()) << "\""
							<< ",\"playlistId\":\"" << json_escape (moved_playlist->id ().to_s ()) << "\""
							<< ",\"sourcePlaylistId\":\"" << json_escape (source_playlist->id ().to_s ()) << "\""
							<< ",\"resolvedVia\":\"" << json_escape (region_resolved_via) << "\""
							<< ",\"trackId\":";
						if (target_track) {
							structured << "\"" << json_escape (target_track->id ().to_s ()) << "\"";
						} else {
							structured << "null";
						}
						structured << ",\"lengthSamples\":" << moved_region->length_samples ()
							<< ",\"crossTrack\":" << (cross_playlist_move ? "true" : "false")
							<< ",\"moveOrigin\":\"" << json_escape (move_origin) << "\""
							<< ",\"moved\":" << (moved ? "true" : "false")
							<< ",\"previousStartSample\":" << previous_start_sample
							<< ",\"requestedStartSample\":" << requested_start_sample
							<< ",\"startSample\":" << moved_start_sample
							<< ",\"endSample\":" << moved_end_sample
							<< ",\"previousStartBbt\":" << bbt_json_at_sample (previous_start_sample)
							<< ",\"requestedStartBbt\":" << bbt_json_at_sample (requested_start_sample)
							<< ",\"startBbt\":" << bbt_json_at_sample (moved_start_sample)
							<< ",\"endBbt\":" << bbt_json_at_sample (moved_end_sample)
							<< ",\"requested\":{";
						if (sample_opt) {
							structured << "\"sample\":" << *sample_opt
								<< ",\"bar\":null,\"beat\":null,\"deltaSamples\":null,\"deltaBeats\":null";
						} else if (have_bbt_target) {
							const int req_bar = root.get<int> ("params.arguments.bar", 0);
							const double req_beat = root.get<double> ("params.arguments.beat", 0.0);
							structured << "\"sample\":null"
								<< ",\"bar\":" << req_bar
								<< ",\"beat\":" << req_beat
								<< ",\"deltaSamples\":null,\"deltaBeats\":null";
						} else if (delta_samples_opt) {
							structured << "\"sample\":null,\"bar\":null,\"beat\":null"
								<< ",\"deltaSamples\":" << *delta_samples_opt
								<< ",\"deltaBeats\":null";
						} else {
							structured << "\"sample\":null,\"bar\":null,\"beat\":null,\"deltaSamples\":null"
								<< ",\"deltaBeats\":" << *delta_beats_opt;
						}
						structured << ",\"trackId\":";
						if (!target_track_id.empty ()) {
							structured << "\"" << json_escape (target_track_id) << "\"";
						} else {
							structured << "null";
						}
						structured << "}}";

						return jsonrpc_result (
							id,
							std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"")
								+ (moved ? "Region moved" : "Region unchanged")
								+ "\"}],\"structuredContent\":" + structured.str () + "}");
					}

				if (tool_name == "plugin/list_available") {
					const std::string search = root.get<std::string> ("params.arguments.search", "");
					const std::string type_token = root.get<std::string> ("params.arguments.type", "");
					const bool include_hidden = root.get<bool> ("params.arguments.includeHidden", false);
					const bool include_internal = root.get<bool> ("params.arguments.includeInternal", false);

					boost::optional<ARDOUR::PluginType> type_filter;
					if (!type_token.empty ()) {
						ARDOUR::PluginType parsed_type;
						if (!parse_plugin_type_token (type_token, parsed_type)) {
							return jsonrpc_error (id, -32602, "Invalid type filter");
						}
						type_filter = parsed_type;
					}

					ARDOUR::PluginManager& manager = ARDOUR::PluginManager::instance ();
					const std::string search_lower = lower_ascii (search);

					std::ostringstream plugins;
					plugins << "[";
					bool first = true;
					size_t count = 0;

					append_plugin_catalog_entries_json (
						plugins,
						manager.windows_vst_plugin_info (),
						manager,
						search_lower,
						type_filter,
						include_hidden,
						include_internal,
						first,
						count);
					append_plugin_catalog_entries_json (
						plugins,
						manager.lxvst_plugin_info (),
						manager,
						search_lower,
						type_filter,
						include_hidden,
						include_internal,
						first,
						count);
					append_plugin_catalog_entries_json (
						plugins,
						manager.mac_vst_plugin_info (),
						manager,
						search_lower,
						type_filter,
						include_hidden,
						include_internal,
						first,
						count);
					append_plugin_catalog_entries_json (
						plugins,
						manager.vst3_plugin_info (),
						manager,
						search_lower,
						type_filter,
						include_hidden,
						include_internal,
						first,
						count);
					append_plugin_catalog_entries_json (
						plugins,
						manager.au_plugin_info (),
						manager,
						search_lower,
						type_filter,
						include_hidden,
						include_internal,
						first,
						count);
					append_plugin_catalog_entries_json (
						plugins,
						manager.ladspa_plugin_info (),
						manager,
						search_lower,
						type_filter,
						include_hidden,
						include_internal,
						first,
						count);
					append_plugin_catalog_entries_json (
						plugins,
						manager.lv2_plugin_info (),
						manager,
						search_lower,
						type_filter,
						include_hidden,
						include_internal,
						first,
						count);
					append_plugin_catalog_entries_json (
						plugins,
						manager.lua_plugin_info (),
						manager,
						search_lower,
						type_filter,
						include_hidden,
						include_internal,
						first,
						count);
					plugins << "]";

					std::ostringstream structured;
					structured << "{\"count\":" << count
						<< ",\"filters\":{\"search\":\"" << json_escape (search) << "\"";
					if (type_filter) {
						structured << ",\"type\":\"" << json_escape (plugin_type_token (*type_filter)) << "\"";
					} else {
						structured << ",\"type\":null";
					}
					structured << ",\"includeHidden\":" << (include_hidden ? "true" : "false")
						<< ",\"includeInternal\":" << (include_internal ? "true" : "false")
						<< "},\"plugins\":" << plugins.str ()
						<< "}";

					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Available plugins listed\"}],\"structuredContent\":")
							+ structured.str () + "}");
				}

				if (tool_name == "plugin/add") {
					const std::string route_id = root.get<std::string> ("params.arguments.id", "");
					const std::string plugin_id = root.get<std::string> ("params.arguments.pluginId", "");
					const std::string unique_id = root.get<std::string> ("params.arguments.uniqueId", "");
					const std::string type_token = root.get<std::string> ("params.arguments.type", "");
					const boost::optional<int64_t> position_opt = root.get_optional<int64_t> ("params.arguments.position");
					const boost::optional<bool> enabled_opt = root.get_optional<bool> ("params.arguments.enabled");

					if (route_id.empty ()) {
						return jsonrpc_error (id, -32602, "Missing route id");
					}
					if (plugin_id.empty () && unique_id.empty ()) {
						return jsonrpc_error (id, -32602, "Provide pluginId or uniqueId");
					}
					if (position_opt && *position_opt < 0) {
						return jsonrpc_error (id, -32602, "Invalid position (expected >= 0)");
					}

					const std::shared_ptr<ARDOUR::Route> route = route_by_mcp_id (_session, route_id);
					if (!route) {
						return jsonrpc_error (id, -32602, "Route not found");
					}

					ARDOUR::PluginManager& manager = ARDOUR::PluginManager::instance ();
					ARDOUR::PluginInfoPtr info;
					std::string resolve_error;
					if (!resolve_plugin_info_for_add (manager, plugin_id, type_token, unique_id, info, resolve_error)) {
						return jsonrpc_error (id, -32602, resolve_error.empty () ? "Could not resolve plugin" : resolve_error);
					}

					std::shared_ptr<ARDOUR::Processor> processor;
					std::string add_error;
					bool add_ok = false;
					const bool activation_allowed = enabled_opt ? *enabled_opt : ARDOUR::Config->get_new_plugins_active ();

					auto perform_add = [&] () {
						const ARDOUR::PluginPtr plugin = info->load (_session);
						if (!plugin) {
							add_error = "Failed to load plugin";
							return;
						}

						processor.reset (new ARDOUR::PluginInsert (_session, *route, plugin));
						ARDOUR::Route::ProcessorStreams err;

						/* Suppress interactive instrument setup prompts so Route::PluginSetup does
						 * not invoke GUI dialog code while handling MCP requests.
						 */
						ScopedInstrumentPromptDisable suppress_prompts;

						int rc = 0;
						if (position_opt) {
							rc = route->add_processor (processor, route->before_processor_for_index ((int) *position_opt), &err, activation_allowed);
						} else {
							rc = route->add_processor (processor, ARDOUR::PreFader, &err, activation_allowed);
						}
						if (rc != 0) {
							add_error = "Failed to add plugin to route";
							processor.reset ();
							return;
						}

						/* Explicitly honor enabled=false requests after insertion. */
						if (enabled_opt && !*enabled_opt) {
							processor->enable (false);
						}

						add_ok = true;
					};

#ifdef __APPLE__
					/* Some macOS plugins (Qt/Cocoa) require construction on the main/UI event loop.
					 * Keep behavior unchanged on other platforms for now.
					 */
					if (_event_loop) {
						std::mutex add_mutex;
						std::condition_variable add_cv;
						bool add_done = false;

						const bool queued = _event_loop->call_slot (MISSING_INVALIDATOR, [&] () {
							perform_add ();
							{
								std::lock_guard<std::mutex> lk (add_mutex);
								add_done = true;
							}
							add_cv.notify_one ();
						});

						if (queued) {
							std::unique_lock<std::mutex> lk (add_mutex);
							add_cv.wait (lk, [&] { return add_done; });
						} else {
							perform_add ();
						}
					} else {
						perform_add ();
					}
#else
					perform_add ();
#endif

					if (!add_ok || !processor) {
						return jsonrpc_error (id, -32000, add_error.empty () ? "Failed to add plugin to route" : add_error);
					}

					int inserted_index = -1;
					for (uint32_t i = 0;; ++i) {
						std::shared_ptr<ARDOUR::Processor> p = route->nth_plugin (i);
						if (!p) {
							break;
						}
						if (p == processor) {
							inserted_index = (int) i;
							break;
						}
					}

					std::ostringstream structured;
					structured << "{\"id\":\"" << json_escape (route->id ().to_s ()) << "\""
						<< ",\"routeName\":\"" << json_escape (route->name ()) << "\""
						<< ",\"pluginIndex\":" << inserted_index
						<< ",\"plugin\":{\"pluginId\":\"" << json_escape (plugin_catalog_id (info)) << "\""
						<< ",\"type\":\"" << json_escape (plugin_type_token (info->type)) << "\""
						<< ",\"typeLabel\":\"" << json_escape (ARDOUR::PluginManager::plugin_type_name (info->type, false)) << "\""
						<< ",\"name\":\"" << json_escape (info->name) << "\""
						<< ",\"uniqueId\":\"" << json_escape (info->unique_id) << "\""
						<< ",\"enabled\":" << (processor->enabled () ? "true" : "false")
						<< ",\"active\":" << (processor->active () ? "true" : "false")
						<< "}"
						<< ",\"plugins\":" << plugin_list_json (route)
						<< "}";

					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Plugin added\"}],\"structuredContent\":")
							+ structured.str () + "}");
				}

				if (tool_name == "plugin/get_description") {
					const std::string route_id = root.get<std::string> ("params.arguments.id", "");
					const int plugin_index = root.get<int> ("params.arguments.pluginIndex", -1);

					if (route_id.empty ()) {
						return jsonrpc_error (id, -32602, "Missing route id");
					}
					if (plugin_index < 0) {
						return jsonrpc_error (id, -32602, "Invalid pluginIndex (expected >= 0)");
					}

					const std::shared_ptr<ARDOUR::Route> route = route_by_mcp_id (_session, route_id);
					if (!route) {
						return jsonrpc_error (id, -32602, "Route not found");
					}

					std::string error_message;
					std::string structured = plugin_descriptor_json (route, plugin_index, &error_message);
					if (structured.empty ()) {
						return jsonrpc_error (id, -32602, error_message.empty () ? "Could not describe plugin" : error_message);
					}

					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Plugin descriptor\"}],\"structuredContent\":") + structured + "}");
				}

				if (tool_name == "plugin/set_parameter") {
					const std::string route_id = root.get<std::string> ("params.arguments.id", "");
					const int plugin_index = root.get<int> ("params.arguments.pluginIndex", -1);
					const boost::optional<int> parameter_index = root.get_optional<int> ("params.arguments.parameterIndex");
					const boost::optional<int> control_id = root.get_optional<int> ("params.arguments.controlId");
					const boost::optional<double> value = root.get_optional<double> ("params.arguments.value");
					const boost::optional<double> interface_value = root.get_optional<double> ("params.arguments.interface");

					if (route_id.empty ()) {
						return jsonrpc_error (id, -32602, "Missing route id");
					}
					if (plugin_index < 0) {
						return jsonrpc_error (id, -32602, "Invalid pluginIndex (expected >= 0)");
					}
					if (!parameter_index && !control_id) {
						return jsonrpc_error (id, -32602, "Provide one of: parameterIndex or controlId");
					}
					if (parameter_index && control_id) {
						return jsonrpc_error (id, -32602, "Provide only one of: parameterIndex or controlId");
					}
					if (!value && !interface_value) {
						return jsonrpc_error (id, -32602, "Provide one of: value or interface");
					}
					if (value && interface_value) {
						return jsonrpc_error (id, -32602, "Provide only one of: value or interface");
					}
					if (parameter_index && *parameter_index < 0) {
						return jsonrpc_error (id, -32602, "Invalid parameterIndex (expected >= 0)");
					}
					if (control_id && *control_id < 0) {
						return jsonrpc_error (id, -32602, "Invalid controlId (expected >= 0)");
					}

					const std::shared_ptr<ARDOUR::Route> route = route_by_mcp_id (_session, route_id);
					if (!route) {
						return jsonrpc_error (id, -32602, "Route not found");
					}

					std::shared_ptr<ARDOUR::Processor> proc = route->nth_plugin (plugin_index);
					if (!proc) {
						return jsonrpc_error (id, -32602, "Plugin not found");
					}

					std::shared_ptr<ARDOUR::PluginInsert> pi = std::dynamic_pointer_cast<ARDOUR::PluginInsert> (proc);
					if (!pi) {
						return jsonrpc_error (id, -32602, "Processor is not a plugin");
					}

					std::shared_ptr<ARDOUR::Plugin> pip = pi->plugin ();
					if (!pip) {
						return jsonrpc_error (id, -32602, "Plugin instance unavailable");
					}

					bool ok = false;
					uint32_t resolved_control_id = 0;
					int resolved_parameter_index = -1;

					if (parameter_index) {
						resolved_control_id = pip->nth_parameter ((uint32_t) *parameter_index, ok);
						if (!ok) {
							return jsonrpc_error (id, -32602, "parameterIndex out of range");
						}
						resolved_parameter_index = *parameter_index;
					} else {
						resolved_control_id = (uint32_t) *control_id;
						for (uint32_t ppi = 0; ppi < pip->parameter_count (); ++ppi) {
							const uint32_t cid = pip->nth_parameter (ppi, ok);
							if (!ok) {
								continue;
							}
							if (cid == resolved_control_id) {
								resolved_parameter_index = (int) ppi;
								break;
							}
						}
						if (resolved_parameter_index < 0) {
							return jsonrpc_error (id, -32602, "controlId not found");
						}
					}

					ARDOUR::ParameterDescriptor pd;
					if (pip->get_parameter_descriptor (resolved_control_id, pd) != 0) {
						return jsonrpc_error (id, -32602, "Could not read parameter descriptor");
					}
					if (!(pip->parameter_is_input (resolved_control_id) || pip->parameter_is_control (resolved_control_id))) {
						return jsonrpc_error (id, -32602, "Parameter is not writable");
					}

					std::shared_ptr<ARDOUR::AutomationControl> c =
						pi->automation_control (Evoral::Parameter (ARDOUR::PluginAutomation, 0, resolved_control_id));
					if (!c) {
						return jsonrpc_error (id, -32602, "Parameter automation control not available");
					}

					double new_value = c->get_value ();
					if (interface_value) {
						if (!std::isfinite (*interface_value)) {
							return jsonrpc_error (id, -32602, "Invalid interface value");
						}
						/* Match OSC plugin-parameter flow: convert interface value through the automation control. */
						new_value = c->interface_to_internal (*interface_value);
					} else {
						if (!std::isfinite (*value)) {
							return jsonrpc_error (id, -32602, "Invalid parameter value");
						}
						new_value = *value;
					}
					if (!std::isfinite (new_value)) {
						return jsonrpc_error (id, -32602, "Parameter mapping produced invalid value");
					}

					/* Clamp to control bounds to match defensive handling used elsewhere in MCP tools. */
					new_value = std::max (c->lower (), std::min (c->upper (), new_value));
					c->set_value (new_value, PBD::Controllable::NoGroup);

					const double current_value = c->get_value ();
					const double current_interface = c->internal_to_interface (current_value);

					std::ostringstream structured;
					structured << "{\"id\":\"" << json_escape (route->id ().to_s ()) << "\""
						<< ",\"routeName\":\"" << json_escape (route->name ()) << "\""
						<< ",\"pluginIndex\":" << plugin_index
						<< ",\"pluginName\":\"" << json_escape (proc->name ()) << "\""
						<< ",\"parameterIndex\":" << resolved_parameter_index
						<< ",\"controlId\":" << resolved_control_id
						<< ",\"label\":\"" << json_escape (pd.label) << "\""
						<< ",\"value\":" << current_value
						<< ",\"interface\":" << current_interface
						<< "}";

					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Plugin parameter updated\"}],\"structuredContent\":")
							+ structured.str () + "}");
				}

				if (tool_name == "plugin/set_enabled") {
					const std::string route_id = root.get<std::string> ("params.arguments.id", "");
					const int plugin_index = root.get<int> ("params.arguments.pluginIndex", -1);
					const boost::optional<bool> enabled = root.get_optional<bool> ("params.arguments.enabled");

					if (route_id.empty ()) {
						return jsonrpc_error (id, -32602, "Missing route id");
					}
					if (plugin_index < 0) {
						return jsonrpc_error (id, -32602, "Invalid pluginIndex (expected >= 0)");
					}
					if (!enabled) {
						return jsonrpc_error (id, -32602, "Missing enabled boolean");
					}

					const std::shared_ptr<ARDOUR::Route> route = route_by_mcp_id (_session, route_id);
					if (!route) {
						return jsonrpc_error (id, -32602, "Route not found");
					}

					std::shared_ptr<ARDOUR::Processor> proc = route->nth_plugin (plugin_index);
					if (!proc) {
						return jsonrpc_error (id, -32602, "Plugin not found");
					}

					std::shared_ptr<ARDOUR::PluginInsert> pi = std::dynamic_pointer_cast<ARDOUR::PluginInsert> (proc);
					if (!pi) {
						return jsonrpc_error (id, -32602, "Processor is not a plugin");
					}

					/* Match OSC route_plugin_activate/deactivate behavior. */
					if (*enabled) {
						pi->activate ();
					} else {
						pi->deactivate ();
					}

					std::ostringstream structured;
					structured << "{\"id\":\"" << json_escape (route->id ().to_s ()) << "\""
						<< ",\"routeName\":\"" << json_escape (route->name ()) << "\""
						<< ",\"pluginIndex\":" << plugin_index
						<< ",\"pluginName\":\"" << json_escape (proc->name ()) << "\""
						<< ",\"requestedEnabled\":" << (*enabled ? "true" : "false")
						<< ",\"enabled\":" << (proc->enabled () ? "true" : "false")
						<< ",\"active\":" << (proc->active () ? "true" : "false")
						<< "}";

					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Plugin enabled state updated\"}],\"structuredContent\":")
							+ structured.str () + "}");
				}

				if (tool_name == "plugin/remove") {
					const std::string route_id = root.get<std::string> ("params.arguments.id", "");
					const int plugin_index = root.get<int> ("params.arguments.pluginIndex", -1);

					if (route_id.empty ()) {
						return jsonrpc_error (id, -32602, "Missing route id");
					}
					if (plugin_index < 0) {
						return jsonrpc_error (id, -32602, "Invalid pluginIndex (expected >= 0)");
					}

					const std::shared_ptr<ARDOUR::Route> route = route_by_mcp_id (_session, route_id);
					if (!route) {
						return jsonrpc_error (id, -32602, "Route not found");
					}

					std::shared_ptr<ARDOUR::Processor> proc = route->nth_plugin (plugin_index);
					if (!proc) {
						return jsonrpc_error (id, -32602, "Plugin not found");
					}

					const std::string removed_name = proc->name ();
					const bool removed_enabled = proc->enabled ();
					const bool removed_active = proc->active ();
					if (route->remove_processor (proc) != 0) {
						return jsonrpc_error (id, -32000, "Failed to remove plugin");
					}

					std::ostringstream structured;
					structured << "{\"removed\":true"
						<< ",\"id\":\"" << json_escape (route->id ().to_s ()) << "\""
						<< ",\"routeName\":\"" << json_escape (route->name ()) << "\""
						<< ",\"pluginIndex\":" << plugin_index
						<< ",\"removedPlugin\":{\"name\":\"" << json_escape (removed_name) << "\""
						<< ",\"enabled\":" << (removed_enabled ? "true" : "false")
						<< ",\"active\":" << (removed_active ? "true" : "false")
						<< "}"
						<< ",\"plugins\":" << plugin_list_json (route)
						<< "}";

					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Plugin removed\"}],\"structuredContent\":")
							+ structured.str () + "}");
				}

				if (tool_name == "plugin/reorder") {
					const std::string route_id = root.get<std::string> ("params.arguments.id", "");
					const int from_index = root.get<int> ("params.arguments.fromIndex", -1);
					const int to_index = root.get<int> ("params.arguments.toIndex", -1);

					if (route_id.empty ()) {
						return jsonrpc_error (id, -32602, "Missing route id");
					}
					if (from_index < 0 || to_index < 0) {
						return jsonrpc_error (id, -32602, "Invalid fromIndex/toIndex (expected >= 0)");
					}

					const std::shared_ptr<ARDOUR::Route> route = route_by_mcp_id (_session, route_id);
					if (!route) {
						return jsonrpc_error (id, -32602, "Route not found");
					}

					std::vector<std::shared_ptr<ARDOUR::Processor> > all_processors;
					route->foreach_processor ([&all_processors] (std::weak_ptr<ARDOUR::Processor> wp) {
						std::shared_ptr<ARDOUR::Processor> p = wp.lock ();
						if (p) {
							all_processors.push_back (p);
						}
					});

					std::vector<std::shared_ptr<ARDOUR::Processor> > plugins;
					for (size_t i = 0; i < all_processors.size (); ++i) {
						if (std::dynamic_pointer_cast<ARDOUR::PluginInsert> (all_processors[i])) {
							plugins.push_back (all_processors[i]);
						}
					}

					if (from_index >= (int) plugins.size () || to_index >= (int) plugins.size ()) {
						return jsonrpc_error (id, -32602, "Plugin index out of range");
					}

					if (from_index == to_index) {
						std::ostringstream structured;
						structured << "{\"moved\":false"
							<< ",\"id\":\"" << json_escape (route->id ().to_s ()) << "\""
							<< ",\"routeName\":\"" << json_escape (route->name ()) << "\""
							<< ",\"fromIndex\":" << from_index
							<< ",\"toIndex\":" << to_index
							<< ",\"plugins\":" << plugin_list_json (route)
							<< "}";
						return jsonrpc_result (
							id,
							std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Plugin order unchanged\"}],\"structuredContent\":")
								+ structured.str () + "}");
					}

					const std::shared_ptr<ARDOUR::Processor> moved_plugin = plugins[(size_t) from_index];
					const std::string moved_name = moved_plugin->name ();
					plugins.erase (plugins.begin () + from_index);
					plugins.insert (plugins.begin () + to_index, moved_plugin);

					ARDOUR::Route::ProcessorList reordered_all;
					size_t plugin_cursor = 0;
					for (size_t i = 0; i < all_processors.size (); ++i) {
						if (std::dynamic_pointer_cast<ARDOUR::PluginInsert> (all_processors[i])) {
							reordered_all.push_back (plugins[plugin_cursor++]);
						} else {
							reordered_all.push_back (all_processors[i]);
						}
					}

					if (route->reorder_processors (reordered_all) != 0) {
						return jsonrpc_error (id, -32000, "Failed to reorder plugins");
					}

					int resolved_to_index = -1;
					const bool settled = wait_for_plugin_index (route, moved_plugin, to_index, 500, resolved_to_index);
					const bool moved = resolved_to_index >= 0 && resolved_to_index != from_index;
					const bool reached_target = resolved_to_index == to_index;

					std::ostringstream structured;
					structured << "{\"moved\":" << (moved ? "true" : "false")
						<< ",\"id\":\"" << json_escape (route->id ().to_s ()) << "\""
						<< ",\"routeName\":\"" << json_escape (route->name ()) << "\""
						<< ",\"pluginName\":\"" << json_escape (moved_name) << "\""
						<< ",\"fromIndex\":" << from_index
						<< ",\"toIndex\":" << to_index
						<< ",\"settled\":" << (settled ? "true" : "false")
						<< ",\"reachedTarget\":" << (reached_target ? "true" : "false")
						<< ",\"resolvedToIndex\":" << resolved_to_index
						<< ",\"plugins\":" << plugin_list_json (route)
						<< "}";

					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"")
							+ (reached_target ? "Plugin order updated" : (moved ? "Plugin order changed" : "Plugin order unchanged"))
							+ "\"}],\"structuredContent\":" + structured.str () + "}");
				}

				if (tool_name == "plugin/set_position") {
					const std::string route_id = root.get<std::string> ("params.arguments.id", "");
					const int plugin_index = root.get<int> ("params.arguments.pluginIndex", -1);
					const boost::optional<bool> post_fader_opt = root.get_optional<bool> ("params.arguments.postFader");

					if (route_id.empty ()) {
						return jsonrpc_error (id, -32602, "Missing route id");
					}
					if (plugin_index < 0) {
						return jsonrpc_error (id, -32602, "Invalid pluginIndex (expected >= 0)");
					}
					if (!post_fader_opt) {
						return jsonrpc_error (id, -32602, "Missing postFader boolean");
					}

					const std::shared_ptr<ARDOUR::Route> route = route_by_mcp_id (_session, route_id);
					if (!route) {
						return jsonrpc_error (id, -32602, "Route not found");
					}

					const std::shared_ptr<ARDOUR::Processor> moved_plugin = route->nth_plugin ((uint32_t) plugin_index);
					if (!moved_plugin) {
						return jsonrpc_error (id, -32602, "Plugin not found");
					}

					const bool requested_post_fader = *post_fader_opt;
					const bool current_post_fader = !moved_plugin->get_pre_fader ();
					if (requested_post_fader == current_post_fader) {
						std::ostringstream structured;
						structured << "{\"moved\":false"
							<< ",\"id\":\"" << json_escape (route->id ().to_s ()) << "\""
							<< ",\"routeName\":\"" << json_escape (route->name ()) << "\""
							<< ",\"pluginIndex\":" << plugin_index
							<< ",\"pluginName\":\"" << json_escape (moved_plugin->name ()) << "\""
							<< ",\"preFader\":" << (moved_plugin->get_pre_fader () ? "true" : "false")
							<< ",\"postFader\":" << (moved_plugin->get_pre_fader () ? "false" : "true")
							<< ",\"plugins\":" << plugin_list_json (route)
							<< "}";
						return jsonrpc_result (
							id,
							std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Plugin placement unchanged\"}],\"structuredContent\":")
								+ structured.str () + "}");
					}

					std::vector<std::shared_ptr<ARDOUR::Processor> > all_processors;
					route->foreach_processor ([&all_processors] (std::weak_ptr<ARDOUR::Processor> wp) {
						std::shared_ptr<ARDOUR::Processor> p = wp.lock ();
						if (p) {
							all_processors.push_back (p);
						}
					});

					std::vector<std::shared_ptr<ARDOUR::Processor> >::iterator it =
						std::find (all_processors.begin (), all_processors.end (), moved_plugin);
					if (it == all_processors.end ()) {
						return jsonrpc_error (id, -32000, "Plugin disappeared before reorder");
					}
					all_processors.erase (it);

					size_t insert_pos = all_processors.size ();
					if (requested_post_fader) {
						const std::shared_ptr<ARDOUR::Processor> main_outs = route->main_outs ();
						if (main_outs) {
							for (size_t i = 0; i < all_processors.size (); ++i) {
								if (all_processors[i] == main_outs) {
									insert_pos = i;
									break;
								}
							}
						}
					} else {
						insert_pos = 0;
						const std::shared_ptr<ARDOUR::Processor> amp = route->amp ();
						if (amp) {
							for (size_t i = 0; i < all_processors.size (); ++i) {
								if (all_processors[i] == amp) {
									insert_pos = i;
									break;
								}
							}
						}
					}
					all_processors.insert (all_processors.begin () + insert_pos, moved_plugin);

					ARDOUR::Route::ProcessorList reordered_all;
					for (size_t i = 0; i < all_processors.size (); ++i) {
						reordered_all.push_back (all_processors[i]);
					}

					if (route->reorder_processors (reordered_all) != 0) {
						return jsonrpc_error (id, -32000, "Failed to update plugin placement");
					}

					int resolved_index = -1;
					bool resolved_post_fader = !moved_plugin->get_pre_fader ();
					const bool settled = wait_for_plugin_post_fader (
						route,
						moved_plugin,
						requested_post_fader,
						500,
						resolved_index,
						resolved_post_fader);
					const bool moved = (resolved_post_fader != current_post_fader);
					const bool reached_target = (resolved_post_fader == requested_post_fader);

					std::ostringstream structured;
					structured << "{\"moved\":" << (moved ? "true" : "false")
						<< ",\"id\":\"" << json_escape (route->id ().to_s ()) << "\""
						<< ",\"routeName\":\"" << json_escape (route->name ()) << "\""
						<< ",\"pluginIndex\":" << plugin_index
						<< ",\"resolvedPluginIndex\":" << resolved_index
						<< ",\"pluginName\":\"" << json_escape (moved_plugin->name ()) << "\""
						<< ",\"requestedPostFader\":" << (requested_post_fader ? "true" : "false")
						<< ",\"settled\":" << (settled ? "true" : "false")
						<< ",\"reachedTarget\":" << (reached_target ? "true" : "false")
						<< ",\"preFader\":" << (resolved_post_fader ? "false" : "true")
						<< ",\"postFader\":" << (resolved_post_fader ? "true" : "false")
						<< ",\"plugins\":" << plugin_list_json (route)
						<< "}";

					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"")
							+ (reached_target ? "Plugin placement updated" : (moved ? "Plugin placement changed" : "Plugin placement unchanged"))
							+ "\"}],\"structuredContent\":" + structured.str () + "}");
				}

				if (tool_name == "track/get_fader") {
				const std::string route_id = root.get<std::string> ("params.arguments.id", "");

				if (route_id.empty ()) {
					return jsonrpc_error (id, -32602, "Missing track id");
				}

					const std::shared_ptr<ARDOUR::Route> route = route_by_mcp_id (_session, route_id);
					if (!route) {
						return jsonrpc_error (id, -32602, "Route not found");
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

				if (tool_name == "track/select") {
					const std::string route_id = root.get<std::string> ("params.arguments.id", "");

					if (route_id.empty ()) {
						return jsonrpc_error (id, -32602, "Missing route id");
					}

					const std::shared_ptr<ARDOUR::Route> route = route_by_mcp_id (_session, route_id);
					if (!route) {
						return jsonrpc_error (id, -32602, "Route not found");
					}

					/* Match OSC /strip/select behavior: set global stripable selection. */
					_session.selection ().select_stripable_and_maybe_group (route, ARDOUR::SelectionSet);

					std::string structured = track_info_json (route);
					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Route selected\"}],\"structuredContent\":") + structured + "}");
				}

				if (tool_name == "track/rename") {
					const std::string route_id = root.get<std::string> ("params.arguments.id", "");
					const std::string requested_name = root.get<std::string> ("params.arguments.newName", "");

					if (route_id.empty ()) {
						return jsonrpc_error (id, -32602, "Missing route id");
					}
					if (requested_name.empty ()) {
						return jsonrpc_error (id, -32602, "Missing newName");
					}

					const std::shared_ptr<ARDOUR::Route> route = route_by_mcp_id (_session, route_id);
					if (!route) {
						return jsonrpc_error (id, -32602, "Route not found");
					}

					const std::string old_name = route->name ();
					if (!route->set_name (requested_name)) {
						return jsonrpc_error (id, -32000, "Failed to rename route");
					}

					std::ostringstream structured;
					structured << "{\"id\":\"" << json_escape (route->id ().to_s ()) << "\""
						<< ",\"oldName\":\"" << json_escape (old_name) << "\""
						<< ",\"newName\":\"" << json_escape (route->name ()) << "\""
						<< ",\"track\":" << track_info_json (route)
						<< "}";

					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Route renamed\"}],\"structuredContent\":")
							+ structured.str () + "}");
				}

				if (tool_name == "track/set_mute") {
					const std::string route_id = root.get<std::string> ("params.arguments.id", "");
					const boost::optional<bool> value = root.get_optional<bool> ("params.arguments.value");

					if (route_id.empty ()) {
						return jsonrpc_error (id, -32602, "Missing route id");
					}
					if (!value) {
						return jsonrpc_error (id, -32602, "Missing boolean value");
					}

					const std::shared_ptr<ARDOUR::Route> route = route_by_mcp_id (_session, route_id);
					if (!route) {
						return jsonrpc_error (id, -32602, "Route not found");
					}
					if (!route->mute_control ()) {
						return jsonrpc_error (id, -32602, "Route has no mute control");
					}

					/* Match OSC mute logic. */
					route->mute_control ()->set_value (*value ? 1.0 : 0.0, PBD::Controllable::NoGroup);

					std::string structured = track_info_json (route);
					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Route mute updated\"}],\"structuredContent\":") + structured + "}");
				}

				if (tool_name == "track/set_solo") {
					const std::string route_id = root.get<std::string> ("params.arguments.id", "");
					const boost::optional<bool> value = root.get_optional<bool> ("params.arguments.value");

					if (route_id.empty ()) {
						return jsonrpc_error (id, -32602, "Missing route id");
					}
					if (!value) {
						return jsonrpc_error (id, -32602, "Missing boolean value");
					}

					const std::shared_ptr<ARDOUR::Route> route = route_by_mcp_id (_session, route_id);
					if (!route) {
						return jsonrpc_error (id, -32602, "Route not found");
					}
					if (!route->solo_control ()) {
						return jsonrpc_error (id, -32602, "Route has no solo control");
					}
					if (route->is_master () || route->is_monitor ()) {
						return jsonrpc_error (id, -32602, "Solo is not supported for this route");
					}

					/* Match OSC solo logic: use Session::set_control for solo-state propagation. */
					_session.set_control (route->solo_control (), *value ? 1.0 : 0.0, PBD::Controllable::NoGroup);

					std::string structured = track_info_json (route);
					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Route solo updated\"}],\"structuredContent\":") + structured + "}");
				}

				if (tool_name == "track/set_rec_enable") {
					const std::string route_id = root.get<std::string> ("params.arguments.id", "");
					const boost::optional<bool> value = root.get_optional<bool> ("params.arguments.value");

					if (route_id.empty ()) {
						return jsonrpc_error (id, -32602, "Missing track id");
					}
					if (!value) {
						return jsonrpc_error (id, -32602, "Missing boolean value");
					}

					const std::shared_ptr<ARDOUR::Route> route = route_by_mcp_id (_session, route_id);
					const std::shared_ptr<ARDOUR::Track> track = std::dynamic_pointer_cast<ARDOUR::Track> (route);
					if (!track) {
						return jsonrpc_error (id, -32602, "Track not found");
					}
					if (!track->rec_enable_control ()) {
						return jsonrpc_error (id, -32602, "Track has no record-enable control");
					}

					/* Match OSC recenable logic. */
					track->rec_enable_control ()->set_value (*value ? 1.0 : 0.0, PBD::Controllable::NoGroup);

					std::string structured = track_info_json (route);
					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Track rec-enable updated\"}],\"structuredContent\":") + structured + "}");
				}

				if (tool_name == "track/set_rec_safe") {
					const std::string route_id = root.get<std::string> ("params.arguments.id", "");
					const boost::optional<bool> value = root.get_optional<bool> ("params.arguments.value");

					if (route_id.empty ()) {
						return jsonrpc_error (id, -32602, "Missing track id");
					}
					if (!value) {
						return jsonrpc_error (id, -32602, "Missing boolean value");
					}

					const std::shared_ptr<ARDOUR::Route> route = route_by_mcp_id (_session, route_id);
					const std::shared_ptr<ARDOUR::Track> track = std::dynamic_pointer_cast<ARDOUR::Track> (route);
					if (!track) {
						return jsonrpc_error (id, -32602, "Track not found");
					}
					if (!track->rec_safe_control ()) {
						return jsonrpc_error (id, -32602, "Track has no record-safe control");
					}

					/* Match OSC record_safe logic. */
					track->rec_safe_control ()->set_value (*value ? 1.0 : 0.0, PBD::Controllable::NoGroup);

					std::string structured = track_info_json (route);
					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Track rec-safe updated\"}],\"structuredContent\":") + structured + "}");
				}

				if (tool_name == "track/set_pan") {
					const std::string route_id = root.get<std::string> ("params.arguments.id", "");
					const boost::optional<double> position = root.get_optional<double> ("params.arguments.position");

					if (route_id.empty ()) {
						return jsonrpc_error (id, -32602, "Missing route id");
					}
					if (!position || !valid_fader_position (*position)) {
						return jsonrpc_error (id, -32602, "Invalid pan position (expected 0.0 to 1.0)");
					}

					const std::shared_ptr<ARDOUR::Route> route = route_by_mcp_id (_session, route_id);
					if (!route) {
						return jsonrpc_error (id, -32602, "Route not found");
					}
					std::shared_ptr<ARDOUR::AutomationControl> pan = route->pan_azimuth_control ();
					if (!pan) {
						return jsonrpc_error (id, -32602, "Route has no pan control");
					}

					/* Match OSC pan_stereo_position logic. */
					pan->set_value (route->pan_azimuth_control ()->interface_to_internal (*position), PBD::Controllable::NoGroup);

					std::string structured = track_info_json (route);
					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Route pan updated\"}],\"structuredContent\":") + structured + "}");
				}

				if (tool_name == "track/set_send_level") {
					const std::string route_id = root.get<std::string> ("params.arguments.id", "");
					const int send_index = root.get<int> ("params.arguments.sendIndex", -1);
					const boost::optional<double> position = root.get_optional<double> ("params.arguments.position");
					const boost::optional<double> db = root.get_optional<double> ("params.arguments.db");

					if (route_id.empty ()) {
						return jsonrpc_error (id, -32602, "Missing route id");
					}
					if (send_index < 0) {
						return jsonrpc_error (id, -32602, "Invalid sendIndex (expected >= 0)");
					}
					if (!position && !db) {
						return jsonrpc_error (id, -32602, "Provide one of: position (0.0 to 1.0) or db");
					}
					if (position && db) {
						return jsonrpc_error (id, -32602, "Provide only one of: position or db");
					}

					const std::shared_ptr<ARDOUR::Route> route = route_by_mcp_id (_session, route_id);
					if (!route) {
						return jsonrpc_error (id, -32602, "Route not found");
					}

					const uint32_t sid = (uint32_t) send_index;
					std::shared_ptr<ARDOUR::Processor> send_proc = route->nth_send (sid);
					if (!send_proc) {
						return jsonrpc_error (id, -32602, "Send not found");
					}

					std::shared_ptr<ARDOUR::AutomationControl> send_gain = route->send_level_controllable (sid);
					if (!send_gain) {
						return jsonrpc_error (id, -32602, "Send has no level control");
					}

					double internal_gain = send_gain->get_value ();
					if (position) {
						if (!valid_fader_position (*position)) {
							return jsonrpc_error (id, -32602, "Invalid send position (expected 0.0 to 1.0)");
						}

						/* Match OSC /strip/send/fader behavior. */
						internal_gain = send_gain->interface_to_internal (*position);
					} else {
						if (!valid_fader_db (*db)) {
							return jsonrpc_error (id, -32602, "Invalid dB value (expected -193.0 to +6.0 dB; use -193.0 for silence)");
						}

						/* Match OSC /strip/send/gain behavior. */
						internal_gain = db_to_gain_with_floor (*db);
					}

					if (!std::isfinite (internal_gain)) {
						return jsonrpc_error (id, -32602, "Invalid mapped send gain");
					}

					internal_gain = std::max (send_gain->lower (), std::min (send_gain->upper (), internal_gain));
					send_gain->set_value (internal_gain, PBD::Controllable::NoGroup);

					std::string structured = send_level_json (route, sid);
					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Send level updated\"}],\"structuredContent\":") + structured + "}");
				}

				if (tool_name == "track/add_send") {
					const std::string route_id = root.get<std::string> ("params.arguments.id", "");
					const std::string target_id = root.get<std::string> ("params.arguments.targetId", "");
					const boost::optional<double> position = root.get_optional<double> ("params.arguments.position");
					const boost::optional<double> db = root.get_optional<double> ("params.arguments.db");
					const boost::optional<bool> enabled = root.get_optional<bool> ("params.arguments.enabled");
					const boost::optional<bool> post_fader = root.get_optional<bool> ("params.arguments.postFader");

					if (route_id.empty ()) {
						return jsonrpc_error (id, -32602, "Missing source route id");
					}
					if (target_id.empty ()) {
						return jsonrpc_error (id, -32602, "Missing target route id");
					}
					if (position && db) {
						return jsonrpc_error (id, -32602, "Provide only one of: position or db");
					}
					if (position && !valid_fader_position (*position)) {
						return jsonrpc_error (id, -32602, "Invalid send position (expected 0.0 to 1.0)");
					}
					if (db && !valid_fader_db (*db)) {
						return jsonrpc_error (id, -32602, "Invalid dB value (expected -193.0 to +6.0 dB; use -193.0 for silence)");
					}

					const std::shared_ptr<ARDOUR::Route> route = route_by_mcp_id (_session, route_id);
					const std::shared_ptr<ARDOUR::Route> target_route = route_by_mcp_id (_session, target_id);
					if (!route) {
						return jsonrpc_error (id, -32602, "Source route not found");
					}
					if (!target_route) {
						return jsonrpc_error (id, -32602, "Target route not found");
					}
					if (route->id () == target_route->id ()) {
						return jsonrpc_error (id, -32602, "Cannot add send to the same route");
					}
					if (route->is_singleton ()) {
						return jsonrpc_error (id, -32602, "Cannot add send from singleton route");
					}
					if (target_route->is_singleton ()) {
						return jsonrpc_error (id, -32602, "Cannot add send to singleton route");
					}
					std::shared_ptr<ARDOUR::Route> monitor_out = _session.monitor_out ();
					if (monitor_out && monitor_out->id () == target_route->id ()) {
						return jsonrpc_error (id, -32602, "Cannot add aux send to monitor route");
					}

					const bool existed_before = (route->internal_send_for (target_route) != 0);
					std::shared_ptr<ARDOUR::Processor> before;
					if (post_fader) {
						before = route->before_processor_for_placement (*post_fader ? ARDOUR::PostFader : ARDOUR::PreFader);
					}
					const int rc = route->add_aux_send (target_route, before);
					if (rc != 0) {
						return jsonrpc_error (id, -32000, "Failed to add send");
					}

					uint32_t sid = 0;
					if (!find_internal_send_index (route, target_route, sid)) {
						return jsonrpc_error (id, -32000, "Send not found after add");
					}

					std::shared_ptr<ARDOUR::AutomationControl> send_gain = route->send_level_controllable (sid);
					if (position || db) {
						if (!send_gain) {
							return jsonrpc_error (id, -32602, "Send has no level control");
						}

						double internal_gain = send_gain->get_value ();
						if (position) {
							internal_gain = send_gain->interface_to_internal (*position);
						} else {
							internal_gain = db_to_gain_with_floor (*db);
						}

						if (!std::isfinite (internal_gain)) {
							return jsonrpc_error (id, -32602, "Invalid mapped send gain");
						}

						internal_gain = std::max (send_gain->lower (), std::min (send_gain->upper (), internal_gain));
						send_gain->set_value (internal_gain, PBD::Controllable::NoGroup);
					}

					if (enabled) {
						std::shared_ptr<ARDOUR::AutomationControl> send_enable = route->send_enable_controllable (sid);
						if (send_enable) {
							send_enable->set_value (*enabled ? 1.0 : 0.0, PBD::Controllable::NoGroup);
						} else {
							std::shared_ptr<ARDOUR::Processor> send_proc = route->nth_send (sid);
							if (!send_proc) {
								return jsonrpc_error (id, -32000, "Send not found while setting enabled state");
							}
							send_proc->enable (*enabled);
						}
					}

					std::ostringstream structured;
					structured << "{\"created\":" << (existed_before ? "false" : "true")
						   << ",\"send\":" << send_level_json (route, sid)
						   << ",\"sends\":" << send_list_json (route)
						   << "}";

					const char* text = existed_before ? "Send already existed; state updated" : "Send added";
					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"") + text + "\"}],\"structuredContent\":" + structured.str () + "}");
				}

				if (tool_name == "track/set_send_position") {
					const std::string route_id = root.get<std::string> ("params.arguments.id", "");
					const int send_index = root.get<int> ("params.arguments.sendIndex", -1);
					const boost::optional<bool> post_fader = root.get_optional<bool> ("params.arguments.postFader");

					if (route_id.empty ()) {
						return jsonrpc_error (id, -32602, "Missing route id");
					}
					if (send_index < 0) {
						return jsonrpc_error (id, -32602, "Invalid sendIndex (expected >= 0)");
					}
					if (!post_fader) {
						return jsonrpc_error (id, -32602, "Missing postFader boolean");
					}

					const std::shared_ptr<ARDOUR::Route> route = route_by_mcp_id (_session, route_id);
					if (!route) {
						return jsonrpc_error (id, -32602, "Route not found");
					}

					uint32_t sid = 0;
					std::string err;
					if (!recreate_aux_send_with_position (route, (uint32_t) send_index, *post_fader, sid, err)) {
						return jsonrpc_error (id, -32000, err.empty () ? "Failed to set send position" : err);
					}

					std::string structured = send_level_json (route, sid);
					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Send position updated\"}],\"structuredContent\":") + structured + "}");
				}

				if (tool_name == "track/remove_send") {
					const std::string route_id = root.get<std::string> ("params.arguments.id", "");
					const int send_index = root.get<int> ("params.arguments.sendIndex", -1);
					const std::string target_id = root.get<std::string> ("params.arguments.targetId", "");

					if (route_id.empty ()) {
						return jsonrpc_error (id, -32602, "Missing route id");
					}
					if (send_index < 0) {
						return jsonrpc_error (id, -32602, "Invalid sendIndex (expected >= 0)");
					}

					const std::shared_ptr<ARDOUR::Route> route = route_by_mcp_id (_session, route_id);
					if (!route) {
						return jsonrpc_error (id, -32602, "Route not found");
					}

					const uint32_t sid = (uint32_t) send_index;
					std::shared_ptr<ARDOUR::Processor> send_proc = route->nth_send (sid);
					if (!send_proc) {
						return jsonrpc_error (id, -32602, "Send not found");
					}

					std::ostringstream removed;
					removed << "{\"routeId\":\"" << json_escape (route->id ().to_s ()) << "\""
					        << ",\"routeName\":\"" << json_escape (route->name ()) << "\""
					        << ",\"sendIndex\":" << sid
					        << ",\"name\":\"" << json_escape (route->send_name (sid)) << "\""
					        << ",\"active\":" << (send_proc->active () ? "true" : "false")
					        << ",\"preFader\":" << (send_proc->get_pre_fader () ? "true" : "false")
					        << ",\"postFader\":" << (send_proc->get_pre_fader () ? "false" : "true");

					std::shared_ptr<ARDOUR::InternalSend> isend = std::dynamic_pointer_cast<ARDOUR::InternalSend> (send_proc);
					if (isend) {
						std::shared_ptr<ARDOUR::Route> target_route = isend->target_route ();
						if (!target_id.empty ()) {
							if (!target_route) {
								return jsonrpc_error (id, -32602, "Internal send has no target route");
							}
							if (target_route->id ().to_s () != target_id) {
								return jsonrpc_error (id, -32602, "Send targetId mismatch");
							}
						}
						if (target_route) {
							removed << ",\"targetRouteId\":\"" << json_escape (target_route->id ().to_s ()) << "\""
							        << ",\"targetRouteName\":\"" << json_escape (target_route->name ()) << "\"";
						}
					} else if (!target_id.empty ()) {
						return jsonrpc_error (id, -32602, "targetId is only supported for internal sends");
					}
					removed << "}";

					if (route->remove_processor (send_proc) != 0) {
						return jsonrpc_error (id, -32000, "Failed to remove send");
					}

					std::ostringstream structured;
					structured << "{\"removed\":true"
					           << ",\"routeId\":\"" << json_escape (route->id ().to_s ()) << "\""
					           << ",\"routeName\":\"" << json_escape (route->name ()) << "\""
					           << ",\"sendIndex\":" << sid
					           << ",\"removedSend\":" << removed.str ()
					           << ",\"sends\":" << send_list_json (route)
					           << "}";

					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Send removed\"}],\"structuredContent\":") + structured.str () + "}");
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

			const std::shared_ptr<ARDOUR::Route> route = route_by_mcp_id (_session, route_id);
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
					return jsonrpc_error (id, -32602, "Invalid dB value (expected -193.0 to +6.0 dB; use -193.0 for silence)");
				}

				/* Match OSC behavior for /gain: map dB to coefficient, then clamp to control bounds. */
				internal_gain = db_to_gain_with_floor (*db);
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
