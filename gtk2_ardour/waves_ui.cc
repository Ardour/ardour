/*
	Copyright (C) 2014 Valeriy Kamyshniy

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
#include "waves_ui.h"

std::ofstream WavesUI::dbg_out("/users/WavesUILog.txt");

double 
WavesUI::xml_property (const XMLNode &node, const char *prop_name, double default_value)
{
	return node.property (prop_name) ? atof(node.property(prop_name)->value().c_str()) : default_value;
}

int
WavesUI::xml_property (const XMLNode &node, const char *prop_name, int default_value)
{
	return node.property (prop_name) ? atoi(node.property(prop_name)->value().c_str()) : default_value;
}

bool
WavesUI::xml_property (const XMLNode &node, const char *prop_name, bool default_value)
{
	std::string property = node.property (prop_name) ? node.property(prop_name)->value() : std::string("");
	if (property.empty())
		return default_value;
	std::transform(property.begin(), property.end(), property.begin(), ::toupper);
	return property == "TRUE";
}

std::string
WavesUI::xml_property (const XMLNode &node, const char *prop_name, const std::string default_value)
{
	return node.property (prop_name) ? node.property(prop_name)->value() : default_value;
}

std::string
WavesUI::xml_property (const XMLNode& node, const char* prop_name, const char* default_value)
{
	return xml_property (node, prop_name, std::string(default_value));
}

Gtk::Widget*
WavesUI::create_widget (const XMLNode& definition, std::map<std::string, Gtk::Widget*> &named_widgets)
{
	Gtk::Widget* child = NULL;
	std::string widget_type = definition.name();
	std::string widget_id = WavesUI::xml_property (definition, "id", "");
	std::string style = WavesUI::xml_property (definition, "style", "");

	std::string text = WavesUI::xml_property (definition, "text", "");
	boost::replace_all(text, "\\n", "\n");

	int height = WavesUI::xml_property (definition, "height", -1);
	int width = WavesUI::xml_property (definition, "width", -1);

	std::transform(widget_type.begin(), widget_type.end(), widget_type.begin(), ::toupper);
	if (widget_type == "BUTTON") {
		child = manage (new WavesButton(text));
		double border_width = WavesUI::xml_property (definition, "borderwidth", 0.0);
		((WavesButton*)child)->set_border_width(border_width);
	} else if (widget_type == "COMBOBOXTEXT") {
		child = manage (new Gtk::ComboBoxText);
	} else if (widget_type == "LABEL") {
		child = manage (new Gtk::Label(text));
	} else if (widget_type == "LAYOUT") {
		child = manage (new Gtk::Layout);
	}

	if (child != NULL) {
		if (!style.empty()) {
			child->set_name (style);
		}

		child->set_size_request (width, height);
		if (!widget_id.empty())
		{
			named_widgets[widget_id] = child;
		}
		
		std::string property = WavesUI::xml_property (definition, "bgnormal", "");
		if (!property.empty()) {
			child->modify_bg(Gtk::STATE_NORMAL, Gdk::Color(property));
		}

		property = WavesUI::xml_property (definition, "bgactive", "");
		if (!property.empty()) {
			child->modify_bg(Gtk::STATE_ACTIVE, Gdk::Color(property));
		}

		property = WavesUI::xml_property (definition, "bghover", "");
		if (!property.empty()) {
			child->modify_bg(Gtk::STATE_PRELIGHT, Gdk::Color(property));
		}

		property = WavesUI::xml_property (definition, "fgnormal", "");
		if (!property.empty()) {
			child->modify_fg(Gtk::STATE_NORMAL, Gdk::Color(property));
		}

		property = WavesUI::xml_property (definition, "fgactive", "");
		if (!property.empty()) {
			child->modify_fg(Gtk::STATE_ACTIVE, Gdk::Color(property));
		}

		property = WavesUI::xml_property (definition, "fghover", "");
		if (!property.empty()) {
			child->modify_fg(Gtk::STATE_PRELIGHT, Gdk::Color(property));
		}

		property = WavesUI::xml_property (definition, "font", "");
		if (!property.empty()) {
			child->modify_font(Pango::FontDescription(property));
		}
	}
	return child;
}


Gtk::Widget*
WavesUI::add_widget (Gtk::Box& parent, const XMLNode& definition, std::map<std::string, Gtk::Widget*> &named_widgets)
{
	Gtk::Widget* child = create_widget(definition, named_widgets);

	if (child != NULL)
	{
		parent.pack_start(*child, false, false);
	}
	return child;
}


Gtk::Widget*
WavesUI::add_widget (Gtk::Layout& parent, const XMLNode& definition, std::map<std::string, Gtk::Widget*> &named_widgets)
{
	Gtk::Widget* child = create_widget(definition, named_widgets);

	if (child != NULL)
	{
		parent.put (*child, 
					WavesUI::xml_property (definition, "x", 0), 
					WavesUI::xml_property (definition, "y", 0));
	}
	return child;
}


Gtk::Widget*
WavesUI::add_widget (Gtk::Widget& parent, const XMLNode &definition, std::map<std::string, Gtk::Widget*> &named_widgets)
{
	Gtk::Widget* child = NULL;
	if(dynamic_cast<Gtk::Layout*> (&parent)) {
		child = add_widget (*dynamic_cast<Gtk::Layout*> (&parent), definition, named_widgets);
	}
	else if(dynamic_cast<Gtk::Box*> (&parent)) {
		child = add_widget (*dynamic_cast<Gtk::Box*> (&parent), definition, named_widgets);
	}

	if (child != NULL)
	{
		create_ui (definition.children(), *child, named_widgets);
	}
	return child;
}

void WavesUI::create_ui (const XMLNodeList& definition, Gtk::Widget& root, std::map<std::string, Gtk::Widget*> &named_widgets)
{
	for (XMLNodeList::const_iterator i = definition.begin(); i != definition.end(); ++i) {
		WavesUI::add_widget ((Gtk::Widget&)root, **i, named_widgets);
	}
}