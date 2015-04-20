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

#include "waves_audiobackend.h"
#include "waves_audioport.h"
#include "waves_midiport.h"

using namespace ARDOUR;

#if defined __MINGW64__ || defined __MINGW32__
	extern "C" __declspec(dllexport) ARDOUR::AudioBackendInfo* descriptor ()
#else
	extern "C" ARDOURBACKEND_API ARDOUR::AudioBackendInfo* descriptor ()
#endif
{
    // COMMENTED DBG LOGS */ std::cout  << "waves_backend.dll : ARDOUR::AudioBackendInfo* descriptor (): " << std::endl;
    return &WavesAudioBackend::backend_info ();
}

void WavesAudioBackend::AudioDeviceManagerNotification (NotificationReason reason, void* parameter)
{
    switch (reason) {
        case WCMRAudioDeviceManagerClient::DeviceDebugInfo:
            std::cout << "-------------------------------  WCMRAudioDeviceManagerClient::DeviceDebugInfo -- " << (char*)parameter << std::endl;
            break;
        case WCMRAudioDeviceManagerClient::BufferSizeChanged:
            std::cout << "-------------------------------  WCMRAudioDeviceManagerClient::BufferSizeChanged: " << *(uint32_t*)parameter << std::endl;
			_buffer_size_change(*(uint32_t*)parameter);
            break;
        case WCMRAudioDeviceManagerClient::RequestReset:
            std::cout << "-------------------------------  WCMRAudioDeviceManagerClient::RequestReset" << std::endl;
            engine.request_backend_reset();
            break;
        case WCMRAudioDeviceManagerClient::RequestResync:
            std::cout << "-------------------------------  WCMRAudioDeviceManagerClient::RequestResync" << std::endl;
            break;
        case WCMRAudioDeviceManagerClient::SamplingRateChanged:
            std::cout << "-------------------------------  WCMRAudioDeviceManagerClient::SamplingRateChanged: " << *(float*)parameter << std::endl;
			set_sample_rate(*(float*)parameter);
            break;
        case WCMRAudioDeviceManagerClient::Dropout:
            std::cout << "-------------------------------  WCMRAudioDeviceManagerClient::Dropout: " << std::endl;
            break;
        case WCMRAudioDeviceManagerClient::DeviceDroppedSamples:
            std::cout << "-------------------------------  WCMRAudioDeviceManagerClient::DeviceDroppedSamples" << std::endl;
            break;
        case WCMRAudioDeviceManagerClient::DeviceStoppedStreaming:
            std::cout << "-------------------------------  WCMRAudioDeviceManagerClient::DeviceStoppedStreaming" << std::endl;
            break;
		case WCMRAudioDeviceManagerClient::DeviceStartsStreaming:
            std::cout << "-------------------------------  WCMRAudioDeviceManagerClient::DeviceStartsStreaming" << std::endl;
			_call_thread_init_callback = true; // streaming will be started from device side, just set thread init flag
            break;
        case WCMRAudioDeviceManagerClient::DeviceConnectionLost:
            std::cout << "-------------------------------  WCMRAudioDeviceManagerClient::DeviceConnectionLost" << std::endl;
            break;
        case WCMRAudioDeviceManagerClient::DeviceListChanged:
            std::cout << "-------------------------------  WCMRAudioDeviceManagerClient::DeviceListChanged" << std::endl;
            engine.request_device_list_update();
            break;
        case WCMRAudioDeviceManagerClient::IODeviceDisconnected:
            std::cout << "-------------------------------  WCMRAudioDeviceManagerClient::DeviceListChanged" << std::endl;
            engine.request_device_list_update();
            break;
        case WCMRAudioDeviceManagerClient::AudioCallback:
            if (parameter) {
                const AudioCallbackData* audio_callback_data = (AudioCallbackData*)parameter;
                _audio_device_callback (
                    audio_callback_data->acdInputBuffer,
                    audio_callback_data->acdOutputBuffer,
                    audio_callback_data->acdFrames,
                    audio_callback_data->acdSampleTime,
                    audio_callback_data->acdCycleStartTimeNanos
                );
            }
        break;
        
        default:
        break;
    };
}


WavesAudioBackend::WavesAudioBackend (AudioEngine& e)
    : AudioBackend (e, __backend_info)
    , _audio_device_manager (this)
    , _midi_device_manager (*this)
    , _device (NULL)
    , _sample_format (FormatFloat)
    , _interleaved (true)
    , _input_channels (0)
    , _max_input_channels (0)
    , _output_channels (0)
    , _max_output_channels (0)
    , _sample_rate (0)
    , _buffer_size (0)
    , _systemic_input_latency (0)
    , _systemic_output_latency (0)
    , _call_thread_init_callback (false)
    , _use_midi (true)
    , _sample_time_at_cycle_start (0)
    , _freewheeling (false)
    , _freewheel_thread_active (false)
    , _dsp_load_accumulator (0)
    , _audio_cycle_period_nanos (0)
    , _dsp_load_history_length(0)
{
}


WavesAudioBackend::~WavesAudioBackend ()
{
    
}

std::string
WavesAudioBackend::name () const
{
#ifdef __APPLE__
    return std::string ("CoreAudio");
#elif PLATFORM_WINDOWS
    return std::string ("ASIO");
#endif
}


bool
WavesAudioBackend::is_realtime () const
{
    return true;
}


bool 
WavesAudioBackend::requires_driver_selection () const
{ 
    return false; 
}


std::vector<std::string> 
WavesAudioBackend::enumerate_drivers () const
{ 
    // this backend does not suppose driver selection
    assert (false);

    return std::vector<std::string> (); 
}


int 
WavesAudioBackend::set_driver (const std::string& /*drivername*/)
{
    //Waves audio backend does not suppose driver selection
    assert (false);

    return -1; 
}


std::vector<AudioBackend::DeviceStatus> 
WavesAudioBackend::enumerate_devices () const
{   
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::enumerate_devices (): " << std::endl;

    std::vector<DeviceStatus> devicesStatus;
    const DeviceInfoVec& deviceInfoList = _audio_device_manager.DeviceInfoList(); 

    for (DeviceInfoVecConstIter deviceInfoIter = deviceInfoList.begin ();  deviceInfoIter != deviceInfoList.end (); ++deviceInfoIter) {
        // COMMENTED DBG LOGS */ std::cout << "\t Device found: " << (*deviceInfoIter)->m_DeviceName << std::endl;
        devicesStatus.push_back (DeviceStatus ((*deviceInfoIter)->m_DeviceName, true));
    }
    
    return devicesStatus;
} 


std::vector<float> 
WavesAudioBackend::available_sample_rates (const std::string& device_name) const
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::available_sample_rates (): [" << device_name << "]" << std::endl;

    std::vector<int> sr;
    
    WTErr retVal = _audio_device_manager.GetDeviceSampleRates(device_name, sr);
    
	if (eNoErr != retVal) {
        std::cerr << "WavesAudioBackend::available_sample_rates (): Failed to find device [" << device_name << "]" << std::endl;
        return std::vector<float> ();
    }

    // COMMENTED DBG LOGS */ std::cout << "\tFound " << devInfo.m_AvailableSampleRates.size () << " sample rates for " << device_name << ":";

    std::vector<float> sample_rates (sr.begin (), sr.end ());
    
    // COMMENTED DBG LOGS */ for (std::vector<float>::iterator i = sample_rates.begin ();  i != sample_rates.end (); ++i) std::cout << " " << *i; std::cout << std::endl;

    return sample_rates;
}


float WavesAudioBackend::default_sample_rate () const 
{ 
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::default_sample_rate (): " << AudioBackend::default_sample_rate () << std::endl;
    return AudioBackend::default_sample_rate (); 
}

uint32_t 
WavesAudioBackend::default_buffer_size (const std::string& device_name) const
{
#ifdef __APPLE__
	return AudioBackend::default_buffer_size (device_name);
#else
    DeviceInfo devInfo;
    WTErr err = _audio_device_manager.GetDeviceInfoByName(device_name, devInfo);

    if (err != eNoErr) {
        std::cerr << "WavesAudioBackend::default_buffer_size (): Failed to get buffer size for device [" << device_name << "]" << std::endl;
        return AudioBackend::default_buffer_size (device_name);
    }
	
	return devInfo.m_DefaultBufferSize; 
#endif
}

std::vector<uint32_t> 
WavesAudioBackend::available_buffer_sizes (const std::string& device_name) const
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::available_buffer_sizes (): [" << device_name << "]" << std::endl;

	std::vector<int> bs;

	WTErr retVal = _audio_device_manager.GetDeviceBufferSizes(device_name, bs);

    if (retVal != eNoErr) {
        std::cerr << "WavesAudioBackend::available_buffer_sizes (): Failed to get buffer size for device [" << device_name << "]" << std::endl;
        return std::vector<uint32_t> ();
    }

    std::vector<uint32_t> buffer_sizes (bs.begin (), bs.end ());

    // COMMENTED DBG LOGS */ std::cout << "\tFound " << buffer_sizes.size () << " buffer sizes for " << device_name << ":";
    // COMMENTED DBG LOGS */ for (std::vector<uint32_t>::const_iterator i = buffer_sizes.begin ();  i != buffer_sizes.end (); ++i) std::cout << " " << *i; std::cout << std::endl;

    return buffer_sizes;
}


uint32_t 
WavesAudioBackend::available_input_channel_count (const std::string& device_name) const
{
    DeviceInfo devInfo;
    WTErr err = _audio_device_manager.GetDeviceInfoByName(device_name, devInfo);
    
	if (eNoErr != err) {
        std::cerr << "WavesAudioBackend::available_input_channel_count (): Failed to find device [" << device_name << "]" << std::endl;
        return 0;
    }

    uint32_t num_of_input_channels = devInfo.m_MaxInputChannels;

    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::available_input_channel_count (): " << num_of_input_channels << std::endl;
    return num_of_input_channels;
}


uint32_t 
WavesAudioBackend::available_output_channel_count (const std::string& device_name) const
{
    DeviceInfo devInfo;
    WTErr err = _audio_device_manager.GetDeviceInfoByName(device_name, devInfo);
    
	if (eNoErr != err) {
        std::cerr << "WavesAudioBackend::available_output_channel_count (): Failed to find device [" << device_name << "]" << std::endl;
        return 0;
    }

    uint32_t num_of_output_channels = devInfo.m_MaxOutputChannels;

    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::available_output_channel_count (): " << num_of_output_channels << std::endl;

    return num_of_output_channels;
}


bool
WavesAudioBackend::can_change_sample_rate_when_running () const
{
    // VERIFY IT CAREFULLY
    return true;
}


bool
WavesAudioBackend::can_change_buffer_size_when_running () const
{
    // VERIFY IT CAREFULLY
    return true;
}


int
WavesAudioBackend::set_device_name (const std::string& device_name)
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::set_device_name (): " << device_name << std::endl;
    
    if (_ports.size ()) {
        std::cerr << "WavesAudioBackend::set_device_name (): There are unregistered ports left after [" << (_device ? _device->DeviceName () : std::string ("<NULL>")) << "]!" << std::endl;
        for (size_t i = 0; i < _ports.size (); ++i) {
            std::cerr << "\t[" << _ports[i]->name () << "]!" << std::endl;
        }
        return -1;
    }

	if (_device && _device->Streaming () ) {
		std::cerr << "WavesAudioBackend::set_device_name (): [" << _device->DeviceName () << "] is streaming! Current device must be stopped before setting another device as current" << std::endl;
	}

	// we must have only one device initialized at a time
	// stop current device first
	WTErr retVal;
    if (_device) {
        retVal = _device->SetActive (false);
        if (retVal != eNoErr) {
            std::cerr << "WavesAudioBackend::set_device_name (): [" << _device->DeviceName () << "]->SetActive (false) failed!" << std::endl;
            return -1;
        }
    }

	// deinitialize it
	_audio_device_manager.DestroyCurrentDevice();
	_device = 0;

    WCMRAudioDevice * device = _audio_device_manager.InitNewCurrentDevice(device_name);

    if (!device) {
        std::cerr << "WavesAudioBackend::set_device_name (): Failed to initialize device [" << device_name << "]!" << std::endl;
        return -1;
    }


    retVal = device->SetActive (true);
    if (retVal != eNoErr) {
        std::cerr << "WavesAudioBackend::set_device_name (): [" << device->DeviceName () << "]->SetActive () failed!" << std::endl;
        return -1;
    }

    _device = device;
    return 0;
}


int
WavesAudioBackend::drop_device()
{
	WTErr wtErr = 0;

	if (_device)
	{
		wtErr = _device->SetActive (false);
		if (wtErr != eNoErr) {
			std::cerr << "WavesAudioBackend::drop_device (): [" << _device->DeviceName () << "]->SetActive () failed!" << std::endl;
			return -1;
		}
	}

	_audio_device_manager.DestroyCurrentDevice();
	_device = 0;

	return 0;
}


int 
WavesAudioBackend::set_sample_rate (float sample_rate)
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::set_sample_rate (): " << sample_rate << std::endl;
    
    WTErr retVal = eNoErr;

    if (!_device) {
        std::cerr << "WavesAudioBackend::set_sample_rate (): No device is set!" << std::endl;
        return -1;
    }

    
    bool device_needs_restart = _device->Streaming ();
    
    if (device_needs_restart) {
        retVal  = _device->SetStreaming (false);
        // COMMENTED DBG LOGS */ std::cout << "\t\t[" << _device->DeviceName() << "]->_device->SetStreaming (false);"<< std::endl;
        if (retVal != eNoErr) {
            std::cerr << "WavesAudioBackend::set_sample_rate (): [" << _device->DeviceName () << "]->SetStreaming (false) failed (" << retVal << ") !" << std::endl;
            return -1;
        }
    }
    
    retVal = _device->SetCurrentSamplingRate ((int)sample_rate);
    
    if (retVal != eNoErr) {
        std::cerr << "WavesAudioBackend::set_sample_rate (): [" << _device->DeviceName() << "]->SetCurrentSamplingRate ((int)" << sample_rate << ") failed (" << retVal << ") !" << std::endl;
        return -1;
    }

	_sample_rate_change(sample_rate);
       
    if (device_needs_restart) {
        // COMMENTED DBG LOGS */ std::cout << "\t\t[" << _device->DeviceName() << "]->SetStreaming (true);"<< std::endl;
        _call_thread_init_callback = true;
        retVal  = _device->SetStreaming (true);
        if (retVal != eNoErr) {
            std::cerr << "WavesAudioBackend::set_sample_rate (): [" << _device->DeviceName () << "]->SetStreaming (true) failed (" << retVal << ") !" << std::endl;
            return -1;
        }
    }
    return 0;
}


int 
WavesAudioBackend::set_buffer_size (uint32_t buffer_size)
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::set_buffer_size (" << buffer_size << "):"<< std::endl;

    WTErr retVal = eNoErr;

    if (!_device) {
        std::cerr << "WavesAudioBackend::set_buffer_size (): No device is set!" << std::endl;
        return -1;
    }

    bool device_needs_restart = _device->Streaming ();
    
    if (device_needs_restart) {
        retVal  = _device->SetStreaming (false);
        // COMMENTED DBG LOGS */ std::cout << "\t\t[" << _device->DeviceName() << "]->SetStreaming (false);"<< std::endl;
        if (retVal != eNoErr) {
            std::cerr << "WavesAudioBackend::set_buffer_size (): [" << _device->DeviceName () << "]->SetStreaming (false) failed (" << retVal << ") !" << std::endl;
            return -1;
        }
    }
    
    retVal = _device->SetCurrentBufferSize (buffer_size);
    
    if (retVal != eNoErr) {
        std::cerr << "WavesAudioBackend::set_buffer_size (): [" << _device->DeviceName() << "]->SetCurrentBufferSize (" << buffer_size << ") failed (" << retVal << ") !" << std::endl;
        return -1;
    }
    
	// if call to set buffer is successful but device buffer size differs from the value we tried to set
	// this means we are driven by device for buffer size
	buffer_size = _device->CurrentBufferSize ();

	_buffer_size_change(buffer_size);
    
    if (device_needs_restart) {
        // COMMENTED DBG LOGS */ std::cout << "\t\t[" << _device->DeviceName() << "]->SetStreaming (true);"<< std::endl;
        _call_thread_init_callback = true;
        retVal  = _device->SetStreaming (true);
        if (retVal != eNoErr) {
            std::cerr << "WavesAudioBackend::set_buffer_size (): [" << _device->DeviceName () << "]->SetStreaming (true) failed (" << retVal << ") !" << std::endl;
            return -1;
        }
    }
    
    return 0;
}


int 
WavesAudioBackend::set_sample_format (SampleFormat sample_format)
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::set_sample_format (): " << sample_format << std::endl;

    _sample_format = sample_format;
    return 0;
}

int 
WavesAudioBackend::reset_device ()
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::_reset_device ():" << std::endl;

    WTErr retVal = eNoErr;

    if (!_device) {
        std::cerr << "WavesAudioBackend::set_buffer_size (): No device is set!" << std::endl;
        return -1;
    }

	return _device->ResetDevice();
}


int 
WavesAudioBackend::_buffer_size_change (uint32_t new_buffer_size)
{
	_buffer_size = new_buffer_size;
    _init_dsp_load_history();
    return engine.buffer_size_change (new_buffer_size);
}


int 
WavesAudioBackend::_sample_rate_change (float new_sample_rate)
{
	_sample_rate = new_sample_rate;
    _init_dsp_load_history();
    return engine.sample_rate_change (new_sample_rate);
}


int 
WavesAudioBackend::set_interleaved (bool yn)
{
    /*you can ignore them totally*/
    _interleaved = yn;
    return 0;
}


int 
WavesAudioBackend::set_input_channels (uint32_t input_channels)
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::set_input_channels (): " << input_channels << std::endl;

    _input_channels = input_channels;
    return 0;
}


int 
WavesAudioBackend::set_output_channels (uint32_t output_channels)
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::set_output_channels (): " << output_channels << std::endl;

    _output_channels = output_channels;
    return 0;
}


std::string  
WavesAudioBackend::device_name () const
{
    if (!_device) {
        return "";
    }
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::device_name (): " << _device->DeviceName () << std::endl;
    
    return _device->DeviceName ();
}


float        
WavesAudioBackend::sample_rate () const
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::sample_rate (): " << std::endl;

    if (!_device) {
        std::cerr << "WavesAudioBackend::sample_rate (): No device is set!" << std::endl;
        return -1;
    }

    int sample_rate = _device->CurrentSamplingRate ();

    // COMMENTED DBG LOGS */ std::cout << "\t[" << _device->DeviceName () << "]->CurrentSamplingRate () returned " << sample_rate << std::endl;

    return (float)sample_rate;
}


uint32_t     
WavesAudioBackend::buffer_size () const
{

    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::buffer_size (): " << std::endl;

    if (!_device) {
        std::cerr << "WavesAudioBackend::buffer_size (): No device is set!" << std::endl;
        return 0;
    }

    int size = _device->CurrentBufferSize ();
    
    // COMMENTED DBG LOGS */ std::cout << "\t[" << _device->DeviceName () << "]->CurrentBufferSize () returned " << size << std::endl;

    return (uint32_t)size;
}


SampleFormat 
WavesAudioBackend::sample_format () const
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::sample_format ()" << std::endl;
    return _sample_format;
}


bool         
WavesAudioBackend::interleaved () const
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::interleaved ()" << std::endl;

    return _interleaved;
}


uint32_t     
WavesAudioBackend::input_channels () const
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::input_channels ()" << std::endl;

    return _input_channels;
}


uint32_t     
WavesAudioBackend::output_channels () const
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::output_channels ()" << std::endl;

    return _output_channels;
}


std::string 
WavesAudioBackend::control_app_name () const
{
    std::string app_name = ""; 

    if (_device && !dynamic_cast<WCMRNativeAudioNoneDevice*> (_device))    {
        app_name = "PortAudioMayKnowIt";
    }

    return app_name; 
}


void
WavesAudioBackend::launch_control_app ()
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::launch_control_app ()" << std::endl;
    if (!_device) {
        std::cerr << "WavesAudioBackend::launch_control_app (): No device is set!" << std::endl;
        return;
    }
    
    WTErr err = _device->ShowConfigPanel (NULL);
    
    if (eNoErr != err) {
        std::cerr << "WavesAudioBackend::launch_control_app (): [" << _device->DeviceName () << "]->ShowConfigPanel () failed (" << err << ")!" << std::endl;
    }

    // COMMENTED DBG LOGS */ else std::cout << "WavesAudioBackend::launch_control_app (): [" << _device->DeviceName () << "]->ShowConfigPanel ()  successfully launched!" << std::endl;
}


int
WavesAudioBackend::_start (bool for_latency_measurement)
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::_start ()" << std::endl;

    if (!_device) {
        std::cerr << "WavesAudioBackend::_start (): No device is set!" << std::endl;
        stop();
		return -1;
    }

    if (_register_system_audio_ports () != 0) {
        std::cerr << "WavesAudioBackend::_start (): _register_system_audio_ports () failed!" << std::endl;
        stop();
		return -1;
    }

    if (_use_midi) {
        if (_midi_device_manager.start () != 0) {
            std::cerr << "WavesAudioBackend::_start (): _midi_device_manager.start () failed!" << std::endl;
            stop();
			return -1;
        }
        if (_register_system_midi_ports () != 0) {
            std::cerr << "WavesAudioBackend::_start (): _register_system_midi_ports () failed!" << std::endl;
            stop();
			return -1;
        }
    }

    if (engine.reestablish_ports () != 0) {
        std::cerr << "WavesAudioBackend::_start (): engine.reestablish_ports () failed!" << std::endl;
    }

    manager.registration_callback ();

    _call_thread_init_callback = true;
    WTErr retVal  = _device->SetStreaming (true);
    if (retVal != eNoErr) {
        std::cerr << "WavesAudioBackend::_start (): [" << _device->DeviceName () << "]->SetStreaming () failed!" << std::endl;
		stop();
        return -1;
    }

    if (_use_midi) {
        if (_midi_device_manager.stream (true)) {
            std::cerr << "WavesAudioBackend::_start (): _midi_device_manager.stream (true) failed!" << std::endl;
            stop();
			return -1;
        }
    }

    return 0;
}


void
WavesAudioBackend::_audio_device_callback (const float* input_buffer, 
                                           float* output_buffer, 
                                           unsigned long nframes,
                                           framepos_t sample_time,
                                           uint64_t cycle_start_time_nanos)
{
    uint64_t dsp_start_time_nanos = __get_time_nanos();
    // COMMENTED FREQUENT DBG LOGS */ std::cout << "WavesAudioBackend::_audio_device_callback ():" << _device->DeviceName () << std::endl;
    _sample_time_at_cycle_start = sample_time;
    _cycle_start_time_nanos = cycle_start_time_nanos;
    
    /* There is the possibility that the thread this runs in may change from
     *  callback to callback, so do it every time.
     */
    _main_thread = pthread_self ();

    if (_buffer_size != nframes) {
        // COMMENTED DBG LOGS */ std::cout << "\tAudioEngine::thread_init_callback() buffer size and nframes are not equal: " << _buffer_size << "!=" << nframes << std::endl;
        return;
    }

    _read_audio_data_from_device (input_buffer, nframes);
    _read_midi_data_from_devices ();

    if (_call_thread_init_callback) {
        _call_thread_init_callback = false;
        // COMMENTED DBG LOGS */ std::cout << "\tAudioEngine::thread_init_callback() invoked for " << std::hex << pthread_self() << std::dec << " !" << std::endl;
        AudioEngine::thread_init_callback (this);
    }

    engine.process_callback (nframes);
    
    _write_audio_data_to_device (output_buffer, nframes);
    _write_midi_data_to_devices (nframes);
    
    uint64_t dsp_end_time_nanos = __get_time_nanos();
    
    _dsp_load_accumulator -= *_dsp_load_history.begin();
        _dsp_load_history.pop_front();
    uint64_t dsp_load_nanos = dsp_end_time_nanos - dsp_start_time_nanos;
    _dsp_load_accumulator += dsp_load_nanos;
    _dsp_load_history.push_back(dsp_load_nanos);

    return;
}


int
WavesAudioBackend::stop ()
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::stop ()" << std::endl;

    WTErr wtErr = eNoErr;
    int retVal = 0;

    // COMMENTED DBG LOGS */ std::cout << "\t[" << _device->DeviceName () << "]" << std::endl;

	if (_device) {
		wtErr = _device->SetStreaming (false);
		if (wtErr != eNoErr) {
			std::cerr << "WavesAudioBackend::stop (): [" << _device->DeviceName () << "]->SetStreaming () failed!" << std::endl;
			retVal = -1;
		}
	}

	_midi_device_manager.stop ();
    _unregister_system_audio_ports ();
    _unregister_system_midi_ports ();
	
    return retVal;
}


int
WavesAudioBackend::freewheel (bool start_stop)
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::freewheel (" << start_stop << "):" << std::endl;

    if (start_stop != _freewheeling) {
        if (start_stop == true) {
            WTErr retval = _device->SetStreaming (false);
            if (retval != eNoErr) {
                std::cerr << "WavesAudioBackend::freewheel (): [" << _device->DeviceName () << "]->SetStreaming () failed!" << std::endl;
                return -1;
            }
            _call_thread_init_callback = true;
            _freewheel_thread ();
            engine.freewheel_callback (start_stop);
        }
        else {
            _freewheel_thread_active = false; // stop _freewheel_thread ()
            engine.freewheel_callback (start_stop);
            _call_thread_init_callback = true;
            WTErr retval = _device->SetStreaming (true);
            if (retval != eNoErr) {
                std::cerr << "WavesAudioBackend::freewheel (): [" << _device->DeviceName () << "]->SetStreaming () failed!" << std::endl;
                return -1;
            }
        }
        _freewheeling = start_stop;
    }
    // already doing what has been asked for
    return 0;
}


void 
WavesAudioBackend::_freewheel_thread ()
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::_freewheel_thread ():" << std::endl;
    if (!_freewheel_thread_active) { // Lets create it
        
        // COMMENTED DBG LOGS */ std::cout << "\tCreating the thread _freewheel_thread () . . ." << std::endl;
        pthread_attr_t attributes;
        pthread_t thread_id;

        ThreadData* thread_data = new ThreadData (this, boost::bind (&WavesAudioBackend::_freewheel_thread, this), __thread_stack_size ());

        if (pthread_attr_init (&attributes)) {
            std::cerr << "WavesAudioBackend::freewheel_thread (): pthread_attr_init () failed!" << std::endl;
            return;
        }
   
        if (pthread_attr_setstacksize (&attributes, __thread_stack_size ())) {
            std::cerr << "WavesAudioBackend::freewheel_thread (): pthread_attr_setstacksize () failed!" << std::endl;
            return;
        }

        _freewheel_thread_active = true;
        if ((pthread_create (&thread_id, &attributes, __start_process_thread, thread_data))) {
            _freewheel_thread_active = false;
            std::cerr << "WavesAudioBackend::freewheel_thread (): pthread_create () failed!" << std::endl;
            return;
        }

        // COMMENTED DBG LOGS */ std::cout << "\t. . . _freewheel_thread () complete." << std::endl;
        return;
    }
    
    if (_call_thread_init_callback) {
        _call_thread_init_callback = false;
        AudioEngine::thread_init_callback (this);
    }

    while (_freewheel_thread_active) {
        engine.process_callback (_buffer_size);
    }
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::_freewheel_thread (): FINISHED" << std::endl;
    return;
}


float
WavesAudioBackend::dsp_load () const
{
    // COMMENTED FREQUENT DBG LOGS */ std::cout << "WavesAudioBackend::dsp_load (): " << std::endl;

    if (!_device) {
        std::cerr << "WavesAudioBackend::cpu_load (): No device is set!" << std::endl;
        return 0;
    }

    float average_dsp_load = (float)_dsp_load_accumulator/_dsp_load_history_length;
    
    return ( average_dsp_load  / _audio_cycle_period_nanos)*100.0;
}


void
WavesAudioBackend::_init_dsp_load_history()
{
    if((_sample_rate <= 0.0) || (_buffer_size <= 0.0)) {
        return;
    }
    
    _audio_cycle_period_nanos = ((uint64_t)1000000000L * _buffer_size) / _sample_rate;
    
    _dsp_load_accumulator = 0;
    
    _dsp_load_history_length = (_sample_rate + _buffer_size - 1) / _buffer_size;
    // COMMENTED DBG LOGS */ std::cout << "\t\t_dsp_load_history_length = " << _dsp_load_history_length << std::endl;
    _dsp_load_history = std::list<uint64_t>(_dsp_load_history_length, 0);
}


void
WavesAudioBackend::transport_start ()
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::transport_start (): " << std::endl;
}


void
WavesAudioBackend::transport_stop () 
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::transport_stop (): " << std::endl;
}


TransportState
WavesAudioBackend::transport_state () const
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::transport_state (): " << std::endl;
    return TransportStopped; 
}


void
WavesAudioBackend::transport_locate (framepos_t pos)
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::transport_locate (" << pos << "): " << std::endl;
}


framepos_t
WavesAudioBackend::transport_frame () const
{ 
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::transport_frame (): " << std::endl;
    return 0; 
}


int
WavesAudioBackend::set_time_master (bool yn)
{ 
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::set_time_master (): " << yn << std::endl;
    return 0; 
}


int
WavesAudioBackend::usecs_per_cycle () const
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::usecs_per_cycle (): " << std::endl;
    return (1000000 * _sample_rate) / _buffer_size;
}


size_t
WavesAudioBackend::raw_buffer_size (DataType data_type)
{
    // COMMENTED FREQUENT DBG LOGS */ std::cout << "WavesAudioBackend::raw_buffer_size (" << data_type.to_string () << "): " << std::endl;
    switch (data_type) {
    case DataType::AUDIO:
            return WavesAudioPort::MAX_BUFFER_SIZE_BYTES;
        break;

    case DataType::MIDI:
            return WavesMidiPort::MAX_BUFFER_SIZE_BYTES;
        break;

        default:
            std::cerr << "WavesAudioBackend::raw_buffer_size (): unexpected data type (" << (uint32_t)data_type <<")!" << std::endl;
        break;
    }
    return 0;
}


framepos_t
WavesAudioBackend::sample_time ()
{
    // WARNING: This is approximate calculation. Implementation of accurate calculation is pending.
    // http://kokkinizita.linuxaudio.org/papers/usingdll.pdf
    
    return _sample_time_at_cycle_start + ((__get_time_nanos () - _cycle_start_time_nanos)*_sample_rate)/1000000000L;
}


uint64_t
WavesAudioBackend::__get_time_nanos ()
{
#ifdef __APPLE__
    // here we exploit the time counting API which is used by the WCMRCoreAudioDeviceManager. However,
    // the API should be a part of WCMRCoreAudioDeviceManager to give a chance of being tied to the
    // audio device transport timeß.
    return AudioConvertHostTimeToNanos (AudioGetCurrentHostTime ());
    
#elif PLATFORM_WINDOWS
	LARGE_INTEGER Count;
    QueryPerformanceCounter (&Count);
    return uint64_t ((Count.QuadPart * 1000000000L / __performance_counter_frequency));
#endif
}


framepos_t
WavesAudioBackend::sample_time_at_cycle_start ()
{
    // COMMENTED FREQUENT DBG LOGS */ std::cout << "WavesAudioBackend::sample_time_at_cycle_start (): " << _sample_time_at_cycle_start << std::endl;
    return _sample_time_at_cycle_start;
}


pframes_t
WavesAudioBackend::samples_since_cycle_start ()
{
    pframes_t diff_sample_time; 
    diff_sample_time = sample_time () - _sample_time_at_cycle_start;
    // COMMENTED DBG LOGS */ std::cout << "samples_since_cycle_start: " << diff_sample_time << std::endl;

    return diff_sample_time;
}


bool
WavesAudioBackend::get_sync_offset (pframes_t& /*offset*/) const
{ 
    // COMMENTED DBG LOGS */ std::cout << "get_sync_offset: false" << std::endl;

    return false; 
}


int
WavesAudioBackend::create_process_thread (boost::function<void ()> func)
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::create_process_thread ():" << std::endl;
    int retVal;
    pthread_attr_t attributes;
    size_t stacksize_aligned;
    pthread_t thread_id;

    // Align stacksize to PTHREAD_STACK_MIN.
    stacksize_aligned = __thread_stack_size ();

    ThreadData* td = new ThreadData (this, func, stacksize_aligned);

    if ((retVal = pthread_attr_init (&attributes))) {
        std::cerr << "Cannot set thread attr init res = " << retVal << endmsg;
        return -1;
    }
   
    if ((retVal = pthread_attr_setstacksize (&attributes, stacksize_aligned))) {
        std::cerr << "Cannot set thread stack size (" << stacksize_aligned << ") res = " << retVal << endmsg;
        return -1;
    }

    if ((retVal = pthread_create (&thread_id, &attributes, __start_process_thread, td))) {
        std::cerr << "Cannot create thread res = " << retVal << endmsg;
        return -1;
    }

    _backend_threads.push_back (thread_id);
    // COMMENTED DBG LOGS */ std::cout << "\t\t\t. . . thread " << std::hex << thread_id << std::dec << " has been created" << std::endl;

    return 0;
}


void*
WavesAudioBackend::__start_process_thread (void* arg)
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::__start_process_thread ():" << std::endl;
    ThreadData* td = reinterpret_cast<ThreadData*> (arg);
    boost::function<void ()> f = td->f;
    delete td;
    f ();
    return 0;
}


int
WavesAudioBackend::join_process_threads ()
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::join_process_thread ()" << std::endl;
    int ret = 0;

    for (std::vector<pthread_t>::const_iterator i = _backend_threads.begin ();
         i != _backend_threads.end ();
         ++i) {
        // COMMENTED DBG LOGS */ std::cout << "\t\t\tstopping thread " << std::hex << *i << std::dec << "...\n";

        void* status;  
        if (pthread_join (*i, &status) != 0) {
            std::cerr << "AudioEngine: cannot stop process thread !" << std::endl;
            ret += -1;
        }
        // COMMENTED DBG LOGS */ std::cout << "\t\t\t\t...done" << std::endl;
    }
    // COMMENTED DBG LOGS */ std::cout << "\t\t\tall threads finished..." << std::endl;
    _backend_threads.clear ();
    // COMMENTED DBG LOGS */ std::cout << "\t\t\tthread list cleared..." << std::endl;

    return ret;
}


bool 
WavesAudioBackend::in_process_thread ()
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::in_process_thread ()" << std::endl;
	if (pthread_equal (_main_thread, pthread_self()) != 0) {
		return true;
	}
	for (std::vector<pthread_t>::const_iterator i = _backend_threads.begin ();
	     i != _backend_threads.end (); i++) {
		if (pthread_equal (*i, pthread_self ()) != 0) {
			return true;
		}
	}
	return false;
}


size_t
WavesAudioBackend::__thread_stack_size ()
{
    // Align stacksize to PTHREAD_STACK_MIN.
#if defined (__APPLE__)
    return (((thread_stack_size () - 1) / PTHREAD_STACK_MIN) + 1) * PTHREAD_STACK_MIN;
#elif defined (PLATFORM_WINDOWS)
    return thread_stack_size ();
#endif
}


uint32_t 
WavesAudioBackend::process_thread_count ()
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::process_thread_count (): returns " << _backend_threads.size () << std::endl;
    return _backend_threads.size ();
}


void
WavesAudioBackend::_read_audio_data_from_device (const float* input_buffer, pframes_t nframes)
{
#if defined(PLATFORM_WINDOWS)
    const float **buffer = (const float**)input_buffer;
    size_t copied_bytes = nframes*sizeof(float);

    for(std::vector<WavesAudioPort*>::iterator it = _physical_audio_inputs.begin ();
        it != _physical_audio_inputs.end();
        ++it)
    {
        memcpy((*it)->buffer(), *buffer, copied_bytes);
        ++buffer;
    }
#else
    std::vector<WavesAudioPort*>::iterator it = _physical_audio_inputs.begin ();

    // Well, let's de-interleave here:
    const Sample* source = input_buffer;

    for (uint32_t chann_cnt = 0; (chann_cnt < _max_input_channels) && (it != _physical_audio_inputs.end ()); ++chann_cnt, ++source, ++it) {
        const Sample* src = source;
        Sample* tgt = (*it)->buffer ();

        for (uint32_t frame = 0; frame < nframes; ++frame, src += _max_input_channels, ++tgt) {
            *tgt = *src;
        }
    }
#endif
}

void
WavesAudioBackend::_write_audio_data_to_device (float* output_buffer, pframes_t nframes)
{
#if defined(_WnonononoINDOWS)
    float **buffer = (float**)output_buffer;
    size_t copied_bytes = nframes*sizeof(float);
    int i = 0;
    for(std::vector<WavesAudioPort*>::iterator it = _physical_audio_outputs.begin ();
        it != _physical_audio_outputs.end();
        ++it)
    {
        memcpy(*buffer, (*it)->buffer(), copied_bytes);
        //*buffer = (*it)->buffer();
        buffer++;
    }
#else
    // Well, let's interleave here:
    std::vector<WavesAudioPort*>::iterator it = _physical_audio_outputs.begin ();
    Sample* target = output_buffer;

    for (uint32_t chann_cnt = 0;
         (chann_cnt < _max_output_channels) && (it != _physical_audio_outputs.end ());
         ++chann_cnt, ++target, ++it) {
        const Sample* src = (Sample*) ((*it)->get_buffer (nframes));
        Sample* tgt = target;
        for (uint32_t frame = 0; frame < nframes; ++frame, tgt += _max_output_channels, ++src) {
            *tgt = *src;
        }
    }
#endif
}


static boost::shared_ptr<WavesAudioBackend> __instance;


boost::shared_ptr<AudioBackend>
WavesAudioBackend::__waves_backend_factory (AudioEngine& e)
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::__waves_backend_factory ():" << std::endl;
    if (!__instance) {
            __instance.reset (new WavesAudioBackend (e));
    }
    return __instance;
}


#if defined(PLATFORM_WINDOWS)

uint64_t WavesAudioBackend::__performance_counter_frequency;

#endif

int 
WavesAudioBackend::__instantiate (const std::string& arg1, const std::string& arg2)
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::__instantiate ():" << "[" << arg1 << "], [" << arg2 << "]" << std::endl;
    __instantiated_name = arg1;
#if defined(PLATFORM_WINDOWS)

	LARGE_INTEGER Frequency;
	QueryPerformanceFrequency(&Frequency);
	__performance_counter_frequency = Frequency.QuadPart;
	std::cout << "__performance_counter_frequency:" << __performance_counter_frequency << std::endl;

#endif
    return 0;
}


int 
WavesAudioBackend::__deinstantiate ()
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::__deinstantiate ():" << std::endl;
    __instance.reset ();
    return 0;
}


bool
WavesAudioBackend::__already_configured ()
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::__already_configured ():" << std::endl;
    return false;
}

bool
WavesAudioBackend::__available ()
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::__available ():" << std::endl;
    return true;
}


void*
WavesAudioBackend::private_handle () const
{
    // COMMENTED DBG LOGS */ std::cout << "WHY DO CALL IT: WavesAudioBackend::private_handle: " << std::endl;
    return NULL;
}


bool
WavesAudioBackend::available () const
{
    // COMMENTED SECONDARY DBG LOGS */// std::cout << "WavesAudioBackend::available: " << std::endl;
    return true;
}


const std::string&
WavesAudioBackend::my_name () const
{
    // COMMENTED SECONDARY DBG LOGS */// std::cout << "WavesAudioBackend::my_name: " << _port_prefix_name << std::endl;
    return __instantiated_name;
}


bool
WavesAudioBackend::can_monitor_input () const
{
    // COMMENTED DBG LOGS */ std::cout << "WavesAudioBackend::can_monitor_input: " << std::endl;
    return false;
}

std::string WavesAudioBackend::__instantiated_name;

AudioBackendInfo WavesAudioBackend::__backend_info = {
#ifdef __APPLE__
    "CoreAudio",
#elif PLATFORM_WINDOWS
    "ASIO",
#endif
    __instantiate,
    WavesAudioBackend::__deinstantiate,
    WavesAudioBackend::__waves_backend_factory,
    WavesAudioBackend::__already_configured,
    WavesAudioBackend::__available,
};


