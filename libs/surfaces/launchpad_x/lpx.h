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

#ifndef __ardour_lpx_h__
#define __ardour_lpx_h__

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
	class AutomationControl;
	class Port;
	class MidiBuffer;
	class MidiTrack;
	class Trigger;
}

namespace ArdourSurface {

class LPX_GUI;

class LaunchPadX : public MIDISurface
{
  public:
	/* use hex for these constants, because we'll see them (as note numbers
	   and CC numbers) in hex within MIDI messages when debugging.
	*/
	enum PadID {
		/* top */
		Up = 0x5b,
		Down = 0x5c,
		Left = 0x5d,
		Right = 0x5e,
		Session = 0x5f,
		Note = 0x60,
		Custom = 0x61,
		CaptureMIDI = 0x62,
		/* right side */
		Volume = 0x59,
		Pan = 0x4f,
		SendA = 0x45,
		SendB = 0x3b,
		StopClip = 0x31,
		Mute = 0x27,
		Solo = 0x1d,
		RecordArm = 0x13,
		Logo = 0x63
	};

	bool light_logo();
	void all_pads_out ();

	LaunchPadX (ARDOUR::Session&);
	~LaunchPadX ();

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
		Programmer
	};

	enum Layout {
		SessionLayout,
		Fader,
		ChordLayout,
		CustomLayout,
		NoteLayout,
		Scale,
		SequencerSettings,
		SequencerSteps,
		SequencerVelocity,
		SequencerPatternSettings,
		SequencerProbability,
		SequencerMutation,
		SequencerMicroStep,
		SequencerProjects,
		SequencerPatterns,
		SequencerTempo,
		SequencerSwing,
		ProgrammerLayout,
		Settings,
		CustomSettings
	};

	enum FaderBank {
		VolumeFaders,
		PanFaders,
		SendFaders,
		DeviceFaders
	};

	static const Layout AllLayouts[];

	struct Pad  {

		enum ColorMode {
			Static = 0x0,
			Flashing = 0x1,
			Pulsing = 0x2
		};

		typedef void (LaunchPadX::*ButtonMethod)(Pad&);
		typedef void (LaunchPadX::*PadMethod)(Pad&, int velocity);

		Pad (PadID pid, ButtonMethod press_method, ButtonMethod long_press_method = &LaunchPadX::relax, ButtonMethod release_method = &LaunchPadX::relax)
			: id (pid)
			, x (-1)
			, y (-1)
		{
			on_press = press_method;
			on_release = release_method;
			on_long_press = long_press_method;
		}

		Pad (int pid, int xx, int yy, PadMethod press_method, ButtonMethod long_press_method = &LaunchPadX::relax, ButtonMethod release_method = &LaunchPadX::relax)
			: id (pid)
			, x (xx)
			, y (yy)
		{
			on_pad_press = press_method;
			on_release = release_method;
			on_long_press = long_press_method;
		}

		MIDI::byte status_byte() const { if (x < 0) return 0xb0; return 0x90; }
		bool is_pad () const { return x >= 0; }
		bool is_button () const { return x < 0; }

		int id;
		int x;
		int y;

		/* It's either a button (CC number) or a pad (note number
		 * w/velocity info), never both.
		 */
		union {
			ButtonMethod on_press;
			PadMethod on_pad_press;
		};

		ButtonMethod on_release;
		ButtonMethod on_long_press;

		sigc::connection timeout_connection;
	};

	void relax (Pad& p);

	std::set<int> consumed;

	MIDI::byte logo_color;

	int scroll_x_offset;
	int scroll_y_offset;
	typedef std::pair<int32_t,int32_t> StripableSlot;
	typedef std::vector<StripableSlot> StripableSlotRow;
	typedef std::vector<StripableSlotRow> StripableSlotColumn;
	StripableSlotColumn stripable_slots;

	StripableSlot get_stripable_slot (int x, int y) const;

	typedef std::map<int,Pad> PadMap;
	PadMap pad_map;
	void build_pad_map();
	Pad* pad_by_id (int pid);

	typedef std::map<int,uint32_t> ColorMap;
	ColorMap color_map;
	void build_color_map ();
	int find_closest_palette_color (uint32_t);

	typedef std::map<uint32_t,int> NearestMap;
	NearestMap nearest_map;

	int begin_using_device ();
	int stop_using_device ();
	int device_acquire () { return 0; }
	void device_release () { }
	void run_event_loop ();
	void stop_event_loop ();

	void stripable_selection_changed ();
	void select_stripable (int col);
	std::weak_ptr<ARDOUR::MidiTrack> _current_pad_target;

	void light_pad (int pad_id, int color, int mode = 0);
	void pad_off (int pad_id);
	void all_pads_off ();
	void all_pads_on (int color);

	void set_device_mode (DeviceMode);
	void set_layout (Layout, int page = 0);

	void handle_midi_controller_message (MIDI::Parser&, MIDI::EventTwoBytes*);
	void handle_midi_note_on_message (MIDI::Parser&, MIDI::EventTwoBytes*);
	void handle_midi_note_off_message (MIDI::Parser&, MIDI::EventTwoBytes*);
	void handle_midi_sysex (MIDI::Parser&, MIDI::byte *, size_t count);

	MIDI::Port* _daw_in_port;
	MIDI::Port* _daw_out_port;
	std::shared_ptr<ARDOUR::Port> _daw_in;
	std::shared_ptr<ARDOUR::Port> _daw_out;

	void port_registration_handler ();
	int ports_acquire ();
	void ports_release ();
	void connect_daw_ports ();

	void daw_write (const MidiByteArray&);
	void daw_write (MIDI::byte const *, size_t);

	void reconnect_for_programmer ();
	void reconnect_for_session ();

	void scroll_text (std::string const &, int color, bool loop, float speed = 0);

	mutable LPX_GUI* _gui;
	void build_gui ();

	Layout _current_layout;

	void maybe_start_press_timeout (Pad& pad);
	void start_press_timeout (Pad& pad);
	bool long_press_timeout (int pad_id);

	enum SessionState {
		SessionMode,
		MixerMode
	};

	bool _session_pressed;
	SessionState _session_mode;

	void cue_press (Pad&, int row);

	void rh0_press (Pad&);
	void rh1_press (Pad&);
	void rh2_press (Pad&);
	void rh3_press (Pad&);
	void rh4_press (Pad&);
	void rh5_press (Pad&);
	void rh6_press (Pad&);
	void rh7_press (Pad&);

	/* named pad methods */
	void down_press (Pad&);
	void down_release (Pad&) {}
	void down_long_press (Pad&) {}
	void up_press (Pad&);
	void up_release (Pad&) {}
	void up_long_press (Pad&) {}
	void left_press (Pad&);
	void left_release (Pad&) {}
	void left_long_press (Pad&) {}
	void right_press (Pad&);
	void right_release (Pad&) {}
	void right_long_press (Pad&) {}
	void session_press (Pad&);
	void session_release (Pad&);
	void session_long_press (Pad&) {}
	void note_press (Pad&);
	void note_release (Pad&) {}
	void note_long_press (Pad&) {}
	void custom_press (Pad&);
	void custom_release (Pad&) {}
	void custom_long_press (Pad&) {}

	void send_a_press (Pad&);
	void send_a_release (Pad&) {}
	void send_b_press (Pad&);
	void send_b_release (Pad&) {}
	void pan_press (Pad&);
	void pan_release (Pad&) {}
	void pan_long_press (Pad&) {}
	void volume_press (Pad&);
	void volume_release (Pad&) {}
	void volume_long_press (Pad&) {}
	void solo_press (Pad&);
	void solo_release (Pad&) {}
	void solo_long_press (Pad&);
	void stop_clip_press (Pad&);
	void stop_clip_release (Pad&) {}
	void mute_press (Pad&);
	void mute_release (Pad&) {}
	void mute_long_press (Pad&) {}
	void record_arm_press (Pad&);
	void record_arm_release (Pad&) {}
	void record_arm_long_press (Pad&) {}
	void capture_midi_press (Pad&);
	void capture_midi_release (Pad&) {}
	void capture_midi_long_press (Pad&) {}

	void fader_long_press (Pad&);
	void fader_release (Pad&);

	void pad_press (Pad&, int velocity);
	void pad_long_press (Pad&);

	void trigger_property_change (PBD::PropertyChange, ARDOUR::Trigger*);
	PBD::ScopedConnectionList trigger_connections;

	void display_session_layout ();
	void transport_state_changed ();
	void record_state_changed ();

	void map_triggers ();
	void map_triggerbox (int col);

	void viewport_changed ();
	void route_property_change (PBD::PropertyChange const &, int x);
	PBD::ScopedConnectionList route_connections;

	void setup_faders (FaderBank);
	void map_faders ();
	void fader_move (int cc, int val);
	void automation_control_change (int n, std::weak_ptr<ARDOUR::AutomationControl>);
	PBD::ScopedConnectionList control_connections;
	FaderBank current_fader_bank;
	bool revert_layout_on_fader_release;
	Layout pre_fader_layout;
};


} /* namespace */

#endif /* __ardour_lpx_h__ */
