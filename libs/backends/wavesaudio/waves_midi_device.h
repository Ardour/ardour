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

#ifndef __libardour_waves_midi_device_h__
#define __libardour_waves_midi_device_h__

#include <portmidi/portmidi.h>
#include <portmidi/pmutil.h>
#include <portmidi/porttime.h>

#include "ardour/types.h"

namespace ARDOUR {

class WavesMidiEvent;

class WavesMidiDevice {
public:
    WavesMidiDevice (const std::string& name);
    ~WavesMidiDevice ();

    inline const std::string& name () const { return _name; }

    int open (PmTimeProcPtr time_proc, void* time_info);
    void close ();
    void do_io ();
    void read_midi ();
    void write_midi ();

    int enqueue_output_waves_midi_event (const WavesMidiEvent* waves_midi_event);
    WavesMidiEvent* dequeue_input_waves_midi_event ();

    inline bool is_input () const { return _pm_input_id != pmNoDevice; };
    inline bool is_output () const { return _pm_output_id != pmNoDevice; };

private:


    PmDeviceID _pm_input_id;
    PmDeviceID _pm_output_id;
    const std::string _name;

    /* shared queues */
    PmQueue* _input_queue;
    PmQueue* _output_queue;

    PmStream* _input_pm_stream;
    PmStream* _output_pm_stream;
    WavesMidiEvent *_incomplete_waves_midi_event;
};

} // namespace

#endif /* __libardour_waves_midi_device_h__ */
    
