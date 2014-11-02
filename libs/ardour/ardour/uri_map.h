/*
    Copyright (C) 2009-2011 Paul Davis
    Author: David Robillard

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __ardour_uri_map_h__
#define __ardour_uri_map_h__

#include <map>

#include <boost/utility.hpp>

#include <glibmm/threads.h>

#include "lv2.h"
#include "lv2/lv2plug.in/ns/ext/uri-map/uri-map.h"
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

/** Implementation of the LV2 uri-map and urid extensions.
 *
 * This just uses a pair of std::map and is not so great in the space overhead
 * department, but it's fast enough and not really performance critical anyway.
 */
class LIBARDOUR_API URIMap : public boost::noncopyable {
public:
	static URIMap& instance();

	URIMap();

	LV2_Feature* uri_map_feature()    { return &_uri_map_feature; }
	LV2_Feature* urid_map_feature()   { return &_urid_map_feature; }
	LV2_Feature* urid_unmap_feature() { return &_urid_unmap_feature; }

	LV2_URID_Map*   urid_map()   { return &_urid_map_feature_data; }
	LV2_URID_Unmap* urid_unmap() { return &_urid_unmap_feature_data; }

	uint32_t    uri_to_id(const char* uri);
	const char* id_to_uri(uint32_t id) const;

	// Cached URIDs for use in real-time code
	struct URIDs {
		void init(URIMap& uri_map);

		uint32_t atom_Chunk;
		uint32_t atom_Path;
		uint32_t atom_Sequence;
		uint32_t atom_eventTransfer;
		uint32_t atom_URID;
		uint32_t atom_Blank;
		uint32_t atom_Object;
		uint32_t atom_Float;
		uint32_t log_Error;
		uint32_t log_Note;
		uint32_t log_Warning;
		uint32_t midi_MidiEvent;
		uint32_t time_Position;
		uint32_t time_bar;
		uint32_t time_barBeat;
		uint32_t time_beatUnit;
		uint32_t time_beatsPerBar;
		uint32_t time_beatsPerMinute;
		uint32_t time_frame;
		uint32_t time_speed;
		uint32_t patch_Get;
		uint32_t patch_Set;
		uint32_t patch_property;
		uint32_t patch_value;
	};

	URIDs urids;

private:
	typedef std::map<const std::string, uint32_t> Map;
	typedef std::map<uint32_t, const std::string> Unmap;

	Map   _map;
	Unmap _unmap;

	LV2_Feature         _uri_map_feature;
	LV2_URI_Map_Feature _uri_map_feature_data;
	LV2_Feature         _urid_map_feature;
	LV2_URID_Map        _urid_map_feature_data;
	LV2_Feature         _urid_unmap_feature;
	LV2_URID_Unmap      _urid_unmap_feature_data;

	mutable Glib::Threads::Mutex _lock;

	static URIMap* uri_map;
};

} // namespace ARDOUR

#endif // __ardour_uri_map_h__
