/*
    Copyright (C) 2000 Paul Barton-Davis 

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

    $Id$
*/

#include <fcntl.h>
#include <map>

#include "timecode/time.h"

#include "pbd/error.h"

#include "midi++/mmc.h"
#include "midi++/port.h"
#include "midi++/jack_midi_port.h"
#include "midi++/parser.h"
#include "midi++/manager.h"

using namespace std;
using namespace MIDI;
using namespace PBD;

static std::map<int,string> mmc_cmd_map;
static void build_mmc_cmd_map ()
{
	pair<int,string> newpair;

	newpair.first = 0x1;
	newpair.second = "Stop";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x2;
	newpair.second = "Play";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x3;
	newpair.second = "DeferredPlay";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x4;
	newpair.second = "FastForward";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x5;
	newpair.second = "Rewind";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x6;
	newpair.second = "RecordStrobe";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x7;
	newpair.second = "RecordExit";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x8;
	newpair.second = "RecordPause";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x9;
	newpair.second = "Pause";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0xA;
	newpair.second = "Eject";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0xB;
	newpair.second = "Chase";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0xC;
	newpair.second = "CommandErrorReset";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0xD;
	newpair.second = "MmcReset";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x20;
	newpair.second = "Illegal Mackie Jog Start";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x21;
	newpair.second = "Illegal Mackie Jog Stop";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x40;
	newpair.second = "Write";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x41;
	newpair.second = "MaskedWrite";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x42;
	newpair.second = "Read";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x43;
	newpair.second = "Update";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x44;
	newpair.second = "Locate";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x45;
	newpair.second = "VariablePlay";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x46;
	newpair.second = "Search";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x47;
	newpair.second = "Shuttle";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x48;
	newpair.second = "Step";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x49;
	newpair.second = "AssignSystemMaster";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x4A;
	newpair.second = "GeneratorCommand";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x4B;
	newpair.second = "MtcCommand";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x4C;
	newpair.second = "Move";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x4D;
	newpair.second = "Add";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x4E;
	newpair.second = "Subtract";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x4F;
	newpair.second = "DropFrameAdjust";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x50;
	newpair.second = "Procedure";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x51;
	newpair.second = "Event";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x52;
	newpair.second = "Group";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x53;
	newpair.second = "CommandSegment";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x54;
	newpair.second = "DeferredVariablePlay";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x55;
	newpair.second = "RecordStrobeVariable";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x7C;
	newpair.second = "Wait";
	mmc_cmd_map.insert (newpair);

	newpair.first = 0x7F;
	newpair.second = "Resume";
	mmc_cmd_map.insert (newpair);
}


MachineControl::MachineControl (Manager* m, ARDOUR::PortEngine& pengine)
{
	build_mmc_cmd_map ();

	_receive_device_id = 0x7f;
	_send_device_id = 0x7f;

	_input_port = m->add_port (new JackMIDIPort ("MMC in", Port::IsInput, pengine));
	_output_port = m->add_port (new JackMIDIPort ("MMC out", Port::IsOutput, pengine));

	_input_port->parser()->mmc.connect_same_thread (port_connections, boost::bind (&MachineControl::process_mmc_message, this, _1, _2, _3));
	_input_port->parser()->start.connect_same_thread (port_connections, boost::bind (&MachineControl::spp_start, this));
	_input_port->parser()->contineu.connect_same_thread (port_connections, boost::bind (&MachineControl::spp_continue, this));
	_input_port->parser()->stop.connect_same_thread (port_connections, boost::bind (&MachineControl::spp_stop, this));
}

void
MachineControl::set_receive_device_id (byte id)
{
	_receive_device_id = id & 0x7f;
}

void
MachineControl::set_send_device_id (byte id)
{
	_send_device_id = id & 0x7f;
}

bool
MachineControl::is_mmc (byte *sysex_buf, size_t len)
{
	if (len < 4 || len > 48) {
		return false;
	}

	if (sysex_buf[1] != 0x7f) {
		return false;
	}

	if (sysex_buf[3] != 0x6 && /* MMC Command */
	    sysex_buf[3] != 0x7) { /* MMC Response */
		return false;
	}
	
	return true;
}

void
MachineControl::process_mmc_message (Parser &, byte *msg, size_t len)
{
	size_t skiplen;
	byte *mmc_msg;
	bool single_byte;

	/* Reject if its not for us. 0x7f is the "all-call" device ID */

	/* msg[0] = 0x7f (MMC sysex ID(
	   msg[1] = device ID
	   msg[2] = 0x6 (MMC command) or 0x7 (MMC response)
	   msg[3] = MMC command code
	   msg[4] = (typically) byte count for following part of command
	*/

#if 0
	cerr << "*** me = " << (int) _receive_device_id << " MMC message: len = " << len << "\n\t";
	for (size_t i = 0; i < len; i++) {
		cerr << hex << (int) msg[i] << dec << ' ';
	}
	cerr << endl;
#endif

	if (msg[1] != 0x7f && msg[1] != _receive_device_id) {
		return;
	}

	mmc_msg = &msg[3];
	len -= 3;

	do {

		single_byte = false;

		/* this works for all non-single-byte "counted"
		   commands. we set it to 1 for the exceptions.
		*/

		std::map<int,string>::iterator x = mmc_cmd_map.find ((int)mmc_msg[0]);
		string cmdname = "unknown";

		if (x != mmc_cmd_map.end()) {
			cmdname = (*x).second;
		}

#if 0
		cerr << "+++ MMC type " 
		     << hex
		     << ((int) *mmc_msg)
		     << dec
		     << " \"" << cmdname << "\" "
		     << " len = " << len
		     << endl;
#endif

		switch (*mmc_msg) {

		/* SINGLE-BYTE, UNCOUNTED COMMANDS */

		case cmdStop:
			Stop (*this);
			single_byte = true;
			break;

		case cmdPlay:
			Play (*this);
			single_byte = true;
			break;

		case cmdDeferredPlay:
			DeferredPlay (*this);
			single_byte = true;
			break;

		case cmdFastForward:
			FastForward (*this);
			single_byte = true;
			break;

		case cmdRewind:
			Rewind (*this);
			single_byte = true;
			break;

		case cmdRecordStrobe:
			RecordStrobe (*this);
			single_byte = true;
			break;

		case cmdRecordExit:
			RecordExit (*this);
			single_byte = true;
			break;

		case cmdRecordPause:
			RecordPause (*this);
			single_byte = true;
			break;

		case cmdPause:
			Pause (*this);
			single_byte = true;
			break;

		case cmdEject:
			Eject (*this);
			single_byte = true;
			break;

		case cmdChase:
			Chase (*this);
			single_byte = true;
			break;

		case cmdCommandErrorReset:
			CommandErrorReset (*this);
			single_byte = true;
			break;

		case cmdMmcReset:
			MmcReset (*this);
			single_byte = true;
			break;

		case cmdIllegalMackieJogStart:
			JogStart (*this);
			single_byte = true;
			break;

		case cmdIllegalMackieJogStop:
			JogStop (*this);
			single_byte = true;
			break;

		/* END OF SINGLE-BYTE, UNCOUNTED COMMANDS */

		case cmdMaskedWrite:
			do_masked_write (mmc_msg, len);
			break;

		case cmdLocate:
			do_locate (mmc_msg, len);
			break;

		case cmdShuttle:
			do_shuttle (mmc_msg, len);
			break;

		case cmdStep:
			do_step (mmc_msg, len);
			break;

		case cmdWrite:
		case cmdRead:
		case cmdUpdate:
		case cmdVariablePlay:
		case cmdSearch:
		case cmdAssignSystemMaster:
		case cmdGeneratorCommand:
		case cmdMtcCommand:
		case cmdMove:
		case cmdAdd:
		case cmdSubtract:
		case cmdDropFrameAdjust:
		case cmdProcedure:
		case cmdEvent:
		case cmdGroup:
		case cmdCommandSegment:
		case cmdDeferredVariablePlay:
		case cmdRecordStrobeVariable:
		case cmdWait:
		case cmdResume:
			error << "MIDI::MachineControl: unimplemented MMC command "
			      << hex << (int) *mmc_msg << dec
			      << endmsg;

			break;

		default:
			error << "MIDI::MachineControl: unknown MMC command "
			      << hex << (int) *mmc_msg << dec
			      << endmsg;

			break;
		}

		/* increase skiplen to cover the command byte and 
		   count byte (if it existed).
		*/

		if (!single_byte) {
			skiplen = mmc_msg[1] + 2;
		} else {
			skiplen = 1;
		}

		if (len <= skiplen) {
			break;
		}

		mmc_msg += skiplen;
		len -= skiplen;

	} while (len > 1); /* skip terminating EOX byte */
}		

int
MachineControl::do_masked_write (byte *msg, size_t len)
{
	/* return the number of bytes "consumed" */

	int retval = msg[1] + 2; /* bytes following + 2 */
	
	switch (msg[2]) {
	case 0x4f:  /* Track Record Ready Status */
		write_track_status (&msg[3], len - 3, msg[2]);
		break;

	case 0x62: /* track mute */
		write_track_status (&msg[3], len - 3, msg[2]);
		break;

	default:
		warning << "MIDI::MachineControl: masked write to "
			<< hex << (int) msg[2] << dec
			<< " not implemented"
			<< endmsg;
	}

	return retval;
}

void
MachineControl::write_track_status (byte *msg, size_t /*len*/, byte reg)
{
	size_t n;
	ssize_t base_track;

	/* Bits 0-4 of the first byte are for special tracks:

	   bit 0: video
	   bit 1: reserved
	   bit 2: time code
	   bit 3: aux track a
	   bit 4: aux track b

	   the format of the message (its an MMC Masked Write) is:
	   
	   0x41      Command Code
	   <count>   byte count of following data
	   <name>    byte value of the field being written
	   <byte #>  byte number of target byte in the 
	   bitmap being written to
	   <mask>    ones in the mask indicate which bits will be changed
	   <data>    new data for the byte being written
	   
	   by the time this code is executing, msg[0] is the
	   byte number of the target byte. if its zero, we
	   are writing to a special byte in the standard
	   track bitmap, in which the first 5 bits are
	   special. hence the bits for tracks 1 + 2 are bits
	   5 and 6 of the first byte of the track
	   bitmap. so:
	   
	   change track 1:  msg[0] = 0;       << first byte of track bitmap 
	                    msg[1] = 0100000; << binary: bit 5 set
	
	   change track 2:  msg[0] = 0;       << first byte of track bitmap
	                    msg[1] = 1000000; << binary: bit 6 set
	
	   change track 3:  msg[0] = 1;       << second byte of track bitmap
	                    msg[1] = 0000001; << binary: bit 0 set
	
	   the (msg[0] * 8) - 6 computation is an attempt to
	   extract the value of the first track: ie. the one
	   that would be indicated by bit 0 being set.
		
           so, if msg[0] = 0, msg[1] = 0100000 (binary),
	   what happens is that base_track = -5, but by the
	   time we check the correct bit, n = 5, and so the
	   computed track for the status change is 0 (first
	   track).

           if msg[0] = 1, then the base track for any change is 2 (the third track), and so on.
	*/

	/* XXX check needed to make sure we don't go outside the
	   supported number of tracks.
	*/

	if (msg[0] == 0) {
		base_track = -5;
	} else {
		base_track = (msg[0] * 8) - 6;
	}

	for (n = 0; n < 7; n++) {
		if (msg[1] & (1<<n)) {

			/* Only touch tracks that have the "mask"
			   bit set.
			*/

			bool val = (msg[2] & (1<<n));
			
			switch (reg) {
			case 0x4f:
				trackRecordStatus[base_track+n] = val;
				TrackRecordStatusChange (*this, base_track+n, val);
				break;
				
			case 0x62:
				trackMute[base_track+n] = val;
				TrackMuteChange (*this, base_track+n, val);
				break;
			}
		} 

	}
}

int
MachineControl::do_locate (byte *msg, size_t /*msglen*/)
{
	if (msg[2] == 0) {
		warning << "MIDI::MMC: locate [I/F] command not supported"
			<< endmsg;
		return 0;
	}

	/* regular "target" locate command */

	Locate (*this, &msg[3]);
	return 0;
}

int
MachineControl::do_step (byte *msg, size_t /*msglen*/)
{
	int steps = msg[2] & 0x3f;

	if (msg[2] & 0x40) {
		steps = -steps;
	}

	Step (*this, steps);
	return 0;
}

int
MachineControl::do_shuttle (byte *msg, size_t /*msglen*/)
{
	size_t forward;
	byte sh = msg[2];
	byte sm = msg[3];
	byte sl = msg[4];
	size_t left_shift;
	size_t integral;
	size_t fractional;
	float shuttle_speed;

	if (sh & (1<<6)) {
		forward = false;
	} else {
		forward = true;
	}
	
	left_shift = (sh & 0x38);

	integral = ((sh & 0x7) << left_shift) | (sm >> (7 - left_shift));
	fractional = ((sm << left_shift) << 7) | sl;

	shuttle_speed = integral + 
		((float)fractional / (1 << (14 - left_shift)));

	Shuttle (*this, shuttle_speed, forward);

	return 0;
}

void
MachineControl::enable_send (bool yn)
{
	_enable_send = yn;
}

/** Send a MMC command to a the MMC port.
 *  @param c command.
 */
void
MachineControl::send (MachineControlCommand const & c)
{
	if (_output_port == 0 || !_enable_send) {
		// cerr << "Not delivering MMC " << _mmc->port() << " - " << session_send_mmc << endl;
		return;
	}

	MIDI::byte buffer[32];
	MIDI::byte* b = c.fill_buffer (this, buffer);

	if (_output_port->midimsg (buffer, b - buffer, 0)) {
		error << "MMC: cannot send command" << endmsg;
	}
}

void
MachineControl::spp_start ()
{
	SPPStart (); /* EMIT SIGNAL */
}

void
MachineControl::spp_continue ()
{
	SPPContinue (); /* EMIT SIGNAL */
}

void
MachineControl::spp_stop ()
{
	SPPStop (); /* EMIT SIGNAL */
}

MachineControlCommand::MachineControlCommand (MachineControl::Command c)
	: _command (c)
{

}

MachineControlCommand::MachineControlCommand (Timecode::Time t)
	: _command (MachineControl::cmdLocate)
	, _time (t)
{

}

MIDI::byte * 
MachineControlCommand::fill_buffer (MachineControl* mmc, MIDI::byte* b) const
{
	*b++ = 0xf0; // SysEx
	*b++ = 0x7f; // Real-time SysEx ID for MMC
	*b++ = mmc->send_device_id();
	*b++ = 0x6; // MMC command

	*b++ = _command;

	if (_command == MachineControl::cmdLocate) {
		*b++ = 0x6; // byte count
		*b++ = 0x1; // "TARGET" subcommand
		*b++ = _time.hours;
		*b++ = _time.minutes;
		*b++ = _time.seconds;
		*b++ = _time.frames;
		*b++ = _time.subframes;
	}

	*b++ = 0xf7;

	return b;
}

