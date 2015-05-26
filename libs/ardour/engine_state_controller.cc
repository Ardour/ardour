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

#include "ardour/engine_state_controller.h"

#include "ardour/audioengine.h"
#include "ardour/session.h"
#include "ardour/rc_configuration.h"
#include "ardour/data_type.h"

#include "pbd/pthread_utils.h"
#include "pbd/error.h"
#include "i18n.h"


using namespace ARDOUR;
using namespace PBD;

namespace {
    
struct DevicePredicate
{
	DevicePredicate (const std::string& device_name)
		: _device_name (device_name)
	{}
        
	bool operator ()(const AudioBackend::DeviceStatus& rhs)
	{
		return _device_name == rhs.name;
	}
        
                                        private:
	std::string _device_name;
};
}

EngineStateController*
EngineStateController::instance ()
{
	static EngineStateController instance;
	return &instance;
}


EngineStateController::EngineStateController ()
	: _current_state ()
	, _last_used_real_device ("")

{
	AudioEngine::instance ()->Running.connect_same_thread (running_connection, boost::bind (&EngineStateController::_on_engine_running, this));
	AudioEngine::instance ()->Stopped.connect_same_thread (stopped_connection, boost::bind (&EngineStateController::_on_engine_stopped, this));
	AudioEngine::instance ()->Halted.connect_same_thread (stopped_connection, boost::bind (&EngineStateController::_on_engine_stopped, this));
    
	/* Subscribe for udpates from AudioEngine */
	AudioEngine::instance ()->PortRegisteredOrUnregistered.connect_same_thread (update_connections, boost::bind (&EngineStateController::_on_ports_registration_update, this));
	AudioEngine::instance ()->SampleRateChanged.connect_same_thread (update_connections, boost::bind (&EngineStateController::_on_sample_rate_change, this, _1));
	AudioEngine::instance ()->BufferSizeChanged.connect_same_thread (update_connections, boost::bind (&EngineStateController::_on_buffer_size_change, this, _1));
	AudioEngine::instance ()->DeviceListChanged.connect_same_thread (update_connections, boost::bind (&EngineStateController::_on_device_list_change, this));
	AudioEngine::instance ()->DeviceError.connect_same_thread (update_connections, boost::bind (&EngineStateController::_on_device_error, this));
    
	/* Global configuration parameters update */
	Config->ParameterChanged.connect_same_thread (update_connections, boost::bind (&EngineStateController::_on_parameter_changed, this, _1));
    
	_deserialize_and_load_engine_states ();
	_deserialize_and_load_midi_port_states ();
	_do_initial_engine_setup ();
    
	// now push the sate to the backend
	push_current_state_to_backend (false);
}


EngineStateController::~EngineStateController ()
{
}


void
EngineStateController::set_session (Session* session)
{
	_session = session;
	_session->SessionLoaded.connect_same_thread (session_connections, boost::bind (&EngineStateController::_on_session_loaded, this));
}


void
EngineStateController::remove_session ()
{
	session_connections.drop_connections ();
	_session = 0;
}


XMLNode&
EngineStateController::serialize_audio_midi_settings ()
{
    
	XMLNode* root = new XMLNode ("AudioMidiSettings");
    
	_serialize_engine_states (root);
	_serialize_midi_port_states (root);
    
	return *root;
}


void
EngineStateController::save_audio_midi_settings ()
{
	Config->add_extra_xml (serialize_audio_midi_settings ());
	Config->save_state ();
}


void
EngineStateController::_deserialize_and_load_engine_states ()
{
	XMLNode* audio_midi_settings_root = ARDOUR::Config->extra_xml ("AudioMidiSettings");
    
	if (!audio_midi_settings_root) {
		return;
	}
    
	XMLNode* engine_states = audio_midi_settings_root->child ("EngineStates");
    
	if (!engine_states) {
		return;
	}
    
	XMLNodeList state_nodes_list = engine_states->children ();
	XMLNodeConstIterator state_node_iter = state_nodes_list.begin ();
    
	for (; state_node_iter != state_nodes_list.end (); ++state_node_iter) {
        
		XMLNode* state_node = *state_node_iter;
		StatePtr engine_state (new State);
		XMLProperty* prop = NULL;
        
		if ((prop = state_node->property ("backend-name")) == 0) {
			continue;
		}
		engine_state->backend_name = prop->value ();
        
		if ((prop = state_node->property ("device-name")) == 0) {
			continue;
		}
		engine_state->device_name = prop->value ();
        
		if ((prop = state_node->property ("sample-rate")) == 0) {
			continue;
		}
		engine_state->sample_rate = atoi (prop->value ());
        
		if ((prop = state_node->property ("buffer-size")) == 0) {
			continue;
		}
		engine_state->buffer_size = atoi (prop->value ());
        
		if ((prop = state_node->property ("active")) == 0) {
			continue;
		}
		engine_state->active = string_is_affirmative (prop->value ());
        
		XMLNodeList state_children_list = state_node->children ();
		XMLNodeConstIterator state_child_iter = state_children_list.begin ();
        
		for (; state_child_iter != state_children_list.end (); ++state_child_iter) {
			XMLNode* state_child = *state_child_iter;
                
			if (state_child->name () == "InputConfiguration") {
                    
				XMLNodeList input_states_nodes = state_child->children ();
				XMLNodeConstIterator input_state_node_iter = input_states_nodes.begin ();
				PortStateList& input_states = engine_state->input_channel_states;
                    
				for (; input_state_node_iter != input_states_nodes.end (); ++input_state_node_iter) {
                        
					XMLNode* input_state_node = *input_state_node_iter;
                        
					if (input_state_node->name () != "input") {
						continue;
					}
					PortState input_state (input_state_node->name ());
                        
					if ((prop = input_state_node->property ("name")) == 0) {
						continue;
					}
					input_state.name = prop->value ();
                        
					if ((prop = input_state_node->property ("active")) == 0) {
						continue;
					}
					input_state.active = string_is_affirmative (prop->value ());
                        
					input_states.push_back (input_state);
				}
                    
			} else if (state_child->name () == "MultiOutConfiguration") {
                    
				XMLNodeList multi_out_state_nodes = state_child->children ();
				XMLNodeConstIterator multi_out_state_node_iter = multi_out_state_nodes.begin ();
				PortStateList& multi_out_states = engine_state->multi_out_channel_states;
                    
				for (; multi_out_state_node_iter != multi_out_state_nodes.end (); ++multi_out_state_node_iter) {
                        
					XMLNode* multi_out_state_node = *multi_out_state_node_iter;
                        
					if (multi_out_state_node->name () != "output") {
						continue;
					}
					PortState multi_out_state (multi_out_state_node->name ());
                        
					if ((prop = multi_out_state_node->property ("name")) == 0) {
						continue;
					}
					multi_out_state.name = prop->value ();
                        
					if ((prop = multi_out_state_node->property ("active")) == 0) {
						continue;
					}
					multi_out_state.active = string_is_affirmative (prop->value ());
                        
					multi_out_states.push_back (multi_out_state);
				}
			} else if (state_child->name () == "StereoOutConfiguration") {
                    
				XMLNodeList stereo_out_state_nodes = state_child->children ();
				XMLNodeConstIterator stereo_out_state_node_iter = stereo_out_state_nodes.begin ();
				PortStateList& stereo_out_states = engine_state->stereo_out_channel_states;
                    
				for (; stereo_out_state_node_iter != stereo_out_state_nodes.end (); ++stereo_out_state_node_iter) {
                        
					XMLNode* stereo_out_state_node = *stereo_out_state_node_iter;
                        
					if (stereo_out_state_node->name () != "output") {
						continue;
					}
					PortState stereo_out_state (stereo_out_state_node->name ());
                        
					if ((prop = stereo_out_state_node->property ("name")) == 0) {
						continue;
					}
					stereo_out_state.name = prop->value ();
                        
					if ((prop = stereo_out_state_node->property ("active")) == 0) {
						continue;
					}                      
					stereo_out_state.active = string_is_affirmative (prop->value ());
                        
					stereo_out_states.push_back (stereo_out_state);
				}
			}
		}
        
		_states.push_back (engine_state);
	}
}


void
EngineStateController::_deserialize_and_load_midi_port_states ()
{  
	XMLNode* audio_midi_settings_root = ARDOUR::Config->extra_xml ("AudioMidiSettings");
    
	if (!audio_midi_settings_root) {
		return;
	}
    
	XMLNode* midi_states = audio_midi_settings_root->child ("MidiStates");
    
	if (!midi_states) {
		return;
	}
    
	XMLNodeList state_nodes_list = midi_states->children ();
	XMLNodeConstIterator state_node_iter = state_nodes_list.begin ();
	for (; state_node_iter != state_nodes_list.end (); ++state_node_iter) {
        
		XMLNode* state_node = *state_node_iter;
		if (state_node->name () == "MidiInputs") {
            
			XMLNodeList input_state_nodes = state_node->children ();
			XMLNodeConstIterator input_state_node_iter = input_state_nodes.begin ();
			_midi_inputs.clear ();
            
			for (; input_state_node_iter != input_state_nodes.end (); ++input_state_node_iter) {
                
				XMLNode* input_state_node = *input_state_node_iter;
				XMLProperty* prop = NULL;
                
				if (input_state_node->name () != "input") {
					continue;
				}
				MidiPortState input_state (input_state_node->name ());
                
				if ((prop = input_state_node->property ("name")) == 0) {
					continue;
				}
				input_state.name = prop->value ();
                
				if ((prop = input_state_node->property ("active")) == 0) {
					continue;
				}
				input_state.active = string_is_affirmative (prop->value ());
                
				if ((prop = input_state_node->property ("scene-connected")) == 0) {
					continue;
				}
				input_state.scene_connected = string_is_affirmative (prop->value ());
                
				if ((prop = input_state_node->property ("mtc-in")) == 0) {
					continue;
				}
				input_state.mtc_in = string_is_affirmative (prop->value ());
                
				_midi_inputs.push_back (input_state);
			}
            
		} else if (state_node->name () == "MidiOutputs") {
            
			XMLNodeList output_state_nodes = state_node->children ();
			XMLNodeConstIterator output_state_node_iter = output_state_nodes.begin ();
			_midi_outputs.clear ();
            
			for (; output_state_node_iter != output_state_nodes.end (); ++output_state_node_iter) {
                
				XMLNode* output_state_node = *output_state_node_iter;
				XMLProperty* prop = NULL;
                
				if (output_state_node->name () != "output") {
					continue;
				}
				MidiPortState output_state (output_state_node->name ());
                
				if ((prop = output_state_node->property ("name")) == 0) {
					continue;
				}
				output_state.name = prop->value ();
                
				if ((prop = output_state_node->property ("active")) == 0) {
					continue;
				}
				output_state.active = string_is_affirmative (prop->value ());
                
				if ((prop = output_state_node->property ("scene-connected")) == 0) {
					continue;
				}
				output_state.scene_connected = string_is_affirmative (prop->value ());
                
				if ((prop = output_state_node->property ("mtc-in")) == 0) {
					continue;
				}
				output_state.mtc_in = string_is_affirmative (prop->value ());
                
				_midi_outputs.push_back (output_state);
			}
		}
	}
}


void
EngineStateController::_serialize_engine_states (XMLNode* audio_midi_settings_node)
{
	if (!audio_midi_settings_node) {
		return;
	}
    
	// clean up state data first
	audio_midi_settings_node->remove_nodes_and_delete ("EngineStates" );
    
	XMLNode* engine_states = new XMLNode ("EngineStates" );
    
	StateList::const_iterator state_iter = _states.begin ();
	for (; state_iter != _states.end (); ++state_iter) {
        
		StatePtr state_ptr = *state_iter;
        
		// create new node for the state
		XMLNode* state_node = new XMLNode ("State");
        
		state_node->add_property ("backend-name", state_ptr->backend_name);
		state_node->add_property ("device-name", state_ptr->device_name);
		state_node->add_property ("sample-rate", state_ptr->sample_rate);
		state_node->add_property ("buffer-size", state_ptr->buffer_size);
		state_node->add_property ("active", state_ptr->active ? "yes" : "no");
        
		// store channel states:
		// inputs
		XMLNode* input_config_node = new XMLNode ("InputConfiguration");
		PortStateList& input_channels = state_ptr->input_channel_states;
		PortStateList::const_iterator input_state_iter = input_channels.begin ();
		for (; input_state_iter != input_channels.end (); ++input_state_iter) {
			XMLNode* input_state_node = new XMLNode ("input");
			input_state_node->add_property ("name", input_state_iter->name);
			input_state_node->add_property ("active", input_state_iter->active ? "yes" : "no");
			input_config_node->add_child_nocopy (*input_state_node);
		}
		state_node->add_child_nocopy (*input_config_node);
        
		// multi out outputs
		XMLNode* multi_out_config_node = new XMLNode ("MultiOutConfiguration");
		PortStateList& multi_out_channels = state_ptr->multi_out_channel_states;
		PortStateList::const_iterator multi_out_state_iter = multi_out_channels.begin ();
		for (; multi_out_state_iter != multi_out_channels.end (); ++multi_out_state_iter) {
			XMLNode* multi_out_state_node = new XMLNode ("output" );
			multi_out_state_node->add_property ("name", multi_out_state_iter->name);
			multi_out_state_node->add_property ("active", multi_out_state_iter->active ? "yes" : "no");
			multi_out_config_node->add_child_nocopy (*multi_out_state_node);
		}
		state_node->add_child_nocopy (*multi_out_config_node);
        
		// stereo out outputs
		XMLNode* stereo_out_config_node = new XMLNode ("StereoOutConfiguration");
		PortStateList& stereo_out_channels = state_ptr->stereo_out_channel_states;
		PortStateList::const_iterator stereo_out_state_iter = stereo_out_channels.begin ();
		for (; stereo_out_state_iter != stereo_out_channels.end (); ++stereo_out_state_iter) {
			XMLNode* stereo_out_state_node = new XMLNode ("output" );
			stereo_out_state_node->add_property ("name", stereo_out_state_iter->name);
			stereo_out_state_node->add_property ("active", stereo_out_state_iter->active ? "yes" : "no");
			stereo_out_config_node->add_child_nocopy (*stereo_out_state_node);
		}
		state_node->add_child_nocopy (*stereo_out_config_node);
    
		engine_states->add_child_nocopy (*state_node);
	}

	audio_midi_settings_node->add_child_nocopy (*engine_states);
}


void
EngineStateController::_serialize_midi_port_states (XMLNode* audio_midi_settings_node)
{
	if (!audio_midi_settings_node) {
		return;
	}
    
	// clean up state data first
	audio_midi_settings_node->remove_nodes_and_delete ("MidiStates" );
    
	XMLNode* midi_states_node = new XMLNode ("MidiStates" );
    
	XMLNode* midi_input_states_node = new XMLNode ("MidiInputs" );
	MidiPortStateList::const_iterator midi_input_state_iter = _midi_inputs.begin ();
	for (; midi_input_state_iter != _midi_inputs.end (); ++midi_input_state_iter) {
		XMLNode* midi_input_node = new XMLNode ("input" );
		midi_input_node->add_property ("name", midi_input_state_iter->name);
		midi_input_node->add_property ("active", midi_input_state_iter->active ? "yes" : "no");
		midi_input_node->add_property ("scene_connected", midi_input_state_iter->scene_connected ? "yes" : "no");
		midi_input_node->add_property ("mtc-in", midi_input_state_iter->mtc_in ? "yes" : "no");
		midi_input_states_node->add_child_nocopy (*midi_input_node);
	}
	midi_states_node->add_child_nocopy (*midi_input_states_node);
    
	XMLNode* midi_output_states_node = new XMLNode ("MidiOutputs" );
	MidiPortStateList::const_iterator midi_output_state_iter = _midi_outputs.begin ();
	for (; midi_output_state_iter != _midi_outputs.end (); ++midi_output_state_iter) {
		XMLNode* midi_output_node = new XMLNode ("output" );
		midi_output_node->add_property ("name", midi_output_state_iter->name);
		midi_output_node->add_property ("active", midi_output_state_iter->active ? "yes" : "no");
		midi_output_node->add_property ("scene_connected", midi_output_state_iter->scene_connected ? "yes" : "no");
		midi_output_node->add_property ("mtc-in", midi_output_state_iter->mtc_in ? "yes" : "no");
		midi_output_states_node->add_child_nocopy (*midi_output_node);
	}
	midi_states_node->add_child_nocopy (*midi_output_states_node);

	audio_midi_settings_node->add_child_nocopy (*midi_states_node);
}


bool
EngineStateController::_apply_state (const StatePtr& state)
{
	bool applied = false;
    
	if (set_new_backend_as_current (state->backend_name)) {
		applied = set_new_device_as_current (state->device_name);
	}
    
	return applied;
}


void
EngineStateController::_do_initial_engine_setup ()
{
	bool state_applied = false;
    
	// if we have no saved state load default values
	if (!_states.empty ()) {
        
		// look for last active state first
		StateList::const_iterator state_iter = _states.begin ();
		for (; state_iter != _states.end (); ++state_iter) {
			if ( (*state_iter)->active ) {
				state_applied = _apply_state (*state_iter);
				break;
			}
		}
        
		// last active state was not applied
		// try others
		if (!state_applied) {
			StateList::const_iterator state_iter = _states.begin ();
			for (; state_iter != _states.end (); ++state_iter) {
				state_applied = _apply_state (*state_iter);
				break;
			}
		}
	}

	if (!state_applied ){
		std::vector<const AudioBackendInfo*> backends = AudioEngine::instance ()->available_backends ();
        
		if (!backends.empty ()) {
            
			if (!set_new_backend_as_current (backends.front ()->name )) {
				std::cerr << "\tfailed to set backend [" << backends.front ()->name << "]\n";
			}
		}
        
	}
}


bool
EngineStateController::_validate_current_device_state ()
{
	boost::shared_ptr<AudioBackend> backend = AudioEngine::instance ()->current_backend ();
	assert (backend);
    
	// check if device parameters from the state record are still valid
	// validate sample rate
	std::vector<float> sample_rates = backend->available_sample_rates (_current_state->device_name);
    
	if (sample_rates.empty ()) {
		return false;
	}

	// check if session desired sample rate (if it's set) could be used with this device
	if (_session != 0) {
        
		if ( !set_new_sample_rate_in_controller (_session->nominal_frame_rate ())) {
			if ( !set_new_sample_rate_in_controller (backend->default_sample_rate ()) ) {
				if (!set_new_sample_rate_in_controller (sample_rates.front ()) ) {
					return false;
				}
			}
		}
    
	} else {
		// check if current sample rate is supported because we have no session desired sample rate value
		if ( !set_new_sample_rate_in_controller (_current_state->sample_rate)) {
			if ( !set_new_sample_rate_in_controller (backend->default_sample_rate ()) ) {
				if (!set_new_sample_rate_in_controller (sample_rates.front ()) ) {
					return false;
				}
			}
		}
	}
    
	// validate buffer size
	std::vector<pframes_t> buffer_sizes = backend->available_buffer_sizes (_current_state->device_name);
	// check if buffer size is supported
	std::vector<pframes_t>::iterator bs_iter = std::find (buffer_sizes.begin (), buffer_sizes.end (), _current_state->buffer_size);
	// if current is not found switch to default if is supported
	if (bs_iter == buffer_sizes.end ()) {
		bs_iter = std::find (buffer_sizes.begin (), buffer_sizes.end (), backend->default_buffer_size (_current_state->device_name));
	
		if (bs_iter != buffer_sizes.end ()) {
			_current_state->buffer_size = backend->default_buffer_size (_current_state->device_name);
		} else {
			if (!buffer_sizes.empty ()) {
				_current_state->buffer_size = buffer_sizes.front ();
			}
		}

	}

	return true;
}


void
EngineStateController::_update_ltc_source_port ()
{
	// this method is called if the list of ports is changed
    
	// check that ltc-in port from Config still exists
	if (_audio_input_port_exists (get_ltc_source_port ())) {
		// audio port, that was saved in Config, exists
		return ;
	}

	//otherwise set first available audio port
	if (!_current_state->input_channel_states.empty ()) {
		set_ltc_source_port (_current_state->input_channel_states.front ().name);
		return ;
	}
    
	// no available audio-in ports
	set_ltc_source_port ("");
}

void
EngineStateController::_update_ltc_output_port ()
{
	// this method is called if the list of ports is changed
    
	// check that ltc-out port from Config still exists
	if (_audio_output_port_exists (get_ltc_output_port ())) {
		// audio port, that was saved in Config, exists
		return ;
	}

	PortStateList* output_states;
	if (Config->get_output_auto_connect () & AutoConnectMaster) {
		output_states = &_current_state->stereo_out_channel_states;
	} else {
		output_states = &_current_state->multi_out_channel_states;
	}
    
	//otherwise set first available audio port
	if (!output_states->empty ()) {
		set_ltc_output_port (output_states->front ().name);
		return ;
	}
    
	// no available audio-out ports
	set_ltc_output_port ("");
}
        

bool
EngineStateController::_audio_input_port_exists (const std::string& port_name)
{
	PortStateList::const_iterator iter = _current_state->input_channel_states.begin ();
	for (; iter != _current_state->input_channel_states.end (); ++iter ) {
		if (iter->name == port_name)
			return true;
	}
	return false;
}

bool
EngineStateController::_audio_output_port_exists (const std::string& port_name)
{
	PortStateList* output_states;
	if (Config->get_output_auto_connect () & AutoConnectMaster) {
		output_states = &_current_state->stereo_out_channel_states;
	} else {
		output_states = &_current_state->multi_out_channel_states;
	}
    
	PortStateList::const_iterator iter = output_states->begin ();
	for (; iter != output_states->end (); ++iter ) {
		if (iter->name == port_name)
			return true;
	}
	return false;
}


const std::string&
EngineStateController::get_current_backend_name () const
{
	return _current_state->backend_name;
}


const std::string&
EngineStateController::get_current_device_name () const
{
	return _current_state->device_name;
}


void
EngineStateController::available_backends (std::vector<const AudioBackendInfo*>& available_backends)
{
	available_backends = AudioEngine::instance ()->available_backends ();
}


void
EngineStateController::enumerate_devices (std::vector<AudioBackend::DeviceStatus>& device_vector) const
{
	boost::shared_ptr<AudioBackend> backend = AudioEngine::instance ()->current_backend ();
	assert (backend);
	device_vector = backend->enumerate_devices ();
}


framecnt_t
EngineStateController::get_current_sample_rate () const
{
	return _current_state->sample_rate;
}


framecnt_t
EngineStateController::get_default_sample_rate () const
{
	boost::shared_ptr<AudioBackend> backend = AudioEngine::instance ()->current_backend ();
	assert (backend);
	return backend->default_sample_rate ();
}


void
EngineStateController::available_sample_rates_for_current_device (std::vector<float>& sample_rates) const
{
	boost::shared_ptr<AudioBackend> backend = AudioEngine::instance ()->current_backend ();
	assert (backend);
	sample_rates = backend->available_sample_rates (_current_state->device_name);
}


uint32_t
EngineStateController::get_current_buffer_size () const
{
	return _current_state->buffer_size;
}


uint32_t
EngineStateController::get_default_buffer_size () const
{
	boost::shared_ptr<AudioBackend> backend = AudioEngine::instance ()->current_backend ();
	assert (backend);
	return backend->default_buffer_size (_current_state->device_name);
}


void
EngineStateController::available_buffer_sizes_for_current_device (std::vector<pframes_t>& buffer_sizes) const
{
	boost::shared_ptr<AudioBackend> backend = AudioEngine::instance ()->current_backend ();
	assert (backend);
	buffer_sizes = backend->available_buffer_sizes (_current_state->device_name);
}


bool
EngineStateController::set_new_backend_as_current (const std::string& backend_name)
{
	if (backend_name == AudioEngine::instance ()->current_backend_name ()) {
		return true;
	}
    
	boost::shared_ptr<AudioBackend> backend = AudioEngine::instance ()->set_backend (backend_name, PROGRAM_NAME, "");
	if (backend)
	{
		if (_current_state != NULL) {
			_current_state->active = false;
		}
        
		StateList::iterator found_state_iter = find_if (_states.begin (), _states.end (),
		                                                State::StatePredicate (backend_name, "None"));
        
		if (found_state_iter != _states.end ()) {
			// we found a record for new engine with None device - switch to it
			_current_state = *found_state_iter;
			_validate_current_device_state ();
		} else {
			// create new record for this engine with default device
			_current_state = boost::shared_ptr<State>(new State ());
			_current_state->backend_name = backend_name;
			_current_state->device_name = "None";
			_validate_current_device_state ();
			_states.push_front (_current_state);
		}
        
		push_current_state_to_backend (false);
        
		return true;
	}
    
	return false;
}


bool
EngineStateController::set_new_device_as_current (const std::string& device_name)
{
	if (_current_state->device_name == device_name) {
		return true;
	}
    
	boost::shared_ptr<AudioBackend> backend = AudioEngine::instance ()->current_backend ();
	assert (backend);
    
	std::vector<AudioBackend::DeviceStatus> device_vector = backend->enumerate_devices ();
    
	// validate the device
	std::vector<AudioBackend::DeviceStatus>::iterator device_iter;
	device_iter = std::find_if (device_vector.begin (), device_vector.end (), DevicePredicate (device_name));
    
	// device is available
	if (device_iter != device_vector.end ()) {
        
		boost::shared_ptr<State> previous_state (_current_state);
        
		// look through state list and find the record for this device and current engine
		StateList::iterator found_state_iter = find_if (_states.begin (), _states.end (),
		                                                State::StatePredicate (backend->name (), device_name));
        
		if (found_state_iter != _states.end ())
		{
			// we found a record for current engine and provided device name - switch to it
            
			_current_state = *found_state_iter;
            
			if (!_validate_current_device_state ()) {
				_current_state = previous_state;
				return false;
			}
        
		} else {
       
			// the record is not found, create new one
			_current_state = boost::shared_ptr<State>(new State ());
            
			_current_state->backend_name = backend->name ();
			_current_state->device_name = device_name;
            
			if (!_validate_current_device_state ()) {
				_current_state = previous_state;
				return false;
			}
            
			_states.push_front (_current_state);
		}
        
		if (previous_state != NULL) {
			previous_state->active = false;
		}
        
		push_current_state_to_backend (false);

		_last_used_real_device.clear ();
        
		if (device_name != "None") {
			_last_used_real_device = device_name;
		}
        
		return true;
	}
    
	// device is not supported by current backend
	return false;
}


bool
EngineStateController::set_new_sample_rate_in_controller (framecnt_t sample_rate)
{
	boost::shared_ptr<AudioBackend> backend = AudioEngine::instance ()->current_backend ();
	assert (backend);
    
	std::vector<float> sample_rates = backend->available_sample_rates (_current_state->device_name);
	std::vector<float>::iterator iter = std::find (sample_rates.begin (), sample_rates.end (), (float)sample_rate);
    
	if (iter != sample_rates.end ()) {
		_current_state->sample_rate = sample_rate;
		return true;
	}
    
	return false;
}


bool
EngineStateController::set_new_buffer_size_in_controller (pframes_t buffer_size)
{    
	boost::shared_ptr<AudioBackend> backend = AudioEngine::instance ()->current_backend ();
	assert (backend);
    
	std::vector<uint32_t> buffer_sizes = backend->available_buffer_sizes (_current_state->device_name);
	std::vector<uint32_t>::iterator iter = std::find (buffer_sizes.begin (), buffer_sizes.end (), buffer_size);

	if (iter != buffer_sizes.end ()) {
		_current_state->buffer_size = buffer_size;
		return true;
	}
    
	return false;
}


uint32_t
EngineStateController::get_available_inputs_count () const
{
	uint32_t available_channel_count = 0;
    
	PortStateList::const_iterator iter = _current_state->input_channel_states.begin ();
    
	for (; iter != _current_state->input_channel_states.end (); ++iter) {
		if (iter->active) {
			++available_channel_count;
		}
	}
    
	return available_channel_count;
}


uint32_t
EngineStateController::get_available_outputs_count () const
{
	uint32_t available_channel_count = 0;
    
	PortStateList* output_states;
	if (Config->get_output_auto_connect () & AutoConnectMaster) {
		output_states = &_current_state->stereo_out_channel_states;
	} else {
		output_states = &_current_state->multi_out_channel_states;
	}
    
	PortStateList::const_iterator iter = output_states->begin ();
    
	for (; iter != output_states->end (); ++iter) {
		if (iter->active) {
			++available_channel_count;
		}
	}
    
	return available_channel_count;
}


void
EngineStateController::get_physical_audio_inputs (std::vector<std::string>& port_names)
{
	port_names.clear ();
    
	PortStateList &input_states = _current_state->input_channel_states;
    
	PortStateList::iterator iter = input_states.begin ();
	for (; iter != input_states.end (); ++iter) {
		if (iter->active) {
			port_names.push_back (iter->name);
		}
	}
}


void
EngineStateController::get_physical_audio_outputs (std::vector<std::string>& port_names)
{
	port_names.clear ();
    
	PortStateList* output_states;
	if (Config->get_output_auto_connect () & AutoConnectMaster) {
		output_states = &_current_state->stereo_out_channel_states;
	} else {
		output_states = &_current_state->multi_out_channel_states;
	}
    
	PortStateList::iterator iter = output_states->begin ();
	for (; iter != output_states->end (); ++iter) {
		if (iter->active) {
			port_names.push_back (iter->name);
		}
	}
}


void
EngineStateController::get_physical_midi_inputs (std::vector<std::string>& port_names)
{
	port_names.clear ();
    
	MidiPortStateList::iterator iter = _midi_inputs.begin ();
	for (; iter != _midi_inputs.end (); ++iter) {
		if (iter->available && iter->active) {
			port_names.push_back (iter->name);
		}
	}
}


void
EngineStateController::get_physical_midi_outputs (std::vector<std::string>& port_names)
{
	port_names.clear ();
    
	MidiPortStateList::iterator iter = _midi_outputs.begin ();
	for (; iter != _midi_outputs.end (); ++iter) {
		if (iter->available && iter->active) {
			port_names.push_back (iter->name);
		}
	}
}


void
EngineStateController::set_physical_audio_input_state (const std::string& port_name, bool state)
{
	PortStateList &input_states = _current_state->input_channel_states;
	PortStateList::iterator found_state_iter;
	found_state_iter = std::find (input_states.begin (), input_states.end (), PortState (port_name));
    
	if (found_state_iter != input_states.end () && found_state_iter->active != state ) {
		found_state_iter->active = state;
		AudioEngine::instance ()->reconnect_session_routes (true, false);
        
		InputConfigChanged ();
	}
}


void
EngineStateController::set_physical_audio_output_state (const std::string& port_name, bool state)
{
	PortStateList* output_states;
	if (Config->get_output_auto_connect () & AutoConnectMaster) {
		output_states = &_current_state->stereo_out_channel_states;
	} else {
		output_states = &_current_state->multi_out_channel_states;
	}
    
	PortStateList::iterator target_state_iter;
	target_state_iter = std::find (output_states->begin (), output_states->end (), PortState (port_name));
    
	if (target_state_iter != output_states->end () && target_state_iter->active != state ) {
		target_state_iter->active = state;
        
		// if StereoOut mode is used
		if (Config->get_output_auto_connect () & AutoConnectMaster) {
        
			// get next element
			PortStateList::iterator next_state_iter (target_state_iter);
            
			// loopback
			if (++next_state_iter == output_states->end ()) {
				next_state_iter = output_states->begin ();
			}
            
            
			// only two outputs should be enabled
			if (output_states->size () <= 2) {
                
				target_state_iter->active = true;
				next_state_iter->active = true;
                
			} else {
                
				// if current was set to active - activate next and disable the rest
				if (target_state_iter->active ) {
					next_state_iter->active = true;
				} else {
					// if current was deactivated but the next is active
					if (next_state_iter->active) {
						if (++next_state_iter == output_states->end ()) {
							next_state_iter = output_states->begin ();
						}
						next_state_iter->active = true;
					} else {
						// if current was deactivated but the previous is active - restore the state of current
						target_state_iter->active = true; // state restored;
						--target_state_iter; // switch to previous to make it stop point in the next cycle
						target_state_iter->active = true;
					}
				}
                
				// now deactivate the rest
				while (++next_state_iter != target_state_iter) {
                    
					if (next_state_iter == output_states->end ()) {
						next_state_iter = output_states->begin ();
						// we jumped, so additional check is required
						if (next_state_iter == target_state_iter) {
							break;
						}
					}
                    
					next_state_iter->active = false;
				}
                
			}
		}
            
		AudioEngine::instance ()->reconnect_session_routes (false, true);
		OutputConfigChanged ();
	}
}


bool
EngineStateController::get_physical_audio_input_state (const std::string& port_name)
{
	bool state = false;
    
	PortStateList &input_states = _current_state->input_channel_states;
	PortStateList::iterator found_state_iter;
	found_state_iter = std::find (input_states.begin (), input_states.end (), PortState (port_name));
    
	if (found_state_iter != input_states.end ()) {
		state = found_state_iter->active;
	}
    
	return state;
}


bool
EngineStateController::get_physical_audio_output_state (const std::string& port_name)
{
	bool state = false;
    
	PortStateList* output_states;
	if (Config->get_output_auto_connect () & AutoConnectMaster) {
		output_states = &_current_state->stereo_out_channel_states;
	} else {
		output_states = &_current_state->multi_out_channel_states;
	}
    
	PortStateList::iterator found_state_iter;
	found_state_iter = std::find (output_states->begin (), output_states->end (), PortState (port_name));
    
	if (found_state_iter != output_states->end ()) {
		state = found_state_iter->active;
	}

	return state;
}


void
EngineStateController::set_physical_midi_input_state (const std::string& port_name, bool state) {
    
	MidiPortStateList::iterator found_state_iter;
	found_state_iter = std::find (_midi_inputs.begin (), _midi_inputs.end (), MidiPortState (port_name));
    
	if (found_state_iter != _midi_inputs.end () && found_state_iter->available && found_state_iter->active != state ) {
		found_state_iter->active = state;
        
		if (_session) {
			// reconnect MTC inputs as well
			if (found_state_iter->mtc_in) {
				_session->reconnect_mtc_ports ();
			}
			_session->reconnect_mmc_ports (true);
		}
        
		MIDIInputConfigChanged ();
	}
}


void
EngineStateController::set_physical_midi_output_state (const std::string& port_name, bool state) {
    
	MidiPortStateList::iterator found_state_iter;
	found_state_iter = std::find (_midi_outputs.begin (), _midi_outputs.end (), MidiPortState (port_name));
    
	if (found_state_iter != _midi_outputs.end () && found_state_iter->available && found_state_iter->active != state ) {
		found_state_iter->active = state;
        
		if (_session) {
			_session->reconnect_mmc_ports (false);
		}
        
		MIDIOutputConfigChanged ();
	}
}


bool
EngineStateController::get_physical_midi_input_state (const std::string& port_name, bool& scene_connected) {
    
	bool state = false;
    
	MidiPortStateList::iterator found_state_iter;
	found_state_iter = std::find (_midi_inputs.begin (), _midi_inputs.end (), MidiPortState (port_name));
    
	if (found_state_iter != _midi_inputs.end () && found_state_iter->available) {
		state = found_state_iter->active;
		scene_connected = found_state_iter->scene_connected;
	}
    
	return state;
}


bool
EngineStateController::get_physical_midi_output_state (const std::string& port_name, bool& scene_connected) {
    
	bool state = false;
    
	MidiPortStateList::iterator found_state_iter;
	found_state_iter = std::find (_midi_outputs.begin (), _midi_outputs.end (), MidiPortState (port_name));
    
	if (found_state_iter != _midi_outputs.end () && found_state_iter->available) {
		state = found_state_iter->active;
		scene_connected = found_state_iter->scene_connected;
	}
    
	return state;
}


void
EngineStateController::set_physical_midi_scene_in_connection_state (const std::string& port_name, bool state) {
    
	MidiPortStateList::iterator found_state_iter;
	found_state_iter = std::find (_midi_inputs.begin (), _midi_inputs.end (), MidiPortState (port_name));
    
	if (found_state_iter != _midi_inputs.end () && found_state_iter->available && found_state_iter->active ) {
		found_state_iter->scene_connected = state;
        
		std::vector<std::string> ports;
		ports.push_back (port_name);
		MIDISceneInputConnectionChanged (ports, state);
	}

}


void
EngineStateController::set_physical_midi_scenen_out_connection_state (const std::string& port_name, bool state) {
   
	MidiPortStateList::iterator found_state_iter;
	found_state_iter = std::find (_midi_outputs.begin (), _midi_outputs.end (), MidiPortState (port_name));
    
	if (found_state_iter != _midi_outputs.end () && found_state_iter->available && found_state_iter->active ) {
		found_state_iter->scene_connected = state;
        
		std::vector<std::string> ports;
		ports.push_back (port_name);
		MIDISceneOutputConnectionChanged (ports, state);
	}

}


void
EngineStateController::set_all_midi_scene_inputs_disconnected ()
{
	MidiPortStateList::iterator iter = _midi_inputs.begin ();
	for (; iter != _midi_inputs.end (); ++iter) {
		iter->scene_connected = false;
	}

	std::vector<std::string> ports;
	MIDISceneInputConnectionChanged (ports, false);
}


void
EngineStateController::set_all_midi_scene_outputs_disconnected ()
{
	MidiPortStateList::iterator iter = _midi_outputs.begin ();
	for (; iter != _midi_outputs.end (); ++iter) {
		iter->scene_connected = false;
	}
    
	std::vector<std::string> ports;
	MIDISceneOutputConnectionChanged (ports, false);
}


void
EngineStateController::set_mtc_source_port (const std::string& port_name)
{
	MidiPortStateList::iterator iter = _midi_inputs.begin ();
	for (; iter != _midi_inputs.end (); ++iter) {
		iter->mtc_in = false;
        
		if (iter->name == port_name) {
			iter->mtc_in = true;
            
			if (_session) {
				_session->reconnect_mtc_ports ();
			}
		}
	}
    
	if (_session && port_name.empty ()) {
		_session->reconnect_mtc_ports ();
	}
    
	MTCInputChanged (port_name);
}


void
EngineStateController::set_state_to_all_inputs (bool state)
{
	bool something_changed = false;
    
	PortStateList::iterator iter = _current_state->input_channel_states.begin ();
	for (; iter != _current_state->input_channel_states.end (); ++iter) {
		if (iter->active != state) {
			iter->active = state;
			something_changed = true;
		}
	}
    
	if (something_changed) {
		AudioEngine::instance ()->reconnect_session_routes (true, false);
		InputConfigChanged ();
	}
}


void
EngineStateController::set_state_to_all_outputs (bool state)
{
	// unapplicable in Stereo Out mode, just return
	if (Config->get_output_auto_connect () & AutoConnectMaster) {
		return;
	}
    
	bool something_changed = false;
    
	PortStateList::iterator iter = _current_state->multi_out_channel_states.begin ();
	for (; iter != _current_state->multi_out_channel_states.end (); ++iter) {
		if (iter->active != state) {
			iter->active = state;
			something_changed = true;
		}
	}
    
	if (something_changed) {
		AudioEngine::instance ()->reconnect_session_routes (false, true);
		OutputConfigChanged ();
	}
}


void
EngineStateController::get_physical_audio_input_states (std::vector<PortState>& channel_states)
{
	PortStateList &input_states = _current_state->input_channel_states;
	channel_states.assign (input_states.begin (), input_states.end ());
}


void
EngineStateController::get_physical_audio_output_states (std::vector<PortState>& channel_states)
{
	PortStateList* output_states;
	if (Config->get_output_auto_connect () & AutoConnectMaster) {
		output_states = &_current_state->stereo_out_channel_states;
	} else {
		output_states = &_current_state->multi_out_channel_states;
	}
    
	channel_states.assign (output_states->begin (), output_states->end ());
}


void
EngineStateController::get_physical_midi_input_states (std::vector<MidiPortState>& channel_states)
{
	channel_states.clear ();
    
	MidiPortStateList::iterator iter = _midi_inputs.begin ();
    
	for (; iter != _midi_inputs.end (); ++iter ) {
		if (iter->available) {
			MidiPortState state (iter->name);
			state.active = iter->active;
			state.available = true;
			state.scene_connected = iter->scene_connected;
			state.mtc_in = iter->mtc_in;
			channel_states.push_back (state);
		}
	}    
}

void
EngineStateController::get_physical_midi_output_states (std::vector<MidiPortState>& channel_states)
{
	channel_states.clear ();
    
	MidiPortStateList::iterator iter = _midi_outputs.begin ();
    
	for (; iter != _midi_outputs.end (); ++iter ) {
		if (iter->available) {
			MidiPortState state (iter->name);
			state.active = iter->active;
			state.available = true;
			state.scene_connected = iter->scene_connected;
			state.mtc_in = iter->mtc_in;
			channel_states.push_back (state);
		}
	}
}


void
EngineStateController::_on_session_loaded ()
{
	if (!_session) {
		return;
	}
    
	AudioEngine::instance ()->reconnect_session_routes (true, true);
	_session->reconnect_mtc_ports ();
	_session->reconnect_mmc_ports (true);
	_session->reconnect_mmc_ports (false);
    
	// This is done during session construction
	// _session->reconnect_ltc_input ();
	// _session->reconnect_ltc_output ();
	
	framecnt_t desired_sample_rate = _session->nominal_frame_rate ();
	if ( desired_sample_rate > 0 && set_new_sample_rate_in_controller (desired_sample_rate))
	{
		push_current_state_to_backend (false);
		SampleRateChanged (); // emit a signal
	}
}


void
EngineStateController::_on_sample_rate_change (framecnt_t new_sample_rate)
{
	if (_current_state->sample_rate != new_sample_rate) {
        
		// if sample rate has been changed
		framecnt_t sample_rate_to_set = new_sample_rate;
		if (AudioEngine::instance ()->session ()) {
			// and we have current session we should restore it back to the one tracks uses
			sample_rate_to_set = AudioEngine::instance ()->session ()->frame_rate ();
		}
        
		if ( !set_new_sample_rate_in_controller (sample_rate_to_set)) {
			// if sample rate can't be set
			// switch to NONE device
			set_new_device_as_current ("None");
			DeviceListChanged (false);
			DeviceError ();
		}
	}
    
	SampleRateChanged (); // emit a signal
}


void
EngineStateController::_on_buffer_size_change (pframes_t new_buffer_size)
{
	if (_current_state->buffer_size != new_buffer_size) {
		_current_state->buffer_size = new_buffer_size;
	}
    
	BufferSizeChanged (); // emit a signal
}


void
EngineStateController::_on_device_list_change ()
{
	bool current_device_disconnected = false;
    
	boost::shared_ptr<AudioBackend> backend = AudioEngine::instance ()->current_backend ();
	assert (backend);
    
	std::vector<AudioBackend::DeviceStatus> device_vector = backend->enumerate_devices ();
    
	// find out out if current device is still available if it's not None
	if (_current_state->device_name != "None")
	{
		std::vector<AudioBackend::DeviceStatus>::iterator device_iter;
		device_iter = std::find_if (device_vector.begin (), device_vector.end (), DevicePredicate (_current_state->device_name));
        
		// if current device is not available any more - switch to None device
		if (device_iter == device_vector.end ()) {
            
			StateList::iterator found_state_iter = find_if (_states.begin (), _states.end (),
			                                                State::StatePredicate (_current_state->backend_name, "None"));
            
			if (found_state_iter != _states.end ()) {
				// found the record - switch to it
				_current_state = *found_state_iter;
				_validate_current_device_state ();
			} else {
				// create new record for this engine with default device
				_current_state = boost::shared_ptr<State>(new State ());
				_current_state->backend_name = backend->name ();
				_current_state->device_name = "None";
				_validate_current_device_state ();
				_states.push_front (_current_state);
			}
            
			push_current_state_to_backend (true);
			current_device_disconnected = true;
		}
	} else {
		// if the device which was active before is available now - switch to it
        
		std::vector<AudioBackend::DeviceStatus>::iterator device_iter;
		device_iter = std::find_if (device_vector.begin (), device_vector.end (), DevicePredicate (_last_used_real_device));
        
		if (device_iter != device_vector.end ()) {
			StateList::iterator found_state_iter = find_if (_states.begin (), _states.end (),
			                                                State::StatePredicate (_current_state->backend_name,
			                                                                      _last_used_real_device));
            
			if (found_state_iter != _states.end ()) {
                
				boost::shared_ptr<State> previous_state (_current_state);
				_current_state = *found_state_iter;
                
				if (_validate_current_device_state ()) {
					push_current_state_to_backend (false);
				} else {
					// cannot use this device right now
					_last_used_real_device.clear ();
					_current_state = previous_state;
				}
			}
		}
	}
    
	DeviceListChanged (current_device_disconnected); // emit a signal    
}


void
EngineStateController::_update_device_channels_state ()
{
	boost::shared_ptr<AudioBackend> backend = AudioEngine::instance ()->current_backend ();
	assert (backend);
    
	// update audio input states
	std::vector<std::string> phys_audio_inputs;
	backend->get_physical_inputs (DataType::AUDIO, phys_audio_inputs);
    
	PortStateList new_input_states;
	PortStateList &input_states = _current_state->input_channel_states;
    
	std::vector<std::string>::const_iterator input_iter = phys_audio_inputs.begin ();
	for (; input_iter != phys_audio_inputs.end (); ++input_iter) {
        
		PortState state (*input_iter);
		state.active = true;
		PortStateList::const_iterator found_state_iter = std::find (input_states.begin (), input_states.end (), state);
        
		if (found_state_iter != input_states.end ()) {
			new_input_states.push_back (*found_state_iter);
		} else {
			new_input_states.push_back (state);
		}
	}
	_current_state->input_channel_states = new_input_states;
    
	// update audio output states (multi out mode)
	std::vector<std::string> phys_audio_outputs;
	backend->get_physical_outputs (DataType::AUDIO, phys_audio_outputs);
    
	PortStateList new_output_states;
	PortStateList &output_multi_states = _current_state->multi_out_channel_states;
    
	std::vector<std::string>::const_iterator output_iter = phys_audio_outputs.begin ();
	for (; output_iter != phys_audio_outputs.end (); ++output_iter) {
        
		PortState state (*output_iter);
		state.active = true;
		PortStateList::const_iterator found_state_iter = std::find (output_multi_states.begin (), output_multi_states.end (), state);
        
		if (found_state_iter != output_multi_states.end ()) {
			new_output_states.push_back (*found_state_iter);
		} else {
			new_output_states.push_back (state);
		}
	}
    
	_current_state->multi_out_channel_states = new_output_states;
    
	// update audio output states (stereo out mode)
	new_output_states.clear ();
	PortStateList &output_stereo_states = _current_state->stereo_out_channel_states;
    
	output_iter = phys_audio_outputs.begin ();
	for (; output_iter != phys_audio_outputs.end (); ++output_iter) {
        
		PortState state (*output_iter);
		state.active = true;
		PortStateList::const_iterator found_state_iter = std::find (output_stereo_states.begin (), output_stereo_states.end (), state);
        
		if (found_state_iter != output_stereo_states.end ()) {
			new_output_states.push_back (*found_state_iter);
		} else {
			new_output_states.push_back (state);
		}
	}

	_current_state->stereo_out_channel_states = new_output_states;
	_refresh_stereo_out_channel_states ();
    
    
	// update midi ports: unlike audio ports which states are saved per device
	// each midi port state is saved individualy
	// so get all midi ports from the backend
	// and compare to the list of midi ports we have
	// if physical port is new, add it to our state list
	// if physical port is present in our state list - mark it available
	// if there is no corresponding physical port to one we have in a list - leave it unavailable
	MidiPortStateList::iterator iter = _midi_inputs.begin ();
	for (; iter != _midi_inputs.end (); ++iter) {
		iter->available = false;
	}
    
	for (iter = _midi_outputs.begin (); iter != _midi_outputs.end (); ++iter) {
		iter->available = false;
	}
    
	// update midi input ports
	std::vector<std::string> phys_midi_inputs;
	backend->get_physical_inputs (DataType::MIDI, phys_midi_inputs);
    
	std::vector<std::string>::const_iterator midi_input_iter = phys_midi_inputs.begin ();
	for (; midi_input_iter != phys_midi_inputs.end (); ++midi_input_iter) {
        
		MidiPortState state (*midi_input_iter);
		state.active = false;
		state.available = true;
		MidiPortStateList::iterator found_state_iter = std::find (_midi_inputs.begin (), _midi_inputs.end (), state);
        
		if (found_state_iter != _midi_inputs.end ()) {
			found_state_iter->available = true;
		} else {
			_midi_inputs.push_back (state);
		}
	}
    
	// update midi output ports
	std::vector<std::string> phys_midi_outputs;
	backend->get_physical_outputs (DataType::MIDI, phys_midi_outputs);
    
	std::vector<std::string>::const_iterator midi_output_iter = phys_midi_outputs.begin ();
	for (; midi_output_iter != phys_midi_outputs.end (); ++midi_output_iter) {
        
		MidiPortState state (*midi_output_iter);
		state.active = false;
		state.available = true;
		MidiPortStateList::iterator found_state_iter = std::find (_midi_outputs.begin (), _midi_outputs.end (), state);
        
		if (found_state_iter != _midi_outputs.end ()) {
			found_state_iter->available = true;
		} else {
			_midi_outputs.push_back (state);
		}
	}
}


void
EngineStateController::_refresh_stereo_out_channel_states ()
{
	PortStateList &output_states = _current_state->stereo_out_channel_states;
	PortStateList::iterator active_iter = output_states.begin ();

	for (; active_iter != output_states.end (); ++active_iter) {
		if (active_iter->active) {
			break;
		}
	}
    
	uint32_t pending_active_channels = 2;
	PortStateList::iterator iter = output_states.begin ();
	// if found active
	if (active_iter != output_states.end ()) {
		iter = active_iter;
		if (++iter == output_states.end ()) {
			iter = output_states.begin ();
		}
            
		(iter++)->active = true;
		pending_active_channels = 0;
	}

	// drop the rest of the states to false (until we reach the end or first existing active channel)
	for (; iter != output_states.end () && iter != active_iter; ++iter) {
		if (pending_active_channels) {
			iter->active = true;
			--pending_active_channels;
		} else {
			iter->active = false;
		}
	}
}


void
EngineStateController::_on_engine_running ()
{
	AudioEngine::instance ()->reconnect_session_routes (true, true);
	_current_state->active = true;
    
	EngineRunning (); // emit a signal
}


void
EngineStateController::_on_engine_stopped ()
{
	EngineStopped ();
}


void
EngineStateController::_on_engine_halted ()
{
	EngineHalted ();
}


void
EngineStateController::_on_device_error ()
{
	set_new_device_as_current ("None");
	push_current_state_to_backend (true);
	DeviceListChanged (false);
	DeviceError ();
}


void
EngineStateController::_on_parameter_changed (const std::string& parameter_name)
{
	if (parameter_name == "output-auto-connect") {
        
		AudioEngine::instance ()->reconnect_session_routes (false, true);
		OutputConfigChanged (); // emit a signal
		OutputConnectionModeChanged (); // emit signal
	}
}


void
EngineStateController::_on_ports_registration_update ()
{
	_update_device_channels_state ();
    
	// update MIDI connections
	if (_session) {
		_session->reconnect_midi_scene_ports (true);
		_session->reconnect_midi_scene_ports (false);
        
		_session->reconnect_mtc_ports ();
        
		_session->reconnect_mmc_ports (true);
		_session->reconnect_mmc_ports (false);
        
		_session->reconnect_ltc_input ();
		_session->reconnect_ltc_output ();
	}
    
	_update_ltc_source_port ();
	_update_ltc_output_port ();
    
	PortRegistrationChanged (); // emit a signal
}


bool
EngineStateController::push_current_state_to_backend (bool start)
{
	boost::shared_ptr<AudioBackend> backend = AudioEngine::instance ()->current_backend ();
	
	if (!backend) {
		return false;
	}
    
	// check if anything changed
	bool state_changed = (_current_state->device_name != backend->device_name ()) ||
		(_current_state->sample_rate != backend->sample_rate ()) ||
		(_current_state->buffer_size != backend->buffer_size ());
    
	bool was_running = AudioEngine::instance ()->running ();
    
	Glib::Threads::RecMutex::Lock sl (AudioEngine::instance ()->state_lock ());
	if (state_changed) {

		if (was_running) {
            
			if (_current_state->device_name != backend->device_name ()) {
				// device has been changed
				// the list of ports has been changed too
				// current ltc_source_port and ltc_output_port aren't available
				set_ltc_source_port ("");
				set_ltc_output_port ("");
			}
            
			if (AudioEngine::instance ()->stop ()) {
				return false;
			}
		}

		int result = 0;
		{
			std::cout << "EngineStateController::Setting device: " << _current_state->device_name << std::endl;
			if ((_current_state->device_name != backend->device_name ()) && (result = backend->set_device_name (_current_state->device_name))) {
				error << string_compose (_("Cannot set device name to %1"), get_current_device_name ()) << endmsg;
			}
        
			if (!result ) {
				std::cout << "EngineStateController::Setting device sample rate " << _current_state->sample_rate << std::endl;
				result = backend->set_sample_rate (_current_state->sample_rate);
				
				if (result) {
					error << string_compose (_("Cannot set sample rate to %1"), get_current_sample_rate ()) << endmsg;
				}
			}
        
			if (!result ) {
				std::cout << "EngineStateController::Setting device buffer size " << _current_state->buffer_size << std::endl;
				result = backend->set_buffer_size (_current_state->buffer_size);

				if (result) {
					error << string_compose (_("Cannot set buffer size to %1"), get_current_buffer_size ()) << endmsg;
				}
			}
		}
        
		if (result) // error during device setup
		{
			//switch to None device and notify about the issue
			set_new_device_as_current ("None");
			DeviceListChanged (false);
			DeviceError ();
		}

		if (AudioEngine::instance ()->backend_reset_requested ()) {
			// device asked for reset, do not start engine now
			// free sate lock and let Engine reset the device as it's required
			return true;
		}
	}
    
	if (start || (was_running && state_changed)) {
		if (AudioEngine::instance ()->start () && !AudioEngine::instance ()->is_reset_requested ()) {
			//switch to None device and notify about the issue
			set_new_device_as_current ("None");
			AudioEngine::instance ()->start ();
			DeviceListChanged (false);
			DeviceError ();
			return false;
		}
	}
    
	save_audio_midi_settings ();
    
	return true;
}


std::string
EngineStateController::get_mtc_source_port ()
{
	MidiPortStateList::const_iterator state_iter = _midi_inputs.begin ();
	for (; state_iter != _midi_inputs.end (); ++state_iter) {
		if (state_iter->available && state_iter->mtc_in) {
			return (state_iter->name);
		}
	}
    
	return "";
}

void
EngineStateController::set_ltc_source_port (const std::string& port)
{
	Config->set_ltc_source_port (port);
}

std::string
EngineStateController::get_ltc_source_port ()
{
	return Config->get_ltc_source_port ();
}

void
EngineStateController::set_ltc_output_port (const std::string& port)
{
	Config->set_ltc_output_port (port);
}

std::string
EngineStateController::get_ltc_output_port ()
{
	return Config->get_ltc_output_port ();
}

