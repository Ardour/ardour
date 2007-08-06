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

#include <sigc++/sigc++.h>
#include <midi++/types.h>

namespace MIDI {

class Port;
class Parser;

class MachineControl : public sigc::trackable

{
  public:
	typedef byte CommandSignature[60];
	typedef byte ResponseSignature[60];

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
	
	MachineControl (Port &port,
			float MMCVersion, 
			CommandSignature &cs,
			ResponseSignature &rs);

	Port &port() { return _port; }
	
	void set_receive_device_id (byte id);
	void set_send_device_id (byte id);
	byte receive_device_id () const { return _receive_device_id; }
	byte send_device_id () const { return _send_device_id; }

	static bool is_mmc (byte *sysex_buf, size_t len);

	/* Signals to connect to if you want to run "callbacks"
	   when certain MMC commands are received.
	*/
			
	sigc::signal<void,MachineControl &> Stop;
	sigc::signal<void,MachineControl &> Play;
	sigc::signal<void,MachineControl &> DeferredPlay;
	sigc::signal<void,MachineControl &> FastForward;
	sigc::signal<void,MachineControl &> Rewind;
	sigc::signal<void,MachineControl &> RecordStrobe;
	sigc::signal<void,MachineControl &> RecordExit;
	sigc::signal<void,MachineControl &> RecordPause;
	sigc::signal<void,MachineControl &> Pause;
	sigc::signal<void,MachineControl &> Eject;
	sigc::signal<void,MachineControl &> Chase;
	sigc::signal<void,MachineControl &> CommandErrorReset;
	sigc::signal<void,MachineControl &> MmcReset;

	sigc::signal<void,MachineControl &> JogStart;
	sigc::signal<void,MachineControl &> JogStop;

	sigc::signal<void,MachineControl &> Write;
	sigc::signal<void,MachineControl &> MaskedWrite;
	sigc::signal<void,MachineControl &> Read;
	sigc::signal<void,MachineControl &> Update;
	sigc::signal<void,MachineControl &> VariablePlay;
	sigc::signal<void,MachineControl &> Search;
	sigc::signal<void,MachineControl &> AssignSystemMaster;
	sigc::signal<void,MachineControl &> GeneratorCommand;
	sigc::signal<void,MachineControl &> MidiTimeCodeCommand;
	sigc::signal<void,MachineControl &> Move;
	sigc::signal<void,MachineControl &> Add;
	sigc::signal<void,MachineControl &> Subtract;
	sigc::signal<void,MachineControl &> DropFrameAdjust;
	sigc::signal<void,MachineControl &> Procedure;
	sigc::signal<void,MachineControl &> Event;
	sigc::signal<void,MachineControl &> Group;
	sigc::signal<void,MachineControl &> CommandSegment;
	sigc::signal<void,MachineControl &> DeferredVariablePlay;
	sigc::signal<void,MachineControl &> RecordStrobeVariable;
	sigc::signal<void,MachineControl &> Wait;
	sigc::signal<void,MachineControl &> Resume;

	/* The second argument is the shuttle speed, the third is
	   true if the direction is "forwards", false for "reverse"
	*/
	
	sigc::signal<void,MachineControl &,float,bool> Shuttle;

	/* The second argument specifies the desired track record enabled
	   status.
	*/

	sigc::signal<void,MachineControl &,size_t,bool> 
		                             TrackRecordStatusChange;
	
	/* The second argument points to a byte array containing
	   the locate target value in MMC Standard Time Code
	   format (5 bytes, roughly: hrs/mins/secs/frames/subframes)
	*/

	sigc::signal<void,MachineControl &, const byte *> Locate;

	/* The second argument is the number of steps to jump */
	
	sigc::signal<void,MachineControl &, int> Step;
	
  protected:

#define MMC_NTRACKS 48

	/* MMC Information fields (think "registers") */

	CommandSignature commandSignature;
	ResponseSignature responseSignature;

	byte updateRate;
	byte responseError;
	byte commandError;
	byte commandErrorLevel;
	
	byte motionControlTally;
	byte velocityTally;
	byte stopMode;
	byte fastMode;
	byte recordMode;
	byte recordStatus;
	bool trackRecordStatus[MMC_NTRACKS];
	bool trackRecordReady[MMC_NTRACKS];
	byte globalMonitor;
	byte recordMonitor;
	byte trackSyncMonitor;
	byte trackInputMonitor;
	byte stepLength;
	byte playSpeedReference;
	byte fixedSpeed;
	byte lifterDefeat;
	byte controlDisable;
	byte trackMute;
	byte failure;
	byte selectedTimeCode;
	byte shortSelectedTimeCode;
	byte timeStandard;
	byte selectedTimeCodeSource;
	byte selectedTimeCodeUserbits;
	byte selectedMasterCode;
	byte requestedOffset;
	byte actualOffset;
	byte lockDeviation;
	byte shortSelectedMasterCode;
	byte shortRequestedOffset;
	byte shortActualOffset;
	byte shortLockDeviation;
	byte resolvedPlayMode;
	byte chaseMode;
	byte generatorTimeCode;
	byte shortGeneratorTimeCode;
	byte generatorCommandTally;
	byte generatorSetUp;
	byte generatorUserbits;
	byte vitcInsertEnable;
	byte midiTimeCodeInput;
	byte shortMidiTimeCodeInput;
	byte midiTimeCodeCommandTally;
	byte midiTimeCodeSetUp;
	byte gp0;
	byte gp1;
	byte gp2;
	byte gp3;
	byte gp4;
	byte gp5;
	byte gp6;
	byte gp7;
	byte shortGp0;
	byte shortGp1;
	byte shortGp2;
	byte shortGp3;
	byte shortGp4;
	byte shortGp5;
	byte shortGp6;
	byte shortGp7;
	byte procedureResponse;
	byte eventResponse;
	byte responseSegment;
	byte wait;
	byte resume;

  private:
	byte _receive_device_id;
	byte _send_device_id;
	MIDI::Port &_port;

	void process_mmc_message (Parser &p, byte *, size_t len);
	
	int  do_masked_write (byte *, size_t len);
	int  do_locate (byte *, size_t len);
	int  do_step (byte *, size_t len);
	int  do_shuttle (byte *, size_t len);

	void write_track_record_ready (byte *, size_t len);
};

} // namespace MIDI

#endif /* __midipp_mmc_h_h__ */
