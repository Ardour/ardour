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
#include "ardour/recent_sessions.h"
#include "ardour/filename_extensions.h"

#include "ardour/utils.h"
#include "ardour_ui.h"
#include "gui_thread.h"
#include "utils.h"
#include "i18n.h"
#include "pbd/convert.h"

#include "timecode/time.h"
#include "time.h"

#include "open_file_dialog_proxy.h"

using namespace ARDOUR;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;
using namespace Glib;

#define dbg_msg(a) MessageDialog (a, PROGRAM_NAME).run();

namespace {

    static const char* audio_capture_name_prefix = "system:capture:";
    static const char* audio_playback_name_prefix = "system:playback:";
    static const char* midi_port_name_prefix = "system_midi:";
    static const char* midi_capture_suffix = " capture";
    static const char* midi_playback_suffix = " playback";
    
    struct MidiDeviceDescriptor {
        std::string name;
        std::string capture_name;
        bool capture_active;
        std::string playback_name;
        bool playback_active;
        
        MidiDeviceDescriptor(const std::string& name) :
        name(name),
        capture_name(""),
        capture_active(false),
        playback_name(""),
        playback_active(false)
        {}
        
        bool operator==(const MidiDeviceDescriptor& rhs) {
            return name == rhs.name;
        }
    };
    
    typedef std::vector<MidiDeviceDescriptor> MidiDeviceDescriptorVec;
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
    EngineStateController::instance()->PortRegistrationChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::on_port_registration_update, this), gui_context());
	EngineStateController::instance()->BufferSizeChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::on_buffer_size_update, this), gui_context());
    EngineStateController::instance()->DeviceListChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::on_device_list_update, this, _1), gui_context());
    EngineStateController::instance()->InputConfigChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::on_audio_input_configuration_changed, this), gui_context());
    EngineStateController::instance()->OutputConfigChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::on_audio_output_configuration_changed, this), gui_context());
    EngineStateController::instance()->MIDIInputConfigChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::on_midi_input_configuration_changed, this), gui_context());
    EngineStateController::instance()->MIDIOutputConfigChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::on_midi_output_configuration_changed, this), gui_context());
    
    /* Global configuration parameters update */
    Config->ParameterChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::on_parameter_changed, this, _1), gui_context());
    
	_engine_combo.signal_changed().connect (sigc::mem_fun (*this, &TracksControlPanel::engine_changed));
	_device_combo.signal_changed().connect (sigc::bind (sigc::mem_fun (*this, &TracksControlPanel::device_changed), true) );
	_sample_rate_combo.signal_changed().connect (sigc::mem_fun (*this, &TracksControlPanel::sample_rate_changed));
	_buffer_size_combo.signal_changed().connect (sigc::mem_fun (*this, &TracksControlPanel::buffer_size_changed));
    
    /* Session configuration parameters update */
    _file_type_combo.signal_changed().connect (sigc::mem_fun (*this, &TracksControlPanel::file_type_changed));
    _bit_depth_combo.signal_changed().connect (sigc::mem_fun (*this, &TracksControlPanel::bit_depth_changed));
    _frame_rate_combo.signal_changed().connect (sigc::mem_fun (*this, &TracksControlPanel::frame_rate_changed));

    _name_tracks_after_driver.signal_clicked.connect(sigc::mem_fun (*this, &TracksControlPanel::on_name_tracks_after_driver));
    _reset_tracks_name_to_default.signal_clicked.connect(sigc::mem_fun (*this, &TracksControlPanel::on_reset_tracks_name_to_default));

    _control_panel_button.signal_clicked.connect(sigc::mem_fun (*this, &TracksControlPanel::on_control_panel_button));
    
    _yes_button.signal_clicked.connect(sigc::mem_fun (*this, &TracksControlPanel::on_yes_button));
    _no_button.signal_clicked.connect(sigc::mem_fun (*this, &TracksControlPanel::on_no_button));
    _yes_button.set_visible(false);
    _no_button.set_visible(false);
    
	populate_engine_combo ();
	populate_output_mode();

    populate_input_channels();
    populate_output_channels();
    populate_midi_ports();
    populate_default_session_path();
    
    // Init session Settings
    populate_file_type_combo();
    populate_bit_depth_combo();
    populate_frame_rate_combo();
    populate_auto_lock_timer();
    
	_audio_settings_tab_button.set_active(true);
}

DeviceConnectionControl& TracksControlPanel::add_device_capture_control(std::string port_name, bool active, uint16_t capture_number, std::string track_name)
{
    std::string device_capture_name("");
    std::string pattern(audio_capture_name_prefix);
    remove_pattern_from_string(port_name, pattern, device_capture_name);
    
	DeviceConnectionControl &capture_control = *manage (new DeviceConnectionControl(device_capture_name, active, capture_number, track_name));
    
    char * id_str = new char [port_name.length()+1];
    std::strcpy (id_str, port_name.c_str());
    capture_control.set_data(DeviceConnectionControl::id_name, id_str);
	
    _device_capture_list.pack_start (capture_control, false, false);
	capture_control.signal_active_changed.connect (sigc::mem_fun (*this, &TracksControlPanel::on_capture_active_changed));
	return capture_control;
}

DeviceConnectionControl& TracksControlPanel::add_device_playback_control(std::string port_name, bool active, uint16_t playback_number)
{
    std::string device_playback_name("");
    std::string pattern(audio_playback_name_prefix);
    remove_pattern_from_string(port_name, pattern, device_playback_name);
    
	DeviceConnectionControl &playback_control = *manage (new DeviceConnectionControl(device_playback_name, active, playback_number));
    
    char * id_str = new char [port_name.length()+1];
    std::strcpy (id_str, port_name.c_str());
    playback_control.set_data(DeviceConnectionControl::id_name, id_str);
    
	_device_playback_list.pack_start (playback_control, false, false);
	playback_control.signal_active_changed.connect(sigc::mem_fun (*this, &TracksControlPanel::on_playback_active_changed));
	return playback_control;
}

MidiDeviceConnectionControl& TracksControlPanel::add_midi_device_control(const std::string& midi_device_name,
                                                                         const std::string& capture_name, bool capture_active,
                                                                         const std::string& playback_name, bool playback_active)
{
	MidiDeviceConnectionControl &midi_device_control = *manage (new MidiDeviceConnectionControl(midi_device_name, !capture_name.empty(), capture_active, !playback_name.empty(), playback_active));
    
    if (!capture_name.empty()) {
        char * capture_id_str = new char [capture_name.length()+1];
        std::strcpy (capture_id_str, capture_name.c_str());
        midi_device_control.set_data(MidiDeviceConnectionControl::capture_id_name, capture_id_str);
    }
    
    if (!playback_name.empty()) {
        char * playback_id_str = new char [playback_name.length()+1];
        std::strcpy (playback_id_str, playback_name.c_str());
        midi_device_control.set_data(MidiDeviceConnectionControl::playback_id_name, playback_id_str);
    }
    
	_midi_device_list.pack_start (midi_device_control, false, false);
	midi_device_control.signal_capture_active_changed.connect (sigc::mem_fun (*this, &TracksControlPanel::on_midi_capture_active_changed));
    midi_device_control.signal_playback_active_changed.connect(sigc::mem_fun (*this, &TracksControlPanel::on_midi_playback_active_changed));
	return midi_device_control;
}

namespace  {
    // Strings which are shown to user in the Preference panel
    const std::string string_CAF = "Caf";
    const std::string string_BWav = "BWav";
    const std::string string_Aiff = "Aiff";
    const std::string string_Wav64 = "Wave64";

    std::string
    HeaderFormat_to_string(HeaderFormat header_format)
    {
        using namespace std;
        
        switch (header_format) {
            case CAF:
                return string_CAF;
            case BWF:
                return string_BWav;
            case AIFF:
                return string_Aiff;
            case WAVE64:
                return string_Wav64;
            default:
                return string("");
        }
        
        return string("");
    }
    
    HeaderFormat
    string_to_HeaderFormat(std::string s)
    {
        if(s == string_CAF)
            return CAF;
        
        if(s == string_BWav)
            return BWF;
        
        if(s == string_Aiff)
            return AIFF;
        
        if(s == string_Wav64)
            return WAVE64;
        
        //defaul value
        return BWF;
    }
    
    std::string
    xml_string_to_user_string(std::string xml_string);
    
    enum SessionProperty {
        Native_File_Header_Format,
        Native_File_Data_Format,
        Timecode_Format
    };
    
    std::string
    read_property_from_last_session(SessionProperty session_property)
    {        
        using namespace std;
        
        ARDOUR::RecentSessions rs;
        ARDOUR::read_recent_sessions (rs);
        
        if( rs.size() > 0 )
        {
            string full_session_name = Glib::build_filename( rs[0].second, rs[0].first );
            full_session_name += statefile_suffix;
            
            // read property from session projectfile
            boost::shared_ptr<XMLTree> state_tree(new XMLTree());
            
            if (!state_tree->read (full_session_name))
                return string("");
            
            XMLNode& root (*state_tree->root());

            if (root.name() != X_("Session"))
                return string("");
            
            XMLNode* config_main_node = root.child ("Config");
            if( !config_main_node )
                return string("");

            XMLNodeList config_nodes_list = config_main_node->children();
            XMLNodeConstIterator config_node_iter = config_nodes_list.begin();
            
            string required_property_name;
            
            switch (session_property) {
                case Native_File_Header_Format:
                    required_property_name = "native-file-header-format";
                    break;
                case Native_File_Data_Format:
                    required_property_name = "native-file-data-format";
                    break;
                case Timecode_Format:
                    required_property_name = "timecode-format";
                    break;
                default:
                    return string("");
            }
            
            for (; config_node_iter != config_nodes_list.end(); ++config_node_iter)
            {
                XMLNode* config_node = *config_node_iter;
                XMLProperty* prop = NULL;
                
                if ( (prop = config_node->property ("name")) != 0 )
                    if( prop->value() == required_property_name )
                        if ( (prop = config_node->property ("value")) != 0 )
                            return xml_string_to_user_string( prop->value() );
            }
        } 
        
        return string("");
    }
}


void
TracksControlPanel::populate_file_type_combo()
{
    using namespace std;
    
    vector<string> file_type_strings;
    file_type_strings.push_back( HeaderFormat_to_string(CAF) );
    file_type_strings.push_back( HeaderFormat_to_string(BWF) );
    file_type_strings.push_back( HeaderFormat_to_string(AIFF) );
    file_type_strings.push_back( HeaderFormat_to_string(WAVE64) );
    
    // Get FILE_TYPE from last used session
    string header_format_string = read_property_from_last_session(Native_File_Header_Format);
    
    ARDOUR_UI* ardour_ui = ARDOUR_UI::instance();
    HeaderFormat header_format = string_to_HeaderFormat(header_format_string);
    ardour_ui->set_header_format( header_format );
    
    {
		// set _ignore_changes flag to ignore changes in combo-box callbacks
		PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
		set_popdown_strings (_file_type_combo, file_type_strings);
		_file_type_combo.set_sensitive (file_type_strings.size() > 1);
        _file_type_combo.set_active_text( HeaderFormat_to_string(header_format) );
	}

    return;
}

namespace {
    // Strings which are shown to user in the Preference panel
    const std::string string_bit32 = "32 bit floating point";
    const std::string string_bit24 = "24 bit";
    const std::string string_bit16 = "16 bit";
    
    std::string
    SampleFormat_to_string(SampleFormat sample_format)
    {
        using namespace std;
        
        switch (sample_format) {
            case FormatFloat:
                return string_bit32;
            case FormatInt24:
                return string_bit24;
            case FormatInt16:
                return string_bit16;
        }
        
        return string("");
    }

    SampleFormat
    string_to_SampleFormat(std::string s)
    {
        if(s == string_bit32)
            return FormatFloat;
        
        if(s == string_bit24)
            return FormatInt24;
        
        if(s == string_bit16)
            return FormatInt16;
        
        // default value
        return FormatInt24;
    }
}

void
TracksControlPanel::populate_bit_depth_combo()
{
    using namespace std;
    
    vector<string> bit_depth_strings;
    bit_depth_strings.push_back(SampleFormat_to_string(FormatInt16));
    bit_depth_strings.push_back(SampleFormat_to_string(FormatInt24));
    bit_depth_strings.push_back(SampleFormat_to_string(FormatFloat));
    
    // Get BIT_DEPTH from last used session
    string sample_format_string = read_property_from_last_session(Native_File_Data_Format);
    
    ARDOUR_UI* ardour_ui = ARDOUR_UI::instance();
    SampleFormat sample_format = string_to_SampleFormat(sample_format_string);
    ardour_ui->set_sample_format( sample_format );
    
    {
		// set _ignore_changes flag to ignore changes in combo-box callbacks
		PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
		set_popdown_strings (_bit_depth_combo, bit_depth_strings);
		_bit_depth_combo.set_sensitive (bit_depth_strings.size() > 1);
        _bit_depth_combo.set_active_text ( SampleFormat_to_string(sample_format) );
	}
   
    return;
}

namespace  {
    const std::string string_24fps = "24 fps";
    const std::string string_25fps = "25 fps";
    const std::string string_30fps = "30 fps";
    const std::string string_23976fps = "23.976 fps";
    const std::string string_2997fps = "29.97 fps";
    
    std::string
    TimecodeFormat_to_string(Timecode::TimecodeFormat timecode_format)
    {
        using namespace std;
        using namespace Timecode;
        
        switch (timecode_format) {
            case timecode_24:
                return string_24fps;
            case timecode_25:
                return string_25fps;
            case timecode_30:
                return string_30fps;
            case timecode_23976:
                return string_23976fps;
            case timecode_2997:
                return string_2997fps;
                
            default:
                return string("");
        }
        
        return string("");
    }
    
    Timecode::TimecodeFormat
    string_to_TimecodeFormat(std::string s)
    {
        using namespace Timecode;
        
        if(s == string_24fps)
            return timecode_24;
        if(s == string_25fps)
            return timecode_25;
        if(s == string_30fps)
            return timecode_30;
        if(s == string_23976fps)
            return timecode_23976;
        if(s == string_2997fps)
            return timecode_2997;
        
        //defaul value
        return timecode_25;
    }
    
    std::string
    xml_string_to_user_string(std::string xml_string)
    {
        // Bit depth format
        if(xml_string == enum_2_string (FormatFloat))
            return string_bit32;
        
        if(xml_string == enum_2_string (FormatInt24))
            return string_bit24;
        
        if(xml_string == enum_2_string (FormatInt16))
            return string_bit16;
        
        
        // Header format (File type)
        if(xml_string == enum_2_string(CAF))
            return string_CAF;
        
        if(xml_string == enum_2_string(BWF))
            return string_BWav;
        
        if(xml_string == enum_2_string(AIFF))
            return string_Aiff;
        
        if(xml_string == enum_2_string(WAVE64))
            return string_Wav64;
        
        // fps (Timecode)
        if(xml_string == enum_2_string(Timecode::timecode_24))
            return string_24fps;
        
        if(xml_string == enum_2_string(Timecode::timecode_25))
            return string_25fps;
        
        if(xml_string == enum_2_string(Timecode::timecode_30))
            return string_30fps;
        
        if(xml_string == enum_2_string(Timecode::timecode_23976))
            return string_23976fps;
        
        if(xml_string == enum_2_string(Timecode::timecode_2997))
            return string_2997fps;
        
        return std::string("");
    }

}

void
TracksControlPanel::populate_frame_rate_combo()
{
    using namespace std;
    
    vector<string> frame_rate_strings;
    frame_rate_strings.push_back(string_24fps);
    frame_rate_strings.push_back(string_25fps);
    frame_rate_strings.push_back(string_30fps);
    frame_rate_strings.push_back(string_23976fps);
    frame_rate_strings.push_back(string_2997fps);
    
    // Get FRAME_RATE from last used session
    string last_used_frame_rate = read_property_from_last_session(Timecode_Format);
    
    ARDOUR_UI* ardour_ui = ARDOUR_UI::instance();
    Timecode::TimecodeFormat timecode_format = string_to_TimecodeFormat(last_used_frame_rate);
    ardour_ui->set_timecode_format( timecode_format );
    
    {
		// set _ignore_changes flag to ignore changes in combo-box callbacks
		PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
		set_popdown_strings (_frame_rate_combo, frame_rate_strings);
		_frame_rate_combo.set_sensitive (frame_rate_strings.size() > 1);
        _frame_rate_combo.set_active_text ( TimecodeFormat_to_string(timecode_format) );
	}
    
    return;
}

void
TracksControlPanel::refresh_session_settings_info()
{
    ARDOUR_UI* ardour_ui = ARDOUR_UI::instance();
    if( !ardour_ui )
        return;
    
    Session* session = ardour_ui->the_session();
    if( !session )
        return;
    _bit_depth_combo.set_active_text( SampleFormat_to_string(session->config.get_native_file_data_format()) );
    _file_type_combo.set_active_text( HeaderFormat_to_string(session->config.get_native_file_header_format()) );
    _frame_rate_combo.set_active_text( TimecodeFormat_to_string(session->config.get_timecode_format()) );
}

void
TracksControlPanel::populate_auto_lock_timer()
{
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

		if (!s.empty() ) {
			std::string active_sr = rate_as_string(EngineStateController::instance()->get_current_sample_rate() );
			_sample_rate_combo.set_active_text(active_sr);
		}
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

		if (!s.empty() ) {
			std::string active_bs = bufsize_as_string(EngineStateController::instance()->get_current_buffer_size());
			_buffer_size_combo.set_active_text(active_bs);
		}
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
    std::vector<EngineStateController::PortState> input_states;
    EngineStateController::instance()->get_physical_audio_input_states(input_states);
    
    std::vector<EngineStateController::PortState>::const_iterator input_iter;
    
    uint16_t number_count = 1;
    for (input_iter = input_states.begin(); input_iter != input_states.end(); ++input_iter ) {
        
        uint16_t number = DeviceConnectionControl::NoNumber;
        std::string track_name;

        if (input_iter->active) {
            
            std::string port_name("");
            std::string pattern(audio_capture_name_prefix);
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
    
    _all_inputs_on_button.set_sensitive(!input_states.empty() );
    _all_inputs_off_button.set_sensitive(!input_states.empty() );
}


void
TracksControlPanel::populate_output_channels()
{
    cleanup_output_channels_list();
        
    // process captures (outputs)
    std::vector<EngineStateController::PortState> output_states;
    EngineStateController::instance()->get_physical_audio_output_states(output_states);
    
    std::vector<EngineStateController::PortState>::const_iterator output_iter;
    
    uint16_t number_count = 1;
    for (output_iter = output_states.begin(); output_iter != output_states.end(); ++output_iter ) {
        
        uint16_t number = DeviceConnectionControl::NoNumber;
        
        if (output_iter->active) {
            number = number_count++;
        }
        
        add_device_playback_control (output_iter->name, output_iter->active, number);
    }
    
    bool stereo_out_disabled = (Config->get_output_auto_connect() & AutoConnectPhysical);
    _all_outputs_on_button.set_sensitive(!output_states.empty() && stereo_out_disabled );
    _all_outputs_off_button.set_sensitive(!output_states.empty() && stereo_out_disabled );
}


void
TracksControlPanel::populate_midi_ports()
{
    cleanup_midi_device_list();
    
    std::vector<EngineStateController::PortState> midi_input_states, midi_output_states;
    EngineStateController::instance()->get_physical_midi_input_states(midi_input_states);
    EngineStateController::instance()->get_physical_midi_output_states(midi_output_states);
    
    // now group corresponding inputs and outputs into a vector of midi device descriptors
    MidiDeviceDescriptorVec midi_device_descriptors;
    std::vector<EngineStateController::PortState>::const_iterator state_iter;
    // process inputs
    for (state_iter = midi_input_states.begin(); state_iter != midi_input_states.end(); ++state_iter) {
        // strip the device name from input port name
        std::string device_name("");
        remove_pattern_from_string(state_iter->name, midi_port_name_prefix, device_name);
        remove_pattern_from_string(device_name, midi_capture_suffix, device_name);
        
        MidiDeviceDescriptor device_descriptor(device_name);
        device_descriptor.capture_name = state_iter->name;
        device_descriptor.capture_active = state_iter->active;
        midi_device_descriptors.push_back(device_descriptor);
    }
    
    // process outputs
    for (state_iter = midi_output_states.begin(); state_iter != midi_output_states.end(); ++state_iter){
        // strip the device name from input port name
        std::string device_name("");
        remove_pattern_from_string(state_iter->name, midi_port_name_prefix, device_name);
        remove_pattern_from_string(device_name, midi_playback_suffix, device_name);
        
        // check if we already have descriptor for this device
        MidiDeviceDescriptor device_descriptor(device_name);
        MidiDeviceDescriptorVec::iterator found_iter;
        found_iter = std::find(midi_device_descriptors.begin(), midi_device_descriptors.end(), device_descriptor );
        
        if (found_iter != midi_device_descriptors.end() ) {
            found_iter->playback_name = state_iter->name;
            found_iter->playback_active = state_iter->active;
        } else {
            device_descriptor.capture_name.clear();
            device_descriptor.playback_name = state_iter->name;
            device_descriptor.playback_active = state_iter->active;
            midi_device_descriptors.push_back(device_descriptor);
        }
    }
    
    // now add midi device controls
    MidiDeviceDescriptorVec::iterator iter;
    for (iter = midi_device_descriptors.begin(); iter != midi_device_descriptors.end(); ++iter ) {
        add_midi_device_control(iter->name, iter->capture_name, iter->capture_active,
                                            iter->playback_name, iter->playback_active);
    }
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
TracksControlPanel::cleanup_midi_device_list()
{
    std::vector<Gtk::Widget*> midi_device_controls = _midi_device_list.get_children();
    
    while (midi_device_controls.size() != 0) {
        Gtk::Widget* item = midi_device_controls.back();
        
        MidiDeviceConnectionControl* control = dynamic_cast<MidiDeviceConnectionControl*>(item);
        
        if (control) {
            control->remove_data(MidiDeviceConnectionControl::capture_id_name);
            control->remove_data(MidiDeviceConnectionControl::playback_id_name);
        }
        
        midi_device_controls.pop_back();
        _midi_device_list.remove(*item);
        delete item;
    }
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
        msg.set_keep_above(true);

        switch (msg.run()) {
            case RESPONSE_NO:
                // set _ignore_changes flag to ignore changes in combo-box callbacks
                PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
                
                _device_combo.set_active_text (EngineStateController::instance()->get_current_device_name());

                return;
        } 

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
TracksControlPanel::on_name_tracks_after_driver(WavesButton*)
{
    _yes_button.set_visible(true);
    _no_button.set_visible(true);

    _tracks_naming_rule = NameAfterDriver;
}

void
TracksControlPanel::on_reset_tracks_name_to_default(WavesButton*)
{
    _yes_button.set_visible(true);
    _no_button.set_visible(true);
    
    _tracks_naming_rule = UseDefaultNames;
}

void
TracksControlPanel::on_yes_button(WavesButton*)
{
    Config->set_tracks_auto_naming(_tracks_naming_rule);
    
    _yes_button.set_visible(false);
    _no_button.set_visible(false);
}

void
TracksControlPanel::on_no_button(WavesButton*)
{
    _yes_button.set_visible(false);
    _no_button.set_visible(false);
}

void
TracksControlPanel::on_control_panel_button(WavesButton*)
{
    boost::shared_ptr<AudioBackend> backend = AudioEngine::instance()->current_backend();
    assert(backend);
    
    backend->launch_control_app ();
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
TracksControlPanel::file_type_changed()
{ 
    if (_ignore_changes) {
		return;
	}
    
    std::string s = _file_type_combo.get_active_text();
    ARDOUR::HeaderFormat header_format = string_to_HeaderFormat(s);
    
    ARDOUR_UI* ardour_ui = ARDOUR_UI::instance();
    ardour_ui->set_header_format( header_format );
}

void
TracksControlPanel::bit_depth_changed()
{
    if (_ignore_changes) {
		return;
	}
    
    std::string s = _bit_depth_combo.get_active_text();
    ARDOUR::SampleFormat sample_format = string_to_SampleFormat(s);
    
    ARDOUR_UI* ardour_ui = ARDOUR_UI::instance();
    ardour_ui->set_sample_format( sample_format );
}

void
TracksControlPanel::frame_rate_changed()
{
    if (_ignore_changes) {
		return;
	}

    std::string s = _frame_rate_combo.get_active_text();
    Timecode::TimecodeFormat timecode_format = string_to_TimecodeFormat(s);
    
    ARDOUR_UI* ardour_ui = ARDOUR_UI::instance();
    ardour_ui->set_timecode_format(timecode_format);    
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
	populate_buffer_size_combo();
	populate_sample_rate_combo();
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

void TracksControlPanel::update_session_config ()
{
    ARDOUR_UI* ardour_ui = ARDOUR_UI::instance();
    
    if( ardour_ui )
    {
        ARDOUR::Session* session = ardour_ui->the_session();
        
        if( session )
        {
            session->config.set_native_file_header_format( string_to_HeaderFormat(_file_type_combo.get_active_text() ) );
            session->config.set_native_file_data_format  ( string_to_SampleFormat(_bit_depth_combo.get_active_text() ) );
            session->config.set_timecode_format(           string_to_TimecodeFormat(_frame_rate_combo.get_active_text() ) );
        }
    }
}

void
TracksControlPanel::on_ok (WavesButton*)
{
	hide();
	EngineStateController::instance()->push_current_state_to_backend(true);
	response(Gtk::RESPONSE_OK);
    
    update_session_config();
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
    
    update_session_config();
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


void TracksControlPanel::on_midi_capture_active_changed(MidiDeviceConnectionControl* control, bool active)
{
    const char * id_name = (char*)control->get_data(MidiDeviceConnectionControl::capture_id_name);
    EngineStateController::instance()->set_physical_midi_input_state(id_name, active);
}


void TracksControlPanel::on_midi_playback_active_changed(MidiDeviceConnectionControl* control, bool active)
{
    const char * id_name = (char*)control->get_data(MidiDeviceConnectionControl::playback_id_name);
    EngineStateController::instance()->set_physical_midi_output_state(id_name, active);
}


void TracksControlPanel::on_port_registration_update()
{
    populate_input_channels();
    populate_output_channels();
    populate_midi_ports();
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
        std::string message = _("Audio device has been removed");
        
        MessageDialog msg (message,
                           false,
                           Gtk::MESSAGE_WARNING,
                           Gtk::BUTTONS_OK,
                           true);
        
        msg.set_position (Gtk::WIN_POS_MOUSE);
        msg.set_keep_above(true);
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
        on_audio_input_configuration_changed ();
    }
}


void
TracksControlPanel::on_audio_input_configuration_changed ()
{
    std::vector<Gtk::Widget*> capture_controls = _device_capture_list.get_children();
    
    std::vector<Gtk::Widget*>::iterator control_iter = capture_controls.begin();
    
    uint16_t number_count = 1;
    for (; control_iter != capture_controls.end(); ++control_iter) {
        DeviceConnectionControl* control = dynamic_cast<DeviceConnectionControl*> (*control_iter);
        
        if (control) {
            
            const char* id_name = (char*)control->get_data(DeviceConnectionControl::id_name);
            
            if (id_name) {
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
}


void
TracksControlPanel::on_audio_output_configuration_changed()
{
    std::vector<Gtk::Widget*> playback_controls = _device_playback_list.get_children();
    
    std::vector<Gtk::Widget*>::iterator control_iter = playback_controls.begin();
    
    uint16_t number_count = 1;
    for (; control_iter != playback_controls.end(); ++control_iter) {
        DeviceConnectionControl* control = dynamic_cast<DeviceConnectionControl*> (*control_iter);
        
        if (control) {
            
            const char * id_name = (char*)control->get_data(DeviceConnectionControl::id_name);
            
            if (id_name != NULL) {
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

}


void
TracksControlPanel::on_midi_input_configuration_changed ()
{
    std::vector<Gtk::Widget*> midi_controls = _midi_device_list.get_children();
    
    std::vector<Gtk::Widget*>::iterator control_iter = midi_controls.begin();
    
    for (; control_iter != midi_controls.end(); ++control_iter) {
        MidiDeviceConnectionControl* control = dynamic_cast<MidiDeviceConnectionControl*> (*control_iter);
        
        if (control && control->has_capture() ) {
            
            const char* capture_id_name = (char*)control->get_data(MidiDeviceConnectionControl::capture_id_name);
            
            if (capture_id_name != NULL) {
                bool new_state = EngineStateController::instance()->get_physical_midi_input_state(capture_id_name );
                control->set_capture_active(new_state);
            }
        }
    }
}


void
TracksControlPanel::on_midi_output_configuration_changed ()
{
    std::vector<Gtk::Widget*> midi_controls = _midi_device_list.get_children();
    
    std::vector<Gtk::Widget*>::iterator control_iter = midi_controls.begin();
    
    for (; control_iter != midi_controls.end(); ++control_iter) {
        MidiDeviceConnectionControl* control = dynamic_cast<MidiDeviceConnectionControl*> (*control_iter);
        
        if (control && control->has_playback() ) {
            
            const char* playback_id_name = (char*)control->get_data(MidiDeviceConnectionControl::playback_id_name);
            
            if (playback_id_name != NULL) {
                bool new_state = EngineStateController::instance()->get_physical_midi_output_state(playback_id_name);
                control->set_playback_active(new_state);
            }
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
