/*
    Copyright (C) 2009 Paul Davis
 
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

#ifndef __gm_midifunction_h__
#define __gm_midifunction_h__

#include <string>

#include "midi++/types.h"

#include "pbd/signals.h"
#include "pbd/stateful.h"

#include "ardour/types.h"

#include "midiinvokable.h"

namespace MIDI {
	class Channel;
	class Parser;
}

class GenericMidiControlProtocol;

class MIDIFunction : public MIDIInvokable
{
  public:
	enum Function { 
		NextBank,
		PrevBank,
		TransportRoll,
		TransportStop,
		TransportZero,
		TransportStart,
		TransportEnd,
		TransportLoopToggle,
		TransportRecordEnable,
		TransportRecordDisable,
		/* 1 argument functions: RID */
		Select,
		SetBank,
		/* 2 argument functions: RID, value */
		TrackSetSolo, 
		TrackSetMute,
		TrackSetGain,
		TrackSetRecordEnable,
		TrackSetSoloIsolate,
	};

	MIDIFunction (MIDI::Parser&);
	virtual ~MIDIFunction ();

	int setup (GenericMidiControlProtocol&, const std::string& function_name, const std::string& argument, MIDI::byte* sysex = 0, size_t ssize = 0);

	const std::string& function_name() const { return _invokable_name; }

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

  private:
	Function        _function;
	std::string     _argument;
	void execute ();
};

#endif // __gm_midicontrollable_h__

