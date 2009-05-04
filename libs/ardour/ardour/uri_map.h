/*
    Copyright (C) 2009 Paul Davis
    Author: Dave Robillard

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
#include <string>
#include <boost/utility.hpp>
#include <slv2/slv2.h>
#include "lv2ext/lv2_uri_map.h"
	
namespace ARDOUR {


/** Implementation of the LV2 URI Map extension
 */
class URIMap : public boost::noncopyable {
public:
	URIMap();
	
	LV2_Feature* feature() { return &uri_map_feature; }

	uint32_t uri_to_id(const char* map,
	                   const char* uri);

private:
	typedef std::map<std::string, uint32_t> Map;
	
	static uint32_t uri_map_uri_to_id(LV2_URI_Map_Callback_Data callback_data,
	                                  const char*               map,
	                                  const char*               uri);
	
	LV2_Feature         uri_map_feature;
	LV2_URI_Map_Feature uri_map_feature_data;
	Map                 uri_map;
	uint32_t            next_uri_id;
};


} // namespace ARDOUR

#endif // __ardour_uri_map_h__
