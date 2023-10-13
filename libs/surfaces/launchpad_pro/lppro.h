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
	class AutomationControl;
	class Port;
	class MidiBuffer;
	class MidiTrack;
	class Trigger;
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
		Lower8 = 0x6c,
		/* Logo */
		Logo = 0x63
	};

	bool light_logo();
	void all_pads_out ();

	static const PadID all_pad_ids[];

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

		typedef void (LaunchPadPro::*ButtonMethod)(Pad&);
		typedef void (LaunchPadPro::*PadMethod)(Pad&, int velocity);

		Pad (PadID pid, ButtonMethod press_method, ButtonMethod long_press_method = &LaunchPadPro::relax, ButtonMethod release_method = &LaunchPadPro::relax)
			: id (pid)
			, x (-1)
			, y (-1)
		{
			on_press = press_method;
			on_release = release_method;
			on_long_press = long_press_method;
		}

		Pad (int pid, int xx, int yy, PadMethod press_method, ButtonMethod long_press_method = &LaunchPadPro::relax, ButtonMethod release_method = &LaunchPadPro::relax)
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
	static std::string input_port_regex ();
	static std::string output_port_regex ();
	static std::string input_daw_port_regex ();
	static std::string output_daw_port_regex ();
	void connect_daw_ports ();

	void daw_write (const MidiByteArray&);
	void daw_write (MIDI::byte const *, size_t);

	void reconnect_for_programmer ();
	void reconnect_for_session ();

	void scroll_text (std::string const &, int color, bool loop, float speed = 0);

	mutable LPPRO_GUI* _gui;
	void build_gui ();

	Layout _current_layout;

	bool pad_filter (ARDOUR::MidiBuffer& in, ARDOUR::MidiBuffer& out) const;

	void maybe_start_press_timeout (Pad& pad);
	void start_press_timeout (Pad& pad);
	bool long_press_timeout (int pad_id);

	bool _shift_pressed;
	bool _clear_pressed;
	bool _duplicate_pressed;
	bool _session_pressed;

	void cue_press (Pad&, int row);

	/* named pad methods */
	void shift_press (Pad&);
	void shift_release (Pad&);
	void shift_long_press (Pad&) {}
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
	void chord_press (Pad&);
	void chord_release (Pad&) {}
	void chord_long_press (Pad&) {}
	void custom_press (Pad&);
	void custom_release (Pad&) {}
	void custom_long_press (Pad&) {}
	void sequencer_press (Pad&);
	void sequencer_release (Pad&) {}
	void sequencer_long_press (Pad&) {}
	void projects_press (Pad&);
	void projects_release (Pad&) {}
	void projects_long_press (Pad&) {}
	void patterns_press (Pad&);
	void patterns_release (Pad&) {}
	void patterns_long_press (Pad&) {}
	void steps_press (Pad&);
	void steps_release (Pad&) {}
	void steps_long_press (Pad&) {}
	void pattern_settings_press (Pad&);
	void pattern_settings_release (Pad&) {}
	void pattern_settings_long_press (Pad&) {}
	void velocity_press (Pad&);
	void velocity_release (Pad&) {}
	void velocity_long_press (Pad&) {}
	void probability_press (Pad&);
	void probability_release (Pad&) {}
	void probability_long_press (Pad&) {}
	void mutation_press (Pad&);
	void mutation_release (Pad&) {}
	void mutation_long_press (Pad&) {}
	void microstep_press (Pad&);
	void microstep_release (Pad&) {}
	void microstep_long_press (Pad&) {}
	void print_to_clip_press (Pad&);
	void print_to_clip_release (Pad&) {}
	void print_to_clip_long_press (Pad&) {}
	void stop_clip_press (Pad&);
	void stop_clip_release (Pad&) {}
	void stop_clip_long_press (Pad&) {}
	void device_press (Pad&);
	void device_release (Pad&) {}
	void device_long_press (Pad&) {}
	void sends_press (Pad&);
	void sends_release (Pad&) {}
	void sends_long_press (Pad&) {}
	void pan_press (Pad&);
	void pan_release (Pad&) {}
	void pan_long_press (Pad&) {}
	void volume_press (Pad&);
	void volume_release (Pad&) {}
	void volume_long_press (Pad&) {}
	void solo_press (Pad&);
	void solo_release (Pad&) {}
	void solo_long_press (Pad&);
	void mute_press (Pad&);
	void mute_release (Pad&) {}
	void mute_long_press (Pad&) {}
	void record_arm_press (Pad&);
	void record_arm_release (Pad&) {}
	void record_arm_long_press (Pad&) {}
	void capture_midi_press (Pad&);
	void capture_midi_release (Pad&) {}
	void capture_midi_long_press (Pad&) {}
	void play_press (Pad&);
	void play_release (Pad&) {}
	void play_long_press (Pad&) {}
	void fixed_length_press (Pad&);
	void fixed_length_release (Pad&) {}
	void fixed_length_long_press (Pad&) {}
	void quantize_press (Pad&);
	void quantize_release (Pad&) {}
	void quantize_long_press (Pad&) {}
	void duplicate_press (Pad&);
	void duplicate_release (Pad&) {}
	void duplicate_long_press (Pad&) {}
	void clear_press (Pad&);
	void clear_release (Pad&);
	void clear_long_press (Pad&) {}
	void down_press (Pad&);
	void down_release (Pad&) {}
	void down_long_press (Pad&) {}
	void up_press (Pad&);
	void up_release (Pad&) {}
	void up_long_press (Pad&) {}
	void lower1_press (Pad&);
	void lower1_release (Pad&) {}
	void lower1_long_press (Pad&) {}
	void lower2_press (Pad&);
	void lower2_release (Pad&) {}
	void lower2_long_press (Pad&) {}
	void lower3_press (Pad&);
	void lower3_release (Pad&) {}
	void lower3_long_press (Pad&) {}
	void lower4_press (Pad&);
	void lower4_release (Pad&) {}
	void lower4_long_press (Pad&) {}
	void lower5_press (Pad&);
	void lower5_release (Pad&) {}
	void lower5_long_press (Pad&) {}
	void lower6_press (Pad&);
	void lower6_release (Pad&) {}
	void lower6_long_press (Pad&) {}
	void lower7_press (Pad&);
	void lower7_release (Pad&) {}
	void lower7_long_press (Pad&) {}
	void lower8_press (Pad&);
	void lower8_release (Pad&) {}
	void lower8_long_press (Pad&) {}

	void fader_long_press (Pad&);
	void fader_release (Pad&);

	void pad_press (Pad&, int velocity);
	void pad_long_press (Pad&);

	void trigger_property_change (PBD::PropertyChange, ARDOUR::Trigger*);
	PBD::ScopedConnectionList trigger_connections;

	void display_session_layout ();
	bool did_session_display;
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

#endif /* __ardour_lppro_h__ */
