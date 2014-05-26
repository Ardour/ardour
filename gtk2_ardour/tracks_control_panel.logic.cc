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

#include "tracks_control_panel.h"
#include "waves_button.h"
#include "pbd/unwind.h"

#include <gtkmm2ext/utils.h>

#include "ardour/engine_state_controller.h"
#include "ardour/rc_configuration.h"

#include "device_connection_control.h"
#include "ardour_ui.h"
#include "gui_thread.h"
#include "utils.h"
#include "i18n.h"
#include "pbd/convert.h"

#include "open_file_dialog_proxy.h"

using namespace ARDOUR;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;
using namespace Glib;

#define dbg_msg(a) MessageDialog (a, PROGRAM_NAME).run();

namespace {
    // if pattern is not found out_str == in_str
    bool remove_pattern_from_string(const std::string& in_str, const std::string& pattern, std::string& out_str) {
        if (in_str.find(pattern) != std::string::npos ) {
            out_str = in_str.substr(pattern.size() );
            return true;
        } else {
            out_str = in_str;
            return false;
        }
    }
}

void
TracksControlPanel::init ()
{
	_ok_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_ok));
	_cancel_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_cancel));
	_apply_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_apply));

	_audio_settings_tab_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_audio_settings));
	_midi_settings_tab_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_midi_settings));
	_session_settings_tab_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_session_settings));
	_control_panel_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_control_panel));
    
    _all_inputs_on_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_all_inputs_on_button));
    _all_inputs_off_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_all_inputs_off_button));
    _all_outputs_on_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_all_outputs_on_button));
    _all_outputs_off_button.signal_clicked.connect (sigc::mem_fun (*this, &TracksControlPanel::on_all_outputs_off_button));
    
    _multi_out_button.signal_clicked.connect(sigc::mem_fun (*this, &TracksControlPanel::on_multi_out));
    _stereo_out_button.signal_clicked.connect(sigc::mem_fun (*this, &TracksControlPanel::on_stereo_out));
    
    _browse_button.signal_clicked.connect(sigc::mem_fun (*this, &TracksControlPanel::on_browse_button));    
    
	EngineStateController::instance ()->EngineRunning.connect (running_connection, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::engine_running, this), gui_context());
	EngineStateController::instance ()->EngineStopped.connect (stopped_connection, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::engine_stopped, this), gui_context());
	EngineStateController::instance ()->EngineHalted.connect (stopped_connection, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::engine_stopped, this), gui_context());

	/* Subscribe for udpates from EngineStateController */
	EngineStateController::instance()->BufferSizeChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::on_buffer_size_update, this), gui_context());
    EngineStateController::instance()->DeviceListChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::on_device_list_update, this, _1), gui_context());
    EngineStateController::instance()->InputConfigChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::on_input_configuration_changed, this), gui_context());
    EngineStateController::instance()->OutputConfigChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::on_output_configuration_changed, this), gui_context());
    
    /* Global configuration parameters update */
    Config->ParameterChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::on_parameter_changed, this, _1), gui_context());
    
	_engine_combo.signal_changed().connect (sigc::mem_fun (*this, &TracksControlPanel::engine_changed));
	_device_combo.signal_changed().connect (sigc::bind (sigc::mem_fun (*this, &TracksControlPanel::device_changed), true) );
	_sample_rate_combo.signal_changed().connect (sigc::mem_fun (*this, &TracksControlPanel::sample_rate_changed));
	_buffer_size_combo.signal_changed().connect (sigc::mem_fun (*this, &TracksControlPanel::buffer_size_changed));

	populate_engine_combo ();
	populate_output_mode();

    populate_input_channels();
    populate_output_channels();
    populate_default_session_path();
    
	_audio_settings_tab_button.set_active(true);
}

DeviceConnectionControl& TracksControlPanel::add_device_capture_control(std::string device_capture_name, bool active, uint16_t capture_number, std::string track_name)
{
    std::string port_name("");
    std::string pattern("system:");
    remove_pattern_from_string(device_capture_name, pattern, port_name);
    
	DeviceConnectionControl &capture_control = *manage (new DeviceConnectionControl(port_name, active, capture_number, track_name));
    
    char * id_str = new char [device_capture_name.length()+1];
    std::strcpy (id_str, device_capture_name.c_str());
    capture_control.set_data(DeviceConnectionControl::id_name, id_str);
	
    _device_capture_list.pack_start (capture_control, false, false);
	capture_control.signal_active_changed.connect (sigc::mem_fun (*this, &TracksControlPanel::on_capture_active_changed));
	return capture_control;
}

DeviceConnectionControl& TracksControlPanel::add_device_playback_control(std::string device_playback_name, bool active, uint16_t playback_number)
{
    std::string port_name("");
    std::string pattern("system:");
    remove_pattern_from_string(device_playback_name, pattern, port_name);
    
	DeviceConnectionControl &playback_control = *manage (new DeviceConnectionControl(port_name, active, playback_number));
    
    char * id_str = new char [device_playback_name.length()+1];
    std::strcpy (id_str, device_playback_name.c_str());
    playback_control.set_data(DeviceConnectionControl::id_name, id_str);
    
	_device_playback_list.pack_start (playback_control, false, false);
	playback_control.signal_active_changed.connect(sigc::mem_fun (*this, &TracksControlPanel::on_playback_active_changed));
	return playback_control;
}

DeviceConnectionControl& TracksControlPanel::add_midi_capture_control(std::string device_capture_name, bool active)
{
	DeviceConnectionControl &capture_control = *manage (new DeviceConnectionControl(device_capture_name, active));
	_midi_capture_list.pack_start (capture_control, false, false);
	capture_control.signal_active_changed.connect (sigc::mem_fun (*this, &TracksControlPanel::on_midi_capture_active_changed));
	return capture_control;
}

DeviceConnectionControl& TracksControlPanel::add_midi_playback_control(bool active)
{
	DeviceConnectionControl &playback_control = *manage (new DeviceConnectionControl(active));
	_midi_playback_list.pack_start (playback_control, false, false);
	playback_control.signal_active_changed.connect(sigc::mem_fun (*this, &TracksControlPanel::on_midi_playback_active_changed));
	return playback_control;
}

void
TracksControlPanel::populate_default_session_path()
{
    std::string std_path = Config->get_default_open_path();
    bool folderExist = Glib::file_test(std_path, FILE_TEST_EXISTS);
    
    if ( !folderExist )
        Config->set_default_open_path(Glib::get_home_dir());
    
    _default_open_path.set_text(Config->get_default_open_path());
}

void
TracksControlPanel::populate_engine_combo()
{
	if (_ignore_changes) {
		return;
	}

	std::vector<std::string> strings;
    std::vector<const AudioBackendInfo*> backends;
    EngineStateController::instance()->available_backends(backends);

	if (backends.empty()) {
		MessageDialog msg (string_compose (_("No audio/MIDI backends detected. %1 cannot run\n\n(This is a build/packaging/system error. It should never happen.)"), PROGRAM_NAME));
		msg.run ();
		throw failed_constructor ();
	}
	for (std::vector<const AudioBackendInfo*>::const_iterator b = backends.begin(); b != backends.end(); ++b) {
		strings.push_back ((*b)->name);
	}

	{
		// set _ignore_changes flag to ignore changes in combo-box callbacks
		PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
		set_popdown_strings (_engine_combo, strings);
		_engine_combo.set_sensitive (strings.size() > 1);
	}

	if (!strings.empty() )
	{
		_engine_combo.set_active_text (EngineStateController::instance()->get_current_backend_name() );
	}
}

void
TracksControlPanel::populate_device_combo()
{
    std::vector<AudioBackend::DeviceStatus> all_devices;
	EngineStateController::instance()->enumerate_devices (all_devices);

    std::vector<std::string> available_devices;

	for (std::vector<AudioBackend::DeviceStatus>::const_iterator i = all_devices.begin(); i != all_devices.end(); ++i) {
		available_devices.push_back (i->name);
	}

	{
		// set _ignore_changes flag to ignore changes in combo-box callbacks
		PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
		set_popdown_strings (_device_combo, available_devices);
		_device_combo.set_sensitive (available_devices.size() > 1);
	
        if(!available_devices.empty() ) {
            _device_combo.set_active_text (EngineStateController::instance()->get_current_device_name() );
        }
    }
    
    if(!available_devices.empty() ) {
        device_changed(false);
    }
}

void
TracksControlPanel::populate_sample_rate_combo()
{
    std::vector<float> sample_rates;
    EngineStateController::instance()->available_sample_rates_for_current_device(sample_rates);
	
	std::vector<std::string> s;
	for (std::vector<float>::const_iterator x = sample_rates.begin(); x != sample_rates.end(); ++x) {
		s.push_back (rate_as_string (*x));
	}

	{
		// set _ignore_changes flag to ignore changes in combo-box callbacks
		PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
		set_popdown_strings (_sample_rate_combo, s);
		_sample_rate_combo.set_sensitive (s.size() > 1);
	}

	if (!s.empty() ) {
		std::string active_sr = rate_as_string(EngineStateController::instance()->get_current_sample_rate() );
        
		_sample_rate_combo.set_active_text(active_sr);
	}
}

void
TracksControlPanel::populate_buffer_size_combo()
{
    std::vector<std::string> s;
	std::vector<pframes_t> buffer_sizes;

	EngineStateController::instance()->available_buffer_sizes_for_current_device(buffer_sizes);
	for (std::vector<pframes_t>::const_iterator x = buffer_sizes.begin(); x != buffer_sizes.end(); ++x) {
		s.push_back (bufsize_as_string (*x));
	}

	{
		// set _ignore_changes flag to ignore changes in combo-box callbacks
		PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
		set_popdown_strings (_buffer_size_combo, s);
		_buffer_size_combo.set_sensitive (s.size() > 1);
	}

	if (!s.empty() ) {
		std::string active_bs = bufsize_as_string(EngineStateController::instance()->get_current_buffer_size());
		_buffer_size_combo.set_active_text(active_bs);
	}
}

void
TracksControlPanel::populate_output_mode()
{
    _multi_out_button.set_active(Config->get_output_auto_connect() & AutoConnectPhysical);
    _stereo_out_button.set_active(Config->get_output_auto_connect() & AutoConnectMaster);
    
    _all_outputs_on_button.set_sensitive(Config->get_output_auto_connect() & AutoConnectPhysical);
    _all_outputs_off_button.set_sensitive(Config->get_output_auto_connect() & AutoConnectPhysical);
}


void
TracksControlPanel::populate_input_channels()
{
    cleanup_input_channels_list();
    
    // process captures (inputs)
    std::vector<EngineStateController::ChannelState> input_states;
    EngineStateController::instance()->get_physical_audio_input_states(input_states);
    
    std::vector<EngineStateController::ChannelState>::const_iterator input_iter;
    
    uint16_t number_count = 1;
    for (input_iter = input_states.begin(); input_iter != input_states.end(); ++input_iter ) {
        
        uint16_t number = DeviceConnectionControl::NoNumber;
        std::string track_name;

        if (input_iter->active) {
            
            std::string port_name("");
            std::string pattern("system:");
            remove_pattern_from_string(input_iter->name, pattern, port_name);
            
            number = number_count++;
            
            if (Config->get_tracks_auto_naming() & UseDefaultNames) {
                track_name = string_compose ("%1 %2", Session::default_trx_track_name_pattern, number);
            } else if (Config->get_tracks_auto_naming() & NameAfterDriver) {
                track_name = port_name;
            }
        }
        
        add_device_capture_control (input_iter->name, input_iter->active, number, track_name);
    }    
}


void
TracksControlPanel::populate_output_channels()
{
    cleanup_output_channels_list();
        
    // process captures (outputs)
    std::vector<EngineStateController::ChannelState> output_states;
    EngineStateController::instance()->get_physical_audio_output_states(output_states);
    
    std::vector<EngineStateController::ChannelState>::const_iterator output_iter;
    
    uint16_t number_count = 1;
    for (output_iter = output_states.begin(); output_iter != output_states.end(); ++output_iter ) {
        
        uint16_t number = DeviceConnectionControl::NoNumber;
        
        if (output_iter->active) {
            number = number_count++;
        }
        
        add_device_playback_control (output_iter->name, output_iter->active, number);
    }
    
}



void update_channel_numbers()
{
    
}


void update_channel_names()
{
    
}


void
TracksControlPanel::cleanup_input_channels_list()
{
    std::vector<Gtk::Widget*> capture_controls = _device_capture_list.get_children();
        
    while (capture_controls.size() != 0) {
        Gtk::Widget* item = capture_controls.back();
        
        DeviceConnectionControl* control = dynamic_cast<DeviceConnectionControl*>(item);
        
        if (control) {
            control->remove_data(DeviceConnectionControl::id_name);
        }
        
        capture_controls.pop_back();
        _device_capture_list.remove(*item);
        delete item;
    }
}


void
TracksControlPanel::cleanup_output_channels_list()
{
    std::vector<Gtk::Widget*> playback_controls = _device_playback_list.get_children();

    while (playback_controls.size() != 0) {
        Gtk::Widget* item = playback_controls.back();
        
        DeviceConnectionControl* control = dynamic_cast<DeviceConnectionControl*>(item);
        
        if (control) {
            control->remove_data(DeviceConnectionControl::id_name);
        }
        
        playback_controls.pop_back();
        _device_capture_list.remove(*item);
        delete item;
    }
}


void
TracksControlPanel::on_control_panel(WavesButton*)
{
}

void TracksControlPanel::engine_changed ()
{
	if (_ignore_changes) {
		return;
	}

	std::string backend_name = _engine_combo.get_active_text();
    
	if ( EngineStateController::instance()->set_new_backend_as_current (backend_name) )
	{
		_have_control = EngineStateController::instance()->is_setup_required ();
        populate_device_combo();
        return;
	}
    
    std::cerr << "\tfailed to set backend [" << backend_name << "]\n";
}

void TracksControlPanel::device_changed (bool show_confirm_dial/*=true*/)
{
	if (_ignore_changes) {
		return;
	}
    
    std::string device_name = _device_combo.get_active_text ();
    
    if( show_confirm_dial )
    {    
        std::string message = _("Would you like to switch to ") + device_name;

        MessageDialog msg (message,
                           false,
                           Gtk::MESSAGE_WARNING,
                           Gtk::BUTTONS_YES_NO,
                           true);
        
        msg.set_position (Gtk::WIN_POS_MOUSE);
        
		set_keep_above(false);

        switch (msg.run()) {
            case RESPONSE_NO:
                // set _ignore_changes flag to ignore changes in combo-box callbacks
                PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
                
                _device_combo.set_active_text (EngineStateController::instance()->get_current_device_name());
				set_keep_above(true);
                return;
        } 

		set_keep_above(true);
    }
    
    if (EngineStateController::instance()->set_new_device_as_current(device_name) )
    {
        populate_buffer_size_combo();
        populate_sample_rate_combo();
        return;
    }
    
    {
		// set _ignore_changes flag to ignore changes in combo-box callbacks
		PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
		// restore previous device name in combo box
        _device_combo.set_active_text (EngineStateController::instance()->get_current_device_name() );
	}
    
    MessageDialog( _("Selected device is not available for current engine"), PROGRAM_NAME).run();
}

void
TracksControlPanel::on_all_inputs_on_button(WavesButton*)
{
    EngineStateController::instance()->set_state_to_all_inputs(true);
}

void
TracksControlPanel::on_all_inputs_off_button(WavesButton*)
{
    EngineStateController::instance()->set_state_to_all_inputs(false);
}

void
TracksControlPanel::on_all_outputs_on_button(WavesButton*)
{
    EngineStateController::instance()->set_state_to_all_outputs(true);
}

void
TracksControlPanel::on_all_outputs_off_button(WavesButton*)
{
    EngineStateController::instance()->set_state_to_all_outputs(false);
}

void 
TracksControlPanel::buffer_size_changed()
{
	if (_ignore_changes) {
		return;
	}

    pframes_t new_buffer_size = get_buffer_size();
    if (EngineStateController::instance()->set_new_buffer_size_in_controller(new_buffer_size) )
    {
        show_buffer_duration();
        return;
    }
    
    {
		// set _ignore_changes flag to ignore changes in combo-box callbacks
		PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
        // restore previous buffer size value in combo box
        std::string buffer_size_str = bufsize_as_string (EngineStateController::instance()->get_current_buffer_size() );
        _buffer_size_combo.set_active_text(buffer_size_str);
    }
    
	MessageDialog( _("Buffer size set to the value which is not supported"), PROGRAM_NAME).run();
}

void
TracksControlPanel::sample_rate_changed()
{
	if (_ignore_changes) {
		return;
	}

    framecnt_t new_sample_rate = get_sample_rate ();
    if (EngineStateController::instance()->set_new_sample_rate_in_controller(new_sample_rate) )
    {
        show_buffer_duration();
        return;
    }
    
    {
		// set _ignore_changes flag to ignore changes in combo-box callbacks
		PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
        // restore previous buffer size value in combo box
        std::string sample_rate_str = rate_as_string (EngineStateController::instance()->get_current_sample_rate() );
        _sample_rate_combo.set_active_text(sample_rate_str);
    }
	
    MessageDialog( _("Sample rate set to the value which is not supported"), PROGRAM_NAME).run();
}


void
TracksControlPanel::engine_running ()
{
    populate_input_channels();
    populate_output_channels();

	_buffer_size_combo.set_active_text (bufsize_as_string (EngineStateController::instance()->get_current_buffer_size() ) );

	_sample_rate_combo.set_active_text (rate_as_string (EngineStateController::instance()->get_current_sample_rate() ) );

	_buffer_size_combo.set_sensitive (true);
	_sample_rate_combo.set_sensitive (true);
}


void
TracksControlPanel::engine_stopped ()
{
}


void
TracksControlPanel::on_audio_settings (WavesButton*)
{
	_midi_settings_layout.hide ();
	_midi_settings_tab_button.set_active(false);
	_session_settings_layout.hide ();
	_session_settings_tab_button.set_active(false);
	_audio_settings_layout.show ();
	_audio_settings_tab_button.set_active(true);
}


void
TracksControlPanel::on_midi_settings (WavesButton*)
{
	_audio_settings_layout.hide ();
	_audio_settings_tab_button.set_active(false);
	_session_settings_layout.hide ();
	_session_settings_tab_button.set_active(false);
	_midi_settings_layout.show ();
	_midi_settings_tab_button.set_active(true);
}


void
TracksControlPanel::on_session_settings (WavesButton*)
{
	_audio_settings_layout.hide ();
	_audio_settings_tab_button.set_active(false);
	_midi_settings_layout.hide ();
	_midi_settings_tab_button.set_active(false);
	_session_settings_layout.show ();
	_session_settings_tab_button.set_active(true);
}


void
TracksControlPanel::on_multi_out (WavesButton*)
{
    if (Config->get_output_auto_connect() & AutoConnectPhysical) {
        return;
    }
    
    Config->set_output_auto_connect(AutoConnectPhysical);
}

void
TracksControlPanel::on_stereo_out (WavesButton*)
{
    if (Config->get_output_auto_connect() & AutoConnectMaster) {
        return;
    }
    
    Config->set_output_auto_connect(AutoConnectMaster);
}

void
TracksControlPanel::on_browse_button (WavesButton*)
{
    using namespace std;
    
#ifdef __APPLE__
    set_keep_above(false);
    _default_path_name = ARDOUR::choose_folder_dialog(Config->get_default_open_path(), _("Choose Default Path"));
    set_keep_above(true);    
    
    if( !_default_path_name.empty() )
        _default_open_path.set_text(_default_path_name);
    else
        _default_open_path.set_text(Config->get_default_open_path());
	
    return;
#endif
    
#ifdef _WIN32
	string fileTitle;
	set_keep_above(false);
	bool result = ARDOUR::choose_folder_dialog(fileTitle, _("Choose Default Path"));
	set_keep_above(true);

	// if path was chosen in dialog
	if (result) {
		_default_path_name = fileTitle;
	}

    if (!_default_path_name.empty())
        _default_open_path.set_text(_default_path_name);
    else
        _default_open_path.set_text(Config->get_default_open_path());
    
	return;
#endif // _WIN32
}

void
TracksControlPanel::save_default_session_path()
{
    if(!_default_path_name.empty())
    {
        Config->set_default_open_path(_default_path_name);
        Config->save_state();
    }
}

void
TracksControlPanel::on_ok (WavesButton*)
{
	hide();
	EngineStateController::instance()->push_current_state_to_backend(true);
	response(Gtk::RESPONSE_OK);
    
    save_default_session_path();    
}


void
TracksControlPanel::on_cancel (WavesButton*)
{
	hide();
	response(Gtk::RESPONSE_CANCEL);    
    _default_open_path.set_text(Config->get_default_open_path());
}


void 
TracksControlPanel::on_apply (WavesButton*)
{
	EngineStateController::instance()->push_current_state_to_backend(true);
	response(Gtk::RESPONSE_APPLY);
    
    save_default_session_path();  
}


void TracksControlPanel::on_capture_active_changed(DeviceConnectionControl* capture_control, bool active)
{
    const char * id_name = (char*)capture_control->get_data(DeviceConnectionControl::id_name);
    EngineStateController::instance()->set_physical_audio_input_state(id_name, active);
}


void TracksControlPanel::on_playback_active_changed(DeviceConnectionControl* playback_control, bool active)
{
    const char * id_name = (char*)playback_control->get_data(DeviceConnectionControl::id_name);
    EngineStateController::instance()->set_physical_audio_output_state(id_name, active);
}


void TracksControlPanel::on_midi_capture_active_changed(DeviceConnectionControl* capture_control, bool active)
{
}


void TracksControlPanel::on_midi_playback_active_changed(DeviceConnectionControl* playback_control, bool active)
{
}


void
TracksControlPanel::on_buffer_size_update ()
{
    populate_buffer_size_combo();
}


void
TracksControlPanel::on_device_list_update (bool current_device_disconnected)
{
    populate_device_combo();
    
    if (current_device_disconnected) {
        std::string message = _("Audio Device Has Been Removed");
        
        MessageDialog msg (message,
                           false,
                           Gtk::MESSAGE_WARNING,
                           Gtk::BUTTONS_OK,
                           true);
        
        msg.set_position (Gtk::WIN_POS_MOUSE);
        msg.run();
        
        return;
    }
}

                                      
void
TracksControlPanel::on_parameter_changed (const std::string& parameter_name)
{
    if (parameter_name == "output-auto-connect") {
        populate_output_mode();
    } else if (parameter_name == "tracks-auto-naming") {
        on_input_configuration_changed ();
    }
}


void
TracksControlPanel::on_input_configuration_changed ()
{
    std::vector<Gtk::Widget*> capture_controls = _device_capture_list.get_children();
    
    std::vector<Gtk::Widget*>::iterator control_iter = capture_controls.begin();
    
    uint16_t number_count = 1;
    for (; control_iter != capture_controls.end(); ++control_iter) {
        DeviceConnectionControl* control = dynamic_cast<DeviceConnectionControl*> (*control_iter);
        
        if (control) {
            
            const char* id_name = (char*)control->get_data(DeviceConnectionControl::id_name);
            bool new_state = EngineStateController::instance()->get_physical_audio_input_state(id_name );
            
            uint16_t number = DeviceConnectionControl::NoNumber;
            std::string track_name ("");
            
            if (new_state) {

                number = number_count++;
                
                if (Config->get_tracks_auto_naming() & UseDefaultNames) {
                    track_name = string_compose ("%1 %2", Session::default_trx_track_name_pattern, number);
                } else if (Config->get_tracks_auto_naming() & NameAfterDriver) {
                    track_name = control->get_port_name();
                }
            }
            
            control->set_track_name(track_name);
            control->set_number(number);
            control->set_active(new_state);
        }
    }
}


void
TracksControlPanel::on_output_configuration_changed()
{
    std::vector<Gtk::Widget*> playback_controls = _device_playback_list.get_children();
    
    std::vector<Gtk::Widget*>::iterator control_iter = playback_controls.begin();
    
    uint16_t number_count = 1;
    for (; control_iter != playback_controls.end(); ++control_iter) {
        DeviceConnectionControl* control = dynamic_cast<DeviceConnectionControl*> (*control_iter);
        
        if (control) {
            
            const char * id_name = (char*)control->get_data(DeviceConnectionControl::id_name);
            
            bool new_state = EngineStateController::instance()->get_physical_audio_output_state(id_name );
            
            uint16_t number = DeviceConnectionControl::NoNumber;
            
            if (new_state) {
                number = number_count++;
            }
            
            control->set_number(number);
            control->set_active(new_state);
        }
    }

}

                                      
std::string
TracksControlPanel::bufsize_as_string (uint32_t sz)
{
	/* Translators: "samples" is always plural here, so no
	need for plural+singular forms.
	*/
	char  buf[32];
	snprintf (buf, sizeof (buf), _("%u samples"), sz);
	return buf;
}


framecnt_t
TracksControlPanel::get_sample_rate () const
{
	float r = atof (_sample_rate_combo.get_active_text ());
	/* the string may have been translated with an abbreviation for
	* thousands, so use a crude heuristic to fix this.
	*/
	if (r < 1000.0) {
		r *= 1000.0;
	}
	return r;
}

pframes_t TracksControlPanel::get_buffer_size() const
{
    std::string bs_text = _buffer_size_combo.get_active_text ();
    pframes_t samples = atoi (bs_text); /* will ignore trailing text */
	return samples;
}

void
TracksControlPanel::show_buffer_duration ()
{
	 float latency = (get_buffer_size() * 1000.0) / get_sample_rate();

	 char buf[256];
	 snprintf (buf, sizeof (buf), _("INPUT LATENCY: %.1f MS      OUTPUT LATENCY: %.1f MS      TOTAL LATENCY: %.1f MS"), 
			   latency, latency, 2*latency);
	 _latency_label.set_text (buf);
}
