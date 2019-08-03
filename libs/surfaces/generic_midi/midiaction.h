/*
 * Copyright (C) 2010-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __gm_midiaction_h__
#define __gm_midiaction_h__

#include <string>

#include "midi++/types.h"

#include "pbd/signals.h"
#include "pbd/stateful.h"

#include "ardour/types.h"

#include "midiinvokable.h"

namespace Gtk {
        class Action;
}

namespace MIDI {
	class Parser;
}

class GenericMidiControlProtocol;

class MIDIAction : public MIDIInvokable
{
  public:
	MIDIAction (MIDI::Parser&);
	virtual ~MIDIAction ();

	int init (GenericMidiControlProtocol&, const std::string& action_name, MIDI::byte* sysex = 0, size_t ssize = 0);

	const std::string& action_name() const { return _invokable_name; }

	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

  private:
	void execute ();
};

#endif // __gm_midicontrollable_h__

