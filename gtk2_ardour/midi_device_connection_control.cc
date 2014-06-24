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

#include "midi_device_connection_control.h"
#include "pbd/convert.h"

const char * MidiDeviceConnectionControl::capture_id_name = "_capture_id_name";
const char * MidiDeviceConnectionControl::playback_id_name = "_playback_id_name";


MidiDeviceConnectionControl::MidiDeviceConnectionControl (const std::string& midi_device_name,
                                                          bool has_capture, bool capture_active,
                                                          bool has_playback, bool playback_active)
: Gtk::Layout ()
, WavesUI ("midi_device_control.xml", *this)
, _has_capture (has_capture)
, _capture_active (capture_active)
, _has_playback (has_playback)
, _playback_active (playback_active)

{
	XMLNode* root  = xml_tree()->root();
	WavesUI::set_attributes(*this, *root, XMLNodeMap());

    _capture_on_button = &get_waves_button ("capture_on_button");
    _capture_off_button = &get_waves_button ("capture_off_button");
    
    if (!_has_capture) {
        _capture_on_button->hide();
        _capture_off_button->hide();
    }

    _playback_on_button = &get_waves_button ("playback_on_button");
    _playback_off_button = &get_waves_button ("playback_off_button");
    
    if (!_has_playback) {
        _playback_on_button->hide();
        _playback_off_button->hide();
    }
    
    _name_label = &get_label ("midi_device_name_label");
    
	init(midi_device_name, capture_active, playback_active);
}


void MidiDeviceConnectionControl::init(const std::string& name, bool capture_active, bool playback_active )
{
    if (_has_capture ) {
        _capture_on_button->signal_clicked.connect (sigc::mem_fun (*this, &MidiDeviceConnectionControl::on_capture_active_on));
        _capture_off_button->signal_clicked.connect (sigc::mem_fun (*this, &MidiDeviceConnectionControl::on_capture_active_off));
    }
	
    if (_has_playback ) {
        _playback_on_button->signal_clicked.connect (sigc::mem_fun (*this, &MidiDeviceConnectionControl::on_playback_active_on));
        _playback_off_button->signal_clicked.connect (sigc::mem_fun (*this, &MidiDeviceConnectionControl::on_playback_active_off));
    }

    _name_label->set_text (name);
	_name_label->set_tooltip_text(name);
    
	set_capture_active(capture_active);
    set_playback_active(playback_active);
}


void
MidiDeviceConnectionControl::set_capture_active (bool active)
{
    if (!_has_capture) {
        return;
    }
    
	_capture_on_button->set_active (active);
	_capture_off_button->set_active (!active);
    _capture_active = active;
}


void
MidiDeviceConnectionControl::set_playback_active (bool active)
{
    if (!_has_playback) {
        return;
    }
    
	_playback_on_button->set_active (active);
	_playback_off_button->set_active (!active);
    _playback_active = active;
}


void
MidiDeviceConnectionControl::on_capture_active_on(WavesButton*)
{
    if (_capture_active) {
        return;
    }
    
	set_capture_active (true);
	signal_capture_active_changed(this, true);
}

void
MidiDeviceConnectionControl::on_capture_active_off(WavesButton*)
{
    if (!_capture_active) {
        return;
    }
    
	set_capture_active (false);
	signal_capture_active_changed(this, false);
}


void
MidiDeviceConnectionControl::on_playback_active_on(WavesButton*)
{
    if (_playback_active) {
        return;
    }
    
	set_playback_active (true);
	signal_playback_active_changed(this, true);
}

void
MidiDeviceConnectionControl::on_playback_active_off(WavesButton*)
{
    if (!_playback_active) {
        return;
    }
    
	set_playback_active (false);
	signal_playback_active_changed(this, false);
}
