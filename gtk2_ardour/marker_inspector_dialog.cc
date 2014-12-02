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
#include "marker_inspector_dialog.h"
#include "waves_button.h"
#include "i18n.h"

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;
using namespace Glib;

MarkerInspectorDialog::MarkerInspectorDialog ()
	: WavesUI ("marker_inspector_dialog.xml", *this)
	, _empty_panel (get_container ("empty_panel"))
	, _inspector_panel (get_container ("inspector_panel"))
	, _color_palette_button (get_waves_button ("color_palette_button"))
	, _color_buttons_home (get_container ("color_buttons_home"))
	, _info_panel_button (get_waves_button ("info_panel_button"))
	, _info_panel_home (get_container ("info_panel_home"))
	, _location_name (get_label ("location_name"))
	, _program_change_button (get_waves_button ("program_change_button"))
	, _marker (0)
{
	_init ();
}

MarkerInspectorDialog::~MarkerInspectorDialog ()
{
}

void
MarkerInspectorDialog::on_realize ()
{
	Gtk::Window::on_realize();
	get_window()->set_decorations (Gdk::WMDecoration (Gdk::DECOR_BORDER|Gdk::DECOR_TITLE));
}
