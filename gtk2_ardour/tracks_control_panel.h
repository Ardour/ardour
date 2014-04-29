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

//#include <gtkmm/checkbutton.h>
//#include <gtkmm/spinbutton.h>
//#include <gtkmm/comboboxtext.h>
//#include <gtkmm/table.h>
//#include <gtkmm/expander.h>
//#include <gtkmm/box.h>
//#include <gtkmm/buttonbox.h>
//#include <gtkmm/button.h>

#include <gtkmm/layout.h>

#include "pbd/signals.h"

#include "waves_dialog.h"
#include "pbd/xml++.h"

class WavesButton;

class TracksControlPanel : public WavesDialog, public PBD::ScopedConnectionList {
  public:
	TracksControlPanel ();
    ~TracksControlPanel ();

  private:
	Gtk::Layout& audio_settings_layout;
	Gtk::Layout& midi_settings_layout;
	Gtk::Layout& session_settings_layout;
	WavesButton& audio_settings_tab_button;
	WavesButton& session_settings_tab_button;
	WavesButton& midi_settings_tab_button;
	WavesButton& multi_out_button;
	WavesButton& stereo_out_button;
	WavesButton& ok_button;
	WavesButton& cancel_button;
	WavesButton& apply_button;
	WavesButton& control_panel_button;
	WavesButton& no_button;
	WavesButton& name_track_after_driver_button;
	WavesButton& reset_track_names_button;
	WavesButton& yes_button;
	Gtk::ComboBoxText& engine_combo;
	Gtk::ComboBoxText& device_combo;
	Gtk::ComboBoxText& sample_rate_combo;
	Gtk::ComboBoxText& buffer_size_combo;
	Gtk::Label& latency_label;

#include "tracks_control_panel.logic.h"
};

#endif /* __gtk2_ardour_tracks_control_panel_h__ */
