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
: Gtk::Layout()
, _capture_active(false)
, _playback_active(false)
, _capture_on_button (NULL)
, _capture_off_button (NULL)
, _playback_on_button (NULL)
, _playback_off_button (NULL)
, _name_label (NULL)

{
	build_layout("midi_device_control.xml");
    
    _capture_on_button = &_children.get_waves_button ("capture_on_button");
    _capture_off_button = &_children.get_waves_button ("capture_off_button");
    
    if (!has_capture) {
        _capture_on_button->hide();
        _capture_off_button->hide();
        _capture_on_button = NULL;
        _capture_off_button = NULL;
    }

    _playback_on_button = &_children.get_waves_button ("playback_on_button");
    _playback_off_button = &_children.get_waves_button ("playback_off_button");
    
    if (!has_playback) {
        _playback_on_button->hide();
        _playback_off_button->hide();
        _playback_on_button = NULL;
        _playback_off_button = NULL;
    }
    
	_name_label = &_children.get_label ("midi_device_name_label");
	init(midi_device_name, capture_active, playback_active);
}


void MidiDeviceConnectionControl::init(const std::string& name, bool capture_active, bool playback_active )
{
	_capture_on_button->signal_clicked.connect (sigc::mem_fun (*this, &MidiDeviceConnectionControl::on_capture_active_on));
	_capture_off_button->signal_clicked.connect (sigc::mem_fun (*this, &MidiDeviceConnectionControl::on_capture_active_off));
    _playback_on_button->signal_clicked.connect (sigc::mem_fun (*this, &MidiDeviceConnectionControl::on_playback_active_on));
	_playback_off_button->signal_clicked.connect (sigc::mem_fun (*this, &MidiDeviceConnectionControl::on_playback_active_off));
    
	if (_name_label != NULL) {
		_name_label->set_text (name);
	}
    
	set_capture_active(capture_active);
    set_playback_active(playback_active);
}


bool
MidiDeviceConnectionControl::build_layout (const std::string& file_name)
{
	const XMLTree* layout = WavesUI::load_layout(file_name);
	if (layout == NULL) {
		return false;
	}
    
	XMLNode* root  = layout->root();
	if ((root == NULL) || strcasecmp(root->name().c_str(), "layout")) {
		return false;
	}
    
	WavesUI::set_attributes(*this, *root, XMLNodeMap());
	WavesUI::create_ui(layout, *this, _children);
	return true;
}


void
MidiDeviceConnectionControl::set_capture_active (bool active)
{
    if (_capture_on_button == NULL || _capture_off_button == NULL) {
        return;
    }
    
	_capture_on_button->set_active (active);
	_capture_off_button->set_active (!active);
    _capture_active = active;
}


void
MidiDeviceConnectionControl::set_playback_active (bool active)
{
    if (_playback_on_button == NULL || _playback_off_button == NULL) {
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
