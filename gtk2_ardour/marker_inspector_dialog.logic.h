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

// class TracksControlPanel : public WavesDialog {
public:
	void set_marker (Marker* marker);

protected:

private:
	Marker* _marker;
	PBD::ScopedConnectionList _marker_connections;

	void _init();
	void _display_marker_data ();
	void _display_scene_change_info ();
	void _enable_program_change (bool);
    void _set_session_dirty ();
	void _on_location_changed (ARDOUR::Location*);
	void _lock_button_clicked (WavesButton *button);
	void _program_change_on_button_clicked (WavesButton *button);
	void _program_change_off_button_clicked (WavesButton *button);
	void on_bank_dropdown_item_changed (WavesDropdown*, int);
	void on_program_dropdown_item_changed (WavesDropdown*, int);
	void on_channel_dropdown_item_changed (WavesDropdown*, int);
//};

