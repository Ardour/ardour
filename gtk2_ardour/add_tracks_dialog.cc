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

#include "add_tracks_dialog.h"

#include "ardour/chan_count.h"

#include <stdio.h>
#include "i18n.h"

#include "utils.h"
#include "pbd/unwind.h"
#include <gtkmm2ext/utils.h>
#include "dbg_msg.h"

using namespace ARDOUR;
using namespace std;
using namespace Gtk;
using namespace Gdk;
using namespace Glib;
using namespace PBD;
using namespace Gtkmm2ext;

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
AddTracksDialog::on_show ()
{
    WavesDialog::on_show ();
    _tracks_counter_entry.set_position (-1); // set cursor at the last position
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
    _tracks_counter_entry.set_position (-1); // set cursor at the last position
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
    _tracks_counter_entry.set_position (-1); // set cursor at the last position
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

void
AddTracksDialog::on_enter_pressed ()
{
    hide();
    response (Gtk::RESPONSE_YES);
}

void
AddTracksDialog::on_esc_pressed ()
{
    hide();
    response (Gtk::RESPONSE_CANCEL);
}
