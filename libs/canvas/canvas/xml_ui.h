/*
	Copyright (C) 2014 Waves Audio Ltd.

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
#ifndef __CANVAS_XML_UI_H__
#define __CANVAS_XML_UI_H__

#include "canvas/visibility.h"
#include <string>
#include <map>
#include <boost/algorithm/string.hpp>
#include <pangomm/layout.h>
#include "pbd/xml++.h"

namespace ArdourCanvas {

class Item;
class Canvas;
class Group;

namespace XMLUI {

	typedef std::map<std::string, XMLNode*> XMLNodeMap;

	extern LIBCANVAS_API void get_styles(const XMLTree& layout, XMLNodeMap &styles);

	extern LIBCANVAS_API double xml_property (const XMLNode& node, const char* prop_name, const XMLNodeMap& styles, double default_value);
	extern LIBCANVAS_API double xml_property (const XMLNode& node, const char* prop_name, double default_value);
	extern LIBCANVAS_API int32_t xml_property (const XMLNode& node, const char* prop_name, const XMLNodeMap& styles, int32_t default_value);
	extern LIBCANVAS_API int32_t xml_property (const XMLNode& node, const char* prop_name, int32_t default_value);
	extern LIBCANVAS_API uint32_t xml_property (const XMLNode& node, const char* prop_name, const XMLNodeMap& styles, uint32_t default_value);
	extern LIBCANVAS_API uint32_t xml_property (const XMLNode& node, const char* prop_name, uint32_t default_value);

	extern LIBCANVAS_API bool xml_property (const XMLNode& node, const char* prop_name, const XMLNodeMap& styles, bool default_value);
	extern LIBCANVAS_API bool xml_property (const XMLNode& node, const char* prop_name, bool default_value);

	extern LIBCANVAS_API std::string xml_property (const XMLNode& node, const char* prop_name, const XMLNodeMap& styles, const std::string& default_value);
	extern LIBCANVAS_API std::string xml_property (const XMLNode& node, const char* prop_name, const std::string& default_value);
	extern LIBCANVAS_API std::string xml_property (const XMLNode& node, const char* prop_name, const XMLNodeMap& styles, const char* default_value);
	extern LIBCANVAS_API std::string xml_property (const XMLNode& node, const char* prop_name, const char* default_value);
		
	extern LIBCANVAS_API std::string	xml_nodetype(const XMLNode& node);
	extern LIBCANVAS_API std::string	xml_id(const XMLNode& node);
	extern LIBCANVAS_API double xml_x (const XMLNode& node, const XMLNodeMap& styles, double default_value = 0);
	extern LIBCANVAS_API double xml_y (const XMLNode& node, const XMLNodeMap& styles, double default_value = 0);
	extern LIBCANVAS_API Pango::Alignment xml_text_alignment (const XMLNode& node, const XMLNodeMap& styles, Pango::Alignment default_value = Pango::ALIGN_LEFT);

#ifdef ARDOUR_CANVAS_HAS_XML_UI
	extern LIBCANVAS_API ArdourCanvas::Item* create_item (Group* parent, const XMLNode& definition, const XMLNodeMap& styles, std::map<std::string, Item*> &named_items);
#endif
}
}

#endif //__CANVAS_XML_UI_H__
