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

#ifndef __WAVES_UI_H__
#define __WAVES_UI_H__

#include <string>
#include <fstream>
#include <boost/algorithm/string.hpp>
#include <gtkmm/box.h>
#include <gtkmm/layout.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/scrolledwindow.h>
#include "canvas/canvas.h"
#include "canvas/xml_ui.h"
#include "waves_button.h"
#include "waves_icon_button.h"

using namespace ArdourCanvas::XMLUI;
namespace WavesUI {

	class WidgetMap : public std::map<std::string, Gtk::Widget*>
	{
	  public:
		Gtk::VBox& get_vbox (char* id);
		Gtk::HBox& get_hbox (char* id);
		Gtk::Layout& get_layout (char* id);
		Gtk::Label& get_label (char* id);
		Gtk::Image& get_image (char* id);
		Gtk::ComboBoxText& get_combo_box_text (char* id);
		WavesButton& get_waves_button (char* id);
	  private:
		Gtk::Widget* get_widget(char *id);
	};

	const XMLTree* load_layout (const std::string xml_file_name);
	void create_ui (const XMLTree& layout, Gtk::Container& root, std::map<std::string, Gtk::Widget*>& named_widgets);
	void create_ui (const XMLNodeList& definition, const XMLNodeMap& styles, Gtk::Container& root, std::map<std::string, Gtk::Widget*>& named_widgets);
	Gtk::Widget* create_widget (const XMLNode& definition, const XMLNodeMap& styles, std::map<std::string, Gtk::Widget*>& named_widgets);
	Gtk::Widget* add_widget (Gtk::Box& parent, const XMLNode& definition, const XMLNodeMap& styles, std::map<std::string, Gtk::Widget*>& named_widgets);
	Gtk::Widget* add_widget (Gtk::ScrolledWindow& parent, const XMLNode& definition, const XMLNodeMap& styles, std::map<std::string, Gtk::Widget*>& named_widgets);
	Gtk::Widget* add_widget (Gtk::Window& parent, const XMLNode& definition, const XMLNodeMap& styles, std::map<std::string, Gtk::Widget*>& named_widgets);
	Gtk::Widget* add_widget (Gtk::Layout& parent, const XMLNode& definition, const XMLNodeMap& styles, std::map<std::string, Gtk::Widget*>& named_widgets);
	Gtk::Widget* add_widget (Gtk::Container& parent, const XMLNode& definition, const XMLNodeMap& styles, std::map<std::string, Gtk::Widget*>& named_widgets);
	void set_attributes (Gtk::Widget& widget, const XMLNode& definition, const XMLNodeMap& styles);

}

#endif //__WAVES_UI_H__