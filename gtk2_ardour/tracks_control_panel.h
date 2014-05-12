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

#ifndef __gtk2_ardour_tracks_control_panel_h__
#define __gtk2_ardour_tracks_control_panel_h__

#include <map>
#include <vector>
#include <string>

#include <gtkmm/layout.h>

#include "pbd/signals.h"
#include "pbd/xml++.h"

#include "waves_dialog.h"
#include "device_connection_control.h"

class WavesButton;

class TracksControlPanel : public WavesDialog, public PBD::ScopedConnectionList {
  public:
	TracksControlPanel ();
    ~TracksControlPanel ();

  private:
	Gtk::VBox& _device_capture_list;
	Gtk::VBox& _device_playback_list;
	Gtk::VBox& _midi_capture_list;
	Gtk::VBox& _midi_playback_list;
	Gtk::Layout& _audio_settings_layout;
	Gtk::Layout& _midi_settings_layout;
	Gtk::Layout& _session_settings_layout;
	WavesButton& _audio_settings_tab_button;
	WavesButton& _session_settings_tab_button;
	WavesButton& _midi_settings_tab_button;
	WavesButton& _multi_out_button;
	WavesButton& _stereo_out_button;
	WavesButton& _ok_button;
	WavesButton& _cancel_button;
	WavesButton& _apply_button;
	WavesButton& _control_panel_button;
	WavesButton& _no_button;
	WavesButton& _name_track_after_driver_button;
	WavesButton& _reset_track_names_button;
	WavesButton& _yes_button;
	Gtk::ComboBoxText& _engine_combo;
	Gtk::ComboBoxText& _device_combo;
	Gtk::ComboBoxText& _sample_rate_combo;
	Gtk::ComboBoxText& _buffer_size_combo;
	Gtk::Label& _latency_label;

#include "tracks_control_panel.logic.h"
};

#endif /* __gtk2_ardour_tracks_control_panel_h__ */
