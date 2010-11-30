/*
    Copyright (C) 2008-2010 Paul Davis
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
	uri_map_feature_data.uri_to_id     = &URIMap::uri_map_uri_to_id;
	uri_map_feature_data.callback_data = this;
	uri_map_feature.URI                = LV2_URI_MAP_URI;
	uri_map_feature.data               = &uri_map_feature_data;

	uri_unmap_feature_data.id_to_uri     = &URIMap::uri_unmap_id_to_uri;
	uri_unmap_feature_data.callback_data = this;
	uri_unmap_feature.URI                = LV2_URI_UNMAP_URI;
	uri_unmap_feature.data               = &uri_unmap_feature_data;
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
				return NULL;
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
	URIMap* me = (URIMap*)callback_data;
	return me->uri_to_id(map, uri);

}


const char*
URIMap::uri_unmap_id_to_uri(LV2_URI_Map_Callback_Data callback_data,
                            const char*               map,
                            uint32_t                  id)
{
	URIMap* me = (URIMap*)callback_data;
	return me->id_to_uri(map, id);
}


} // namespace ARDOUR

