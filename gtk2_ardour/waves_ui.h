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

#ifndef __WAVES_UI_H__
#define __WAVES_UI_H__

#include <string>
#include <fstream>
#include <boost/algorithm/string.hpp>
#include "gtkmm/box.h"
#include "gtkmm/layout.h"
#include "gtkmm/label.h"
#include "canvas/xml_ui.h"

using namespace ArdourCanvas::XMLUI;
namespace WavesUI {

	void create_ui (const XMLTree& layout, Gtk::Widget& root, std::map<std::string, Gtk::Widget*> &named_widgets);
	void create_ui (const XMLNodeList& definition, const XMLNodeMap& styles, Gtk::Widget& root, std::map<std::string, Gtk::Widget*> &named_widgets);
	Gtk::Widget* create_widget (const XMLNode& definition, const XMLNodeMap& styles, std::map<std::string, Gtk::Widget*> &named_widgets);
	Gtk::Widget* add_widget (Gtk::Box& parent, const XMLNode &definition, const XMLNodeMap& styles, std::map<std::string, Gtk::Widget*> &named_widgets);
	Gtk::Widget* add_widget (Gtk::Layout& parent, const XMLNode &definition, const XMLNodeMap& styles, std::map<std::string, Gtk::Widget*> &named_widgets);
	Gtk::Widget* add_widget (Gtk::Widget& parent, const XMLNode &definition, const XMLNodeMap& styles, std::map<std::string, Gtk::Widget*> &named_widgets);

}

#endif //__WAVES_UI_H__