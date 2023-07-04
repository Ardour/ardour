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
	/* use hex for these constants, because we'll see them (as note numbers
	   and CC numbers) in hex within MIDI messages when debugging.
	*/
	enum PadID {
		/* top */
		Shift = 0x5a,
		Left = 0x5b,
		Right = 0x5c,
		Session = 0x5d,
		Note = 0x5e,
		Chord = 0x5f,
		Custom = 0x60,
		Sequencer = 0x61,
		Projects = 0x62,
		/* right side */
		Patterns = 0x59,
		Steps = 0x4f,
		PatternSettings = 0x45,
		Velocity = 0x3b,
		Probability = 0x31,
		Mutation = 0x27,
		MicroStep = 0x1d,
		PrintToClip = 0x13,
		/* lower bottom */
		StopClip = 0x8,
		Device = 0x7,
		Sends = 0x6,
		Pan = 0x5,
		Volume = 0x4,
		Solo = 0x3,
		Mute = 0x2,
		RecordArm = 0x1,
		/* left side */
		CaptureMIDI = 0xa,
		Play = 0x14,
		FixedLength = 0x1e,
		Quantize = 0x28,
		Duplicate = 0x32,
		Clear = 0x3c,
		Down = 0x46,
		Up = 0x50,
		/* upper bottom */
		Lower1 = 0x65,
		Lower2 = 0x66,
		Lower3 = 0x67,
		Lower4 = 0x68,
		Lower5 = 0x69,
		Lower6 = 0x6a,
		Lower7 = 0x6b,
		Lower8 = 0x6c
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
	enum DeviceMode {
		Standalone,
		DAW,
		Live,
		Programmer
	};

	typedef std::map<int,int> ColorMap;
	ColorMap color_map;
	void build_color_map ();

	struct Pad  {

		enum WhenPressed {
			Nothing,
			FlashOn,
			FlashOff,
		};

		enum ColorMode {
			Static = 0x0,
			Flashing = 0x1,
			Pulsing = 0x2
		};

		Pad (PadID pid)
			: id (pid)
			, x (-1)
			, y (-1)
			, do_when_pressed (FlashOn)
			, filtered (false)
			, perma_color (0)
			, color (0)
			, mode (Static)
		{}

		Pad (int pid, int xx, int yy)
			: id (pid)
			, x (xx)
			, y (yy)
			, do_when_pressed (FlashOn)
			, filtered (true)
			, perma_color (0)
			, color (0)
			, mode (Static)
		{}

		void set (int c, ColorMode m) {
			color = c;
			mode = m;
		}
		void off() { set (0, Static); }

		MidiByteArray state_msg () const { return MidiByteArray (3, 0x90|mode, id, color); }

		/* This returns a negative value for edge pads */
		int coord () const { return (y * 8) + x; } 
		/* Just an alias, really. */
		int note_number() const { return id; }

		int id;
		int x;
		int y;
		int do_when_pressed;
		int filtered;
		int perma_color;
		int color;
		ColorMode mode;
	};

	typedef std::map<int,Pad> PadMap;
	PadMap pad_map;
	void build_pad_map();
	Pad* pad_by_id (int pid);

	int begin_using_device ();
	int stop_using_device ();
	int device_acquire () { return 0; }
	void device_release () { }
	void run_event_loop ();
	void stop_event_loop ();

	void stripable_selection_changed ();

	void light_pad (int pad_id, int color, Pad::ColorMode);
	void pad_off (int pad_id);
	void all_pads_off ();
	void all_pads_on ();

	void set_device_mode (DeviceMode);

	void handle_midi_controller_message (MIDI::Parser&, MIDI::EventTwoBytes*);
	void handle_midi_note_on_message (MIDI::Parser&, MIDI::EventTwoBytes*);
	void handle_midi_note_off_message (MIDI::Parser&, MIDI::EventTwoBytes*);
	void handle_midi_sysex (MIDI::Parser&, MIDI::byte *, size_t count);

	mutable LPPRO_GUI* _gui;
	void build_gui ();
};

} /* namespace */

#endif /* __ardour_lppro_h__ */
