/*
 * Copyright (C) 2016-2018 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_lppro_h__
#define __ardour_lppro_h__

#include <vector>
#include <map>
#include <stack>
#include <list>
#include <set>

#include <libusb.h>

#define ABSTRACT_UI_EXPORTS
#include "pbd/abstract_ui.h"

#include "midi++/types.h"

#include "ardour/mode.h"
#include "ardour/types.h"

#include "control_protocol/control_protocol.h"
#include "control_protocol/types.h"

#include "gtkmm2ext/colors.h"

#include "midi_surface/midi_byte_array.h"
#include "midi_surface/midi_surface.h"

namespace MIDI {
	class Parser;
	class Port;
}

namespace ARDOUR {
	class Port;
	class MidiBuffer;
	class MidiTrack;
}

namespace ArdourSurface {

class LPPRO_GUI;

class LaunchPadPro : public MIDISurface
{
  public:
	enum ButtonID {
		Left,
		Right,
		Session,
		Note,
		Chord,
		Custom,
		Sequencer,
		Projects,
		Patterns,
		Steps,
		PatternSettings,
		Velocity,
		Probability,
		Mutation,
		MicroStep,
		PrintToClip,
		StopClip,
		Device,
		Sends,
		Pan,
		Volume,
		Solo,
		Mute,
		RecordArm,
		CaptureMIDI,
		Play,
		FixedLength,
		Quantize,
		Duplicate,
		Clear,
		Down,
		Up
	};

	LaunchPadPro (ARDOUR::Session&);
	~LaunchPadPro ();

	static bool available ();
	static bool match_usb (uint16_t, uint16_t);
	static bool probe (std::string&, std::string&);

	std::string input_port_name () const;
	std::string output_port_name () const;

	bool has_editor () const { return true; }
	void* get_gui () const;
	void  tear_down_gui ();

	int set_active (bool yn);
	XMLNode& get_state() const;
	int set_state (const XMLNode & node, int version);

  private:
	int begin_using_device ();
	int stop_using_device ();
	int device_acquire () { return 0; }
	void device_release () { }
	void run_event_loop ();
	void stop_event_loop ();

	void stripable_selection_changed ();

	mutable LPPRO_GUI* _gui;
	void build_gui ();
};

} /* namespace */

#endif /* __ardour_lppro_h__ */
