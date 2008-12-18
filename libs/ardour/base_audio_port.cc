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
#include <glib.h>
#include <ardour/base_audio_port.h>
#include <ardour/audioengine.h>
#include <ardour/data_type.h>

using namespace ARDOUR;
using namespace std;

nframes_t BaseAudioPort::_short_over_length = 2;
nframes_t BaseAudioPort::_long_over_length = 10;

BaseAudioPort::BaseAudioPort (const std::string& name, Flags flgs)
	: Port (name, flgs)
	, _buffer (0)
	, _own_buffer (false)
{
	_type = DataType::AUDIO;
	_mixdown = default_mixdown;
}

BaseAudioPort::~BaseAudioPort ()
{
	if (_own_buffer) {
		delete _buffer;
	}
}

void
BaseAudioPort::reset()
{
	Port::reset();

	if (_own_buffer && _buffer) {
		_buffer->resize (engine->frames_per_cycle());
		_buffer->clear ();
	}
	
	_metering = 0;
	reset_meters ();
}

void
BaseAudioPort::default_mixdown (const set<Port*>& ports, AudioBuffer* dest, nframes_t cnt, nframes_t offset, bool first_overwrite)
{
	set<Port*>::const_iterator p = ports.begin();

	if (first_overwrite) {
		dest->read_from ((dynamic_cast<BaseAudioPort*>(*p))->get_audio_buffer( cnt, offset ), cnt, offset);
		p++;
	}

	for (; p != ports.end(); ++p) {
		dest->accumulate_from ((dynamic_cast<BaseAudioPort*>(*p))->get_audio_buffer( cnt, offset ), cnt, offset);
	}
}

void 
BaseAudioPort::set_mixdown_function (void (*func)(const set<Port*>&, AudioBuffer*, nframes_t, nframes_t, bool))
{
	g_atomic_pointer_set(&_mixdown, func);
}


