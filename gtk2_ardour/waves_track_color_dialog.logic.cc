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

#include "gui_thread.h"
#include "ardour_ui.h"
#include "public_editor.h"
#include "waves_track_color_dialog.h"
#include "i18n.h"
#include "dbg_msg.h"

void
WavesTrackColorDialog::set_route (boost::shared_ptr<ARDOUR::Route> route)
{
    if ((_route != route) && !_deletion_in_progress) {
		_route_connections.drop_connections ();
		_route = route;
		_color_buttons_home.set_visible(_route);
		_empty_panel.set_visible(!_route);
		if (_route) {
			_route->gui_changed.connect (_route_connections, 
										 invalidator (*this), 
										 boost::bind (&WavesTrackColorDialog::_on_route_gui_changed, this, _1), gui_context ());
			_route_color_changed();
		}
	}
}

void
WavesTrackColorDialog::_init ()
{
	set_type_hint (Gdk::WINDOW_TYPE_HINT_UTILITY);
	set_resizable(false);
	for (size_t i=0; i<(sizeof(color_button)/sizeof(color_button[0])); i++) {
		color_button[i]->signal_clicked.connect (sigc::mem_fun (*this, &WavesTrackColorDialog::_on_color_button_clicked));
	}
}

void
WavesTrackColorDialog::_on_color_button_clicked (WavesButton *button)
{
	button->set_active (true);
	for (size_t i=0; i<(sizeof(color_button)/sizeof(color_button[0])); i++) {
		if (button != color_button[i]) {
			color_button[i]->set_active (false);
		} else {
			TrackSelection& track_selection =  ARDOUR_UI::instance()->the_editor().get_selection().tracks;
            track_selection.foreach_route_ui (boost::bind (&RouteUI::set_color, _1, Gdk::Color (RouteUI::XMLColor[i])));
		}
	}
}

void
WavesTrackColorDialog::_on_route_gui_changed (std::string what_changed)
{
	if (what_changed == "color") {
		_route_color_changed ();
	}
}

void
WavesTrackColorDialog::_route_color_changed ()
{
	if (_route) {
		std::string route_state_id (string_compose (X_("route %1"), _route->id().to_s()));
		const std::string str = AxisView::gui_object_state().get_string (route_state_id, X_("color"));

		int r(0);
		int g(0);
		int b(0);

		if (!str.empty ()) {
			sscanf (str.c_str(), "%d:%d:%d", &r, &g, &b);
		}

		Gdk::Color new_color;
		new_color.set_rgb (r, g, b);
		for (size_t i=0; i<(sizeof(color_button)/sizeof(color_button[0])); i++) {
			color_button[i]->set_active (new_color == Gdk::Color (RouteUI::XMLColor[i]));
		}
	}
}

void
WavesTrackColorDialog::reset_route ()
{
    _route_connections.drop_connections ();
    _route.reset ();
    _color_buttons_home.set_visible (false);
    _empty_panel.set_visible (true);
}
