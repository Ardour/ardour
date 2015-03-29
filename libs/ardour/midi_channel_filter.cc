/*
    Copyright (C) 2006-2015 Paul Davis
    Author: David Robillard

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

#include "ardour/buffer_set.h"
#include "ardour/midi_buffer.h"
#include "ardour/midi_channel_filter.h"
#include "pbd/ffs.h"

namespace ARDOUR {

MidiChannelFilter::MidiChannelFilter()
	: _mode_mask(0x0000FFFF)
{}

void
MidiChannelFilter::filter(BufferSet& bufs)
{
	ChannelMode mode;
	uint16_t    mask;
	get_mode_and_mask(&mode, &mask);

	if (mode == AllChannels) {
		return;
	}

	MidiBuffer& buf = bufs.get_midi(0);

	for (MidiBuffer::iterator e = buf.begin(); e != buf.end(); ) {
		Evoral::MIDIEvent<framepos_t> ev(*e, false);

		if (ev.is_channel_event()) {
			switch (mode) {
			case FilterChannels:
				if (0 == ((1 << ev.channel()) & mask)) {
					e = buf.erase (e);
				} else {
					++e;
				}
				break;
			case ForceChannel:
				ev.set_channel(PBD::ffs(mask) - 1);
				++e;
				break;
			case AllChannels:
				/* handled by the opening if() */
				++e;
				break;
			}
		} else {
			++e;
		}
	}
}

bool
MidiChannelFilter::filter(uint8_t* buf, uint32_t len)
{
	ChannelMode mode;
	uint16_t    mask;
	get_mode_and_mask(&mode, &mask);

	const uint8_t type             = buf[0] & 0xF0;
	const bool    is_channel_event = (0x80 <= type) && (type <= 0xE0);
	if (!is_channel_event) {
		return false;
	}

	const uint8_t channel = buf[0] & 0x0F;
	switch (mode) {
	case AllChannels:
		return false;
	case FilterChannels:
		return !((1 << channel) & mask);
	case ForceChannel:
		buf[0] = (0xF0 & buf[0]) | (0x0F & (PBD::ffs(mask) - 1));
		return false;
	}

	return false;
}

/** If mode is ForceChannel, force mask to the lowest set channel or 1 if no
 *  channels are set.
 */
static inline uint16_t
force_mask(const ChannelMode mode, const uint16_t mask)
{
	return ((mode == ForceChannel)
	        ? (mask ? (1 << (PBD::ffs(mask) - 1)) : 1)
	        : mask);
}

bool
MidiChannelFilter::set_channel_mode(ChannelMode mode, uint16_t mask)
{
	ChannelMode old_mode;
	uint16_t    old_mask;
	get_mode_and_mask(&old_mode, &old_mask);

	if (old_mode != mode || old_mask != mask) {
		mask = force_mask(mode, mask);
		g_atomic_int_set(&_mode_mask, (uint32_t(mode) << 16) | uint32_t(mask));
		ChannelModeChanged();
		return true;
	}

	return false;
}

bool
MidiChannelFilter::set_channel_mask(uint16_t mask)
{
	ChannelMode mode;
	uint16_t    old_mask;
	get_mode_and_mask(&mode, &old_mask);

	if (old_mask != mask) {
		mask = force_mask(mode, mask);
		g_atomic_int_set(&_mode_mask, (uint32_t(mode) << 16) | uint32_t(mask));
		ChannelMaskChanged();
		return true;
	}

	return false;
}

} /* namespace ARDOUR */
