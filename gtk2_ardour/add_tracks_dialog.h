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

#ifndef tracks_add_tracks_dialog_h
#define tracks_add_tracks_dialog_h

#include "waves_dialog.h"
#include <string.h>
#include "ardour/chan_count.h"

namespace TrackFormat {
    const std::string FormatMono = "Mono";
    const std::string FormatStereo = "Stereo";
}

class AddTracksDialog : public WavesDialog {
public:
    
    AddTracksDialog ();
    void setup();
    int count();
    ARDOUR::ChanCount input_channels ();
    
protected:
    void on_enter_pressed ();
    void on_esc_pressed ();
    
private:
	WavesButton& _decrement_button;
    WavesButton& _increment_button;
    WavesButton& _cancel_button;
    WavesButton& _ok_button;
    
	WavesDropdown& _tracks_format_dropdown;
    Gtk::Entry& _tracks_counter_entry;
    
    void populate_tracks_format_dropdown();
    
    void on_cancel_button (WavesButton*);
    void on_ok_button (WavesButton*);
    void on_decrement_button (WavesButton*);
    void on_increment_button (WavesButton*);
    
    void set_track_count(int track_count);
};

#endif
