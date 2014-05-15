//
//  engine_state_controller.cpp
//  Tracks
//
//  Created by Grygorii Zharun on 4/30/14.
//  Copyright (c) 2014 Waves. All rights reserved.
//

#include "engine_state_controller.h"

#include "ardour_ui.h"
#include "ardour/audioengine.h"
#include "pbd/error.h"
#include "i18n.h"
#include "gui_thread.h"

using namespace ARDOUR;
using namespace PBD;

namespace {
    
    struct DevicePredicate
    {
        DevicePredicate(const std::string& device_name)
        : _device_name(device_name)
        {}
        
        bool operator()(const AudioBackend::DeviceStatus& rhs)
        {
            return _device_name == rhs.name;
        }
        
    private:
        std::string _device_name;
    };
}

EngineStateController*
EngineStateController::instance()
{
    static EngineStateController instance;
    return &instance;
}


EngineStateController::EngineStateController()
: _current_state()
, _last_used_real_device("")
, _desired_sample_rate(0)
, _have_control(false)

{
    AudioEngine::instance ()->Running.connect (running_connection, MISSING_INVALIDATOR, boost::bind (&EngineStateController::_on_engine_running, this), gui_context());
	AudioEngine::instance ()->Stopped.connect (stopped_connection, MISSING_INVALIDATOR, boost::bind (&EngineStateController::_on_engine_stopped, this), gui_context());
	AudioEngine::instance ()->Halted.connect (stopped_connection, MISSING_INVALIDATOR, boost::bind (&EngineStateController::_on_engine_stopped, this), gui_context());
    
	/* Subscribe for udpates from AudioEngine */
    AudioEngine::instance()->SampleRateChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&EngineStateController::_on_sample_rate_change, this, _1), gui_context());
	AudioEngine::instance()->BufferSizeChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&EngineStateController::_on_buffer_size_change, this, _1), gui_context());
    AudioEngine::instance()->DeviceListChanged.connect (update_connections, MISSING_INVALIDATOR, boost::bind (&EngineStateController::_on_device_list_change, this), gui_context());
    
    _deserialize_and_load_states();
    _set_last_active_state_as_current();
    
    // now push the sate to the backend
    push_current_state_to_backend(false);
}


EngineStateController::~EngineStateController()
{
    
}


void
EngineStateController::_deserialize_and_load_states()
{
    
}


void
EngineStateController::_serialize_and_save_current_state()
{
    // **** add code to save the state list to the file ****
}


void
EngineStateController::_set_last_active_state_as_current()
{
    // if we have no saved state load default values
    if (!_states.empty() ) {
    } else {
        _current_state = boost::shared_ptr<State>(new State() );
        
        std::vector<const AudioBackendInfo*> backends = AudioEngine::instance()->available_backends();
        
        if (!backends.empty() )
        {
            
            if (set_new_backend_as_current(backends.front()->name ) ) {
                _current_state->device_name = "None";
                _validate_current_device_state();
                _states.push_front(_current_state);
            } else {
                std::cerr << "\tfailed to set backend [" << backends.front()->name << "]\n";
            }
        }
        
    }
}


void
EngineStateController::_validate_current_device_state()
{
    boost::shared_ptr<AudioBackend> backend = AudioEngine::instance()->current_backend();
    assert(backend);
    
    // check if device parameters from the state record are still valid
    // validate sample rate
    std::vector<float> sample_rates = backend->available_sample_rates (_current_state->device_name);
    
    // check if session desired sample rate (if it's set) could be used with this device
    if (_desired_sample_rate != 0) {
        std::vector<float>::iterator sr_iter = std::find (sample_rates.begin(), sample_rates.end(), (float)_desired_sample_rate);
        
        if (sr_iter != sample_rates.end() ) {
            _current_state->sample_rate = _desired_sample_rate;
        } else {
            _current_state->sample_rate = backend->default_sample_rate();
        }
    
    } else {
        // check if current sample rate is supported because we have no session desired sample rate value
        std::vector<float>::iterator sr_iter = std::find (sample_rates.begin(), sample_rates.end(), (float)_current_state->sample_rate);
        // switch to default if current sample rate is not supported
        if (sr_iter == sample_rates.end() ) {
            
            _current_state->sample_rate = backend->default_sample_rate();
        }
    }
    
    // validate buffer size
    std::vector<pframes_t> buffer_sizes = backend->available_buffer_sizes (_current_state->device_name);
    // check if buffer size is supported
    std::vector<pframes_t>::iterator bs_iter = std::find (buffer_sizes.begin(), buffer_sizes.end(), _current_state->buffer_size);
    // if current is not found switch to default if is supported
    if (bs_iter == buffer_sizes.end() ) {
		std::find (buffer_sizes.begin(), buffer_sizes.end(), backend->default_buffer_size () );
	
		if (bs_iter != buffer_sizes.end() ) {
			_current_state->buffer_size = backend->default_buffer_size ();
		} else {
			if (!buffer_sizes.empty() ) {
				_current_state->buffer_size = buffer_sizes.front();
			}
		}

	}

	
}


const std::string&
EngineStateController::get_current_backend_name() const
{
    return _current_state->backend_name;
}


const std::string&
EngineStateController::get_current_device_name() const
{
    return _current_state->device_name;
}


void
EngineStateController::available_backends(std::vector<const AudioBackendInfo*>& available_backends)
{
    available_backends = AudioEngine::instance()->available_backends();
}


void
EngineStateController::enumerate_devices (std::vector<AudioBackend::DeviceStatus>& device_vector) const
{
    boost::shared_ptr<AudioBackend> backend = AudioEngine::instance()->current_backend();
    assert(backend);
    device_vector = backend->enumerate_devices();
}


framecnt_t
EngineStateController::get_current_sample_rate() const
{
    return _current_state->sample_rate;
}


framecnt_t
EngineStateController::get_default_sample_rate() const
{
    boost::shared_ptr<AudioBackend> backend = AudioEngine::instance()->current_backend();
    assert(backend);
    return backend->default_sample_rate();
}


void
EngineStateController::available_sample_rates_for_current_device(std::vector<float>& sample_rates) const
{
    boost::shared_ptr<AudioBackend> backend = AudioEngine::instance()->current_backend();
    assert(backend);
    sample_rates = backend->available_sample_rates (_current_state->device_name);
}


uint32_t
EngineStateController::get_current_buffer_size() const
{
    return _current_state->buffer_size;
}


uint32_t
EngineStateController::get_default_buffer_size() const
{
    boost::shared_ptr<AudioBackend> backend = AudioEngine::instance()->current_backend();
    assert(backend);
    return backend->default_buffer_size();
}


void
EngineStateController::available_buffer_sizes_for_current_device(std::vector<pframes_t>& buffer_sizes) const
{
    boost::shared_ptr<AudioBackend> backend = AudioEngine::instance()->current_backend();
    assert(backend);
    buffer_sizes = backend->available_buffer_sizes (_current_state->device_name);
}


bool
EngineStateController::set_new_backend_as_current(const std::string& backend_name)
{
    if (backend_name == AudioEngine::instance()->current_backend_name () ) {
        return true;
    }
    
    boost::shared_ptr<AudioBackend> backend = AudioEngine::instance()->set_backend (backend_name, "ardour", "");
	if (backend)
	{
        StateList::iterator found_state_iter = find_if (_states.begin(), _states.end(),
                                                        State::StatePredicate(backend_name, "None") );
        
        if (found_state_iter != _states.end() ) {
            // we found a record for new engine with None device - switch to it
            _current_state = *found_state_iter;
            
        } else {
            // create new record for this engine with default device
            _current_state = boost::shared_ptr<State>(new State() );
            _current_state->backend_name = backend_name;
            _current_state->device_name = "None";
            _validate_current_device_state();
            _states.push_front(_current_state);
        }
        
        
		return true;
	}
    
    return false;
}


bool
EngineStateController::set_new_current_device_in_controller(const std::string& device_name)
{
    if (_current_state->device_name == device_name) {
        return true;
    }
    
    _last_used_real_device.clear();
    
    if (device_name != "None") {
        _last_used_real_device = device_name;
    }
    
    boost::shared_ptr<AudioBackend> backend = AudioEngine::instance()->current_backend();
    assert(backend);
    
    std::vector<AudioBackend::DeviceStatus> device_vector = backend->enumerate_devices();
    
    // validate the device
    std::vector<AudioBackend::DeviceStatus>::iterator device_iter;
    device_iter = std::find_if (device_vector.begin(), device_vector.end(), DevicePredicate(device_name) );
    
    // device is available
    if (device_iter != device_vector.end() ) {
    
        // look through state list and find the record for this device and current engine
        StateList::iterator found_state_iter = find_if (_states.begin(), _states.end(),
                                                        State::StatePredicate(backend->name(), device_name) );
        
        if (found_state_iter != _states.end() )
        {
            // we found a record for current engine and provided device name - switch to it
            _current_state = *found_state_iter;
            _validate_current_device_state();
        
        } else {
       
            // the record is not found, create new one
            _current_state = boost::shared_ptr<State>(new State() );
            
            _current_state->backend_name = backend->name();
            _current_state->device_name = device_name;
            _validate_current_device_state();
            _states.push_front(_current_state);
        }
        
		push_current_state_to_backend(false);

        return true;
    }
    
    // device is not supported by current backend
    return false;
}



bool
EngineStateController::set_new_sample_rate_in_controller(framecnt_t sample_rate)
{
    if (_current_state->sample_rate == sample_rate) {
        return true;
    }
    
    boost::shared_ptr<AudioBackend> backend = AudioEngine::instance()->current_backend();
    assert(backend);
    
    std::vector<float> sample_rates = backend->available_sample_rates (_current_state->device_name);
    std::vector<float>::iterator iter = std::find (sample_rates.begin(), sample_rates.end(), (float)sample_rate);
    
    if (iter != sample_rates.end() ) {
        _current_state->sample_rate = sample_rate;
        return true;
    }
    
    return false;
}




bool
EngineStateController::set_new_buffer_size_in_controller(pframes_t buffer_size)
{
    if (_current_state->buffer_size == buffer_size) {
        return true;
    }
    
    boost::shared_ptr<AudioBackend> backend = AudioEngine::instance()->current_backend();
    assert(backend);
    
    std::vector<uint32_t> buffer_sizes = backend->available_buffer_sizes (_current_state->device_name);
    std::vector<uint32_t>::iterator iter = std::find (buffer_sizes.begin(), buffer_sizes.end(), buffer_size);

    if (iter != buffer_sizes.end() ) {
        _current_state->buffer_size = buffer_size;
        return true;
    }
    
    return false;
}


void
EngineStateController::_on_sample_rate_change(framecnt_t new_sample_rate)
{
	// validate the change
	if (set_new_sample_rate_in_controller(new_sample_rate)) {
		SampleRateChanged(); // emit a signal if successful
	
	} else {
		// restore previous state in backend
		push_current_state_to_backend(false);
	}

}


void
EngineStateController::_on_buffer_size_change(pframes_t new_buffer_size)
{
	// validate the change
    if (set_new_buffer_size_in_controller(new_buffer_size) ) {
		BufferSizeChanged(); // emit a signal if successful
	
	} else {
		// restore previous state in backend
		push_current_state_to_backend(false);
	}
    
    
}


void
EngineStateController::_on_device_list_change()
{
    bool current_device_disconnected = false;
    
    boost::shared_ptr<AudioBackend> backend = AudioEngine::instance()->current_backend();
    assert(backend);
    
    std::vector<AudioBackend::DeviceStatus> device_vector = backend->enumerate_devices();
    
    // find out out if current device is still available if it's not None
    if (_current_state->device_name != "None")
    {
        std::vector<AudioBackend::DeviceStatus>::iterator device_iter;
        device_iter = std::find_if (device_vector.begin(), device_vector.end(), DevicePredicate(_current_state->device_name) );
        
        // if current device is not available any more - switch to None device
        if (device_iter == device_vector.end() ) {
            
            StateList::iterator found_state_iter = find_if (_states.begin(), _states.end(),
                                                            State::StatePredicate(_current_state->backend_name, "None") );
            
            if (found_state_iter != _states.end() ) {
                // found the record - switch to it
                _current_state = *found_state_iter;
            } else {
                // create new record for this engine with default device
                _current_state = boost::shared_ptr<State>(new State() );
                _current_state->backend_name = backend->name();
                _current_state->device_name = "None";
                _validate_current_device_state();
                _states.push_front(_current_state);
            }
            
            push_current_state_to_backend(false);
            current_device_disconnected = true;
        }
    } else {
        // if the device which was active before is available now - switch to it
        
        std::vector<AudioBackend::DeviceStatus>::iterator device_iter;
        device_iter = std::find_if (device_vector.begin(), device_vector.end(), DevicePredicate(_last_used_real_device) );
        
        if (device_iter != device_vector.end() ) {
            StateList::iterator found_state_iter = find_if (_states.begin(), _states.end(),
                                                            State::StatePredicate(_current_state->backend_name,
                                                                                  _last_used_real_device) );
            
            if (found_state_iter != _states.end() ) {
                _current_state = *found_state_iter;
                _validate_current_device_state();
                
                push_current_state_to_backend(false);
            }
        }
        
    }
    
    DeviceListChanged(current_device_disconnected); // emit a signal    
}


void
EngineStateController::_on_engine_running ()
{
}


void
EngineStateController::_on_engine_stopped ()
{
}

bool
EngineStateController::push_current_state_to_backend(bool start)
{
    boost::shared_ptr<AudioBackend> backend = AudioEngine::instance()->current_backend();
	
	if (!backend) {
		return false;
	}
    
    // check if anything changed
    bool state_changed = (_current_state->device_name != backend->device_name() ) ||
                         (_current_state->sample_rate != backend->sample_rate() ) ||
                         (_current_state->buffer_size != backend->buffer_size() );
    
    bool was_running = AudioEngine::instance()->running();
    
    if (state_changed) {
        
        if (was_running) {
            if (ARDOUR_UI::instance()->disconnect_from_engine () ) {
                return false;
            }
        }
        
        if ((_current_state->device_name != backend->device_name()) && backend->set_device_name (_current_state->device_name)) {
            error << string_compose (_("Cannot set device name to %1"), get_current_device_name()) << endmsg;
        }
        
        if (backend->set_sample_rate (_current_state->sample_rate )) {
            error << string_compose (_("Cannot set sample rate to %1"), get_current_sample_rate()) << endmsg;
        }
        
        if (backend->set_buffer_size (_current_state->buffer_size )) {
            error << string_compose (_("Cannot set buffer size to %1"), get_current_buffer_size()) << endmsg;
        }
        
        //if (backend->set_input_channels (get_input_channels())) {
        //	error << string_compose (_("Cannot set input channels to %1"), get_input_channels()) << endmsg;
        //	return -1;
        //}
        
        //if (backend->set_output_channels (get_output_channels())) {
        //	error << string_compose (_("Cannot set output channels to %1"), get_output_channels()) << endmsg;
        //	return -1;
        //}
        
        //if (backend->set_systemic_input_latency (get_input_latency())) {
        //	error << string_compose (_("Cannot set input latency to %1"), get_input_latency()) << endmsg;
        //	return -1;
        //}
        
        //if (backend->set_systemic_output_latency (get_output_latency())) {
        //	error << string_compose (_("Cannot set output latency to %1"), get_output_latency()) << endmsg;
        //	return -1;
        //}
    }
    
    _serialize_and_save_current_state();
    
	if(start || (was_running && state_changed) ) {
        if (ARDOUR_UI::instance()->reconnect_to_engine () ) {
            return false;
        }
	}
    
    return true;
}


void
EngineStateController::set_desired_sample_rate(framecnt_t session_desired_sr)
{
    if (session_desired_sr == 0 || session_desired_sr == _desired_sample_rate) {
        return;
    }
    
    _desired_sample_rate = session_desired_sr;
    _validate_current_device_state();
    
    // if we swithced to new desired sample rate successfuly - push the new state to the backend
    if (_current_state->sample_rate == session_desired_sr) {
        push_current_state_to_backend(false);
    }
}

