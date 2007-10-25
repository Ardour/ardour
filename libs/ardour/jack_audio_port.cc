/*
    Copyright (C) 2006 Paul Davis 

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

#include <cassert>
#include <ardour/audioengine.h>
#include <ardour/jack_audio_port.h>

using namespace ARDOUR;

JackAudioPort::JackAudioPort(const std::string& name, Flags flgs)
	: Port (DataType::AUDIO, flgs), AudioPort (flgs), JackPort (name, DataType::AUDIO, flgs)
{

}

int
JackAudioPort::reestablish ()
{
	int ret = JackPort::reestablish ();
	
	if (ret == 0 && _flags & IsOutput) {
		_buffer.silence (jack_get_buffer_size (engine->jack()));
	}

	return ret;
}
