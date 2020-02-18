/*
 * Copyright (C) 2014 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __libardour_midi_scene_change_h__
#define __libardour_midi_scene_change_h__

#include "evoral/PatchChange.h"

#include "pbd/signals.h"

#include "ardour/scene_change.h"

namespace ARDOUR
{

class MidiPort;

class MIDISceneChange : public SceneChange
{
  public:
	MIDISceneChange (int channel, int bank = -1, int program = -1);
	MIDISceneChange (const XMLNode&, int version);
	~MIDISceneChange ();

	void set_channel (int channel);
	void set_program (int program);
	void set_bank (int bank);

	int channel () const { return _channel; }
	int program () const { return _program; }
	int bank () const { return _bank; }

	size_t get_bank_msb_message (uint8_t* buf, size_t size) const;
	size_t get_bank_lsb_message (uint8_t* buf, size_t size) const;
	size_t get_program_message (uint8_t* buf, size_t size) const;

	XMLNode& get_state();
	int set_state (const XMLNode&, int version);

	bool operator==(const MIDISceneChange& other) const;

  private:
	int _bank;
	int _program;
	uint8_t _channel;
};

} /* namespace */


#endif /* __libardour_scene_change_h__ */
