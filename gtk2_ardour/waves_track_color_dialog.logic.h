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

//class WavesTrackColorDialog : public Gtk::Window
//{

public:
	void set_route (boost::shared_ptr<ARDOUR::Route> _route);
    void reset_route ();

private:

    PBD::ScopedConnectionList _route_connections;
	boost::shared_ptr<ARDOUR::Route> _route;
    bool _deletion_in_progress;

	void _route_color_changed ();
	void _on_color_button_clicked (WavesButton *button);
	void _on_route_gui_changed (std::string what_changed);
	
//};
