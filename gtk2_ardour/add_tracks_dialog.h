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

class AddTracksDialog : public WavesDialog {
public:
    
    AddTracksDialog ();
    void setup (unsigned int);
    unsigned int get_track_count ();
    ARDOUR::ChanCount input_channels ();
    void on_show ();
	unsigned int max_tracks_count () const { return _max_tracks_count; }
    
private:
	WavesButton& _decrement_button;
    WavesButton& _increment_button;
    WavesButton& _cancel_button;
    WavesButton& _ok_button;
    
	WavesDropdown& _tracks_format_dropdown;
    Gtk::Entry& _tracks_counter_entry;
    
	unsigned int _max_tracks_to_add;
	unsigned int _max_tracks_count; // Just a storage for usecase's limit
    
    void on_cancel_button (WavesButton*);
    void on_ok_button (WavesButton*);
    void on_decrement_button (WavesButton*);
    void on_increment_button (WavesButton*);
    
    void set_track_count(unsigned int track_count);
};

#endif
