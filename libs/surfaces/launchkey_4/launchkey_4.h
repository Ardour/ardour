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

#ifndef __ardour_lk4_h__
#define __ardour_lk4_h__

#include <functional>
#include <list>
#include <map>
#include <stack>
#include <set>
#include <vector>

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
	class Plugin;
	class PluginInsert;
	class Port;
	class MidiBuffer;
	class MidiTrack;
	class Trigger;
}

#ifdef LAUNCHPAD_MINI
#define LAUNCHPAD_NAMESPACE LP_MINI
#else
#define LAUNCHPAD_NAMESPACE LP_X
#endif

namespace ArdourSurface { namespace LAUNCHPAD_NAMESPACE {

class LK4_GUI;

class LaunchKey4 : public MIDISurface
{
  public:
	/* use hex for these constants, because we'll see them (as note numbers
	   and CC numbers) in hex within MIDI messages when debugging.
	*/

	enum ButtonID {
		Button1 = 0x25,
		Button2 = 0x26,
		Button3 = 0x27,
		Button4 = 0x28,
		Button5 = 0x29,
		Button6 = 0x2a,
		Button7 = 0x2b,
		Button8 = 0x2c,
		Button9 = 0x2d,

		Volume = 0x0b,
		Custom1 = 0x0c,
		Custom2 = 0x0d,
		Custom3 = 0x0e,
		Custom4 = 0x0f,
		PartA = 0x10,
		PartB = 0x11,
		Split = 0x12,
		Layer = 0x13,
		Shift = 0x13,
		// Settings = 0x23,
		TrackLeft = 0x67,
		TrackRight =0x66,
		Up = 0x6a,
		Down = 0x6b,
		CaptureMidi = 0x3,
		Undo = 0x4d,
		Quantize = 0x4b,
		Metronome = 0x4c,
		// Stop = 0x34 .. sends Stop
		// Play = 0x36 .. sends Play
		Play = 0x73,
		Stop = 0x74,
		RecEnable = 0x75,
		Loop = 0x76,
		Function = 0x69,
		Scene = 0x68,
		EncUp = 0x33,
		EncDown = 0x44,
	};

	enum KnobID {
		Knob1 = 0x55,
		Knob2 = 0x56,
		Knob3 = 0x57,
		Knob4 = 0x58,
		Knob5 = 0x59,
		Knob6 = 0x5a,
		Knob7 = 0x5b,
		Knob8 = 0x5c,
	};

	LaunchKey4 (ARDOUR::Session&);
	~LaunchKey4 ();

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
	enum FaderBank {
		VolumeFaders,
		PanFaders,
		SendAFaders,
		SendBFaders,
	};

	struct Pad  {

		enum ColorMode {
			Static = 0x0,
			Flashing = 0x1,
			Pulsing = 0x2
		};

		typedef void (LaunchKey4::*PadMethod)(Pad&, int velocity);

		Pad (int pid, int xx, int yy) 
			: id (pid)
			, x (xx)
			, y (yy)
		{
		}

		Pad () : id (-1), x (-1), y (-1)
		{
		}


		int id;
		int x;
		int y;

		sigc::connection timeout_connection;
	};

	void relax (Pad& p);
	void relax (Pad&, int);

	std::set<int> consumed;

	Pad pads[16];
	void build_pad_map ();

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

	void finish_begin_using_device ();

	void stripable_selection_changed ();
	void select_stripable (int col);
	std::weak_ptr<ARDOUR::MidiTrack> _current_pad_target;

	void handle_midi_controller_message (MIDI::Parser&, MIDI::EventTwoBytes*);
	void handle_midi_controller_message_chnF (MIDI::Parser&, MIDI::EventTwoBytes*);
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

	mutable LK4_GUI* _gui;
	void build_gui ();

	void maybe_start_press_timeout (Pad& pad);
	void start_press_timeout (Pad& pad);
	bool long_press_timeout (int pad_id);

	void button_press (int button);
	void button_release (int button);

	void trigger_property_change (PBD::PropertyChange, ARDOUR::Trigger*);
	void trigger_pad_light (Pad& pad, std::shared_ptr<ARDOUR::Route> r, ARDOUR::Trigger* t);
	PBD::ScopedConnectionList trigger_connections;

	void display_session_layout ();
	void transport_state_changed ();
	void record_state_changed ();

	void map_selection ();
	void map_mute_solo ();
	void map_rec_enable ();
	void map_triggers ();

	void map_triggerbox (int col);

	void route_property_change (PBD::PropertyChange const &, int x);
	PBD::ScopedConnectionList route_connections;

	void fader_move (int which, int val);
	void automation_control_change (int n, std::weak_ptr<ARDOUR::AutomationControl>);
	PBD::ScopedConnectionList control_connections;
	FaderBank current_fader_bank;
	bool revert_layout_on_fader_release;

	void use_encoders (bool);
	void encoder (int which, int step);
	void knob (int which, int value);

	int scroll_x_offset;
	int scroll_y_offset;

	uint16_t device_pid;

	enum DisplayTarget {
		StationaryDisplay = 0x20,
		GlobalTemporaryDisplay = 0x21,
		DAWPadFunctionDisplay = 0x22,
		DawDrumrackModeDisplay = 0x23,
		MixerPotMode = 0x24,
		PluginPotMode = 0x25,
		SendPotMode = 0x26,
		TransportPotMode = 0x27,
		FaderMode = 0x28,
	};

	void select_display_target (DisplayTarget dt);
	void set_display_target (DisplayTarget dt, int field, std::string const &, bool display = false);
	void configure_display (DisplayTarget dt, int config);
	void set_plugin_encoder_name (int encoder, int field, std::string const &);

	void set_daw_mode (bool);
	int mode_channel;

	enum PadFunction {
		MuteSolo,
		Triggers,
	};

	PadFunction pad_function;
	void set_pad_function (PadFunction);
	void pad_mute_solo (Pad&);
	void pad_trigger (Pad&, int velocity);
	void pad_release (Pad&);

	bool shift_pressed;
	bool layer_pressed;

	void function_press ();
	void undo_press ();
	void metronome_press ();
	void quantize_press ();
	void button_left ();
	void button_right ();
	void button_down ();
	void button_up ();

	/* stripables */

	int32_t                            bank_start;
	PBD::ScopedConnectionList          stripable_connections;
	std::shared_ptr<ARDOUR::Stripable> stripable[8];
	void stripables_added ();
	void stripable_property_change (PBD::PropertyChange const& what_changed, uint32_t which);
	void switch_bank (uint32_t);

	void solo_changed ();
	void mute_changed (uint32_t which);
	void rec_enable_changed (uint32_t which);

	enum LightingMode {
		Off,
		Solid,
		Flash,
		Pulse,
	};

	void light_button (int which, LightingMode, int color_index);
	void light_pad (int pid, LightingMode, int color_index);

	enum ButtonMode {
		ButtonsRecEnable,
		ButtonsSelect
	};

	ButtonMode button_mode;

	void toggle_button_mode ();
	void show_selection (int which);
	void show_rec_enable (int which);
	void show_mute (int which);
	void show_solo (int which);

	enum EncoderMode {
		EncoderPlugins,
		EncoderMixer,
		EncoderSendA,
		EncoderTransport
	};

	EncoderMode encoder_mode;
	int encoder_bank;
	void set_encoder_bank (int);
	void set_encoder_mode (EncoderMode);
	void set_encoder_titles_to_route_names ();
	void setup_screen_for_encoder_plugins ();
	void label_encoders ();
	void show_encoder_value (int which, std::shared_ptr<ARDOUR::Plugin> plugin, int control, std::shared_ptr<ARDOUR::AutomationControl> ac, bool display);

	void encoder_plugin (int which, int step);
	void encoder_mixer (int which, int step);
	void encoder_pan (int which, int step);
	void encoder_level (int which, int step);
	void encoder_senda (int which, int step);
	void encoder_transport (int which, int step);

	void transport_shuttle (int step);
	void zoom (int step);
	void loop_start_move (int step);
	void loop_end_move (int step);
	void jump_to_marker (int step);

	void light_pad (int pad_id, int color_index);
	void unlight_pad (int pad_id);
	void all_pads (int color_index);
	void all_pads_out ();

	void show_scene_ids ();
	void scene_press ();

	void in_msecs (int msecs, std::function<void()> func);

	std::weak_ptr<ARDOUR::AutomationControl> controls[24];
	std::weak_ptr<ARDOUR::Plugin> current_plugin;
	void plugin_selected (std::weak_ptr<ARDOUR::PluginInsert>);
	uint32_t num_plugin_controls;
};


} } /* namespaces */

#endif /* __ardour_lk4_h__ */
