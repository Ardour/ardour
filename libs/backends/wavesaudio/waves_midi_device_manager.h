/*
    Copyright (C) 2013 Waves Audio Ltd.

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

#ifndef __libardour_waves_midi_device_manager_h__
#define __libardour_waves_midi_device_manager_h__

#include "waves_midi_device.h"

namespace ARDOUR {

class WavesAudioBackend;

class WavesMidiDeviceManager {
public:
    WavesMidiDeviceManager (WavesAudioBackend& audiobackend);
    ~WavesMidiDeviceManager ();

    inline const std::vector<WavesMidiDevice *>& devices () const 
    {
        return _devices;
    }

    int start ();
    int stop ();
    int stream (bool yn);
    int is_streaming () { return _streaming; }
    void do_read ();
    void do_write ();

private:

    int _create_devices ();
    int _delete_devices ();
    static void __portmidi_callback (PtTimestamp timestamp, void * userData);
    void _portmidi_callback (PtTimestamp timestamp);
    /** __get_time_ms is given to Pm_Open functions (see WavesMidiDevice.cc)
     *  to provide the time in milliseconds using the time of audio
     *  transport. 
     *  time_info is a pointer on the backend instance, which agregates the
     *  audio and miditransports. It's not checked for correctness to consume
     *  no time. 
     */
    static PmTimestamp __get_time_ms (void *time_info);

    WavesMidiDevice* _get_device (const std::string& name);

    std::vector<WavesMidiDevice*> _devices; // Vector for midi devices
    bool _active;
    bool _streaming;

    size_t _input_device_count;
    size_t _output_device_count;
    WavesAudioBackend& _audiobackend;
};

} // namespace

#endif /* __libardour_waves_midi_device_manager_h__ */
    
