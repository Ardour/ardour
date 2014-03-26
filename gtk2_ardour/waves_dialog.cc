/*
    Copyright (C) 2002 Paul Davis

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

#include <iostream>
#include <fstream>
#include <sigc++/bind.h>
#include <boost/algorithm/string.hpp>
#include "pbd/xml++.h"

#include <gtkmm2ext/doi.h>

#include "waves_dialog.h"
#include "waves_button.h"
#include "ardour/filesystem_paths.h"
#include "pbd/file_utils.h"
#include "i18n.h"

#include "ardour_ui.h"
#include "keyboard.h"
#include "splash.h"
#include "utils.h"
#include "window_manager.h"

#if defined(_WIN32)
    #define strcasecmp _stricmp
#endif

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;
using namespace ARDOUR;

std::ofstream dbg_out("/users/VKamyshniy/WavesDialogLog.txt");

WavesDialog::WavesDialog (string layout_script_file, bool modal, bool use_seperator)
	: Dialog ("", modal, use_seperator)
	, proxy (0)
    , _splash_pushed (false)
{
	list<Glib::RefPtr<Gdk::Pixbuf> > window_icons;
	Glib::RefPtr<Gdk::Pixbuf> icon;
	
	if ((icon = ::get_icon ("ardour_icon_16px")) != 0) {
		window_icons.push_back (icon);
	}
	if ((icon = ::get_icon ("ardour_icon_22px")) != 0) {
		window_icons.push_back (icon);
	}
	if ((icon = ::get_icon ("ardour_icon_32px")) != 0) {
		window_icons.push_back (icon);
	}
	if ((icon = ::get_icon ("ardour_icon_48px")) != 0) {
		window_icons.push_back (icon);
	}
	if (!window_icons.empty ()) {
		set_default_icon_list (window_icons);
	}

	set_border_width (0);

	set_type_hint (Gdk::WINDOW_TYPE_HINT_DIALOG);

	Gtk::Window* parent_window = WM::Manager::instance().transient_parent();

	if (parent_window) {
		set_transient_for (*parent_window);
	}

	ARDOUR_UI::CloseAllDialogs.connect (sigc::bind (sigc::mem_fun (*this, &WavesDialog::response), RESPONSE_CANCEL));

	proxy = new WM::ProxyTemporary (get_title(), this);
	WM::Manager::instance().register_window (proxy);

	get_vbox()->set_spacing (0);
	get_vbox()->set_border_width (0);
	get_vbox()->pack_start (parent, false, false);

	read_layout(layout_script_file);
	set_position (Gtk::WIN_POS_MOUSE);
}

WavesDialog::~WavesDialog ()
{
    if (_splash_pushed) {
            Splash* spl = Splash::instance();
                
            if (spl) {
                    spl->pop_front();
            }
    }
	WM::Manager::instance().remove (proxy);
}

bool
WavesDialog::on_key_press_event (GdkEventKey* ev)
{
	return relay_key_press (ev, this);
}

bool
WavesDialog::on_enter_notify_event (GdkEventCrossing *ev)
{
	Keyboard::the_keyboard().enter_window (ev, this);
	return Dialog::on_enter_notify_event (ev);
}

bool
WavesDialog::on_leave_notify_event (GdkEventCrossing *ev)
{
	Keyboard::the_keyboard().leave_window (ev, this);
	return Dialog::on_leave_notify_event (ev);
}

void
WavesDialog::on_unmap ()
{
	Keyboard::the_keyboard().leave_window (0, this);
	Dialog::on_unmap ();
}

void
WavesDialog::on_show ()
{
	Dialog::on_show ();

	// never allow the splash screen to obscure any dialog
	Splash* spl = Splash::instance();

	if (spl && spl->is_visible()) {
		spl->pop_back_for (*this);
                _splash_pushed = true;
	}
}

bool
WavesDialog::on_delete_event (GdkEventAny*)
{
	hide ();
	return false;
}


// Layout

void
WavesDialog::set_layout_size (int width, int height)
{
	parent.set_size_request (width, height);
}

Gtk::Widget*
WavesDialog::create_widget (const XMLNode& definition)
{
	Gtk::Widget* child = NULL;
	std::string widget_type = definition.name();
	std::string widget_id = xml_property (definition, "id", "");
	string style = xml_property (definition, "style", "");

	string text = xml_property (definition, "text", "");
	boost::replace_all(text, "\\n", "\n");

	int height = xml_property (definition, "height", -1);
	int width = xml_property (definition, "width", -1);

	std::transform(widget_type.begin(), widget_type.end(), widget_type.begin(), ::toupper);
	if (widget_type == "BUTTON") {
		WavesButton::Element visual_element = WavesButton::default_elements;
		bool flat = xml_property(definition, "flat", false);
		if (flat) {
			dbg_out << "button [" << text << "] is flat" << std::endl;
			visual_element = WavesButton::Element (visual_element | WavesButton::FlatFace);
		}
		child = manage (new WavesButton(text, visual_element));
	} else if (widget_type == "COMBOBOXTEXT") {
		child = manage (new Gtk::ComboBoxText);
	} else if (widget_type == "LABEL") {
		child = manage (new Gtk::Label(text));
	} else if (widget_type == "LAYOUT") {
		child = manage (new Gtk::Layout);
		string color = xml_property (definition, "bgcolor", "");
		if (!color.empty()) {
			child->modify_bg(Gtk::STATE_NORMAL, Gdk::Color(color));
		}
	}

	if (child != NULL) {
		if (!style.empty()) {
			child->set_name (style);
		}
		child->set_size_request (width, height);
		if (!widget_id.empty())
		{
			_children[widget_id] = child;
		}
	}
	return child;
}

Gtk::Widget*
WavesDialog::add_widget (Gtk::Layout& parent, const XMLNode& definition)
{
	Gtk::Widget* child = create_widget(definition);

	if (child != NULL)
	{
		parent.put (*child, 
					xml_property (definition, "x", 0), 
					xml_property (definition, "y", 0));
	}
	return child;
}


Gtk::Widget*
WavesDialog::add_widget (Gtk::Widget& parent, const XMLNode &definition)
{
	Gtk::Widget* child = NULL;
	if(dynamic_cast<Gtk::Layout*> (&parent)) {
		child = add_widget (*dynamic_cast<Gtk::Layout*> (&parent), definition);
	}
	if (child != NULL)
	{
		const XMLNodeList& children = definition.children();
		for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
			add_widget(*child, **i);
		}
	}
	return child;
}

bool
WavesDialog::read_layout (std::string file_name)
{
	std::string layout_file; 
	Searchpath spath (ardour_data_search_path());
	spath.add_subdirectory_to_paths("ui");

	if (!find_file_in_search_path (spath, file_name, layout_file)) {
		return false;
	}

	XMLTree layout(layout_file, false);
	XMLNode* root  = layout.root();
	if ((root == NULL) || strcasecmp(root->name().c_str(), "dialog")) {
		return false;
	}

	string title = xml_property (*root, "title", "");
	set_title(title);

	int height = xml_property(*root, "height",  -1);
	int width = xml_property(*root, "width", -1);

	set_layout_size (width, height);

	const XMLNodeList& children = root->children(); 
	for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
		add_widget ((Gtk::Widget&)parent, **i);
	}

	return true;
}


Gtk::Label&
WavesDialog::add_label(const std::string& label, int x, int y, int width, int height)
{
	Gtk::Label& l = *manage (new Gtk::Label(label));
	parent.put(l, x, y);
	l.set_size_request(width, height);
	return l;
}


WavesButton&
WavesDialog::add_button (const std::string& label, const std::string name, int x, int y, int width, int height)
{
	WavesButton& ab = *manage (new WavesButton(label));
	ab.set_name (name);
	parent.put(ab, x, y);
	ab.set_size_request(width, height);
	return ab;
}


Gtk::ComboBoxText&
WavesDialog::add_combo_box_text (int x, int y, int width, int height)
{
}


Gtk::Widget*
WavesDialog::get_widget(char *id)
{
	Gtk::Widget *child = NULL;
	std::map<std::string, Gtk::Widget*>::iterator it = _children.find(id);
	if(it != _children.end())
		child = it->second;

	return child;
}

Gtk::Layout&
WavesDialog::get_layout (char* id)
{
	Gtk::Layout* child = dynamic_cast<Gtk::Layout*> (get_widget(id));
	if (child == NULL ) {
		throw exception();
	}
	return *child;
}

Gtk::Label&
WavesDialog::get_label (char* id)
{
	Gtk::Label* child = dynamic_cast<Gtk::Label*> (get_widget(id));
	if (child == NULL ) {
		throw exception();
	}
	return *child;
}


Gtk::ComboBoxText&
WavesDialog::get_combo_box_text (char* id)
{
	Gtk::ComboBoxText* child = dynamic_cast<Gtk::ComboBoxText*> (get_widget(id));
	if (child == NULL ) {
		throw exception();
	}
	return *child;
}


WavesButton&
WavesDialog::get_waves_button (char* id)
{
	WavesButton* child = dynamic_cast<WavesButton*> (get_widget(id));
	if (child == NULL ) {
		throw exception();
	}
	return *child;
}

double 
WavesDialog::xml_property (const XMLNode &node, const char *prop_name, double default_value)
{
	return node.property (prop_name) ? atof(node.property(prop_name)->value()) : default_value;
}

int
WavesDialog::xml_property (const XMLNode &node, const char *prop_name, int default_value)
{
	return node.property (prop_name) ? atoi(node.property(prop_name)->value()) : default_value;
}

bool
WavesDialog::xml_property (const XMLNode &node, const char *prop_name, bool default_value)
{
	std::string property = node.property (prop_name) ? node.property(prop_name)->value() : std::string("");
	if (property.empty())
		return default_value;
	std::transform(property.begin(), property.end(), property.begin(), ::toupper);
	return property == "TRUE";
}

std::string
WavesDialog::xml_property (const XMLNode &node, const char *prop_name, const std::string default_value)
{
	return node.property (prop_name) ? node.property(prop_name)->value() : default_value;
}
