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

#ifndef __gtk2_route_inspector_h__
#define __gtk2_route_inspector_h__

#include "mixer_strip.h"

class RouteInspector : public MixerStrip
{
  public:
	RouteInspector (ARDOUR::Session*, boost::shared_ptr<ARDOUR::Route>, const std::string& layout_script_file, size_t max_name_size = 0);
	RouteInspector (ARDOUR::Session*, const std::string& layout_script_file, size_t max_name_size = 0);
	~RouteInspector ();

	void set_route (boost::shared_ptr<ARDOUR::Route>);

  private:
	void init ();
	void update_inspector_info_panel ();

	WavesButton&   color_palette_button;
	Gtk::Container& color_palette_home;
	Gtk::Container& color_palette_button_home;
	Gtk::Container& color_buttons_home;

	WavesButton&   info_panel_button;
	Gtk::Widget& info_panel_home;
	Gtk::Label& input_info_label;
	Gtk::Label& output_info_label;
    
    void update_info_panel ();
	WavesButton* color_button[15];

    PBD::ScopedConnectionList _input_output_channels_update;
    
	void route_color_changed ();
	void color_button_clicked (WavesButton *button);
	void color_palette_button_clicked (WavesButton *button);
	void info_panel_button_clicked (WavesButton *button);
};

#endif /* __gtk2_route_inspector_h__ */

// How to see comment_area on the screen?
