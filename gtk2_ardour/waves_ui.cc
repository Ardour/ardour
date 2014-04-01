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

void
WavesUI::get_styles(const XMLTree& layout, WavesUI::XMLNodeMap &styles)
{
	XMLNode* root  = layout.root();
	if (root != NULL) {
		WavesUI::dbg_out << "WavesUI::get_styles\n";

		for (XMLNodeList::const_iterator i = root->children().begin(); i != root->children().end(); ++i) {
			if ( !strcasecmp((*i)->name().c_str(), "style")) {
				std::string style_name = ((*i)->property("name") ? (*i)->property("name")->value() : std::string(""));
				if (!style_name.empty()) {
					WavesUI::dbg_out << "\tstyle [" << style_name << "] added:" << *i << "\n";
					styles[style_name] = *i;
				}
			}
		}
	}
}

double
WavesUI::xml_property (const XMLNode &node, const char *prop_name, const XMLNodeMap& styles, double default_value)
{
	WavesUI::dbg_out << "double xml_property ( " << prop_name << ")\n";
	std::string property = xml_property(node, prop_name, styles, "");
	if (property.empty()) {
		return default_value;
	}
	return atof(property.c_str());
}

int
WavesUI::xml_property (const XMLNode &node, const char *prop_name, const XMLNodeMap& styles, int default_value)
{
	WavesUI::dbg_out << "int xml_property ( " << prop_name << ")\n";
	std::string property = xml_property(node, prop_name, styles, "");
	if (property.empty()) {
		return default_value;
	}

	return atoi(property.c_str());
}

bool
WavesUI::xml_property (const XMLNode &node, const char *prop_name, const XMLNodeMap& styles, bool default_value)
{
	WavesUI::dbg_out << "bool xml_property ( " << prop_name << ")\n";
	std::string property = xml_property(node, prop_name, styles, "");
	if (property.empty()) {
		return default_value;
	}
	std::transform(property.begin(), property.end(), property.begin(), ::toupper);
	return property == "TRUE";
}

std::string
WavesUI::xml_property (const XMLNode &node, const char *prop_name, const XMLNodeMap& styles, const std::string default_value)
{
	WavesUI::dbg_out << "std::string xml_property (<" << node.name() << ">, " << prop_name << " )\n";
	std::string property = node.property (prop_name) ? node.property(prop_name)->value() : "";
	if (property.empty()) {
		WavesUI::dbg_out << "\tlooking for style_name to read [" << prop_name << "]\n";
		std::string style_name = node.property ("style") ? node.property("style")->value() : "";
		if (!style_name.empty()) {
			WavesUI::dbg_out << "\tstyle_name [" << style_name << "] found\n";
			XMLNodeMap::const_iterator style = styles.find(style_name);
			if (style != styles.end()) {
				return WavesUI::xml_property (*style->second, prop_name, styles, default_value);
			}
		}
	} else {
		WavesUI::dbg_out << "\t" << prop_name << " = [" << property << "]\n";
	}


	if (property.empty()) {
		return default_value;
	}
	return property;
}

std::string
WavesUI::xml_property (const XMLNode& node, const char* prop_name, const XMLNodeMap& styles, const char* default_value)
{
	return xml_property (node, prop_name, styles, std::string(default_value));
}

Gtk::Widget*
WavesUI::create_widget (const XMLNode& definition, const XMLNodeMap& styles, std::map<std::string, Gtk::Widget*> &named_widgets)
{
	Gtk::Widget* child = NULL;
	std::string widget_type = definition.name();
	std::string widget_id = WavesUI::xml_property (definition, "id", styles, "");

	std::string text = WavesUI::xml_property (definition, "text", styles, "");
	boost::replace_all(text, "\\n", "\n");

	int height = WavesUI::xml_property (definition, "height", styles, -1);
	int width = WavesUI::xml_property (definition, "width", styles, -1);

	std::transform(widget_type.begin(), widget_type.end(), widget_type.begin(), ::toupper);
	if (widget_type == "BUTTON") {
		child = manage (new WavesButton(text));
		((WavesButton*)child)->set_border_width (WavesUI::xml_property (definition, "borderwidth", styles, "0").c_str());
		((WavesButton*)child)->set_border_color (WavesUI::xml_property (definition, "bordercolor", styles, "#000000").c_str());
	} else if (widget_type == "COMBOBOXTEXT") {
		child = manage (new Gtk::ComboBoxText);
	} else if (widget_type == "LABEL") {
		child = manage (new Gtk::Label(text));
	} else if (widget_type == "LAYOUT") {
		child = manage (new Gtk::Layout);
	}

	if (child != NULL) {
		child->set_size_request (width, height);
		if (!widget_id.empty())
		{
			named_widgets[widget_id] = child;
		}
		
		std::string property = WavesUI::xml_property (definition, "bgnormal", styles, "");
		if (!property.empty()) {
			child->modify_bg(Gtk::STATE_NORMAL, Gdk::Color(property));
		}

		property = WavesUI::xml_property (definition, "bgactive", styles, "");
		if (!property.empty()) {
			child->modify_bg(Gtk::STATE_ACTIVE, Gdk::Color(property));
		}

		property = WavesUI::xml_property (definition, "bghover", styles, "");
		if (!property.empty()) {
			child->modify_bg(Gtk::STATE_PRELIGHT, Gdk::Color(property));
		}

		property = WavesUI::xml_property (definition, "fgnormal", styles, "");
		if (!property.empty()) {
			child->modify_fg(Gtk::STATE_NORMAL, Gdk::Color(property));
		}

		property = WavesUI::xml_property (definition, "fgactive", styles, "");
		if (!property.empty()) {
			child->modify_fg(Gtk::STATE_ACTIVE, Gdk::Color(property));
		}

		property = WavesUI::xml_property (definition, "fghover", styles, "");
		if (!property.empty()) {
			child->modify_fg(Gtk::STATE_PRELIGHT, Gdk::Color(property));
		}

		property = WavesUI::xml_property (definition, "font", styles, "");
		if (!property.empty()) {
			child->modify_font(Pango::FontDescription(property));
		}
	}
	return child;
}


Gtk::Widget*
WavesUI::add_widget (Gtk::Box& parent, const XMLNode& definition, const XMLNodeMap& styles, std::map<std::string, Gtk::Widget*> &named_widgets)
{
	Gtk::Widget* child = create_widget(definition, styles, named_widgets);

	if (child != NULL)
	{
		parent.pack_start(*child, false, false);
	}
	return child;
}


Gtk::Widget*
WavesUI::add_widget (Gtk::Layout& parent, const XMLNode& definition, const XMLNodeMap& styles, std::map<std::string, Gtk::Widget*> &named_widgets)
{
	Gtk::Widget* child = create_widget(definition, styles, named_widgets);

	if (child != NULL)
	{
		parent.put (*child, 
					WavesUI::xml_property (definition, "x", styles, 0), 
					WavesUI::xml_property (definition, "y", styles, 0));
	}
	return child;
}


Gtk::Widget*
WavesUI::add_widget (Gtk::Widget& parent, const XMLNode &definition, const XMLNodeMap& styles, std::map<std::string, Gtk::Widget*> &named_widgets)
{
	Gtk::Widget* child = NULL;
	if(dynamic_cast<Gtk::Layout*> (&parent)) {
		child = WavesUI::add_widget (*dynamic_cast<Gtk::Layout*> (&parent), definition, styles, named_widgets);
	}
	else if(dynamic_cast<Gtk::Box*> (&parent)) {
		child = WavesUI::add_widget (*dynamic_cast<Gtk::Box*> (&parent), definition, styles, named_widgets);
	}

	if (child != NULL) {
		WavesUI::create_ui (definition.children(), styles, *child, named_widgets);
	}
	return child;
}

void
WavesUI::create_ui (const XMLNodeList& definition, const XMLNodeMap& styles, Gtk::Widget& root, std::map<std::string, Gtk::Widget*> &named_widgets)
{
	for (XMLNodeList::const_iterator i = definition.begin(); i != definition.end(); ++i) {
		WavesUI::add_widget ((Gtk::Widget&)root, **i, styles, named_widgets);
	}
}

void
WavesUI::create_ui (const XMLTree& layout, Gtk::Widget& root, std::map<std::string, Gtk::Widget*> &named_widgets)
{
	WavesUI::dbg_out << "const XMLTree& layout, Gtk::Widget& root, std::map<std::string, Gtk::Widget*> &named_widgets):\n";
	XMLNodeMap styles;
	WavesUI::get_styles(layout, styles);
	const XMLNodeList& definition = layout.root()->children();
	WavesUI::create_ui (definition, styles, root, named_widgets);
}