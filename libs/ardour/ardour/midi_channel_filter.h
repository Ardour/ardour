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

#ifndef __ardour_channel_filter_h__
#define __ardour_channel_filter_h__

#include <stdint.h>

#include <glib.h>

#include "ardour/types.h"
#include "pbd/signals.h"

namespace ARDOUR
{

class BufferSet;

/** Filter/mapper for MIDI channels.
 *
 * Channel mapping is configured by setting a mode and "mask", where the
 * meaning of the mask depends on the mode.
 *
 * If mode is FilterChannels, each mask bit represents a midi channel (bit 0 =
 * channel 0, bit 1 = channel 1, ...).  Only events whose channel corresponds
 * to a 1 bit will be passed.
 *
 * If mode is ForceChannel, mask is simply a channel number which all events
 * will be forced to.
 */
class LIBARDOUR_API MidiChannelFilter
{
public:
	MidiChannelFilter();

	/** Filter `bufs` in-place. */
	void filter(BufferSet& bufs);

	/** Filter/map a MIDI message by channel.
	 *
	 * May modify the channel in `buf` if necessary.
	 *
	 * @return true if this event should be filtered out.
	 */
	bool filter(uint8_t* buf, uint32_t len);

	/** Atomically set the channel mode and corresponding mask.
	 * @return true iff configuration changed.
	 */
	bool set_channel_mode(ChannelMode mode, uint16_t mask);

	/** Atomically set the channel mask for the current mode.
	 * @return true iff configuration changed.
	 */
	bool set_channel_mask(uint16_t mask);

	/** Atomically get both the channel mode and mask. */
	void get_mode_and_mask(ChannelMode* mode, uint16_t* mask) const {
		const uint32_t mm = g_atomic_int_get(&_mode_mask);
		*mode = static_cast<ChannelMode>((mm & 0xFFFF0000) >> 16);
		*mask = (mm & 0x0000FFFF);
	}

	ChannelMode get_channel_mode() const {
		return static_cast<ChannelMode>((g_atomic_int_get(&_mode_mask) & 0xFFFF0000) >> 16);
	}

	uint16_t get_channel_mask() const {
		return g_atomic_int_get(&_mode_mask) & 0x0000FFFF;
	}

	PBD::Signal0<void> ChannelMaskChanged;
	PBD::Signal0<void> ChannelModeChanged;

private:
	uint32_t _mode_mask;  ///< 16 bits mode, 16 bits mask
};

} /* namespace ARDOUR */

#endif /* __ardour_channel_filter_h__ */
