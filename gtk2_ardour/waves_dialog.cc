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
#include "waves_ui.h"

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

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;
using namespace ARDOUR;

std::ofstream dbg_out("/users/VKamyshniy/WavesDialogLog.txt");

WavesDialog::WavesDialog (std::string layout_script_file, bool modal, bool use_seperator)
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

	std::string title = WavesUI::xml_property (*root, "title", WavesUI::XMLNodeMap(), "");
	set_title(title);
	bool resizeable = WavesUI::xml_property (*root, "resizeable", WavesUI::XMLNodeMap(), false);
	property_allow_grow().set_value(resizeable);

	set_border_width(0);

	WavesUI::create_ui(layout, *get_vbox(), _children);

	return true;
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
