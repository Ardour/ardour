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

#include <exception>

#include "waves_ui.h"
#include "pbd/file_utils.h"
#include "pbd/failed_constructor.h"
#include "ardour/filesystem_paths.h"
#include "utils.h"

#include "pbd/convert.h"
#include "dbg_msg.h"

using namespace PBD;
using namespace ARDOUR;

std::map<std::string, const XMLTree*> WavesUI::__xml_tree_cache;

WavesUI::WavesUI (std::string layout_script_file, Gtk::Container& root)
	: _xml_tree (NULL)
{
	// To avoid a need of reading the same file many times:
	std::map<std::string, const XMLTree*>::const_iterator it = __xml_tree_cache.find(layout_script_file);
	if (it != __xml_tree_cache.end()) {
		_xml_tree = (*it).second;
	} else {
		std::string layout_file; 
		Searchpath spath (ardour_data_search_path());
		spath.add_subdirectory_to_paths("ui");

		if (!find_file_in_search_path (spath, layout_script_file, layout_file)) {
			dbg_msg("File not found: " + layout_script_file);
			throw failed_constructor ();
		}

		_xml_tree = new XMLTree (layout_file, false);
		__xml_tree_cache[layout_script_file] = _xml_tree;
	}

	create_ui(_xml_tree, root);
}

Gtk::Widget*
WavesUI::create_widget (const XMLNode& definition, const XMLNodeMap& styles)
{
	Gtk::Object* child = NULL;
	std::string widget_type = definition.name();
	std::string widget_id = xml_property (definition, "id", styles, "");

	std::string text = xml_property (definition, "text", styles, "");
	boost::replace_all(text, "\\n", "\n");

	std::transform(widget_type.begin(), widget_type.end(), widget_type.begin(), ::toupper);

	if (widget_type == "BUTTON") {
		child = manage (new WavesButton(text));
	} else if (widget_type == "ICONBUTTON") {
		child = manage (new WavesIconButton);
	} else if (widget_type == "ICON") {
		std::string image_path;
		Searchpath spath(ARDOUR::ardour_data_search_path());

		spath.add_subdirectory_to_paths("icons");
    
		if (find_file_in_search_path (spath, 
									  xml_property (definition, "source", styles, ""),
									  image_path)) {
			Gtk::Image& icon = *manage (new Gtk::Image(image_path));
			child = &icon;
		} else {
			dbg_msg(xml_property (definition, "source", styles, "") + " NOT FOUND");
		}
	} else if (widget_type == "COMBOBOXTEXT") {
		child = manage (new Gtk::ComboBoxText);
	} else if (widget_type == "LABEL") {
		child = manage (new Gtk::Label(text));
	} else if (widget_type == "LAYOUT") {
		child = manage (new Gtk::Layout);
	} else if (widget_type == "CANVAS") {
		std::map<std::string, ArdourCanvas::Item*> named_items;
		child = manage (new ArdourCanvas::GtkCanvas(definition, styles, named_items));
	} else if (widget_type == "SCROLLEDWINDOW") {
		child = manage (new Gtk::ScrolledWindow);
	} else if (widget_type == "VBOX") {
		child = manage (new Gtk::VBox);
	} else if (widget_type == "HBOX") {
		child = manage (new Gtk::HBox);
	} else if (widget_type == "EVENTBOX") {
		child = manage (new Gtk::EventBox);
	} else if (widget_type == "FADER") {
		std::string face_image = xml_property (definition, "facesource", styles, "");
		if (face_image.empty()) {
			dbg_msg("Fader's facesource NOT SPECIFIED!");
			throw std::exception();
		}
		std::string handle_image = xml_property (definition, "handlesource", styles, "");
		if (handle_image.empty()) {
			dbg_msg("Fader's handlesource NOT SPECIFIED!");
			throw std::exception();
		}
		std::string active_handle_image = xml_property (definition, "activehandlesource", styles, handle_image);
		if (handle_image.empty()) {
			dbg_msg("Fader's handlesource NOT SPECIFIED!");
			throw std::exception();
		}
		std::string adjustment_id = xml_property (definition, "adjustment", styles, "");
		if (adjustment_id.empty()) {
			dbg_msg("Fader's adjustment_id NOT SPECIFIED!");
			throw std::exception();
		}
		int minposx = xml_property (definition, "minposx", styles, -1);
		int minposy = xml_property (definition, "minposy", styles, -1);
		int maxposx = xml_property (definition, "maxposx", styles, minposx);
		int maxposy = xml_property (definition, "maxposy", styles, minposy);
		Gtk::Adjustment& adjustment = get_adjustment(adjustment_id.c_str());
		child = manage (new Gtkmm2ext::Fader(adjustment, face_image, handle_image, active_handle_image, minposx, minposy, maxposx, maxposy));
	} else if (widget_type == "ADJUSTMENT") {
        //dbg_msg("Creating ADJUSTMENT");
		double min_value = xml_property (definition, "minvalue", styles, 0.0);
		double max_value = xml_property (definition, "maxvalue", styles, 100.0);
		double initial_value = xml_property (definition, "initialvalue", styles, min_value);
		double step = xml_property (definition, "step", styles, (max_value-min_value)/100.0);
		double page_increment = xml_property (definition, "pageincrement", styles, (max_value-min_value)/10);

		child = manage (new Gtk::Adjustment (initial_value,
											 min_value,
											 max_value,
											 step,
											 page_increment));
	}

	if (child != NULL) {
		if (!widget_id.empty()) {
			(*this)[widget_id] = child;
		}
		if (dynamic_cast<Gtk::Widget*>(child)) {
			set_attributes(*dynamic_cast<Gtk::Widget*>(child), definition, styles);
		}
	}
	return dynamic_cast<Gtk::Widget*>(child);
}


Gtk::Widget*
WavesUI::add_widget (Gtk::Box& parent, const XMLNode& definition, const XMLNodeMap& styles)
{
	Gtk::Widget* child = create_widget(definition, styles);

	if (child != NULL)
	{
		std::string property = xml_property (definition, "box.pack", styles, "start");
		bool expand = xml_property (definition, "box.expand", styles, false);
		bool fill = xml_property (definition, "box.fill", styles, false);
		uint32_t padding = xml_property (definition, "box.padding", styles, 0);
		
		if (property == "start") {
			parent.pack_start(*child, expand, fill, padding);
		} else {
			parent.pack_end(*child, expand, fill, padding);
		}

	}
	return child;
}


Gtk::Widget*
WavesUI::add_widget (Gtk::ScrolledWindow& parent, const XMLNode& definition, const XMLNodeMap& styles)
{
	Gtk::Widget* child = create_widget(definition, styles);

	if (child != NULL)
	{
		parent.add(*child);
	}
	return child;
}


Gtk::Widget*
WavesUI::add_widget (Gtk::Window& parent, const XMLNode& definition, const XMLNodeMap& styles)
{
	Gtk::Widget* child = create_widget(definition, styles);

	if (child != NULL)
	{
		parent.add(*child);
	}
	return child;
}


Gtk::Widget*
WavesUI::add_widget (Gtk::EventBox& parent, const XMLNode& definition, const XMLNodeMap& styles)
{
	Gtk::Widget* child = create_widget(definition, styles);

	if (child != NULL)
	{
		parent.add(*child);
	}
	return child;
}


Gtk::Widget*
WavesUI::add_widget (Gtk::Layout& parent, const XMLNode& definition, const XMLNodeMap& styles)
{
	Gtk::Widget* child = create_widget(definition, styles);

	if (child != NULL)
	{
		parent.put (*child, 
					xml_property (definition, "x", styles, 0), 
					xml_property (definition, "y", styles, 0));
	}
	return child;
}


Gtk::Widget*
WavesUI::add_widget (Gtk::Container& parent, const XMLNode& definition, const XMLNodeMap& styles)
{
	Gtk::Widget* child = NULL;
	if(dynamic_cast<Gtk::Layout*> (&parent)) {
		child = WavesUI::add_widget (*dynamic_cast<Gtk::Layout*> (&parent), definition, styles);
	} else if(dynamic_cast<Gtk::Box*> (&parent)) {
		child = WavesUI::add_widget (*dynamic_cast<Gtk::Box*> (&parent), definition, styles);
	} else if(dynamic_cast<Gtk::ScrolledWindow*> (&parent)) {
		child = WavesUI::add_widget (*dynamic_cast<Gtk::ScrolledWindow*> (&parent), definition, styles);
	} else if(dynamic_cast<Gtk::Window*> (&parent)) {
		child = WavesUI::add_widget (*dynamic_cast<Gtk::Window*> (&parent), definition, styles);
	} else if(dynamic_cast<Gtk::EventBox*> (&parent)) {
		child = WavesUI::add_widget (*dynamic_cast<Gtk::EventBox*> (&parent), definition, styles);
	}

	Gtk::Container* container = dynamic_cast<Gtk::Container*>(child);

	if (container != NULL) {
		WavesUI::create_ui (definition.children(), styles, *container);
		Gtk::ScrolledWindow* sw = dynamic_cast<Gtk::ScrolledWindow*>(child);
		if (sw != NULL) {
			Gtk::Viewport* vp = (Gtk::Viewport*)sw->get_child();
			if (vp != NULL) {
				set_attributes(*(Gtk::Widget*)vp, definition, styles);
				vp->set_shadow_type(Gtk::SHADOW_NONE);
			}
		}
	}
	return child;
}

void
WavesUI::create_ui (const XMLNodeList& definition, const XMLNodeMap& styles, Gtk::Container& root)
{
	for (XMLNodeList::const_iterator i = definition.begin(); i != definition.end(); ++i) {
		WavesUI::add_widget (root, **i, styles);
	}
}

void
WavesUI::create_ui (const XMLTree& layout, Gtk::Container& root)
{
	XMLNodeMap styles;
	get_styles(layout, styles);
	const XMLNodeList& definition = layout.root()->children();
	WavesUI::create_ui (definition, styles, root);
}

const XMLTree*
WavesUI::load_layout (const std::string xml_file_name)
{
	std::map<std::string, const XMLTree*>::const_iterator it = __xml_tree_cache.find(xml_file_name);
	if (it != __xml_tree_cache.end()) {
		return (*it).second;
	}

	std::string layout_file; 
	Searchpath spath (ardour_data_search_path());
	spath.add_subdirectory_to_paths("ui");

	if (!find_file_in_search_path (spath, xml_file_name, layout_file)) {
		dbg_msg("File not found: " + xml_file_name);
		return NULL;
	}

	const XMLTree* tree = new XMLTree (layout_file, false);
	__xml_tree_cache[xml_file_name] = tree;
	
	return tree;
}

void 
WavesUI::set_attributes (Gtk::Widget& widget, const XMLNode& definition, const XMLNodeMap& styles)
{
	int height = xml_property (definition, "height", styles, -1);
	int width = xml_property (definition, "width", styles, -1);
	widget.set_size_request (width, height);
		
	std::string property = xml_property (definition, "bgnormal", styles, "");
	if (!property.empty ()) {
		widget.unset_bg(Gtk::STATE_NORMAL);
		widget.modify_bg(Gtk::STATE_NORMAL, Gdk::Color(property));
	}

	property = xml_property (definition, "bgdisabled", styles, property);
	if (!property.empty ()) {
		widget.unset_bg(Gtk::STATE_INSENSITIVE);
		widget.modify_bg(Gtk::STATE_INSENSITIVE, Gdk::Color(property));
	}

	property = xml_property (definition, "bgactive", styles, "");
	if (!property.empty ()) {
		widget.unset_bg(Gtk::STATE_ACTIVE);
		widget.modify_bg(Gtk::STATE_ACTIVE, Gdk::Color(property));
	}

	property = xml_property (definition, "bghover", styles, "");
	if (!property.empty ()) {
		widget.unset_bg(Gtk::STATE_PRELIGHT);
		widget.modify_bg(Gtk::STATE_PRELIGHT, Gdk::Color(property));
	}

	property = xml_property (definition, "fgnormal", styles, "");
	if (!property.empty ()) {
		widget.modify_fg(Gtk::STATE_NORMAL, Gdk::Color(property));
	}

	property = xml_property (definition, "fgdisabled", styles, property);
	if (!property.empty ()) {
		widget.modify_fg(Gtk::STATE_INSENSITIVE, Gdk::Color(property));
	}

	property = xml_property (definition, "fgactive", styles, "");
	if (!property.empty ()) {
		widget.modify_fg(Gtk::STATE_ACTIVE, Gdk::Color(property));
	}

	property = xml_property (definition, "fghover", styles, "");
	if (!property.empty ()) {
		widget.modify_fg(Gtk::STATE_PRELIGHT, Gdk::Color(property));
	}

	property = xml_property (definition, "font", styles, "");
	if (!property.empty ()) {
		widget.modify_font(Pango::FontDescription(property));
	}

	if (xml_property (definition, "visible", styles, true)) {
		widget.show();
	} else {
		widget.hide();
	}

	property = xml_property (definition, "tooltip", styles, "");
	if (!property.empty ()) {
		widget.set_tooltip_text (property);
	}

	Gtk::Box* box = dynamic_cast<Gtk::Box*> (&widget);
	if (box) {
		box->set_spacing(xml_property (definition, "spacing", styles, 0));
	}

	Gtk::Container* container = dynamic_cast<Gtk::Container*> (&widget);
	if (container) {
		container->set_border_width (xml_property (definition, "borderwidth", styles, 0));
	}

	WavesButton* button = dynamic_cast<WavesButton*> (&widget);
	if (button) {
		button->set_border_width (xml_property (definition, "borderwidth", styles, "0").c_str());
		button->set_border_color (xml_property (definition, "bordercolor", styles, "#000000").c_str());
		button->set_active (xml_property (definition, "active", styles, false));
	}

	WavesIconButton* iconbutton = dynamic_cast<WavesIconButton*> (&widget);
	if (iconbutton) {
		property = xml_property (definition, "normalicon", styles, "");
		if (!property.empty ()) {
			iconbutton->set_normal_image(get_icon(property.c_str()));
		}
		property = xml_property (definition, "activeicon", styles, "");
		if (!property.empty ()) {
			iconbutton->set_active_image(get_icon(property.c_str()));
		}
		property = xml_property (definition, "prelighticon", styles, "");
		if (!property.empty ()) {
			iconbutton->set_prelight_image(get_icon(property.c_str()));
		}
		property = xml_property (definition, "inactiveicon", styles, "");
		if (!property.empty ()) {
			iconbutton->set_inactive_image(get_icon(property.c_str()));
		}
	}

	Gtk::ScrolledWindow* scrolled_window = dynamic_cast<Gtk::ScrolledWindow*> (&widget);

	if (scrolled_window) {
		Gtk::PolicyType hscrollbar_policy = Gtk::POLICY_AUTOMATIC; 
		Gtk::PolicyType vscrollbar_policy = Gtk::POLICY_AUTOMATIC;
		property = xml_property (definition, "hscroll", styles, "");
		if (property == "never") {
			hscrollbar_policy = Gtk::POLICY_NEVER;
		} else if (property == "always") {
			hscrollbar_policy = Gtk::POLICY_ALWAYS;
		} else if (property == "auto") {
			hscrollbar_policy = Gtk::POLICY_AUTOMATIC; 
		}
		property = xml_property (definition, "vscroll", styles, "");
		if (property == "never") {
			vscrollbar_policy = Gtk::POLICY_NEVER;
		} else if (property == "always") {
			vscrollbar_policy = Gtk::POLICY_ALWAYS;
		} else if (property == "auto") {
			vscrollbar_policy = Gtk::POLICY_AUTOMATIC; 
		}
		scrolled_window->set_policy(hscrollbar_policy, vscrollbar_policy);
	}
}

Gtk::Object* 
WavesUI::get_object(const char *id)
{
	Gtk::Object* object = NULL;
	WavesUI::iterator it = find(id);
	if(it != end())
		object = it->second;

	return object;
}

Gtk::Adjustment&
WavesUI::get_adjustment(const char* id)
{
	Gtk::Adjustment* child = dynamic_cast<Gtk::Adjustment*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("Adjustment ") + id + " not found !");
		throw std::exception();
	}
	return *child;
}

Gtk::VBox&
WavesUI::get_v_box (const char* id)
{
	Gtk::VBox* child = dynamic_cast<Gtk::VBox*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("Gtk::VBox ") + id + " not found !");
		throw std::exception();
	}
	return *child;
}


Gtk::HBox&
WavesUI::get_h_box (const char* id)
{
	Gtk::HBox* child = dynamic_cast<Gtk::HBox*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("Gtk::HBox ") + id + " not found !");
		throw std::exception();
	}
	return *child;
}


Gtk::Layout&
WavesUI::get_layout (const char* id)
{
	Gtk::Layout* child = dynamic_cast<Gtk::Layout*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("Gtk::Layout ") + id + " not found !");
		throw std::exception();
	}
	return *child;
}


Gtk::Label&
WavesUI::get_label (const char* id)
{
	Gtk::Label* child = dynamic_cast<Gtk::Label*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("Gtk::Label ") + id + " not found !");
		throw std::exception();
	}
	return *child;
}


Gtk::ComboBoxText&
WavesUI::get_combo_box_text (const char* id)
{
	Gtk::ComboBoxText* child = dynamic_cast<Gtk::ComboBoxText*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("Gtk::ComboBoxText ") + id + " not found !");
		throw std::exception();
	}
	return *child;
}


WavesButton&
WavesUI::get_waves_button (const char* id)
{
	WavesButton* child = dynamic_cast<WavesButton*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("WavesButton ") + id + " not found !");
		throw std::exception();
	}
	return *child;
}

Gtkmm2ext::Fader&
WavesUI::get_fader (const char* id)
{
	Gtkmm2ext::Fader* child = dynamic_cast<Gtkmm2ext::Fader*> (get_object(id));
	if (child == NULL ) {
		dbg_msg (std::string("Gtkmm2ext::Fader ") + id + " not found !");
		throw std::exception();
	}
	return *child;
}

