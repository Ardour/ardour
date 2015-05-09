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

#ifndef __gtk2_ardour__engine_state_controller__
#define __gtk2_ardour__engine_state_controller__

#include <vector>
#include <list>

#include "ardour/types.h"
#include "ardour/audio_backend.h"

namespace ARDOUR {

class AudioBackendInfo;
    
/**
 * @class EngineStateController
 * @brief EngineStateController class.
 *
 * Implements usecases for Audio devices and Audio/Midi ports.
 * Persistantly saves to the config device configuration settings and audio/midi port states
 */
class EngineStateController
{
   public:
    
	// public data types:
    
	/** 
	 * @struct PortState
	 * Structure which represents AudioPort state
	 */
	struct PortState {
		std::string name; ///< Audio Port name
		bool active;      ///< Audio Port state
        
		PortState ()
			: name(""),
			  active(false)
		{
		}
        
		PortState (const std::string& name)
			: name(name),
			  active(false)
		{
		}
        
		bool operator==(const PortState& rhs) {return rhs.name == name; }
        
	};
    
	/// @typedef Type for the list of all available audio ports
	typedef std::list<PortState> PortStateList;

	/**
	 * @struct MidiPortState
	 * Structure which represents MidiPort state.
	 */
	struct MidiPortState
	{
		std::string name;     ///< Midi Port name
		bool active;          ///< Midi Port state
		bool available;       ///< Midi Port availability - if it is physicaly available or not
		bool scene_connected; ///< Is midi port used for scene MIDI marker in/out
		bool mtc_in;          ///< Is midi port used as MTC in
        
		MidiPortState(const std::string& name):
			name(name),
			active(false),
			available(false),
			scene_connected(false),
			mtc_in(false)
		{}
        
		bool operator==(const MidiPortState& rhs)
		{
			return name == rhs.name;
		}
	};
    
	/// @typedef Type for the list of MidiPorts ever registered in the system
	typedef std::list<MidiPortState> MidiPortStateList;
    

	//Interfaces
    
	/** Get an instance of EngineStateController singleton.
	 * @return EngineStateController instance pointer
	 */
	static EngineStateController* instance ();
    
	/** Associate session with EngineStateController instance.
	 */
	void set_session (Session* session);
    
	/** Remove link to the associated session.
	 */
	void remove_session ();
    
	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	// General backend/device information methods
    
	/** Provides names of all available backends.
	 *
	 * @param[out] available_backends - vector of available backends
	 */
	void available_backends (std::vector<const AudioBackendInfo*>& available_backends);
    
	/** Provides the name of currently used backend.
	 *
	 * @return the name of currently used backend
	 */
	const std::string& get_current_backend_name() const;
    
	/** Provides the name of currently used device.
	 *
	 * @return the name of currently used device
	 */
	const std::string& get_current_device_name () const;
    
	/** Provides names for all available devices.
	 *
	 * @param[out] device_vector - vector of available devices
	 */
	void enumerate_devices (std::vector<ARDOUR::AudioBackend::DeviceStatus>& device_vector) const;
    
	/** Get sample rate used by current device.
	 *
	 * @return current sample rate
	 */
	ARDOUR::framecnt_t get_current_sample_rate () const;
    
	/** Get default sample rate for current backend.
	 *
	 * @return default sample rate for current backend
	 */
	ARDOUR::framecnt_t get_default_sample_rate () const;
    
	/** Get sample rates which are supported by current device and current backend.
	 *
	 * @param[out] sample_rates - vector of supported sample rates
	 */
	void available_sample_rates_for_current_device (std::vector<float>& sample_rates) const;
    
	/** Get buffer size used by current device.
	 *
	 * @return current buffer size
	 */
	ARDOUR::pframes_t get_current_buffer_size () const;
    
	/** Get default buffer size for current backend.
	 *
	 * @return default buffer size for current backend
	 */
	ARDOUR::pframes_t get_default_buffer_size () const;
    
	/** Get buffer sizes which are supported by current device and current backend.
	 *
	 * @param[out] buffer_sizes - vector of supported buffer_sizes
	 */
	void available_buffer_sizes_for_current_device (std::vector<ARDOUR::pframes_t>& buffer_sizes) const;

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// device state control methods
    
	/** Get the number of all enabled Audio inputs.
	 *
	 * @return number of all enabled Audio inputs
	 */
	uint32_t            get_available_inputs_count() const;
	/** Get the number of all enabled Audio outputs.
	 *
	 * @return number of all enabled Audio outputs
	 */
	uint32_t            get_available_outputs_count () const;
    
	/** Get vector of all enabled physical Audio input port names.
	 *
	 * @param[out] port_names - vector of all enabled Audio input names
	 */
	void                get_physical_audio_inputs (std::vector<std::string>& port_names);
	/** Get vector of all enabled physical Audio input port names.
	 *
	 * @param[out] port_names - vector of all enabled Audio input names
	 */
	void                get_physical_audio_outputs (std::vector<std::string>& port_names);
    
	/** Get vector of all enabled physical MIDI input port names.
	 *
	 * @param[out] port_names - vector of all enabled MIDI input names
	 */
	void                get_physical_midi_inputs (std::vector<std::string>& port_names);
	/** Get vector of all enabled physical MIDI output port names.
	 *
	 * @param[out] port_names - vector of all enabled MIDI output names
	 */
	void                get_physical_midi_outputs (std::vector<std::string>& port_names);
    
	/** Sets new state to all Audio inputs.
	 *
	 * @param[in] state - new state
	 */
	void                set_state_to_all_inputs(bool state);
	/** Sets new state to all Audio outputs.
	 * @note Does nothing in Stereo Out mode
	 * @param[in] state - new state
	 */
	void                set_state_to_all_outputs(bool state);
    
	/** Get vector of states for all physical Audio input ports.
	 *
	 * @param[out] channel_states - vector of input port states
	 */
	void                get_physical_audio_input_states(std::vector<PortState>& channel_states);
	/** Get vector of states for all physical Audio output ports.
	 *
	 * @param[out] channel_states - vector of output port states
	 */
	void                get_physical_audio_output_states(std::vector<PortState>& channel_states);
    
	/** Set state of the specified Audio input port.
	 *
	 * @param[in] port_name - input name
	 * @param[in] state - new state
	 */
	void                set_physical_audio_input_state(const std::string& port_name, bool state);
	/** Set state of the specified Audio output port.
	 *
	 * @param[in] port_name - output name
	 * @param[in] state - new state
	 */
	void                set_physical_audio_output_state(const std::string& port_name, bool state);
    
	/** Get state of the specified Audio input port.
	 *
	 * @param[in] port_name - input name
	 * @return input state
	 */
	bool                get_physical_audio_input_state(const std::string& port_name);
	/** Get state of the specified Audi output port.
	 *
	 * @param[in] port_name - output name
	 * @return output state
	 */
	bool                get_physical_audio_output_state(const std::string& port_name);
    
	/** Get vector of all enabled MIDI input port names.
	 *
	 * @param[out] channel_states - vector of enabled inputs
	 */
	void                get_physical_midi_input_states (std::vector<MidiPortState>& channel_states);
	/** Get vector of all enabled MIDI output port names.
	 *
	 * @param[out] channel_states - vector of enabled outputs
	 */
	void                get_physical_midi_output_states (std::vector<MidiPortState>& channel_states);
    
	/** Set state of the specified MIDI input port.
	 *
	 * @param[in] port_name - input name
	 * @param[in] state - new state
	 */
	void                set_physical_midi_input_state(const std::string& port_name, bool state);
	/** Set state of the specified MIDI output port.
	 *
	 * @param[in] port_name - output name
	 * @param[in] state - new state
	 */
	void                set_physical_midi_output_state(const std::string& port_name, bool state);
	/** Get state of the specified MIDI input port.
	 *
	 * @param[in] port_name - input name
	 * @param[out] scene_connected - is port used as Scene In or not
	 * @return input state
	 */
	bool                get_physical_midi_input_state(const std::string& port_name, bool& scene_connected);
	/** Get state of the specified MIDI output port.
	 *
	 * @param[in] port_name - output name
	 * @param[out] scene_connected - is port used as Scene Out or not
	 * @return output state
	 */
	bool                get_physical_midi_output_state(const std::string& port_name, bool& scene_connected);
    
	/** Set state of Scene In connection for the specified MIDI input port.
	 *
	 * @param[in] port_name - input name
	 * @param[in] state - new state
	 */
	void                set_physical_midi_scene_in_connection_state(const std::string& port_name, bool state);
	/** Set state of Scene Out connection for the specified MIDI output port.
	 *
	 * @param[in] port_name - input name
	 * @param[in] state - new state
	 */
	void                set_physical_midi_scenen_out_connection_state(const std::string&, bool);
    
	/** Disocnnect all MIDI input ports from Scene In.
	 */
	void                set_all_midi_scene_inputs_disconnected();
	/** Disocnnect all MIDI output ports from Scene Out.
	 */
	void                set_all_midi_scene_outputs_disconnected();
    
	/** Set MIDI TimeCode input port
	 * @note There is a sense to choose MIDI TimeCode input only because
	 * our MIDI TimeCode is propagated to all midi output ports.
	 */
	void                set_mtc_input(const std::string&);
    
	/** Check if AudioEngine setup is required
	 * @return true if setup is required, otherwise - false
	 */
	bool                is_setup_required() const {return ARDOUR::AudioEngine::instance()->setup_required (); }
    
	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Methods set parameters inside the controller
	// the state of engine won't change untill we make a "push" of this state to the backend
	// NOTE: Use push_state_to_backend() method to update backend with the most recent controller state
    
	/** Set new sample rate for current device in EngineStateController database
	 * @note Use push_state_to_backend() method to update backend/device state with the most recent controller state
	 * @param sample_rate - new sample rate
	 */
	bool        set_new_sample_rate_in_controller(framecnt_t sample_rate);
	/** Set new buffer size for current device in EngineStateController database
	 * @note Use push_state_to_backend() method to update backend/device state with the most recent controller state
	 * @param buffer_size - new buffer size
	 */
	bool        set_new_buffer_size_in_controller(pframes_t buffer_size);
    
	/** @brief push current controller state to backend.
	 * Propagate and set all current EngineStateController parameters to the backend
	 * @note Engine will be restarted if it's running when this method is called.
	 * @note If an attempt ot set parameters is unsuccessful current device will be switched to "None".
	 * @param start - start the Engine if it was not running when this function was called.
	 * @return true on success, otherwise - false
	 */
	bool        push_current_state_to_backend(bool start);
	/** Switch to new backend
	 * @note The change will be propagated emmidiatelly as if push_current_state_to_backend () was called.
	 * @param backend_name - new backend name.
	 * @return true on success, otherwise - false
	 */
	bool        set_new_backend_as_current(const std::string& backend_name);
	/** Switch to new device
	 * @note The change will be propagated emmidiatelly as if push_current_state_to_backend () was called.
	 * @param device_name - new device name.
	 * @return true on success, otherwise - false
	 */
	bool        set_new_device_as_current(const std::string& device_name);

    
	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Methods to save/serialize setting states

	/** Serialize Audio/Midi settings (entire EngineStateController database) to XML
	 * @return XML note with serialized states
	 */
	XMLNode&    serialize_audio_midi_settings();
	/** Save Audio/Midi settings (entire EngineStateController database) to config persistently
	 */
	void        save_audio_midi_settings();
    
	////////////////////////////////////////////////////////////////////////////////////////////////////
	//UPDATE SIGNALS
	/** This signal is emitted if the sample rate changes */
	PBD::Signal0<void> SampleRateChanged;
	/** This signal is emitted if the buffer size changes */
	PBD::Signal0<void> BufferSizeChanged;
	/** This signal is emitted if the device list changes */
	PBD::Signal1<void, bool> DeviceListChanged;
	/** This signal is emitted if the device cannot operate properly */
	PBD::Signal0<void> DeviceError;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	//ENGINE STATE SIGNALS
	/** This signal is emitted when the engine is started */
	PBD::Signal0<void> EngineRunning;
	/** This signal is emitted when the engine is stopped */
	PBD::Signal0<void> EngineStopped;
	/** This signal is emitted if Engine processing is terminated */
	PBD::Signal0<void> EngineHalted;
    
	/** This signal is emitted if the AUDIO input channel configuration changes */
	PBD::Signal0<void> InputConfigChanged;
	/** This signal is emitted if the AUDIO output channel configuration changes */
	PBD::Signal0<void> OutputConfigChanged;
	/** This signal is emitted if the AUDIO output connection mode changes
	 * @note By output connection mode "Stereo Out" or "Multi Out" is meant
	 */
	PBD::Signal0<void> OutputConnectionModeChanged;
    
	/** This signals is emitted if the MIDI input channel configuration changes */
	PBD::Signal0<void> MIDIInputConfigChanged;
	/** This signals is emitted if the MIDI output channel configuration changes */
	PBD::Signal0<void> MIDIOutputConfigChanged;
	/** This signals is emitted if the MIDI Scene In connection changes */
	PBD::Signal2<void, const std::vector<std::string>&, bool> MIDISceneInputConnectionChanged;
	/** This signals is emitted if the MIDI Scene Out connection changes */
	PBD::Signal2<void, const std::vector<std::string>&, bool> MIDISceneOutputConnectionChanged;
    
	/** This signal is emitted if the MTC Input channel is changed */
	PBD::Signal1<void, const std::string&> MTCInputChanged;
    
	/** This signal is emitted if new Audio/MIDI ports are registered or unregistered */
	PBD::Signal0<void> PortRegistrationChanged;
    
   private:

	EngineStateController(); /// singleton
	~EngineStateController(); /// singleton
	EngineStateController(const EngineStateController& ); /// prohibited
	EngineStateController& operator=(const EngineStateController&); /// prohibited
    
	////////////////////////////////////////////////////////////////////////////////////////////
	// private data structures
    
	/** @struct Engine state
	 * @brief State structure.
	 * Contains information about single device/backend state
	 */
	struct State {
		std::string backend_name; ///< state backend name
		std::string device_name; ///< state device name
		ARDOUR::framecnt_t sample_rate; ///< sample rate used by the device in this state
		ARDOUR::pframes_t buffer_size; ///< buffer size used by the device in this state
		
		PortStateList input_channel_states; ///< states of device Audio inputs
		PortStateList multi_out_channel_states; ///< states of device Audio inputs in Multi Out mode
		PortStateList stereo_out_channel_states; ///< states of device Audio inputs in Stereo Out mode
		bool active; ///< was this state the most recent active one
        
		State()
			: sample_rate(0)
			, buffer_size(0)
			, input_channel_states (0)
			, multi_out_channel_states (0)
			, stereo_out_channel_states (0)
			, active (false)
		{
		}
        
		bool operator==(const State& rhs)
		{
			return (backend_name == rhs.backend_name) && (device_name == rhs.device_name);
		}
        
		/** Forms string name for the state
		 * @return name string
		 */
		std::string form_state_name() {
			return std::string("State:" + backend_name + ":" + device_name);
		}
        
		/** @struct StatepPredicate
		 * This predicate is used to identify a state during search in the list of states
		 */
		struct StatePredicate
		{
			StatePredicate(const std::string& backend_name, const std::string& device_name)
				: _backend_name (backend_name)
				, _device_name (device_name)
			{}
            
			bool operator()(boost::shared_ptr<ARDOUR::EngineStateController::State> rhs)
			{
				return (_backend_name == rhs->backend_name) && (_device_name == rhs->device_name);
			}
            
		                                        private:
			std::string _backend_name;
			std::string _device_name;
		};
	};
    
	/// @typedef Type for the state pointer
	typedef boost::shared_ptr<State> StatePtr;
	/// @typedef Type for the list of states
	typedef std::list<StatePtr> StateList;
    
	////////////////////////////////////////////////////////////////////////////////////////////////////
	// methods to manage setting states
    
	/** Deserializes and loads Engine and Audio port states from the config to EngineStateController
	 */
	void _deserialize_and_load_engine_states();
	/** Deserializes and loads MIDI port states from the config to EngineStateController
	 */
	void _deserialize_and_load_midi_port_states();
	/** Serializes Engine and Audio port states from EngineStateController to XML node
	 * @param[in,out] audio_midi_settings_node - node to serialize the satets to
	 */
	void _serialize_engine_states(XMLNode* audio_midi_settings_node);
	/** Serializes MIDI port states from EngineStateController to XML node
	 * @param[in,out] audio_midi_settings_node - node to serialize the satets to
	 */
	void _serialize_midi_port_states(XMLNode* audio_midi_settings_node);
    
	/** Provides initial state configuration.
	 * It loades the last active state if there is one and it is aplicable.
	 * Otherwise default state (None device with default sample rate and buffer size) is loaded.
	 */
	void _do_initial_engine_setup();
    
	/** Loades provided state.
	 * @note It's possible that provided state can't be loaded
	 * (device disconnected or reqested parameters are not supported anymore).
	 * @param state - state to apply
	 * @return true on success, otherwise - false
	 */
	bool _apply_state(const StatePtr& state);
    
	/** Gets available device channels from engine and updates internal controller state
	 */
	void _update_device_channels_state();
    
	/** Check "Stereo Out" mode channels state configuration and make it correspond Stereo Out mode requirements
	 */
	void _refresh_stereo_out_channel_states();
    
	////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// internal helper functions
	/** make sure that current device parameters are supported and fit session requirements
	 * @return true if current state is valid and all parameters are supported, otherwise - false
	 */
	bool _validate_current_device_state();
	////////////////////////////////////////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////
	// callbacks
	/** Invoked when Engine starts running
	 */
	void _on_engine_running();
	/** Invoked when Engine is halted
	 */
	void _on_engine_halted();
	/** Invoked when Engine processing is terminated
	 */
	void _on_engine_stopped();
	/** Invoked when Device error accured, it failed to start or didn't accept the change which should
	 */
	void _on_device_error();
	/** Invoked when current device changes sample rate
	 */
	void _on_sample_rate_change(ARDOUR::framecnt_t);
	/** Invoked when current device changes buffer size
	 */
	void _on_buffer_size_change(ARDOUR::pframes_t);
	/** Invoked when the list of available devices is changed
	 */
	void _on_device_list_change();
	/** Invoked when the config parameter is changed
	 */
	void _on_parameter_changed (const std::string&);
	/** Invoked when Audio/MIDI ports are registered or unregistered
	 */
	void _on_ports_registration_update ();
	/** Invoked when session loading process is complete
	 */
	void _on_session_loaded();
	////////////////////////////////////////
        
	////////////////////////////////////////
	// attributes
	StatePtr _current_state; ///< current state pointer
	// list of system states
	StateList _states; ///< list of all available states
    
	MidiPortStateList _midi_inputs; ///< midi input states
	MidiPortStateList _midi_outputs; ///< midi output states
    
	std::string _last_used_real_device; ///< last active non-default (real) device
    
	Session* _session; ///< current session

	// Engine connections stuff
	PBD::ScopedConnectionList update_connections; ///< connection container for update signals
	PBD::ScopedConnectionList session_connections; ///< connection container for session signals
	PBD::ScopedConnection running_connection; ///< connection container for EngineRunning signal
	PBD::ScopedConnection halt_connection; ///< connection container for EngineHalted signal
	PBD::ScopedConnection stopped_connection; ///< connection container for EngineStopped signal
};

} // namespace ARDOUR
    
#endif /* defined(__gtk2_ardour__engine_state_controller__) */
