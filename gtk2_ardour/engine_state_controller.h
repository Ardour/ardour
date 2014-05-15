//
//  engine_state_controller.h
//  Tracks
//
//  Created by Grygorii Zharun on 4/30/14.
//  Copyright (c) 2014 Waves. All rights reserved.
//

#ifndef __gtk2_ardour__engine_state_controller__
#define __gtk2_ardour__engine_state_controller__

#include <vector>

#include "ardour/types.h"
#include "ardour/audio_backend.h"

namespace ARDOUR {

class AudioBackendInfo;
    
class EngineStateController
{
public:
    static EngineStateController* instance();
    
    //Interfaces
    void available_backends(std::vector<const AudioBackendInfo*>&);
    
    const std::string&  get_current_backend_name() const;
    
    const std::string&  get_current_device_name() const;
    void                change_current_device_to(const std::string&) const;
    void                enumerate_devices (std::vector<ARDOUR::AudioBackend::DeviceStatus>&) const;
    
    ARDOUR::framecnt_t  get_current_sample_rate() const;
    ARDOUR::framecnt_t  get_default_sample_rate() const;
    void                available_sample_rates_for_current_device(std::vector<float>&) const;
    
    ARDOUR::pframes_t   get_current_buffer_size() const;
    ARDOUR::pframes_t   get_default_buffer_size() const;
    void                available_buffer_sizes_for_current_device(std::vector<ARDOUR::pframes_t>&) const;

    uint32_t            get_available_inputs_count() const {return 0; }
    uint32_t            get_available_outputs_count () const {return 0; }
    
    bool        is_setup_required() const {return ARDOUR::AudioEngine::instance()->setup_required (); }
    
    // set parameters inside the controller,
    // the state of engine won't change untill we make a "push" of this state to the backend
    // NOTE: Use push_state_to_backend() method to update backend with the most recent controller state
    bool        set_new_sample_rate_in_controller(framecnt_t);
    bool        set_new_buffer_size_in_controller(pframes_t);
    
    
    // push current controller state to backend
    bool        push_current_state_to_backend(bool);
    // switch engine to new backend
    bool        set_new_backend_as_current(const std::string&);
    // switch engine to new device
    bool        set_new_device_as_current(const std::string& device_name);
    // switch backend to session sample rate
    void        set_desired_sample_rate(framecnt_t);

    
    //SIGNALS
    /* this signal is emitted if the sample rate changes */
    PBD::Signal0<void> SampleRateChanged;
    
    /* this signal is emitted if the buffer size changes */
    PBD::Signal0<void> BufferSizeChanged;
    
    /* this signal is emitted if the device list changes */
    PBD::Signal1<void, bool> DeviceListChanged;
    
private:

    EngineStateController(); // singleton
    ~EngineStateController(); // singleton
    EngineStateController(const EngineStateController& ); // prohibited
    EngineStateController& operator=(const EngineStateController&); // prohibited
    
    // data types:
    struct State {
		std::string backend_name;
		std::string device_name;
		ARDOUR::framecnt_t sample_rate;
		ARDOUR::pframes_t buffer_size;
		uint32_t input_latency;
		uint32_t output_latency;
		uint32_t input_channels;
		uint32_t output_channels;
		bool active;
		std::string midi_option;
        
		State()
        : input_latency (0)
        , output_latency (0)
        , input_channels (0)
        , output_channels (0)
        , active (false)
        {
        }
        
        bool operator==(const State& rhs)
        {
            return (backend_name == rhs.backend_name) && (device_name == rhs.device_name);
        }
        
        // predicates for search
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
    
    typedef boost::shared_ptr<State> StatePtr;
    typedef std::list<StatePtr> StateList;
    
    // state control methods////////////////
    void _deserialize_and_load_states();
    void _serialize_and_save_current_state();
    // sets last active state as current state
    // if no last active state found it loads default state
    void _set_last_active_state_as_current();
    ////////////////////////////////////////
    
    // internal helper functions////////////
    // make sure that current device parameters are supported and fit session requirements 
    void _validate_current_device_state();
    ////////////////////////////////////////
        
    ////////////////////////////////////////
    // callbacks
    void _on_engine_running();
    void _on_engine_halted(const char*);
    void _on_engine_stopped();
    void _on_sample_rate_change(ARDOUR::framecnt_t);
    void _on_buffer_size_change(ARDOUR::pframes_t);
    void _on_device_list_change();
    ////////////////////////////////////////
        
    ////////////////////////////////////////
    // attributes
    StatePtr _current_state;
    // list of system states
    StateList _states;
    
    // last active non-default (real) device
    std::string _last_used_real_device;
    
    ARDOUR::framecnt_t  _desired_sample_rate;
    bool                _have_control;
    
    // Engine connections stuff
	PBD::ScopedConnectionList update_connections;
    PBD::ScopedConnection running_connection;
    PBD::ScopedConnection halt_connection;
    PBD::ScopedConnection stopped_connection;

};

} // namespace ARDOUR
    
#endif /* defined(__gtk2_ardour__engine_state_controller__) */
