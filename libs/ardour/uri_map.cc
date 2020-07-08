/*
 * Copyright (C) 2009-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2010-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016-2019 Robin Gareus <robin@gareus.org>
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

#include <cassert>
#include <string>
#include <utility>

#include <stdint.h>
#include <string.h>

#include "pbd/error.h"

#include "ardour/uri_map.h"
#include "ardour/lv2_extensions.h"

namespace ARDOUR {

URIMap* URIMap::uri_map;

void
URIMap::URIDs::init(URIMap& uri_map)
{
	// Use string literals here instead of LV2 defines to avoid LV2 dependency
	atom_Chunk          = uri_map.uri_to_id("http://lv2plug.in/ns/ext/atom#Chunk");
	atom_Path           = uri_map.uri_to_id("http://lv2plug.in/ns/ext/atom#Path");
	atom_Sequence       = uri_map.uri_to_id("http://lv2plug.in/ns/ext/atom#Sequence");
	atom_eventTransfer  = uri_map.uri_to_id("http://lv2plug.in/ns/ext/atom#eventTransfer");
	atom_URID           = uri_map.uri_to_id("http://lv2plug.in/ns/ext/atom#URID");
	atom_Blank          = uri_map.uri_to_id("http://lv2plug.in/ns/ext/atom#Blank");
	atom_Object         = uri_map.uri_to_id("http://lv2plug.in/ns/ext/atom#Object");
	atom_Float          = uri_map.uri_to_id("http://lv2plug.in/ns/ext/atom#Float");
	log_Error           = uri_map.uri_to_id("http://lv2plug.in/ns/ext/log#Error");
	log_Note            = uri_map.uri_to_id("http://lv2plug.in/ns/ext/log#Note");
	log_Trace           = uri_map.uri_to_id("http://lv2plug.in/ns/ext/log#Trace");
	log_Warning         = uri_map.uri_to_id("http://lv2plug.in/ns/ext/log#Warning");
	midi_MidiEvent      = uri_map.uri_to_id("http://lv2plug.in/ns/ext/midi#MidiEvent");
	time_Position       = uri_map.uri_to_id("http://lv2plug.in/ns/ext/time#Position");
	time_bar            = uri_map.uri_to_id("http://lv2plug.in/ns/ext/time#bar");
	time_barBeat        = uri_map.uri_to_id("http://lv2plug.in/ns/ext/time#barBeat");
	time_beatUnit       = uri_map.uri_to_id("http://lv2plug.in/ns/ext/time#beatUnit");
	time_beatsPerBar    = uri_map.uri_to_id("http://lv2plug.in/ns/ext/time#beatsPerBar");
	time_beatsPerMinute = uri_map.uri_to_id("http://lv2plug.in/ns/ext/time#beatsPerMinute");
	time_frame          = uri_map.uri_to_id("http://lv2plug.in/ns/ext/time#frame");
	time_speed          = uri_map.uri_to_id("http://lv2plug.in/ns/ext/time#speed");
	time_scale          = uri_map.uri_to_id("http://ardour.org/lv2/time#scale"); // XXX
	patch_Get           = uri_map.uri_to_id("http://lv2plug.in/ns/ext/patch#Get");
	patch_Set           = uri_map.uri_to_id("http://lv2plug.in/ns/ext/patch#Set");
	patch_property      = uri_map.uri_to_id("http://lv2plug.in/ns/ext/patch#property");
	patch_value         = uri_map.uri_to_id("http://lv2plug.in/ns/ext/patch#value");
	state_StateChanged  = uri_map.uri_to_id("http://lv2plug.in/ns/ext/state#StateChanged"); // since LV2 1.15.1
#ifdef LV2_EXTENDED
	auto_event         = uri_map.uri_to_id(LV2_AUTOMATE_URI__event);
	auto_setup         = uri_map.uri_to_id(LV2_AUTOMATE_URI__setup);
	auto_finalize      = uri_map.uri_to_id(LV2_AUTOMATE_URI__finalize);
	auto_start         = uri_map.uri_to_id(LV2_AUTOMATE_URI__start);
	auto_end           = uri_map.uri_to_id(LV2_AUTOMATE_URI__end);
	auto_parameter     = uri_map.uri_to_id(LV2_AUTOMATE_URI__parameter);
	auto_value         = uri_map.uri_to_id(LV2_AUTOMATE_URI__value);
#endif
}

URIMap&
URIMap::instance()
{
	if (!URIMap::uri_map) {
		URIMap::uri_map = new URIMap();
	}
	return *URIMap::uri_map;
}

static LV2_URID
c_urid_map(LV2_URID_Map_Handle handle,
           const char*         uri)
{
	URIMap* const me = (URIMap*)handle;
	return me->uri_to_id(uri);
}

static const char*
c_urid_unmap(LV2_URID_Unmap_Handle handle,
             LV2_URID              urid)
{
	URIMap* const me = (URIMap*)handle;
	return me->id_to_uri(urid);
}

URIMap::URIMap()
{
	_urid_map_feature_data.map    = c_urid_map;
	_urid_map_feature_data.handle = this;
	_urid_map_feature.URI         = LV2_URID_MAP_URI;
	_urid_map_feature.data        = &_urid_map_feature_data;

	_urid_unmap_feature_data.unmap  = c_urid_unmap;
	_urid_unmap_feature_data.handle = this;
	_urid_unmap_feature.URI         = LV2_URID_UNMAP_URI;
	_urid_unmap_feature.data        = &_urid_unmap_feature_data;

	urids.init(*this);
}

uint32_t
URIMap::uri_to_id(const char* uri)
{
	Glib::Threads::Mutex::Lock lm (_lock);

	const std::string urimm(uri);
	const Map::const_iterator i = _map.find(urimm);
	if (i != _map.end()) {
		return i->second;
	}
	const uint32_t id = _map.size() + 1;
	_map.insert(std::make_pair(urimm, id));
	_unmap.insert(std::make_pair(id, urimm));
	return id;
}

const char*
URIMap::id_to_uri(const uint32_t id) const
{
	Glib::Threads::Mutex::Lock lm (_lock);

	const Unmap::const_iterator i = _unmap.find(id);
	return (i != _unmap.end()) ? i->second.c_str() : NULL;
}

} // namespace ARDOUR

