//
//  add_tracks_dialog.cc
//  tracks
//
//  Created by User on 8/5/14.
//
//

#include "add_tracks_dialog.h"

#include "ardour/chan_count.h"

#include <stdio.h>
#include "i18n.h"

#include "utils.h"
#include "pbd/unwind.h"
#include <gtkmm2ext/utils.h>

using namespace ARDOUR;
using namespace std;
using namespace Gtk;
using namespace Gdk;
using namespace Glib;
using namespace PBD;
using namespace Gtkmm2ext;

#define dbg_msg(a) MessageDialog (a, PROGRAM_NAME).run();


AddTracksDialog::AddTracksDialog ()
: WavesDialog (_("add_tracks_dialog.xml"), true, false)
, _decrement_button (get_waves_button ("decrement_button"))
, _increment_button (get_waves_button ("increment_button"))
, _cancel_button (get_waves_button ("cancel_button"))
, _ok_button (get_waves_button ("ok_button"))
, _tracks_format_dropdown (get_waves_dropdown ("tracks_format_dropdown"))
, _tracks_counter_entry (get_entry("tracks_counter_entry"))
{
    populate_tracks_format_dropdown();
    _tracks_counter_entry.set_text("1");
    
    _cancel_button.signal_clicked.connect (sigc::mem_fun (*this, &AddTracksDialog::on_cancel_button));
    _ok_button.signal_clicked.connect (sigc::mem_fun (*this, &AddTracksDialog::on_ok_button));
    _decrement_button.signal_clicked.connect (sigc::mem_fun (*this, &AddTracksDialog::on_decrement_button));
    _increment_button.signal_clicked.connect (sigc::mem_fun (*this, &AddTracksDialog::on_increment_button));
}

void
AddTracksDialog::populate_tracks_format_dropdown ()
{
    _tracks_format_dropdown.set_text(TrackFormat::FormatMono);
}

void
AddTracksDialog::on_cancel_button (WavesButton*)
{
    hide();
	response (Gtk::RESPONSE_CANCEL);
}

void
AddTracksDialog::on_ok_button (WavesButton*)
{
    hide();
	response (Gtk::RESPONSE_YES);
}

void
AddTracksDialog::on_decrement_button (WavesButton*)
{
    int track_count = count();
    
    if( track_count > 1 )
    {
        --track_count;
        set_track_count(track_count);
    } else
    {
        set_track_count(1);
    }
}

void
AddTracksDialog::on_increment_button (WavesButton*)
{
    int track_count = count();
    
    if( 1<=track_count && track_count <= 256 )
    {
        ++track_count;
        set_track_count(track_count);
    } else
    {
        set_track_count(1);
    }
    
    input_channels ();
}

int
AddTracksDialog::count ()
{
    string str_track_count = _tracks_counter_entry.get_text();
    char * pEnd;
    return strtol( str_track_count.c_str(), &pEnd, 10 );
}

void
AddTracksDialog::set_track_count (int track_count)
{
    stringstream ss;
    ss << track_count;
    string str_track_count = ss.str();
    
    _tracks_counter_entry.set_text(str_track_count);
}

ChanCount
AddTracksDialog::input_channels ()
{
    ChanCount channel_count;
    
    string track_format = _tracks_format_dropdown.get_text();
    
    if( track_format == TrackFormat::FormatMono )
        channel_count.set(DataType::AUDIO, 1);
    else if( track_format == TrackFormat::FormatStereo )
        channel_count.set(DataType::AUDIO, 2);
    
    channel_count.set (DataType::MIDI, 0);
    
    return channel_count;
}

void
AddTracksDialog::setup ()
{
    set_track_count(1);
    _tracks_format_dropdown.set_text(TrackFormat::FormatMono);
}

