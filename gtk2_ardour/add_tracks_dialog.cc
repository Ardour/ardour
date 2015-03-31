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

AddTracksDialog::AddTracksDialog ()
  : WavesDialog (_("add_tracks_dialog.xml"), true, false)
  , _decrement_button (get_waves_button ("decrement_button"))
  , _increment_button (get_waves_button ("increment_button"))
  , _cancel_button (get_waves_button ("cancel_button"))
  , _ok_button (get_waves_button ("ok_button"))
  , _tracks_format_dropdown (get_waves_dropdown ("tracks_format_dropdown"))
  , _tracks_counter_entry (get_entry("tracks_counter_entry"))
  , _max_tracks_count (xml_property (*xml_tree ()->root (), "maxtrackscount", 256))

{
    _tracks_format_dropdown.set_current_item (0);
    
    _cancel_button.signal_clicked.connect (sigc::mem_fun (*this, &AddTracksDialog::on_cancel_button));
    _ok_button.signal_clicked.connect (sigc::mem_fun (*this, &AddTracksDialog::on_ok_button));
    _decrement_button.signal_clicked.connect (sigc::mem_fun (*this, &AddTracksDialog::on_decrement_button));
    _increment_button.signal_clicked.connect (sigc::mem_fun (*this, &AddTracksDialog::on_increment_button));
}

void
AddTracksDialog::on_show ()
{
    WavesDialog::on_show ();
    _tracks_counter_entry.select_region (0, -1);
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
	response (WavesDialog::RESPONSE_DEFAULT);
}

void
AddTracksDialog::on_decrement_button (WavesButton*)
{
    unsigned int track_count = get_track_count ();
    
    set_track_count (track_count - 1);
	_tracks_counter_entry.set_position (-1); // set cursor at the last position
}

void
AddTracksDialog::on_increment_button (WavesButton*)
{
    unsigned int track_count = get_track_count ();
    
    set_track_count (track_count + 1);
	_tracks_counter_entry.set_position (-1); // set cursor at the last position
}

unsigned int
AddTracksDialog::get_track_count ()
{
    std::string str_track_count = _tracks_counter_entry.get_text();
    char * pEnd;
    int number = strtol( str_track_count.c_str(), &pEnd, 10 );
    number = number >= 0 ? number : 0;
    return std::min (_max_tracks_to_add, (unsigned int)number);
}

void
AddTracksDialog::set_track_count (unsigned int track_count)
{
    _tracks_counter_entry.set_text ( string_compose ("%1", std::max ((unsigned int)1, std::min (track_count, _max_tracks_to_add))) );
}

ARDOUR::ChanCount
AddTracksDialog::input_channels ()
{
    ARDOUR::ChanCount channel_count;
    
    unsigned int n_channels = _tracks_format_dropdown.get_item_data_u (_tracks_format_dropdown.get_current_item ());
    
    channel_count.set(ARDOUR::DataType::AUDIO, n_channels);
    channel_count.set (ARDOUR::DataType::MIDI, 0);
    
    return channel_count;
}

void
AddTracksDialog::setup (unsigned int max_tracks_to_add)
{
	_max_tracks_to_add = max_tracks_to_add;

    set_track_count(1);
    _tracks_format_dropdown.set_current_item (0);
}
