/*
 * Copyright (C) 1998-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
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

#include <cstring>
#include "midi++/types.h"
#include "midi++/port.h"
#include "midi++/channel.h"

using namespace MIDI;

Channel::Channel (MIDI::byte channelnum, Port &p)
	: _port (p)
	, _channel_number (channelnum)
	, _rpn_msb (0)
	, _rpn_lsb (0)
	, _nrpn_msb (0)
	, _nrpn_lsb (0)
	, _rpn_state (RPNState (0))
	, _nrpn_state (RPNState (0))
{
	reset (0, 1, false);
}

void
Channel::connect_signals ()
{
	_port.parser()->channel_pressure[_channel_number].connect_same_thread (*this, boost::bind (&Channel::process_chanpress, this, _1, _2));
	_port.parser()->channel_note_on[_channel_number].connect_same_thread (*this, boost::bind (&Channel::process_note_on, this, _1, _2));
	_port.parser()->channel_note_off[_channel_number].connect_same_thread (*this, boost::bind (&Channel::process_note_off, this, _1, _2));
	_port.parser()->channel_poly_pressure[_channel_number].connect_same_thread (*this, boost::bind (&Channel::process_polypress, this, _1, _2));
	_port.parser()->channel_program_change[_channel_number].connect_same_thread (*this, boost::bind (&Channel::process_program_change, this, _1, _2));
	_port.parser()->channel_controller[_channel_number].connect_same_thread (*this, boost::bind (&Channel::process_controller, this, _1, _2));
	_port.parser()->channel_pitchbend[_channel_number].connect_same_thread (*this, boost::bind (&Channel::process_pitchbend, this, _1, _2));

	_port.parser()->reset.connect_same_thread (*this, boost::bind (&Channel::process_reset, this, _1));
}

void
Channel::reset (timestamp_t timestamp, samplecnt_t /*nframes*/, bool notes_off)
{
	_program_number = _channel_number;
	_bank_number = 0;
	_pitch_bend = 0;

	_last_note_on = 0;
	_last_note_off = 0;
	_last_on_velocity = 0;
	_last_off_velocity = 0;

	if (notes_off) {
		all_notes_off (timestamp);
	}

	memset (_polypress, 0, sizeof (_polypress));
	memset (_controller_msb, 0, sizeof (_controller_msb));
	memset (_controller_lsb, 0, sizeof (_controller_lsb));

	/* zero all controllers XXX not necessarily the right thing */

	memset (_controller_val, 0, sizeof (_controller_val));

	for (int n = 0; n < 128; n++) {
		_controller_14bit[n] = false;
	}

	rpn_reset ();
	nrpn_reset ();

	_omni = true;
	_poly = false;
	_mono = true;
	_notes_on = 0;
}

void
Channel::rpn_reset ()
{
	_rpn_msb = 0;
	_rpn_lsb = 0;
	_rpn_val_msb = 0;
	_rpn_val_lsb = 0;
	_rpn_state = RPNState (0);
}

void
Channel::nrpn_reset ()
{
	_nrpn_msb = 0;
	_nrpn_lsb = 0;
	_nrpn_val_msb = 0;
	_nrpn_val_lsb = 0;
	_nrpn_state = RPNState (0);
}

void
Channel::process_note_off (Parser & /*parser*/, EventTwoBytes *tb)
{
	_last_note_off = tb->note_number;
	_last_off_velocity = tb->velocity;

	if (_notes_on) {
		_notes_on--;
	}
}

void
Channel::process_note_on (Parser & /*parser*/, EventTwoBytes *tb)
{
	_last_note_on = tb->note_number;
	_last_on_velocity = tb->velocity;
	_notes_on++;
}

const Channel::RPNState Channel::RPN_READY_FOR_VALUE = RPNState (HaveLSB|HaveMSB);
const Channel::RPNState Channel::RPN_VALUE_READY = RPNState (HaveLSB|HaveMSB|HaveValue);

bool
Channel::maybe_process_rpns (Parser& parser, EventTwoBytes *tb)
{
	switch (tb->controller_number) {
	case 0x62:
		_rpn_state = RPNState (_rpn_state|HaveMSB);
		_rpn_lsb = tb->value;
		if (_rpn_msb == 0x7f && _rpn_lsb == 0x7f) {
			rpn_reset ();
		}
		return true;
	case 0x63:
		_rpn_state = RPNState (_rpn_state|HaveLSB);
		_rpn_msb = tb->value;
		if (_rpn_msb == 0x7f && _rpn_lsb == 0x7f) {
			rpn_reset ();
		}
		return true;

	case 0x64:
		_nrpn_state = RPNState (_rpn_state|HaveMSB);
		_rpn_lsb = tb->value;
		if (_nrpn_msb == 0x7f && _nrpn_lsb == 0x7f) {
			nrpn_reset ();
		}
		return true;
	case 0x65:
		_nrpn_state = RPNState (_rpn_state|HaveLSB);
		_rpn_msb = tb->value;
		if (_rpn_msb == 0x7f && _rpn_lsb == 0x7f) {
			nrpn_reset ();
		}
		return true;
	}

	if ((_nrpn_state & RPN_READY_FOR_VALUE) == RPN_READY_FOR_VALUE) {

		uint16_t rpn_id = (_rpn_msb << 7)|_rpn_lsb;

		switch (tb->controller_number) {
		case 0x60:
			/* data increment */
			_nrpn_state = RPNState (_nrpn_state|HaveValue);
			parser.channel_nrpn_change[_channel_number] (parser, rpn_id, 1); /* EMIT SIGNAL */
			return true;
		case 0x61:
			/* data decrement */
			_nrpn_state = RPNState (_nrpn_state|HaveValue);
			parser.channel_nrpn_change[_channel_number] (parser, rpn_id, -1); /* EMIT SIGNAL */
			return true;
		case 0x06:
			/* data entry MSB */
			_nrpn_state = RPNState (_nrpn_state|HaveValue);
			_nrpn_val_msb = tb->value;
			break;
		case 0x26:
			/* data entry LSB */
			_nrpn_state = RPNState (_nrpn_state|HaveValue);
			_nrpn_val_lsb = tb->value;
		}

		if (_nrpn_state == RPN_VALUE_READY) {

			float rpn_val = ((_rpn_val_msb << 7)|_rpn_val_lsb)/16384.0;

			std::pair<RPNList::iterator,bool> result = nrpns.insert (std::make_pair (rpn_id, rpn_val));

			if (!result.second) {
				result.first->second = rpn_val;
			}

			parser.channel_nrpn[_channel_number] (parser, rpn_id, rpn_val); /* EMIT SIGNAL */
			return true;
		}

	} else if ((_rpn_state & RPN_READY_FOR_VALUE) == RPN_READY_FOR_VALUE) {

		uint16_t rpn_id = (_rpn_msb << 7)|_rpn_lsb;

		switch (tb->controller_number) {
		case 0x60:
			/* data increment */
			_rpn_state = RPNState (_rpn_state|HaveValue);
			parser.channel_rpn_change[_channel_number] (parser, rpn_id, 1); /* EMIT SIGNAL */
			return true;
		case 0x61:
			/* data decrement */
			_rpn_state = RPNState (_rpn_state|HaveValue);
			parser.channel_rpn_change[_channel_number] (parser, rpn_id, -1); /* EMIT SIGNAL */
			return true;
		case 0x06:
			/* data entry MSB */
			_rpn_state = RPNState (_rpn_state|HaveValue);
			_rpn_val_msb = tb->value;
			break;
		case 0x26:
			/* data entry LSB */
			_rpn_state = RPNState (_rpn_state|HaveValue);
			_rpn_val_lsb = tb->value;
		}

		if (_rpn_state == RPN_VALUE_READY) {

			float    rpn_val = ((_rpn_val_msb << 7)|_rpn_val_lsb)/16384.0;

			std::pair<RPNList::iterator,bool> result = rpns.insert (std::make_pair (rpn_id, rpn_val));

			if (!result.second) {
				result.first->second = rpn_val;
			}

			parser.channel_rpn[_channel_number] (parser, rpn_id, rpn_val); /* EMIT SIGNAL */
			return true;
		}
	}

	return false;
}

void
Channel::process_controller (Parser & parser, EventTwoBytes *tb)
{
	unsigned short cv;

	/* XXX arguably need a lock here to protect non-atomic changes
	   to controller_val[...]. or rather, need to make sure that
	   all changes *are* atomic.
	*/

	if (maybe_process_rpns (parser, tb)) {
		return;
	}

	/* Note: if RPN data controllers (0x60, 0x61, 0x6, 0x26) are received
	 * without a previous RPN parameter ID message, or after the RPN ID
	 * has been reset, they will be treated like ordinary CC messages.
	 */


	if (tb->controller_number < 32) { /* unsigned: no test for >= 0 */

		/* if this controller is already known to use 14 bits,
		   then treat this value as the MSB, and combine it
		   with the existing LSB.

		   otherwise, just treat it as a 7 bit value, and set
		   it directly.
		*/

		cv = (unsigned short) _controller_val[tb->controller_number];

		if (_controller_14bit[tb->controller_number]) {
			cv = ((tb->value & 0x7f) << 7) | (cv & 0x7f);
		} else {
			cv = tb->value;
		}

		_controller_val[tb->controller_number] = (controller_value_t)cv;

	} else if ((tb->controller_number >= 32 &&
		    tb->controller_number <= 63)) {

		int cn = tb->controller_number - 32;

		cv = (unsigned short) _controller_val[cn];

		/* LSB for CC 0-31 arrived.

		   If this is the first time (i.e. its currently
		   flagged as a 7 bit controller), mark the
		   controller as 14 bit, adjust the existing value
		   to be the MSB, and OR-in the new LSB value.

		   otherwise, OR-in the new low 7bits with the old
		   high 7.
		*/


		if (_controller_14bit[cn] == false) {
			_controller_14bit[cn] = true;
			cv = (cv << 7) | (tb->value & 0x7f);
		} else {
			cv = (cv & 0x3f80) | (tb->value & 0x7f);
		}

		/* update the 14 bit value */
		_controller_val[cn] = (controller_value_t) cv;

		/* also store the "raw" 7 bit value in the incoming controller
		   value store
		*/
		_controller_val[tb->controller_number] = (controller_value_t) tb->value;

	} else {

		/* controller can only take 7 bit values */

		_controller_val[tb->controller_number] =
			(controller_value_t) tb->value;
	}

	/* bank numbers are special, in that they have their own signal
	 */

	if (tb->controller_number == 0 || tb->controller_number == 0x20) {
		_bank_number = _controller_val[0];
		_port.parser()->bank_change (*_port.parser(), _bank_number);
		_port.parser()->channel_bank_change[_channel_number] (*_port.parser(), _bank_number);
	}
}

void
Channel::process_program_change (Parser & /*parser*/, MIDI::byte val)
{
	_program_number = val;
}

void
Channel::process_chanpress (Parser & /*parser*/, MIDI::byte val)
{
	_chanpress = val;
}

void
Channel::process_polypress (Parser & /*parser*/, EventTwoBytes *tb)
{
	_polypress[tb->note_number] = tb->value;
}

void
Channel::process_pitchbend (Parser & /*parser*/, pitchbend_t val)
{
	_pitch_bend = val;
}

void
Channel::process_reset (Parser & /*parser*/)
{
	reset (0, 1);
}

/** Write a message to a channel.
 * \return true if success
 */
bool
Channel::channel_msg (MIDI::byte id, MIDI::byte val1, MIDI::byte val2, timestamp_t timestamp)
{
	unsigned char msg[3];
	int len = 0;

	msg[0] = id | (_channel_number & 0xf);

	switch (id) {
	case off:
		msg[1] = val1 & 0x7F;
		msg[2] = val2 & 0x7F;
		len = 3;
		break;

	case on:
		msg[1] = val1 & 0x7F;
		msg[2] = val2 & 0x7F;
		len = 3;
		break;

	case MIDI::polypress:
		msg[1] = val1 & 0x7F;
		msg[2] = val2 & 0x7F;
		len = 3;
		break;

	case controller:
		msg[1] = val1 & 0x7F;
		msg[2] = val2 & 0x7F;
		len = 3;
		break;

	case MIDI::program:
		msg[1] = val1 & 0x7F;
		len = 2;
		break;

	case MIDI::chanpress:
		msg[1] = val1 & 0x7F;
		len = 2;
		break;

	case MIDI::pitchbend:
		msg[1] = val1 & 0x7F;
		msg[2] = val2 & 0x7F;
		len = 3;
		break;
	}

	return _port.midimsg (msg, len, timestamp);
}

float
Channel::rpn_value (uint16_t rpn) const
{
	return rpn_value_absolute (rpn) / 16384.0f;
}

float
Channel::rpn_value_absolute (uint16_t rpn) const
{
	RPNList::const_iterator r = rpns.find (rpn);
	if (r == rpns.end()) {
		return 0.0;
	}
	return r->second;
}

float
Channel::nrpn_value (uint16_t nrpn) const
{
	return nrpn_value_absolute (nrpn) / 16384.0f;
}

float
Channel::nrpn_value_absolute (uint16_t nrpn) const
{
	RPNList::const_iterator r = nrpns.find (nrpn);
	if (r == nrpns.end()) {
		return 0.0;
	}
	return r->second;
}
