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

using namespace Gtkmm2ext;
using namespace PBD;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;

WavesDialog::WavesDialog (const std::string& layout_script_file, bool modal, bool use_seperator)
	: Gtk::Dialog ("", modal, use_seperator)
	, WavesUI (layout_script_file, *get_vbox())
	, _proxy (0)
    , _splash_pushed (false)
{
	std::list<Glib::RefPtr<Gdk::Pixbuf> > window_icons;
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

	ARDOUR_UI::CloseAllDialogs.connect (sigc::bind (sigc::mem_fun (*this, &WavesDialog::response), Gtk::RESPONSE_CANCEL));

	_proxy = new WM::ProxyTemporary (get_title(), this);
	WM::Manager::instance().register_window (_proxy);

	get_vbox()->set_spacing (0);
	get_vbox()->set_border_width (0);

	XMLNode* root  = xml_tree()->root();
	std::string title = xml_property (*root, "title", "");
	set_title(title);
	
	bool resizeable = xml_property (*root, "resizeable", false);
	set_resizable(resizeable);

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
	WM::Manager::instance().remove (_proxy);
}

void
WavesDialog::on_realize ()
{
	Gtk::Dialog::on_realize();
	get_window()->set_decorations (Gdk::WMDecoration (Gdk::DECOR_BORDER|Gdk::DECOR_TITLE));
}

bool
WavesDialog::on_key_press_event (GdkEventKey* ev)
{
    switch (ev->keyval)
    {
        case GDK_Return:
            hide ();
            response (WavesDialog::RESPONSE_DEFAULT);
            return true;
        case GDK_Escape:
            hide ();
            response (Gtk::RESPONSE_CANCEL);
            return true;
    }
    
	return Gtk::Dialog::on_key_press_event (ev);
}

void
WavesDialog::on_show ()
{
	Dialog::on_show ();
	ARDOUR_UI::instance ()->hide_splash ();
}


bool
WavesDialog::on_delete_event (GdkEventAny*)
{
	hide ();
	return false;
}
