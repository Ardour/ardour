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
#include <stdlib.h>
#include <string>
#include <stdio.h>
#include "ardour/location.h"
#include "ardour/midi_scene_change.h"

#include "marker.h"
#include "marker_inspector_dialog.h"

#include "dbg_msg.h"

void
MarkerInspectorDialog::_init ()
{
	set_type_hint (Gdk::WINDOW_TYPE_HINT_UTILITY);
	_color_palette_button.signal_clicked.connect (sigc::mem_fun (*this, &MarkerInspectorDialog::_color_palette_button_clicked));
	_info_panel_button.signal_clicked.connect (sigc::mem_fun (*this, &MarkerInspectorDialog::_info_panel_button_clicked));
	_program_change_button.signal_clicked.connect (sigc::mem_fun (*this, &MarkerInspectorDialog::_program_change_button_clicked));
}

void MarkerInspectorDialog::set_marker (Marker* marker)
{
	_empty_panel.set_visible (!marker);
	_inspector_panel.set_visible (marker);
	_marker = marker;
	if (_marker && marker->location()) {
		boost::shared_ptr<ARDOUR::MIDISceneChange> msc = boost::dynamic_pointer_cast<ARDOUR::MIDISceneChange> (marker->location ()->scene_change ());
        _program_change_button.set_active (msc && msc->active ());
		_location_name.set_text (_marker->name ());
	}
}

void
MarkerInspectorDialog::_color_palette_button_clicked (WavesButton *button)
{
	_color_buttons_home.set_visible (_color_palette_button.active_state () == Gtkmm2ext::ExplicitActive);
}

void
MarkerInspectorDialog::_info_panel_button_clicked (WavesButton *button)
{
	_info_panel_home.set_visible (_info_panel_button.active_state () == Gtkmm2ext::ExplicitActive);
}

void
MarkerInspectorDialog::_program_change_button_clicked (WavesButton *button)
{
	if (_marker && _marker->location()) {
		boost::shared_ptr<ARDOUR::SceneChange> sc = _marker->location ()->scene_change ();
		if (sc) {
			boost::shared_ptr<ARDOUR::MIDISceneChange> msc = boost::dynamic_pointer_cast<ARDOUR::MIDISceneChange> (sc);
			if (msc) {
				msc->set_active (_program_change_button.active_state () == Gtkmm2ext::ExplicitActive);
			}
		} else if (_program_change_button.active_state () == Gtkmm2ext::ExplicitActive ) {
			_marker->location()->set_scene_change(boost::shared_ptr<ARDOUR::MIDISceneChange> (new ARDOUR::MIDISceneChange (1, -1, 1)));
		}
	}
}
