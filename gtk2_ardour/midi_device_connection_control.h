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

#ifndef __midi_device_connection_control_h__
#define __midi_device_connection_control_h__

#include <inttypes.h>
#include <gtkmm/layout.h>
#include "waves_ui.h"


class MidiDeviceConnectionControl : public Gtk::Layout, public WavesUI
{
public:

    static const char* capture_id_name;
    static const char* playback_id_name;
    
	MidiDeviceConnectionControl (const std::string& midi_device_name,
                                 bool has_capture, bool capture_active,
                                 bool has_playback, bool playback_active);
    
    bool has_capture() {return _has_capture; }
    bool has_playback() {return _has_playback; }
	void set_capture_active (bool active);
    void set_playback_active (bool active);
    
    sigc::signal2<void, MidiDeviceConnectionControl*, bool> signal_capture_active_changed;
    sigc::signal2<void, MidiDeviceConnectionControl*, bool> signal_playback_active_changed;
    
private:
    void init(const std::string& name, bool capture_active, bool playback_active );

	void on_capture_active_on(WavesButton*);
	void on_capture_active_off(WavesButton*);
    void on_playback_active_on(WavesButton*);
	void on_playback_active_off(WavesButton*);
    
    // flag which reflects control "active" state
    bool _has_capture;
    bool _capture_active;
    bool _has_playback;
    bool _playback_active;
	
	WavesButton* _capture_on_button;
	WavesButton* _capture_off_button;
    WavesButton* _playback_on_button;
	WavesButton* _playback_off_button;
	Gtk::Label* _name_label;
};

#endif // __midi_device_connection_control_h__
