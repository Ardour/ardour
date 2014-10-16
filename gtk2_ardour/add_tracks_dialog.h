//
//  add_tracks_dialog.h
//  tracks
//
//  Created by User on 8/5/14.
//
//

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
