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
#include <memory>
#include <sstream>
#include <vector>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include "pbd/error.h"
#include "pbd/id.h"
#include "pbd/controllable.h"
#include "pbd/memento_command.h"
#include "pbd/xml++.h"

#include "ardour/audio_track.h"
#include "ardour/dB.h"
#include "ardour/internal_send.h"
#include "ardour/location.h"
#include "ardour/midi_track.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/processor.h"
#include "ardour/route.h"
#include "ardour/selection.h"
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
	std::shared_ptr<ARDOUR::RouteList const> routes = session.get_routes ();
	std::vector<std::shared_ptr<ARDOUR::Route> > sorted;

	if (routes) {
		sorted.assign (routes->begin (), routes->end ());
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

static std::string
marker_type_json (uint32_t flags)
{
	std::ostringstream ss;
	ss << "[";

	bool first = true;
	struct Kind {
		uint32_t bit;
		const char* name;
	};
	const Kind kinds[] = {
		{(uint32_t) ARDOUR::Location::IsMark, "mark"},
		{(uint32_t) ARDOUR::Location::IsCueMarker, "cue"},
		{(uint32_t) ARDOUR::Location::IsCDMarker, "cd"},
		{(uint32_t) ARDOUR::Location::IsXrun, "xrun"},
		{(uint32_t) ARDOUR::Location::IsSection, "section"},
		{(uint32_t) ARDOUR::Location::IsScene, "scene"},
		{(uint32_t) ARDOUR::Location::IsRangeMarker, "range"},
		{(uint32_t) ARDOUR::Location::IsSessionRange, "session_range"},
		{(uint32_t) ARDOUR::Location::IsAutoLoop, "auto_loop"},
		{(uint32_t) ARDOUR::Location::IsAutoPunch, "auto_punch"},
		{(uint32_t) ARDOUR::Location::IsClockOrigin, "clock_origin"},
		{(uint32_t) ARDOUR::Location::IsSkip, "skip"},
	};

	for (size_t i = 0; i < (sizeof (kinds) / sizeof (kinds[0])); ++i) {
		if (!(flags & kinds[i].bit)) {
			continue;
		}
		if (!first) {
			ss << ",";
		}
		first = false;
		ss << "\"" << kinds[i].name << "\"";
	}

	ss << "]";
	return ss.str ();
}

static bool
parse_location_flags (const XMLNode& node, uint32_t& flags_out)
{
	flags_out = 0;
	if (node.get_property ("flags", flags_out)) {
		return true;
	}

	const XMLProperty* prop = node.property ("flags");
	if (!prop) {
		return false;
	}

	const std::string v = prop->value ();
	if (v.empty ()) {
		return false;
	}

	char* endptr = 0;
	unsigned long parsed = std::strtoul (v.c_str (), &endptr, 0);
	if (endptr && *endptr == '\0') {
		flags_out = (uint32_t) parsed;
		return true;
	}

	struct FlagName {
		const char* name;
		uint32_t bit;
	};
	const FlagName named_flags[] = {
		{"IsMark", (uint32_t) ARDOUR::Location::IsMark},
		{"IsAutoPunch", (uint32_t) ARDOUR::Location::IsAutoPunch},
		{"IsAutoLoop", (uint32_t) ARDOUR::Location::IsAutoLoop},
		{"IsHidden", (uint32_t) ARDOUR::Location::IsHidden},
		{"IsCDMarker", (uint32_t) ARDOUR::Location::IsCDMarker},
		{"IsRangeMarker", (uint32_t) ARDOUR::Location::IsRangeMarker},
		{"IsSessionRange", (uint32_t) ARDOUR::Location::IsSessionRange},
		{"IsSkip", (uint32_t) ARDOUR::Location::IsSkip},
		{"IsSkipping", (uint32_t) ARDOUR::Location::IsSkipping},
		{"IsClockOrigin", (uint32_t) ARDOUR::Location::IsClockOrigin},
		{"IsXrun", (uint32_t) ARDOUR::Location::IsXrun},
		{"IsCueMarker", (uint32_t) ARDOUR::Location::IsCueMarker},
		{"IsSection", (uint32_t) ARDOUR::Location::IsSection},
		{"IsScene", (uint32_t) ARDOUR::Location::IsScene},
	};

	bool matched = false;
	size_t start = 0;
	while (start < v.size ()) {
		size_t comma = v.find (',', start);
		if (comma == std::string::npos) {
			comma = v.size ();
		}

		size_t token_begin = v.find_first_not_of (" \t", start);
		size_t token_end = comma;
		while (token_end > start && (v[token_end - 1] == ' ' || v[token_end - 1] == '\t')) {
			--token_end;
		}

		if (token_begin != std::string::npos && token_begin < token_end) {
			const std::string token = v.substr (token_begin, token_end - token_begin);
			for (size_t i = 0; i < (sizeof (named_flags) / sizeof (named_flags[0])); ++i) {
				if (token == named_flags[i].name) {
					flags_out |= named_flags[i].bit;
					matched = true;
					break;
				}
			}
		}

		start = comma + 1;
	}

	return matched;
}

static bool
parse_location_samples (const XMLNode& node, samplepos_t& start_sample, samplepos_t& end_sample)
{
	if (node.get_property ("start", start_sample) && node.get_property ("end", end_sample)) {
		return true;
	}

	Temporal::timepos_t start;
	Temporal::timepos_t end;
	if (!node.get_property ("start", start) || !node.get_property ("end", end)) {
		return false;
	}

	start_sample = start.samples ();
	end_sample = end.samples ();
	return true;
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

	std::unique_ptr<XMLNode> locations_state (&locations->get_state ());
	if (!locations_state.get ()) {
		return "{\"markers\":[]}";
	}

	struct MarkerSnapshot {
		std::string name;
		samplepos_t entry_start_sample;
		samplepos_t entry_end_sample;
		uint32_t flags;
		int32_t cue_id;
		bool have_cue;
		bool hidden;
		std::string location_id;
		bool have_location_id;
		std::string location_name;
		samplepos_t location_start_sample;
		samplepos_t location_end_sample;
		bool synthetic;
		bool boundary_start;
	};

	std::vector<MarkerSnapshot> markers;
	const XMLNodeList children = locations_state->children ();

	for (XMLNodeConstIterator it = children.begin (); it != children.end (); ++it) {
		if (!(*it) || (*it)->name () != "Location") {
			continue;
		}

		uint32_t flags = 0;
		if (!parse_location_flags (*(*it), flags)) {
			continue;
		}

		std::string name;
		samplepos_t start_sample = 0;
		samplepos_t end_sample = 0;
		if (!(*it)->get_property ("name", name) || !parse_location_samples (*(*it), start_sample, end_sample)) {
			continue;
		}
		std::string location_id;
		const bool have_location_id = (*it)->get_property ("id", location_id);

		int32_t cue_id = 0;
		const bool have_cue = (*it)->get_property ("cue", cue_id);

		if (flags & (uint32_t) ARDOUR::Location::IsSessionRange) {
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
			start_marker.have_location_id = have_location_id;
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
			end_marker.have_location_id = have_location_id;
			end_marker.location_name = name;
			end_marker.location_start_sample = start_sample;
			end_marker.location_end_sample = end_sample;
			end_marker.synthetic = true;
			end_marker.boundary_start = false;
			markers.push_back (end_marker);
			continue;
		}

		if (!(flags & ((uint32_t) ARDOUR::Location::IsMark |
		               (uint32_t) ARDOUR::Location::IsRangeMarker |
		               (uint32_t) ARDOUR::Location::IsAutoLoop |
		               (uint32_t) ARDOUR::Location::IsAutoPunch))) {
			continue;
		}

		MarkerSnapshot marker;
		marker.name = name;
		marker.entry_start_sample = start_sample;
		marker.entry_end_sample = end_sample;
		marker.flags = flags;
		marker.cue_id = cue_id;
		marker.have_cue = have_cue && (flags & (uint32_t) ARDOUR::Location::IsCueMarker);
		marker.hidden = (flags & (uint32_t) ARDOUR::Location::IsHidden) != 0;
		marker.location_id = location_id;
		marker.have_location_id = have_location_id;
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

		const bool is_mark = (marker.flags & (uint32_t) ARDOUR::Location::IsMark) != 0;
		const bool is_cue = (marker.flags & (uint32_t) ARDOUR::Location::IsCueMarker) != 0;
		const bool is_cd = (marker.flags & (uint32_t) ARDOUR::Location::IsCDMarker) != 0;
		const bool is_xrun = (marker.flags & (uint32_t) ARDOUR::Location::IsXrun) != 0;
		const bool is_section = (marker.flags & (uint32_t) ARDOUR::Location::IsSection) != 0;
		const bool is_scene = (marker.flags & (uint32_t) ARDOUR::Location::IsScene) != 0;
		const bool is_range_marker = (marker.flags & (uint32_t) ARDOUR::Location::IsRangeMarker) != 0;
		const bool is_session_range = (marker.flags & (uint32_t) ARDOUR::Location::IsSessionRange) != 0;
		const bool is_auto_loop = (marker.flags & (uint32_t) ARDOUR::Location::IsAutoLoop) != 0;
		const bool is_auto_punch = (marker.flags & (uint32_t) ARDOUR::Location::IsAutoPunch) != 0;
		const bool is_clock_origin = (marker.flags & (uint32_t) ARDOUR::Location::IsClockOrigin) != 0;
		const bool is_skip = (marker.flags & (uint32_t) ARDOUR::Location::IsSkip) != 0;
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

		if (marker.have_location_id) {
			ss << ",\"locationId\":\"" << json_escape (marker.location_id) << "\"";
		} else {
			ss << ",\"locationId\":null";
		}

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
		   << ",\"flagBits\":" << marker.flags
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
	const uint32_t flags = (uint32_t) location.flags ();
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
	const uint32_t flags = (uint32_t) location.flags ();
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
	const uint32_t flags = (uint32_t) location.flags ();
	const std::string start_bbt = bbt_json_at_sample (start_sample);
	const std::string end_bbt = bbt_json_at_sample (end_sample);
	const bool is_hidden = (flags & (uint32_t) ARDOUR::Location::IsHidden) != 0;

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
	uint32_t flags)
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
	uint32_t flags)
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
				"{\"name\":\"transport/locate\",\"title\":\"Transport Locate\",\"description\":\"Move playhead to a target position by sample, or by bar+beat (fractional beat allowed).\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"sample\":{\"type\":\"integer\",\"minimum\":0},\"bar\":{\"type\":\"integer\",\"minimum\":1},\"beat\":{\"type\":\"number\",\"minimum\":1}},\"additionalProperties\":false}},"
				"{\"name\":\"transport/play\",\"title\":\"Transport Play\",\"description\":\"Start transport playback.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}},"
				"{\"name\":\"transport/stop\",\"title\":\"Transport Stop\",\"description\":\"Stop transport playback.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}},"
				"{\"name\":\"markers/list\",\"title\":\"Markers List\",\"description\":\"List all session markers regardless of marker subtype.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}},"
				"{\"name\":\"markers/add\",\"title\":\"Add Marker\",\"description\":\"Add a marker at the current audible position or at a specific BBT location. Optional type: mark, section (arrangement), or scene. If name is omitted or empty, Ardour assigns a default marker name.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"},\"type\":{\"type\":\"string\",\"enum\":[\"mark\",\"section\",\"scene\",\"arrangement\"]},\"bar\":{\"type\":\"integer\",\"minimum\":1},\"beat\":{\"type\":\"number\",\"minimum\":1}},\"additionalProperties\":false}},"
				"{\"name\":\"markers/add_range\",\"title\":\"Add Range Marker\",\"description\":\"Add a range marker using start/end sample or start/end bar+beat.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"},\"startSample\":{\"type\":\"integer\",\"minimum\":0},\"endSample\":{\"type\":\"integer\",\"minimum\":0},\"startBar\":{\"type\":\"integer\",\"minimum\":1},\"startBeat\":{\"type\":\"number\",\"minimum\":1},\"endBar\":{\"type\":\"integer\",\"minimum\":1},\"endBeat\":{\"type\":\"number\",\"minimum\":1}},\"additionalProperties\":false}},"
				"{\"name\":\"markers/set_auto_loop\",\"title\":\"Set Auto Loop Range\",\"description\":\"Set auto-loop range using start/end sample or start/end bar+beat.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"startSample\":{\"type\":\"integer\",\"minimum\":0},\"endSample\":{\"type\":\"integer\",\"minimum\":0},\"startBar\":{\"type\":\"integer\",\"minimum\":1},\"startBeat\":{\"type\":\"number\",\"minimum\":1},\"endBar\":{\"type\":\"integer\",\"minimum\":1},\"endBeat\":{\"type\":\"number\",\"minimum\":1}},\"additionalProperties\":false}},"
				"{\"name\":\"markers/hide_auto_loop\",\"title\":\"Hide Auto Loop Range\",\"description\":\"Hide or show the current auto-loop range without changing its endpoints.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"hidden\":{\"type\":\"boolean\"}},\"additionalProperties\":false}},"
				"{\"name\":\"markers/set_auto_punch\",\"title\":\"Set Auto Punch Range\",\"description\":\"Set auto-punch range using start/end sample or start/end bar+beat.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"startSample\":{\"type\":\"integer\",\"minimum\":0},\"endSample\":{\"type\":\"integer\",\"minimum\":0},\"startBar\":{\"type\":\"integer\",\"minimum\":1},\"startBeat\":{\"type\":\"number\",\"minimum\":1},\"endBar\":{\"type\":\"integer\",\"minimum\":1},\"endBeat\":{\"type\":\"number\",\"minimum\":1}},\"additionalProperties\":false}},"
				"{\"name\":\"markers/hide_auto_punch\",\"title\":\"Hide Auto Punch Range\",\"description\":\"Hide or show the current auto-punch range without changing its endpoints.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"hidden\":{\"type\":\"boolean\"}},\"additionalProperties\":false}},"
				"{\"name\":\"markers/delete\",\"title\":\"Delete Marker\",\"description\":\"Delete one marker by location ID (preferred), or by name with optional sample for disambiguation.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"locationId\":{\"type\":\"string\"},\"name\":{\"type\":\"string\"},\"sample\":{\"type\":\"integer\",\"minimum\":0}},\"additionalProperties\":false}},"
				"{\"name\":\"markers/rename\",\"title\":\"Rename Marker\",\"description\":\"Rename one marker by location ID (preferred), or by name with optional sample for disambiguation.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"locationId\":{\"type\":\"string\"},\"name\":{\"type\":\"string\"},\"sample\":{\"type\":\"integer\",\"minimum\":0},\"newName\":{\"type\":\"string\"}},\"required\":[\"newName\"],\"additionalProperties\":false}},"
				"{\"name\":\"tracks/list\",\"title\":\"Tracks List\",\"description\":\"List session tracks and buses.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"includeHidden\":{\"type\":\"boolean\"}},\"additionalProperties\":false}},"
					"{\"name\":\"track/get_info\",\"title\":\"Get Track Info\",\"description\":\"Get one track info (fader, pan, rec, mute, solo, sends, plugins).\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"}},\"required\":[\"id\"],\"additionalProperties\":false}},"
					"{\"name\":\"plugin/get_description\",\"title\":\"Plugin Description\",\"description\":\"Get plugin descriptor and parameter metadata for a route plugin.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"pluginIndex\":{\"type\":\"integer\",\"minimum\":0}},\"required\":[\"id\",\"pluginIndex\"],\"additionalProperties\":false}},"
					"{\"name\":\"plugin/set_parameter\",\"title\":\"Set Plugin Parameter\",\"description\":\"Set a plugin parameter by parameter index or control ID.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"pluginIndex\":{\"type\":\"integer\",\"minimum\":0},\"parameterIndex\":{\"type\":\"integer\",\"minimum\":0},\"controlId\":{\"type\":\"integer\",\"minimum\":0},\"value\":{\"type\":\"number\"},\"interface\":{\"type\":\"number\"}},\"required\":[\"id\",\"pluginIndex\"],\"additionalProperties\":false}},"
				"{\"name\":\"track/get_fader\",\"title\":\"Get Track Fader\",\"description\":\"Get current track fader as normalized position and dB.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"}},\"required\":[\"id\"],\"additionalProperties\":false}},"
				"{\"name\":\"track/select\",\"title\":\"Select Track\",\"description\":\"Select a route in Ardour.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"}},\"required\":[\"id\"],\"additionalProperties\":false}},"
				"{\"name\":\"track/set_mute\",\"title\":\"Set Track Mute\",\"description\":\"Set route mute state.\","
				"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"value\":{\"type\":\"boolean\"}},\"required\":[\"id\",\"value\"],\"additionalProperties\":false}},"
					"{\"name\":\"track/set_solo\",\"title\":\"Set Track Solo\",\"description\":\"Set route solo state.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"value\":{\"type\":\"boolean\"}},\"required\":[\"id\",\"value\"],\"additionalProperties\":false}},"
					"{\"name\":\"track/set_rec_enable\",\"title\":\"Set Record Enable\",\"description\":\"Set track record-enable state.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"value\":{\"type\":\"boolean\"}},\"required\":[\"id\",\"value\"],\"additionalProperties\":false}},"
					"{\"name\":\"track/set_rec_safe\",\"title\":\"Set Record Safe\",\"description\":\"Set track record-safe state.\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"value\":{\"type\":\"boolean\"}},\"required\":[\"id\",\"value\"],\"additionalProperties\":false}},"
					"{\"name\":\"track/set_pan\",\"title\":\"Set Pan\",\"description\":\"Set route pan position (0.0 to 1.0).\","
					"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"position\":{\"type\":\"number\",\"minimum\":0,\"maximum\":1}},\"required\":[\"id\",\"position\"],\"additionalProperties\":false}},"
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

				if (tool_name == "markers/set_auto_loop") {
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
						return jsonrpc_result (
							id,
							std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Auto-loop range unchanged\"}],\"structuredContent\":") + structured + "}");
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
					return jsonrpc_result (
						id,
						std::string ("{\"content\":[{\"type\":\"text\",\"text\":\"Auto-loop range updated\"}],\"structuredContent\":") + structured + "}");
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

					ARDOUR::Location* target = 0;

					if (!location_id.empty ()) {
						target = locations->get_location_by_id (PBD::ID (location_id));
						if (!target) {
							return jsonrpc_error (id, -32602, "Marker locationId not found");
						}
					} else {
						std::unique_ptr<XMLNode> locations_state (&locations->get_state ());
						if (!locations_state.get ()) {
							return jsonrpc_error (id, -32602, "Could not read session locations");
						}

						struct Candidate {
							std::string id;
							samplepos_t sample;
						};

						std::vector<Candidate> candidates;
						const XMLNodeList children = locations_state->children ();

						for (XMLNodeConstIterator it = children.begin (); it != children.end (); ++it) {
							if (!(*it) || (*it)->name () != "Location") {
								continue;
							}

							uint32_t flags = 0;
							if (!parse_location_flags (*(*it), flags)) {
								continue;
							}
							if (!(flags & ((uint32_t) ARDOUR::Location::IsMark | (uint32_t) ARDOUR::Location::IsRangeMarker))) {
								continue;
							}

							std::string candidate_name;
							std::string candidate_id;
							samplepos_t start_sample = 0;
							samplepos_t end_sample = 0;
							if (!(*it)->get_property ("name", candidate_name) ||
							    !(*it)->get_property ("id", candidate_id) ||
							    !parse_location_samples (*(*it), start_sample, end_sample)) {
								continue;
							}
							if (candidate_name != name) {
								continue;
							}
							if (sample_opt && start_sample != (samplepos_t) *sample_opt) {
								continue;
							}

							Candidate c;
							c.id = candidate_id;
							c.sample = start_sample;
							candidates.push_back (c);
						}

						if (candidates.empty ()) {
							return jsonrpc_error (id, -32602, "Marker not found by name/sample");
						}
						if (candidates.size () > 1) {
							return jsonrpc_error (id, -32602, "Ambiguous marker name; provide locationId or sample");
						}

						target = locations->get_location_by_id (PBD::ID (candidates[0].id));
						if (!target) {
							return jsonrpc_error (id, -32602, "Marker disappeared before deletion");
						}
					}

						if (!(target->is_mark () || target->is_range_marker ())) {
							return jsonrpc_error (id, -32602, "Location is not a marker/range");
						}

						const std::string removed_id = target->id ().to_s ();
						const std::string removed_name = target->name ();
						const samplepos_t removed_start_sample = target->start_sample ();
						const samplepos_t removed_end_sample = target->end_sample ();
						const uint32_t removed_flags = (uint32_t) target->flags ();

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

					ARDOUR::Location* target = 0;

					if (!location_id.empty ()) {
						target = locations->get_location_by_id (PBD::ID (location_id));
						if (!target) {
							return jsonrpc_error (id, -32602, "Marker locationId not found");
						}
					} else {
						std::unique_ptr<XMLNode> locations_state (&locations->get_state ());
						if (!locations_state.get ()) {
							return jsonrpc_error (id, -32602, "Could not read session locations");
						}

						struct Candidate {
							std::string id;
							samplepos_t sample;
						};

						std::vector<Candidate> candidates;
						const XMLNodeList children = locations_state->children ();

						for (XMLNodeConstIterator it = children.begin (); it != children.end (); ++it) {
							if (!(*it) || (*it)->name () != "Location") {
								continue;
							}

							uint32_t flags = 0;
							if (!parse_location_flags (*(*it), flags)) {
								continue;
							}
							if (!(flags & ((uint32_t) ARDOUR::Location::IsMark | (uint32_t) ARDOUR::Location::IsRangeMarker))) {
								continue;
							}

							std::string candidate_name;
							std::string candidate_id;
							samplepos_t start_sample = 0;
							samplepos_t end_sample = 0;
							if (!(*it)->get_property ("name", candidate_name) ||
							    !(*it)->get_property ("id", candidate_id) ||
							    !parse_location_samples (*(*it), start_sample, end_sample)) {
								continue;
							}
							if (candidate_name != name) {
								continue;
							}
							if (sample_opt && start_sample != (samplepos_t) *sample_opt) {
								continue;
							}

							Candidate c;
							c.id = candidate_id;
							c.sample = start_sample;
							candidates.push_back (c);
						}

						if (candidates.empty ()) {
							return jsonrpc_error (id, -32602, "Marker not found by name/sample");
						}
						if (candidates.size () > 1) {
							return jsonrpc_error (id, -32602, "Ambiguous marker name; provide locationId or sample");
						}

						target = locations->get_location_by_id (PBD::ID (candidates[0].id));
						if (!target) {
							return jsonrpc_error (id, -32602, "Marker disappeared before rename");
						}
					}

					if (!(target->is_mark () || target->is_range_marker ())) {
						return jsonrpc_error (id, -32602, "Location is not a marker/range");
					}

					const std::string renamed_id = target->id ().to_s ();
					const std::string old_name = target->name ();
					const samplepos_t marker_start_sample = target->start_sample ();
					const samplepos_t marker_end_sample = target->end_sample ();
					const uint32_t marker_flags = (uint32_t) target->flags ();

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

				if (tool_name == "plugin/get_description") {
					const std::string route_id = root.get<std::string> ("params.arguments.id", "");
					const int plugin_index = root.get<int> ("params.arguments.pluginIndex", -1);

					if (route_id.empty ()) {
						return jsonrpc_error (id, -32602, "Missing route id");
					}
					if (plugin_index < 0) {
						return jsonrpc_error (id, -32602, "Invalid pluginIndex (expected >= 0)");
					}

					const std::shared_ptr<ARDOUR::Route> route = _session.route_by_id (PBD::ID (route_id));
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

					const std::shared_ptr<ARDOUR::Route> route = _session.route_by_id (PBD::ID (route_id));
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

				if (tool_name == "track/select") {
					const std::string route_id = root.get<std::string> ("params.arguments.id", "");

					if (route_id.empty ()) {
						return jsonrpc_error (id, -32602, "Missing route id");
					}

					const std::shared_ptr<ARDOUR::Route> route = _session.route_by_id (PBD::ID (route_id));
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

				if (tool_name == "track/set_mute") {
					const std::string route_id = root.get<std::string> ("params.arguments.id", "");
					const boost::optional<bool> value = root.get_optional<bool> ("params.arguments.value");

					if (route_id.empty ()) {
						return jsonrpc_error (id, -32602, "Missing route id");
					}
					if (!value) {
						return jsonrpc_error (id, -32602, "Missing boolean value");
					}

					const std::shared_ptr<ARDOUR::Route> route = _session.route_by_id (PBD::ID (route_id));
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

					const std::shared_ptr<ARDOUR::Route> route = _session.route_by_id (PBD::ID (route_id));
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

					const std::shared_ptr<ARDOUR::Route> route = _session.route_by_id (PBD::ID (route_id));
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

					const std::shared_ptr<ARDOUR::Route> route = _session.route_by_id (PBD::ID (route_id));
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

					const std::shared_ptr<ARDOUR::Route> route = _session.route_by_id (PBD::ID (route_id));
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
