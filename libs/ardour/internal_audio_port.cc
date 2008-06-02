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
#include <ardour/internal_audio_port.h>
#include <ardour/audioengine.h>

using namespace ARDOUR;
using namespace std;

void
InternalAudioPort::default_mixdown (const list<InternalPort*>& ports, AudioBuffer& dest, nframes_t cnt, nframes_t offset)
{
	list<InternalPort*>::const_iterator p = ports.begin();

	dest.read_from ((dynamic_cast<AudioPort*>(*p))->get_audio_buffer(), cnt, offset);

	for (; p != ports.end(); ++p) {
		dest.accumulate_from ((dynamic_cast<AudioPort*>(*p))->get_audio_buffer(), cnt, offset);
	}
}

InternalAudioPort::InternalAudioPort(const string& name, Flags flags)
	: Port (DataType::AUDIO, flags)
	, AudioPort (flags, engine->frames_per_cycle())
	, InternalPort (name, DataType::AUDIO, flags)
{
	_mixdown = default_mixdown;
}

void 
InternalAudioPort::set_mixdown_function (void (*func)(const list<InternalPort*>&, AudioBuffer&, nframes_t, nframes_t))
{
	_mixdown = func;
}

void
InternalAudioPort::reset ()
{
	_buffer.resize (engine->frames_per_cycle());
	_buffer.silence (_buffer.size());
}

AudioBuffer&
InternalAudioPort::get_audio_buffer ()
{
	if (_connections.empty()) {
		return AudioPort::get_audio_buffer();
	}

	/* XXX what about offset/size being more dynamic ? */
	
	(*_mixdown) (_connections, _buffer, _buffer.size(), 0);

	return _buffer;
}
