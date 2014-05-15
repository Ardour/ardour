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

#include "engine_state_controller.h"
#include "ardour/rc_configuration.h"
#include "device_connection_control.h"
#include "ardour_ui.h"
#include "gui_thread.h"
#include "utils.h"
#include "i18n.h"
#include "pbd/convert.h"

using namespace ARDOUR;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;
using namespace Glib;

#define dbg_msg(a) MessageDialog (a, PROGRAM_NAME).run();

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
    
    _multi_out_button.signal_clicked.connect(sigc::mem_fun (*this, &TracksControlPanel::on_multi_out));
    _stereo_out_button.signal_clicked.connect(sigc::mem_fun (*this, &TracksControlPanel::on_stereo_out));
    
    
	AudioEngine::instance ()->Running.connect (running_connection, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::engine_running, this), gui_context());
	AudioEngine::instance ()->Stopped.connect (stopped_connection, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::engine_stopped, this), gui_context());
	AudioEngine::instance ()->Halted.connect (stopped_connection, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::engine_stopped, this), gui_context());

	/* Subscribe for udpates from AudioEngine */
	EngineStateController::instance()->BufferSizeChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::on_buffer_size_update, this), gui_context());
    EngineStateController::instance()->DeviceListChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&TracksControlPanel::on_device_list_update, this, _1), gui_context());

	_engine_combo.signal_changed().connect (sigc::mem_fun (*this, &TracksControlPanel::engine_changed));
	_device_combo.signal_changed().connect (sigc::bind (sigc::mem_fun (*this, &TracksControlPanel::device_changed), true) );
	_sample_rate_combo.signal_changed().connect (sigc::mem_fun (*this, &TracksControlPanel::sample_rate_changed));
	_buffer_size_combo.signal_changed().connect (sigc::mem_fun (*this, &TracksControlPanel::buffer_size_changed));

	populate_engine_combo ();
	populate_output_mode();

	_audio_settings_tab_button.set_active(true);
}

DeviceConnectionControl& TracksControlPanel::add_device_capture_control(std::string device_capture_name, bool active, uint16_t capture_number, std::string track_name)
{
	DeviceConnectionControl &capture_control = *manage (new DeviceConnectionControl(device_capture_name, active, capture_number, track_name));
	_device_capture_list.pack_start (capture_control, false, false);
	capture_control.signal_active_changed.connect (sigc::mem_fun (*this, &TracksControlPanel::on_capture_active_changed));
	return capture_control;
}

DeviceConnectionControl& TracksControlPanel::add_device_playback_control(std::string device_playback_name, bool active, uint16_t playback_number)
{
	DeviceConnectionControl &playback_control = *manage (new DeviceConnectionControl(device_playback_name, active, playback_number));
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
}

void
TracksControlPanel::on_control_panel(WavesButton*)
{
// ******************************* ATTENTION!!! ****************************
// here is just demo code to remove it in future
// *************************************************************************
/*
	static uint16_t number = 0;
	static bool active = false;

	number++;
	active = !active;

	std::string name = string_compose (_("Audio Capture %1"), number);
	add_device_capture_control (name, active, number, name);

	name = string_compose (_("Audio Playback Output %1"), number);
	add_device_playback_control (name, active, number);

	name = string_compose (_("Midi Capture %1"), number);
	add_midi_capture_control (name, active);

	add_midi_playback_control (active);
*/
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
        std::string message = "Would you like to switch to " + device_name;

        MessageDialog msg (message,
                           false,
                           Gtk::MESSAGE_WARNING,
                           Gtk::BUTTONS_YES_NO,
                           true);
        
        msg.set_position (Gtk::WIN_POS_MOUSE);
        
        switch (msg.run()) {
            case RESPONSE_NO:
                // set _ignore_changes flag to ignore changes in combo-box callbacks
                PBD::Unwinder<uint32_t> protect_ignore_changes (_ignore_changes, _ignore_changes + 1);
                
                _device_combo.set_active_text (EngineStateController::instance()->get_current_device_name());
                return;
        }   
    }
    
    if (EngineStateController::instance()->set_new_current_device_in_controller(device_name) )
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
    _stereo_out_button.set_active(false);
    _multi_out_button.set_active(true);
}


void
TracksControlPanel::on_stereo_out (WavesButton*)
{

    if (Config->get_output_auto_connect() & AutoConnectMaster) {
        return;
    }
    
    Config->set_output_auto_connect(AutoConnectMaster);
    _multi_out_button.set_active(false);
    _stereo_out_button.set_active(true);
}

void
TracksControlPanel::on_ok (WavesButton*)
{
	hide();
	EngineStateController::instance()->push_current_state_to_backend(true);
	response(Gtk::RESPONSE_OK);
}


void
TracksControlPanel::on_cancel (WavesButton*)
{
	hide();
	response(Gtk::RESPONSE_CANCEL);
}


void 
TracksControlPanel::on_apply (WavesButton*)
{
	EngineStateController::instance()->push_current_state_to_backend(true);
	response(Gtk::RESPONSE_APPLY);
}


void TracksControlPanel::on_capture_active_changed(DeviceConnectionControl* capture_control, bool active)
{
// ******************************* ATTENTION!!! ****************************
// here is just demo code to replace it with a meaningful app logic in future
// *************************************************************************
	capture_control->set_number ( active ? 1000 : DeviceConnectionControl::NoNumber);
}


void TracksControlPanel::on_playback_active_changed(DeviceConnectionControl* playback_control, bool active)
{
// ******************************* ATTENTION!!! ****************************
// here is just demo code to replace it with a meaningful app logic in future
// *************************************************************************
	playback_control->set_number ( active ? 1000 : DeviceConnectionControl::NoNumber);
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
        // REACT ON CURRENT DEVICE DISCONNECTION
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
