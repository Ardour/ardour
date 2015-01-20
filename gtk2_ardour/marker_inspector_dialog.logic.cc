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
#include "public_editor.h"
#include "ardour/midi_scene_change.h"

#include "main_clock.h"
#include "marker.h"
#include "marker_inspector_dialog.h"
#include "ardour_ui.h"

#include "gui_thread.h"
#include "dbg_msg.h"

void MarkerInspectorDialog::set_marker (Marker* marker)
{
	_empty_panel.set_visible (!marker);
	_inspector_panel.set_visible (marker);
	_marker = marker;
	if (_marker && _marker->location()) {
		boost::shared_ptr<ARDOUR::MIDISceneChange> msc = boost::dynamic_pointer_cast<ARDOUR::MIDISceneChange> (_marker->location ()->scene_change ());
		_marker->location()->LockChanged.connect (_marker_connections, invalidator (*this), boost::bind (&MarkerInspectorDialog::_display_marker_data, this), gui_context());
		_marker->location()->NameChanged.connect (_marker_connections, invalidator (*this), boost::bind (&MarkerInspectorDialog::_display_marker_data, this), gui_context());
		_marker->location()->StartChanged.connect (_marker_connections, invalidator (*this), boost::bind (&MarkerInspectorDialog::_display_marker_data, this), gui_context());		
		_display_marker_data ();
	}
}

void
MarkerInspectorDialog::_display_marker_data ()
{
	if (_marker && _marker->location()) {
		boost::shared_ptr<ARDOUR::MIDISceneChange> msc = boost::dynamic_pointer_cast<ARDOUR::MIDISceneChange> (_marker->location ()->scene_change ());
		_display_scene_change_info ();
		_location_name_label.set_text (_marker->location()->name ());
		_location_time_label.set_text (ARDOUR_UI::instance()->format_session_time(_marker->location()->start()));
		_lock_button.set_active (_marker->location()->locked ());
		_enable_program_change (msc && msc->active ());
	}
}

void 
MarkerInspectorDialog::_on_location_changed (ARDOUR::Location*)
{
	_display_marker_data ();
}

void
MarkerInspectorDialog::_init ()
{
	set_type_hint (Gdk::WINDOW_TYPE_HINT_UTILITY);
	set_resizable(false);
	_lock_button.signal_clicked.connect (sigc::mem_fun (*this, &MarkerInspectorDialog::_lock_button_clicked));
	_program_change_on_button.signal_clicked.connect (sigc::mem_fun (*this, &MarkerInspectorDialog::_program_change_on_button_clicked));
	_program_change_off_button.signal_clicked.connect (sigc::mem_fun (*this, &MarkerInspectorDialog::_program_change_off_button_clicked));
	_bank_dropdown.selected_item_changed.connect (mem_fun(*this, &MarkerInspectorDialog::on_bank_dropdown_item_changed ));
	_program_dropdown.selected_item_changed.connect (mem_fun(*this, &MarkerInspectorDialog::on_program_dropdown_item_changed ));
	_channel_dropdown.selected_item_changed.connect (mem_fun(*this, &MarkerInspectorDialog::on_channel_dropdown_item_changed ));
	ARDOUR_UI::instance()->primary_clock->ModeChanged.connect(sigc::mem_fun (*this, &MarkerInspectorDialog::_display_marker_data));
}

void
MarkerInspectorDialog::_display_scene_change_info ()
{
	boost::shared_ptr<ARDOUR::MIDISceneChange> msc = boost::dynamic_pointer_cast<ARDOUR::MIDISceneChange> (_marker->location ()->scene_change ());
	if (msc) {
		_bank_dropdown.set_current_item (msc->bank () + 1);
		_program_dropdown.set_current_item (msc->program ());
		_channel_dropdown.set_current_item (msc->channel () - 1);
	}
}

void
MarkerInspectorDialog::_enable_program_change (bool yn)
{
	_program_change_on_button.set_active (yn);
	_program_change_off_button.set_active (!yn);
	_program_change_info_panel.set_visible (yn);

	if (_marker && _marker->location()) {
		boost::shared_ptr<ARDOUR::SceneChange> sc = _marker->location ()->scene_change ();
		if (sc) {
			boost::shared_ptr<ARDOUR::MIDISceneChange> msc = boost::dynamic_pointer_cast<ARDOUR::MIDISceneChange> (sc);
			if (msc && (msc->active () != yn)) {
				msc->set_active (yn);
                _set_session_dirty ();
			}
		} else if (yn) {
			_marker->location()->set_scene_change(boost::shared_ptr<ARDOUR::MIDISceneChange> (new ARDOUR::MIDISceneChange (1, -1, 1)));
			_display_scene_change_info ();
            _set_session_dirty ();
		}
	}
}

void
MarkerInspectorDialog::_set_session_dirty ()
{
    ARDOUR_UI::instance()->set_session_dirty ();
}


void
MarkerInspectorDialog::_lock_button_clicked (WavesButton *button)
{
	if (_marker && _marker->location()) {
		if (_marker->location()->locked ()) {
			_marker->location()->unlock ();
		} else {
			_marker->location()->lock ();
		}
	}
}

void
MarkerInspectorDialog::_program_change_on_button_clicked (WavesButton *button)
{
	_enable_program_change (true);
}

void
MarkerInspectorDialog::_program_change_off_button_clicked (WavesButton *button)
{
	_enable_program_change (false);
}

void
MarkerInspectorDialog::on_bank_dropdown_item_changed (WavesDropdown*, int selected_item)
{
	if (_marker && _marker->location()) {
		boost::shared_ptr<ARDOUR::MIDISceneChange> msc = boost::dynamic_pointer_cast<ARDOUR::MIDISceneChange> (_marker->location ()->scene_change ());
        int bank = selected_item - 1;
		if (msc && (msc->bank () != bank)) {
			msc->set_bank (bank);
            _set_session_dirty ();
		}
	}
}

void
MarkerInspectorDialog::on_program_dropdown_item_changed (WavesDropdown*, int selected_item)
{
	if (_marker && _marker->location()) {
		boost::shared_ptr<ARDOUR::MIDISceneChange> msc = boost::dynamic_pointer_cast<ARDOUR::MIDISceneChange> (_marker->location ()->scene_change ());
		if (msc && (msc->program () != selected_item)) {
			msc->set_program (selected_item);
            _set_session_dirty ();
		}
	}
}

void
MarkerInspectorDialog::on_channel_dropdown_item_changed (WavesDropdown*, int selected_item)
{
	if (_marker && _marker->location()) {
		boost::shared_ptr<ARDOUR::MIDISceneChange> msc = boost::dynamic_pointer_cast<ARDOUR::MIDISceneChange> (_marker->location ()->scene_change ());
        int channel = selected_item + 1;
		if (msc && (msc->channel () != channel)) {
			msc->set_channel (channel);
            _set_session_dirty ();
		}
	}
}
