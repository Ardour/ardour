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
#include "canvas/xml_ui.h"
#include "canvas/rectangle.h"
#include "canvas/pixbuf.h"
#include "canvas/text.h"

//std::ofstream dbg_out("/users/WavesUILog.txt");
namespace ArdourCanvas {
namespace XMLUI {

void
get_styles(const XMLTree& layout, XMLNodeMap &styles)
{
	XMLNode* root  = layout.root();
	if (root != NULL) {

		for (XMLNodeList::const_iterator i = root->children().begin(); i != root->children().end(); ++i) {
			if ( !strcasecmp((*i)->name().c_str(), "style")) {
				std::string style_name = ((*i)->property("name") ? (*i)->property("name")->value() : std::string(""));
				if (!style_name.empty()) {
					styles[style_name] = *i;
				}
			}
		}
	}
}

double
xml_property (const XMLNode &node, const char *prop_name, const XMLNodeMap& styles, double default_value)
{
	std::string property = xml_property(node, prop_name, styles, "");
	if (property.empty()) {
		return default_value;
	}

	// get float value from string
	double value;
	std::istringstream ss (property);
	// set classic locale with "." float delimeter as default
	ss.imbue(std::locale("C"));
	ss >> value;

	return value;
}

double
xml_property (const XMLNode &node, const char *prop_name, double default_value)
{
	return xml_property (node, prop_name, XMLNodeMap(), default_value);
}


int32_t
xml_property (const XMLNode &node, const char *prop_name, const XMLNodeMap& styles, int32_t default_value)
{
	std::string property = xml_property(node, prop_name, styles, "");
	if (property.empty()) {
		return default_value;
	}

	int32_t value;
	int num_of_read_values;
	
	const char* source = property.c_str();
	if (source[0] == '#') {
		num_of_read_values = sscanf(property.c_str()+1, "%x", &value);
	} else {
		num_of_read_values = sscanf(property.c_str(), "%d", &value);
	}

	if(num_of_read_values != 1) { 
		return default_value;
	}

	return value;
}


int32_t
xml_property (const XMLNode &node, const char *prop_name, int32_t default_value)
{
	return xml_property (node, prop_name, XMLNodeMap(), default_value);
}


uint32_t
xml_property (const XMLNode &node, const char *prop_name, const XMLNodeMap& styles, uint32_t default_value)
{
	std::string property = xml_property(node, prop_name, styles, "");
	if (property.empty()) {
		return default_value;
	}

	uint32_t value;
	int num_of_read_values;
	
	const char* source = property.c_str();
	if (source[0] == '#') {
		num_of_read_values = sscanf(property.c_str()+1, "%x", &value);
	} else {
		num_of_read_values = sscanf(property.c_str(), "%u", &value);
	}

	if(num_of_read_values != 1) { 
		return default_value;
	}

	return value;
}

uint32_t
xml_property (const XMLNode &node, const char *prop_name, uint32_t default_value)
{
	return xml_property (node, prop_name, XMLNodeMap(), default_value);
}

bool
xml_property (const XMLNode &node, const char *prop_name, const XMLNodeMap& styles, bool default_value)
{
	std::string property = xml_property(node, prop_name, styles, "");

	if (property.empty()) {
		return default_value;
	}

	std::transform(property.begin(), property.end(), property.begin(), ::toupper);
	return property == "TRUE";
}

bool
xml_property (const XMLNode &node, const char *prop_name, bool default_value)
{
	std::string property = xml_property(node, prop_name, "");

	if (property.empty()) {
		return default_value;
	}

	std::transform(property.begin(), property.end(), property.begin(), ::toupper);
	return property == "TRUE";
}

std::string
xml_property (const XMLNode &node, const char *prop_name, const XMLNodeMap& styles, const std::string& default_value)
{
	if (!node.property (prop_name)) {
		std::string style_name = node.property ("style") ? node.property("style")->value() : "";
		if (!style_name.empty()) {
			XMLNodeMap::const_iterator style = styles.find(style_name);
			if (style != styles.end()) {
				return xml_property (*style->second, prop_name, styles, std::string(default_value));
			}
		}
	}
	else
	{
		return node.property(prop_name)->value();
	}
	return default_value;
}

std::string
xml_property (const XMLNode &node, const char *prop_name, const std::string& default_value)
{
	return node.property (prop_name) ? node.property(prop_name)->value() : default_value;
}

std::string
xml_property (const XMLNode& node, const char* prop_name, const XMLNodeMap& styles, const char* default_value)
{
	return xml_property (node, prop_name, styles, std::string(default_value));
}

std::string
xml_property (const XMLNode& node, const char* prop_name, const char* default_value)
{
	return xml_property (node, prop_name, std::string(default_value));
}

std::string	
xml_nodetype(const XMLNode& node)
{
	std::string item_type = node.name();
	std::transform(item_type.begin(), item_type.end(), item_type.begin(), ::toupper);
	return item_type;
}

std::string
xml_id(const XMLNode& node)
{
	return xml_property (node, "id", std::string(""));
}

double xml_x (const XMLNode& node, const XMLNodeMap& styles, double default_value)
{
	return xml_property (node, "x", styles, default_value);
}

double xml_y (const XMLNode& node, const XMLNodeMap& styles, double default_value)
{
	return xml_property (node, "y", styles, default_value);
}

Pango::Alignment xml_text_alignment (const XMLNode& node, const XMLNodeMap& styles, Pango::Alignment default_value)
{
	std::string property = xml_property(node, "alignment", styles, "");

	std::transform(property.begin(), property.end(), property.begin(), ::toupper);
	if (property == "LEFT") {
		return Pango::ALIGN_LEFT;
	} else if (property == "RIGHT") {
		return Pango::ALIGN_RIGHT;
	} else if (property == "CENTER") {
		return Pango::ALIGN_CENTER;
	}

	return default_value;
}

#ifdef ARDOUR_CANVAS_HAS_XML_UI
ArdourCanvas::Item*
create_item (Group* parent, const XMLNode& definition, const XMLNodeMap& styles, std::map<std::string, ArdourCanvas::Item*>& named_items)
{
	ArdourCanvas::Item* child = NULL;
	std::string item_type = definition.name();
	std::transform(item_type.begin(), item_type.end(), item_type.begin(), ::toupper);

	if (item_type == "GROUP") {
		child = new Group (parent, definition, styles, named_items);
	} else if (item_type == "RECTANGLE") {
		child = new Rectangle (parent, definition, styles, named_items);
	} else if (item_type == "ICON") {
		child = new Pixbuf (parent, definition, styles, named_items);
	} else if (item_type == "TEXT") {
		child = new Text (parent, definition, styles, named_items);
	} else if (item_type == "FROMSTYLE") {
		std::string style_name = xml_property (definition, "style", "");
		if (!style_name.empty()) {
			XMLNodeMap::const_iterator style = styles.find(style_name);
			if (style != styles.end()) {
				const XMLNodeList& children = (*style->second).children();
				for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
					create_item(parent, **i, styles, named_items);
				}
			}
		}
	}

	return child;
}
#endif
}
}

