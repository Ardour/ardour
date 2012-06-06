/*
    Copyright (C) 2012 Paul Davis

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

#include <gtkmm.h>
#include "panner_editor.h"

class StereoPanner;

/** Editor dialog for the stereo panner */
class StereoPannerEditor : public PannerEditor
{
public:
	StereoPannerEditor (StereoPanner *);

private:
	void panner_going_away ();
	void update_editor ();
	void position_changed ();
	void width_changed ();
	void set_position_range ();
	void set_width_range ();
	
	StereoPanner* _panner;
	Gtk::SpinButton _position;
	Gtk::SpinButton _width;
	bool _ignore_changes;

	PBD::ScopedConnectionList _connections;
};
