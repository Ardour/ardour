/*
    Copyright (C) 2008-2011 Paul Davis
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

#include <cassert>
#include <string>
#include <utility>

#include <stdint.h>
#include <string.h>

#include "pbd/error.h"

#include "ardour/uri_map.h"

namespace ARDOUR {

static uint32_t
c_uri_map_uri_to_id(LV2_URI_Map_Callback_Data callback_data,
                    const char*               map,
                    const char*               uri)
{
	URIMap* const me = (URIMap*)callback_data;
	const uint32_t id = me->uri_to_id(uri);

	/* The event context with the uri-map extension guarantees a value in the
	   range of uint16_t.  Ardour used to map to a separate range to achieve
	   this, but unfortunately some plugins are broken and use the incorrect
	   context.  To compensate, we simply use the same context for everything
	   and hope that anything in the event context gets mapped before
	   UINT16_MAX is reached (which will be fine unless something seriously
	   weird is going on).  If this fails there is nothing we can do, die.
	*/
	assert(!map || strcmp(map, "http://lv2plug.in/ns/ext/event")
	       || id < UINT16_MAX);

	return id;
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
	_uri_map_feature_data.uri_to_id     = c_uri_map_uri_to_id;
	_uri_map_feature_data.callback_data = this;
	_uri_map_feature.URI                = LV2_URI_MAP_URI;
	_uri_map_feature.data               = &_uri_map_feature_data;

	_urid_map_feature_data.map    = c_urid_map;
	_urid_map_feature_data.handle = this;
	_urid_map_feature.URI         = LV2_URID_MAP_URI;
	_urid_map_feature.data        = &_urid_map_feature_data;

	_urid_unmap_feature_data.unmap  = c_urid_unmap;
	_urid_unmap_feature_data.handle = this;
	_urid_unmap_feature.URI         = LV2_URID_UNMAP_URI;
	_urid_unmap_feature.data        = &_urid_unmap_feature_data;
}

uint32_t
URIMap::uri_to_id(const char* uri)
{
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
	const Unmap::const_iterator i = _unmap.find(id);
	return (i != _unmap.end()) ? i->second.c_str() : NULL;
}

} // namespace ARDOUR

