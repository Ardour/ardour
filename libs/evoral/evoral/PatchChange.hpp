/*
  Copyright (C) 2010 Paul Davis
  Author: Carl Hetherington <cth@carlh.net>

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

#ifndef EVORAL_PATCH_CHANGE_HPP
#define EVORAL_PATCH_CHANGE_HPP

#include "evoral/Event.hpp"
#include "evoral/MIDIEvent.hpp"

namespace Evoral {

/** Event representing a `patch change', composed of a LSB and MSB
 *  bank select and then a program change.
 */
template<typename Time>
class PatchChange
{
public:
	/** @param t Time.
	 *  @param c Channel.
	 *  @param p Program change number (counted from 0).
	 *  @param b Bank number (counted from 0, 14-bit).
	 */
	PatchChange (Time t, uint8_t c, uint8_t p, int b)
		: _bank_change_msb (0, t, 3, 0, true)
		, _bank_change_lsb (0, t, 3, 0, true)
		, _program_change (0, t, 2, 0, true)
	{
		_bank_change_msb.buffer()[0] = MIDI_CMD_CONTROL | c;
		_bank_change_msb.buffer()[1] = MIDI_CTL_MSB_BANK;
		_bank_change_msb.buffer()[2] = (b >> 7) & 0x7f;

		_bank_change_lsb.buffer()[0] = MIDI_CMD_CONTROL | c;
		_bank_change_lsb.buffer()[1] = MIDI_CTL_LSB_BANK;
		_bank_change_lsb.buffer()[2] = b & 0x7f;

		_program_change.buffer()[0] = MIDI_CMD_PGM_CHANGE | c;
		_program_change.buffer()[1] = p;
	}

	PatchChange (const PatchChange & other)
		: _bank_change_msb (other._bank_change_msb, true)
		, _bank_change_lsb (other._bank_change_lsb, true)
		, _program_change (other._program_change, true)
	{
		set_id (other.id ());
	}

	event_id_t id () const {
		return _program_change.id ();
	}

	void set_id (event_id_t id) {
		_bank_change_msb.set_id (id);
		_bank_change_lsb.set_id (id);
		_program_change.set_id (id);
	}

	Time time () const {
		return _program_change.time ();
	}

	void set_time (Time t) {
		_bank_change_msb.set_time (t);
		_bank_change_lsb.set_time (t);
		_program_change.set_time (t);
	}

	void set_channel (uint8_t c) {
		_bank_change_msb.buffer()[0] &= 0xf0;
		_bank_change_msb.buffer()[0] |= c;
		_bank_change_lsb.buffer()[0] &= 0xf0;
		_bank_change_lsb.buffer()[0] |= c;
		_program_change.buffer()[0] &= 0xf0;
		_program_change.buffer()[0] |= c;
	}

	uint8_t program () const {
		return _program_change.buffer()[1];
	}

	void set_program (uint8_t p) {
		_program_change.buffer()[1] = p;
	}

	int bank () const {
		return (bank_msb() << 7) | bank_lsb();
	}

	void set_bank (int b) {
		_bank_change_msb.buffer()[2] = (b >> 7) & 0x7f;
		_bank_change_lsb.buffer()[2] = b & 0x7f;
	}

	uint8_t bank_msb () const {
		return _bank_change_msb.buffer()[2];
	}

	uint8_t bank_lsb () const {
		return _bank_change_lsb.buffer()[2];
	}

	uint8_t channel () const { return _program_change.buffer()[0] & 0xf; }

	inline bool operator< (const PatchChange<Time>& o) const {
		if (!musical_time_equal (time(), o.time())) {
			return time() < o.time();
		}

		if (bank != o.bank()) {
			return bank() < o.bank();
		}

		return (program() < o.program());
	}

	inline bool operator== (const PatchChange<Time>& o) const {
		return (musical_time_equal (time(), o.time()) && program() == o.program() && bank() == o.bank());
	}

	/** The PatchChange is made up of messages() MIDI messages; this method returns them by index.
	 *  @param i index of message to return.
	 */
	MIDIEvent<Time> const & message (int i) const {
		switch (i) {
		case 0:
			return _bank_change_msb;
		case 1:
			return _bank_change_lsb;
		case 2:
			return _program_change;
		default:
			abort ();
			/*NOTREACHED*/
			return _program_change;
		}
	}

	/** @return Number of MIDI messages that make up this change */
	int messages () const {
		return 3;
	}

private:
	MIDIEvent<Time> _bank_change_msb;
	MIDIEvent<Time> _bank_change_lsb;
	MIDIEvent<Time> _program_change;
};

}

template<typename Time>
std::ostream& operator<< (std::ostream& o, const Evoral::PatchChange<Time>& p) {
	o << "Patch Change " << p.id() << " @ " << p.time() << " bank " << (int) p.bank() << " program " << (int) p.program();
	return o;
}

#endif
