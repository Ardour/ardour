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
#include "tracks_control_panel.h"
#include "waves_button.h"
//#include <exception>
//#include <vector>
//#include <cmath>
//#include <fstream>
//#include <map>

//#include <boost/scoped_ptr.hpp>

//#include <gtkmm/messagedialog.h>

//#include "pbd/error.h"
//#include "pbd/xml++.h"
//#include "pbd/unwind.h"
//#include "pbd/failed_constructor.h"

//#include <gtkmm/alignment.h>
//#include <gtkmm/stock.h>
//#include <gtkmm/notebook.h>
//#include <gtkmm2ext/utils.h>

//#include "ardour/audio_backend.h"
//#include "ardour/audioengine.h"
//#include "ardour/mtdm.h"
//#include "ardour/rc_configuration.h"
//#include "ardour/types.h"

//#include "pbd/convert.h"
//#include "pbd/error.h"

//#include "ardour_ui.h"
//#include "tracks_control_panel.h"
//#include "gui_thread.h"
//#include "utils.h"
#include "i18n.h"

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;
using namespace Glib;

TracksControlPanel::TracksControlPanel ()
	: WavesDialog ("tracks_preferences.xml")
	, _device_capture_list (get_v_box("device_capture_list"))
	, _device_playback_list (get_v_box("device_playback_list"))
	, _midi_device_list (get_v_box("midi_device_list"))
	, _all_inputs_on_button (get_waves_button("all_inputs_on_button"))
    , _all_inputs_off_button (get_waves_button("all_inputs_off_button"))
    , _all_outputs_on_button (get_waves_button("all_outputs_on_button"))
    , _all_outputs_off_button (get_waves_button("all_outputs_off_button"))
	, _audio_settings_layout (get_layout ("audio_settings_layout"))
	, _midi_settings_layout (get_layout ("midi_settings_layout"))
	, _session_settings_layout (get_layout ("session_settings_layout"))
	, _audio_settings_tab_button (get_waves_button ("audio_settings_tab_button"))
	, _midi_settings_tab_button (get_waves_button ("midi_settings_tab_button"))
	, _session_settings_tab_button (get_waves_button ("session_settings_tab_button"))
	, _ok_button (get_waves_button ("ok_button"))
	, _cancel_button (get_waves_button ("cancel_button"))
	, _apply_button (get_waves_button ("apply_button"))
	, _control_panel_button (get_waves_button ("control_panel_button"))
	, _no_button (get_waves_button ("no_button"))
	, _yes_button (get_waves_button ("yes_button"))
	, _engine_combo (get_combo_box_text ("engine_combo"))
	, _device_combo (get_combo_box_text ("device_combo"))
	, _sample_rate_combo (get_combo_box_text ("sample_rate_combo"))
	, _buffer_size_combo (get_combo_box_text ("buffer_size_combo"))
	, _latency_label (get_label("latency_label"))
    , _default_open_path (get_label("default_open_path"))
	, _multi_out_button(get_waves_button ("multi_out_button"))
	, _stereo_out_button(get_waves_button ("stereo_out_button"))
    , _name_tracks_after_driver(get_waves_button ("name_tracks_after_driver_button"))
    , _reset_tracks_name_to_default(get_waves_button ("reset_tracks_name_to_default_button"))
    , _file_type_combo (get_combo_box_text ("file_type_combo"))
    , _bit_depth_combo (get_combo_box_text ("bit_depth_combo"))
    , _frame_rate_combo (get_combo_box_text ("frame_rate_combo"))
    , _browse_button(get_waves_button("browse_default_folder"))
    , _have_control (false)
	, _ignore_changes (0)
{
	init();
}

TracksControlPanel::~TracksControlPanel ()
{
	_ignore_changes = true;
}
