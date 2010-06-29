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

*/

#ifndef __midipp_mmc_h_h__
#define __midipp_mmc_h_h__

#include "control_protocol/timecode.h"
#include "pbd/signals.h"
#include "pbd/ringbuffer.h"
#include "midi++/types.h"

namespace MIDI {

class Port;
class Parser;
class MachineControlCommand;	

/** Class to handle incoming and outgoing MIDI machine control messages */
class MachineControl 
{
  public:
	typedef PBD::Signal1<void,MachineControl&> MMCSignal;

	enum Command {
		cmdStop = 0x1,
		cmdPlay = 0x2,
		cmdDeferredPlay = 0x3,
		cmdFastForward = 0x4,
		cmdRewind = 0x5,
		cmdRecordStrobe = 0x6,

		cmdRecordExit = 0x7,
		cmdRecordPause = 0x8,
		cmdPause = 0x9,
		cmdEject = 0xA,
		cmdChase = 0xB,
		cmdCommandErrorReset = 0xC,
		cmdMmcReset = 0xD,
		
		cmdIllegalMackieJogStart = 0x20,
		cmdIllegalMackieJogStop = 0x21,
		
		cmdWrite = 0x40,
		cmdMaskedWrite = 0x41,
		cmdRead = 0x42,
		cmdUpdate = 0x43,
		cmdLocate = 0x44,
		cmdVariablePlay = 0x45,
		cmdSearch = 0x46,

		cmdShuttle = 0x47,
		cmdStep = 0x48,
		cmdAssignSystemMaster = 0x49,
		cmdGeneratorCommand = 0x4A,
		cmdMtcCommand = 0x4B,
		cmdMove = 0x4C,
		cmdAdd = 0x4D,

		cmdSubtract = 0x4E,
		cmdDropFrameAdjust = 0x4F,
		cmdProcedure = 0x50,
		cmdEvent = 0x51,
		cmdGroup = 0x52,
		cmdCommandSegment = 0x53,
		cmdDeferredVariablePlay = 0x54,

		cmdRecordStrobeVariable = 0x55,

		cmdWait = 0x7C,
		cmdResume = 0x7F
	};
	
	MachineControl ();
	void set_port (Port* p);

	Port* port() { return _port; }
	
	void set_receive_device_id (byte id);
	void set_send_device_id (byte id);
	byte receive_device_id () const { return _receive_device_id; }
	byte send_device_id () const { return _send_device_id; }
	void enable_send (bool);
	void send (MachineControlCommand const &);
	void flush_pending ();

	static bool is_mmc (byte *sysex_buf, size_t len);
	static void set_sending_thread (pthread_t);

	/* Signals to connect to if you want to run "callbacks"
	   when certain MMC commands are received.
	*/
			
	MMCSignal Stop;
	MMCSignal Play;
	MMCSignal DeferredPlay;
	MMCSignal FastForward;
	MMCSignal Rewind;
	MMCSignal RecordStrobe;
	MMCSignal RecordExit;
	MMCSignal RecordPause;
	MMCSignal Pause;
	MMCSignal Eject;
	MMCSignal Chase;
	MMCSignal CommandErrorReset;
	MMCSignal MmcReset;
	MMCSignal JogStart;
	MMCSignal JogStop;
	MMCSignal Write;
	MMCSignal MaskedWrite;
	MMCSignal Read;
	MMCSignal Update;
	MMCSignal VariablePlay;
	MMCSignal Search;
	MMCSignal AssignSystemMaster;
	MMCSignal GeneratorCommand;
	MMCSignal MidiTimeCodeCommand;
	MMCSignal Move;
	MMCSignal Add;
	MMCSignal Subtract;
	MMCSignal DropFrameAdjust;
	MMCSignal Procedure;
	MMCSignal Event;
	MMCSignal Group;
	MMCSignal CommandSegment;
	MMCSignal DeferredVariablePlay;
	MMCSignal RecordStrobeVariable;
	MMCSignal Wait;
	MMCSignal Resume;

	/* The second argument is the shuttle speed, the third is
	   true if the direction is "forwards", false for "reverse"
	*/
	
	PBD::Signal3<void,MachineControl&,float,bool> Shuttle;

	/* The second argument specifies the desired track record enabled
	   status.
	*/

	PBD::Signal3<void,MachineControl &,size_t,bool> 
		                             TrackRecordStatusChange;
	
	/* The second argument specifies the desired track record enabled
	   status.
	*/

	PBD::Signal3<void,MachineControl &,size_t,bool> 
		                             TrackMuteChange;
	
	/* The second argument points to a byte array containing
	   the locate target value in MMC Standard Time Code
	   format (5 bytes, roughly: hrs/mins/secs/frames/subframes)
	*/

	PBD::Signal2<void,MachineControl &, const byte *> Locate;

	/* The second argument is the number of steps to jump */
	
	PBD::Signal2<void,MachineControl &, int> Step;
	
  private:
	byte _receive_device_id;
	byte _send_device_id;
	Port* _port;
	bool _enable_send; ///< true if MMC sending is enabled

	/** A ringbuffer of MMC commands that were `sent' from the wrong thread, which
	    are queued up and sent when flush_pending() is called.
	*/
	RingBuffer<MachineControlCommand> _pending;

	/** The thread to use for sending MMC commands */
	static pthread_t _sending_thread;

	void process_mmc_message (Parser &p, byte *, size_t len);
	PBD::ScopedConnection mmc_connection; ///< connection to our parser for incoming data

	int  do_masked_write (byte *, size_t len);
	int  do_locate (byte *, size_t len);
	int  do_step (byte *, size_t len);
	int  do_shuttle (byte *, size_t len);
	void send_immediately (MachineControlCommand const &);
	
	void write_track_status (byte *, size_t len, byte reg);
};

/** Class to describe a MIDI machine control command to be sent.
 *  In an ideal world we might use a class hierarchy for this, but objects of this type
 *  have to be allocated off the stack for thread safety.
 */
class MachineControlCommand
{
public:
	MachineControlCommand () : _command (MachineControl::Command (0)) {}
	MachineControlCommand (MachineControl::Command);
	MachineControlCommand (Timecode::Time);
	
	MIDI::byte* fill_buffer (MachineControl *mmc, MIDI::byte *) const;

private:
	MachineControl::Command _command;
	Timecode::Time _time;
};

} // namespace MIDI

#endif /* __midipp_mmc_h_h__ */
