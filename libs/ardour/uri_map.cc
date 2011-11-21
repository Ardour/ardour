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
#include <iostream>

#include <stdint.h>
#include <string.h>

#include <glib.h>

#include "pbd/error.h"

#include "ardour/uri_map.h"

using namespace std;

namespace ARDOUR {


URIMap::URIMap()
{
	_uri_map_feature_data.uri_to_id     = &URIMap::uri_map_uri_to_id;
	_uri_map_feature_data.callback_data = this;
	_uri_map_feature.URI                = LV2_URI_MAP_URI;
	_uri_map_feature.data               = &_uri_map_feature_data;

	_urid_map_feature_data.map    = &URIMap::urid_map;
	_urid_map_feature_data.handle = this;
	_urid_map_feature.URI         = LV2_URID_MAP_URI;
	_urid_map_feature.data        = &_urid_map_feature_data;

	_urid_unmap_feature_data.unmap  = &URIMap::urid_unmap;
	_urid_unmap_feature_data.handle = this;
	_urid_unmap_feature.URI         = LV2_URID_UNMAP_URI;
	_urid_unmap_feature.data        = &_urid_unmap_feature_data;
}


uint32_t
URIMap::uri_to_id(const char* map,
                  const char* uri)
{
	const uint32_t id = static_cast<uint32_t>(g_quark_from_string(uri));
	if (map && !strcmp(map, "http://lv2plug.in/ns/ext/event")) {
		GlobalToEvent::iterator i = _global_to_event.find(id);
		if (i != _global_to_event.end()) {
			return i->second;
		} else {
			if (_global_to_event.size() + 1 > UINT16_MAX) {
				PBD::error << "Event URI " << uri << " ID out of range." << endl;
				return 0;
			}
			const uint16_t ev_id = _global_to_event.size() + 1;
			assert(_event_to_global.find(ev_id) == _event_to_global.end());
			_global_to_event.insert(make_pair(id, ev_id));
			_event_to_global.insert(make_pair(ev_id, id));
			return ev_id;
		}
	} else {
		return id;
	}
}


const char*
URIMap::id_to_uri(const char*    map,
                  const uint32_t id)
{
	if (map && !strcmp(map, "http://lv2plug.in/ns/ext/event")) {
		EventToGlobal::iterator i = _event_to_global.find(id);
		if (i == _event_to_global.end()) {
			PBD::error << "Failed to unmap event URI " << id << endl;
			return NULL;
		}
		return g_quark_to_string(i->second);
	} else {
		return g_quark_to_string(id);
	}

}


uint32_t
URIMap::uri_map_uri_to_id(LV2_URI_Map_Callback_Data callback_data,
                          const char*               map,
                          const char*               uri)
{
	URIMap* const me = (URIMap*)callback_data;
	return me->uri_to_id(map, uri);
}


LV2_URID
URIMap::urid_map(LV2_URID_Map_Handle handle,
                 const char*         uri)
{
	URIMap* const me = (URIMap*)handle;
	return me->uri_to_id(NULL, uri);
}


const char*
URIMap::urid_unmap(LV2_URID_Unmap_Handle handle,
                   LV2_URID              urid)
{
	URIMap* const me = (URIMap*)handle;
	return me->id_to_uri(NULL, urid);
}


} // namespace ARDOUR

