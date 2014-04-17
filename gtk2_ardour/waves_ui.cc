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
#include "canvas/canvas.h"
#include "waves_button.h"

//std::ofstream dbg_out("/users/WavesUILog.txt");

Gtk::Widget*
WavesUI::create_widget (const XMLNode& definition, const XMLNodeMap& styles, std::map<std::string, Gtk::Widget*> &named_widgets)
{
	Gtk::Widget* child = NULL;
	std::string widget_type = definition.name();
	std::string widget_id = xml_property (definition, "id", styles, "");

	std::string text = xml_property (definition, "text", styles, "");
	boost::replace_all(text, "\\n", "\n");

	std::transform(widget_type.begin(), widget_type.end(), widget_type.begin(), ::toupper);
	if (widget_type == "BUTTON") {
		child = manage (new WavesButton(text));
		((WavesButton*)child)->set_border_width (xml_property (definition, "borderwidth", styles, "0").c_str());
		((WavesButton*)child)->set_border_color (xml_property (definition, "bordercolor", styles, "#000000").c_str());
	} else if (widget_type == "COMBOBOXTEXT") {
		child = manage (new Gtk::ComboBoxText);
	} else if (widget_type == "LABEL") {
		child = manage (new Gtk::Label(text));
	} else if (widget_type == "LAYOUT") {
		child = manage (new Gtk::Layout);
	} else if (widget_type == "CANVAS") {
		std::map<std::string, ArdourCanvas::Item*> named_items;
		child = new ArdourCanvas::GtkCanvas(definition, styles, named_items);
	}

	if (child != NULL) {
		int height = xml_property (definition, "height", styles, -1);
		int width = xml_property (definition, "width", styles, -1);
		child->set_size_request (width, height);
		if (!widget_id.empty())
		{
			named_widgets[widget_id] = child;
		}
		
		std::string property = xml_property (definition, "bgnormal", styles, "");
		if (!property.empty()) {
			child->modify_bg(Gtk::STATE_NORMAL, Gdk::Color(property));
		}

		property = xml_property (definition, "bgdisabled", styles, property);
		if (!property.empty()) {
			child->modify_bg(Gtk::STATE_INSENSITIVE, Gdk::Color(property));
		}

		property = xml_property (definition, "bgactive", styles, "");
		if (!property.empty()) {
			child->modify_bg(Gtk::STATE_ACTIVE, Gdk::Color(property));
		}

		property = xml_property (definition, "bghover", styles, "");
		if (!property.empty()) {
			child->modify_bg(Gtk::STATE_PRELIGHT, Gdk::Color(property));
		}

		property = xml_property (definition, "fgnormal", styles, "");
		if (!property.empty()) {
			child->modify_fg(Gtk::STATE_NORMAL, Gdk::Color(property));
		}

		property = xml_property (definition, "fgdisabled", styles, property);
		if (!property.empty()) {
			child->modify_fg(Gtk::STATE_INSENSITIVE, Gdk::Color(property));
		}

		property = xml_property (definition, "fgactive", styles, "");
		if (!property.empty()) {
			child->modify_fg(Gtk::STATE_ACTIVE, Gdk::Color(property));
		}

		property = xml_property (definition, "fghover", styles, "");
		if (!property.empty()) {
			child->modify_fg(Gtk::STATE_PRELIGHT, Gdk::Color(property));
		}

		property = xml_property (definition, "font", styles, "");
		if (!property.empty()) {
			child->modify_font(Pango::FontDescription(property));
		}

		if (xml_property (definition, "visible", styles, true)) {
			child->show();
		} else {
			child->hide();
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
					xml_property (definition, "x", styles, 0), 
					xml_property (definition, "y", styles, 0));
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
	XMLNodeMap styles;
	get_styles(layout, styles);
	const XMLNodeList& definition = layout.root()->children();
	WavesUI::create_ui (definition, styles, root, named_widgets);
}