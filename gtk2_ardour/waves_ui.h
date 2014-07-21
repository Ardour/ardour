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
#include <gtkmm/adjustment.h>
#include <gtkmm/box.h>
#include <gtkmm/layout.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/scrolledwindow.h>
#include "gtkmm2ext/fader.h"
#include "gtkmm2ext/focus_entry.h"
#include "canvas/canvas.h"
#include "canvas/xml_ui.h"
#include "waves_button.h"
#include "waves_icon_button.h"

using namespace ArdourCanvas::XMLUI;
class WavesUI : public std::map<std::string, Gtk::Object*> {

  public:
	WavesUI (const std::string& layout_script_file, Gtk::Container& root);
	virtual ~WavesUI () {}

	Gtk::Adjustment& get_adjustment (const char* id);
	Gtk::Container& get_container (const char* id);
	Gtk::EventBox& get_event_box (const char* id);
	Gtk::Box& get_box (const char* id);
	Gtk::VBox& get_v_box (const char* id);
	Gtk::HBox& get_h_box (const char* id);
	Gtk::Layout& get_layout (const char* id);
	Gtk::Label& get_label (const char* id);
	Gtk::Image& get_image (const char* id);
	Gtk::ComboBoxText& get_combo_box_text (const char* id);
	Gtk::Entry& get_entry (const char* id);
	Gtkmm2ext::FocusEntry& get_focus_entry (const char* id);
    Gtk::SpinButton& get_spin_button (const char* id);
	WavesButton& get_waves_button (const char* id);
	Gtkmm2ext::Fader& get_fader (const char* id);
	const XMLTree* xml_tree () { return _xml_tree; }
	Gtk::Container& root () { return _root_container; }

  protected:
	void set_attributes (Gtk::Widget& widget, const XMLNode& definition, const XMLNodeMap& styles);

  private:
	static std::map<std::string, const XMLTree*> __xml_tree_cache;
	const XMLTree* _xml_tree;
	const std::string _scrip_file_name;
	Gtk::Container& _root_container;

	Gtk::Object* get_object (const char *id);
	const XMLTree* load_layout (const std::string& xml_file_name);
	void create_ui (const XMLTree& layout, Gtk::Container& root);
	void create_ui (const XMLNodeList& definition, const XMLNodeMap& styles, Gtk::Container& root);
	Gtk::Widget* create_widget (const XMLNode& definition, const XMLNodeMap& styles);
	Gtk::Widget* add_widget (Gtk::Box& parent, const XMLNode& definition, const XMLNodeMap& styles);
	Gtk::Widget* add_widget (Gtk::ScrolledWindow& parent, const XMLNode& definition, const XMLNodeMap& styles);
	Gtk::Widget* add_widget (Gtk::Window& parent, const XMLNode& definition, const XMLNodeMap& styles);
	Gtk::Widget* add_widget (Gtk::Layout& parent, const XMLNode& definition, const XMLNodeMap& styles);
	Gtk::Widget* add_widget (Gtk::Container& parent, const XMLNode& definition, const XMLNodeMap& styles);
	Gtk::Widget* add_widget (Gtk::EventBox& parent, const XMLNode& definition, const XMLNodeMap& styles);
};

#endif //__WAVES_UI_H__