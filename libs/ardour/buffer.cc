/*
 * Copyright (C) 2006-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2009 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "ardour/buffer.h"
#include "ardour/audio_buffer.h"
#include "ardour/midi_buffer.h"

namespace ARDOUR {


Buffer*
Buffer::create(DataType type, size_t capacity)
{
	if (type == DataType::AUDIO)
		return new AudioBuffer(capacity);
	else if (type == DataType::MIDI)
		return new MidiBuffer(capacity);
	else
		return NULL;
}


} // namespace ARDOUR

