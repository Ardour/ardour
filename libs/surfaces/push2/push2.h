/*
    Copyright (C) 2016 Paul Davis

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

#ifndef __ardour_push2_h__
#define __ardour_push2_h__

#include <vector>
#include <map>
#include <list>
#include <set>

#include <libusb.h>

#include <cairomm/refptr.h>

#define ABSTRACT_UI_EXPORTS
#include "pbd/abstract_ui.h"
#include "midi++/types.h"
#include "ardour/types.h"
#include "control_protocol/control_protocol.h"

#include "midi_byte_array.h"

namespace Cairo {
	class ImageSurface;
	class Context;
}

namespace Pango {
	class Layout;
}

namespace MIDI {
	class Parser;
	class Port;
}

namespace ARDOUR {
	class AsyncMIDIPort;
	class Port;
	class MidiBuffer;
}

namespace ArdourSurface {

struct Push2Request : public BaseUI::BaseRequestObject {
public:
	Push2Request () {}
	~Push2Request () {}
};

class Push2 : public ARDOUR::ControlProtocol
            , public AbstractUI<Push2Request>
{
   public:
	Push2 (ARDOUR::Session&);
	~Push2 ();

	static bool probe ();
	static void* request_factory (uint32_t);

	bool has_editor () const { return true; }
	void* get_gui () const;
	void  tear_down_gui ();

	int set_active (bool yn);
	XMLNode& get_state();
	int set_state (const XMLNode & node, int version);

	PBD::Signal0<void> ConnectionChange;

	boost::shared_ptr<ARDOUR::Port> input_port();
	boost::shared_ptr<ARDOUR::Port> output_port();

	uint8_t pad_note (int row, int col) const;

   private:
	libusb_device_handle *handle;
	uint8_t   frame_header[16];
	uint16_t* device_frame_buffer;
	int  device_buffer;
	Cairo::RefPtr<Cairo::ImageSurface> frame_buffer;
	sigc::connection vblank_connection;
	sigc::connection periodic_connection;

	enum ModifierState {
		None = 0,
		ModShift = 0x1,
		ModSelect = 0x2,
	};

	ModifierState modifier_state;

	static const int cols;
	static const int rows;
	static const int pixels_per_row;

	void do_request (Push2Request*);
	int stop ();
	int open ();
	int close ();
	bool redraw ();
	int blit_to_device_frame_buffer ();
	bool vblank ();

	enum ButtonID {
		TapTempo,
		Metronome,
		Upper1, Upper2, Upper3, Upper4, Upper5, Upper6, Upper7, Upper8,
		Setup,
		User,
		Delete,
		AddDevice,
		Device,
		Mix,
		Undo,
		AddTrack,
		Browse,
		Clip,
		Mute,
		Solo,
		Stop,
		Lower1, Lower2, Lower3, Lower4, Lower5, Lower6, Lower7, Lower8,
		Master,
		Convert,
		DoubleLoop,
		Quantize,
		Duplicate,
		New,
		FixedLength,
		Automate,
		RecordEnable,
		Play,
		Fwd32ndT,
		Fwd32nd,
		Fwd16thT,
		Fwd16th,
		Fwd8thT,
		Fwd8th,
		Fwd4trT,
		Fwd4tr,
		Up,
		Right,
		Down,
		Left,
		Repeat,
		Accent,
		Scale,
		Layout,
		Note,
		Session,
		OctaveUp,
		PageRight,
		OctaveDown,
		PageLeft,
		Shift,
		Select
	};

	struct LED
	{
		enum State {
			NoTransition,
			OneShot24th,
			OneShot16th,
			OneShot8th,
			OneShot4th,
			OneShot2th,
			Pulsing24th,
			Pulsing16th,
			Pulsing8th,
			Pulsing4th,
			Pulsing2th,
			Blinking24th,
			Blinking16th,
			Blinking8th,
			Blinking4th,
			Blinking2th
		};

		enum Colors {
			Black = 0,
			Red = 127,
			Green = 126,
			Blue = 125,
			DarkGray = 124,
			LightGray = 123,
			White = 122
		};

		LED (uint8_t e) : _extra (e), _color_index (0), _state (NoTransition) {}
		virtual ~LED() {}

		uint8_t extra () const { return _extra; }
		uint8_t color_index () const { return _color_index; }
		State   state () const { return _state; }

		void set_color (uint8_t color_index);
		void set_state (State state);

		virtual MidiByteArray state_msg() const = 0;

	     protected:
		uint8_t _extra;
		uint8_t _color_index;
		State   _state;
	};

	struct Pad : public LED {
		Pad (int xx, int yy, uint8_t ex)
			: LED (ex)
			, x (xx)
			, y (yy) {}

		MidiByteArray state_msg () const { return MidiByteArray (3, 0x90|_state, _extra, _color_index); }

		int coord () const { return (y * 8) + x; }
		int note_number() const { return extra(); }

		int x;
		int y;
	};

	struct Button : public LED {
		Button (ButtonID bb, uint8_t ex)
			: LED (ex)
			, id (bb)
			, press_method (&Push2::relax)
			, release_method (&Push2::relax)
			, long_press_method (&Push2::relax)
		{}

		Button (ButtonID bb, uint8_t ex, void (Push2::*press)())
			: LED (ex)
			, id (bb)
			, press_method (press)
			, release_method (&Push2::relax)
			, long_press_method (&Push2::relax)
		{}

		Button (ButtonID bb, uint8_t ex, void (Push2::*press)(), void (Push2::*release)())
			: LED (ex)
			, id (bb)
			, press_method (press)
			, release_method (release)
			, long_press_method (&Push2::relax)
		{}

		Button (ButtonID bb, uint8_t ex, void (Push2::*press)(), void (Push2::*release)(), void (Push2::*long_press)())
			: LED (ex)
			, id (bb)
			, press_method (press)
			, release_method (release)
			, long_press_method (long_press)
		{}

		MidiByteArray state_msg () const { return MidiByteArray (3, 0xb0|_state, _extra, _color_index); }
		int controller_number() const { return extra(); }

		ButtonID id;
		void (Push2::*press_method)();
		void (Push2::*release_method)();
		void (Push2::*long_press_method)();
		sigc::connection timeout_connection;
	};

	struct ColorButton : public Button {
		ColorButton (ButtonID bb, uint8_t ex)
			: Button (bb, ex) {}


		ColorButton (ButtonID bb, uint8_t ex, void (Push2::*press)())
			: Button (bb, ex, press) {}

		ColorButton (ButtonID bb, uint8_t ex, void (Push2::*press)(), void (Push2::*release)())
			: Button (bb, ex, press, release) {}

		ColorButton (ButtonID bb, uint8_t ex, void (Push2::*press)(), void (Push2::*release)(), void (Push2::*long_press)())
			: Button (bb, ex, press, release, long_press) {}
	};

	struct WhiteButton : public Button {
		WhiteButton (ButtonID bb, uint8_t ex)
			: Button (bb, ex) {}

		WhiteButton (ButtonID bb, uint8_t ex, void (Push2::*press)())
			: Button (bb, ex, press) {}

		WhiteButton (ButtonID bb, uint8_t ex, void (Push2::*press)(), void (Push2::*release)())
			: Button (bb, ex, press, release) {}

		WhiteButton (ButtonID bb, uint8_t ex, void (Push2::*press)(), void (Push2::*release)(), void (Push2::*long_press)())
			: Button (bb, ex, press, release, long_press) {}
	};

	void relax () {}

	/* map of Buttons by CC */
	typedef std::map<int,Button*> CCButtonMap;
	CCButtonMap cc_button_map;
	/* map of Buttons by ButtonID */
	typedef std::map<ButtonID,Button*> IDButtonMap;
	IDButtonMap id_button_map;
	std::set<ButtonID> buttons_down;
	std::set<ButtonID> consumed;

	bool button_long_press_timeout (ButtonID id);
	void start_press_timeout (Button&, ButtonID);

	void init_buttons (bool startup);
	void init_touch_strip ();

	/* map of Pads by note number */
	typedef std::map<int,Pad*> NNPadMap;
	NNPadMap nn_pad_map;
	/* map of Pads by coordinate
	 *
	 * coord = row * 64 + column;
	 *
	 * rows start at top left
	 */
	typedef std::map<int,Pad*> CoordPadMap;
	CoordPadMap coord_pad_map;

	void set_button_color (ButtonID, uint8_t color_index);
	void set_button_state (ButtonID, LED::State);
	void set_led_color (ButtonID, uint8_t color_index);
	void set_led_state (ButtonID, LED::State);

	void build_maps ();

	MIDI::Port* _input_port;
	MIDI::Port* _output_port;
	boost::shared_ptr<ARDOUR::Port> _async_in;
	boost::shared_ptr<ARDOUR::Port> _async_out;

	void connect_to_parser ();
	void handle_midi_pitchbend_message (MIDI::Parser&, MIDI::pitchbend_t);
	void handle_midi_controller_message (MIDI::Parser&, MIDI::EventTwoBytes*);
	void handle_midi_note_on_message (MIDI::Parser&, MIDI::EventTwoBytes*);
	void handle_midi_note_off_message (MIDI::Parser&, MIDI::EventTwoBytes*);
	void handle_midi_sysex (MIDI::Parser&, MIDI::byte *, size_t count);

	void write (const MidiByteArray&);
	bool midi_input_handler (Glib::IOCondition ioc, MIDI::Port* port);
	bool periodic ();

	void thread_init ();

	PBD::ScopedConnectionList session_connections;
	void connect_session_signals ();
	void notify_record_state_changed ();
	void notify_transport_state_changed ();
	void notify_loop_state_changed ();
	void notify_parameter_changed (std::string);
	void notify_solo_active_changed (bool);

	/* Button methods */
	void button_play ();
	void button_recenable ();
	void button_up ();
	void button_down ();
	void button_right ();
	void button_left ();
	void button_metronome ();
	void button_repeat ();
	void button_solo ();
	void button_fixed_length ();
	void button_new ();
	void button_browse ();
	void button_clip ();
	void button_upper (uint32_t n);
	void button_lower (uint32_t n);
	void button_upper_1 () { button_upper (0); }
	void button_upper_2 () { button_upper (1); }
	void button_upper_3 () { button_upper (2); }
	void button_upper_4 () { button_upper (3); }
	void button_upper_5 () { button_upper (4); }
	void button_upper_6 () { button_upper (5); }
	void button_upper_7 () { button_upper (6); }
	void button_upper_8 () { button_upper (7); }
	void button_lower_1 () { button_lower (0); }
	void button_lower_2 () { button_lower (1); }
	void button_lower_3 () { button_lower (2); }
	void button_lower_4 () { button_lower (3); }
	void button_lower_5 () { button_lower (4); }
	void button_lower_6 () { button_lower (5); }
	void button_lower_7 () { button_lower (6); }
	void button_lower_8 () { button_lower (7); }
	void button_undo ();
	void button_fwd32t ();
	void button_fwd32 ();
	void button_fwd16t ();
	void button_fwd16 ();
	void button_fwd8t ();
	void button_fwd8 ();
	void button_fwd4t ();
	void button_fwd4 ();
	void button_add_track ();
	void button_stop ();
	void button_shift_press ();
	void button_shift_release ();
	void button_shift_long_press ();
	void button_select_press ();
	void button_select_release ();
	void button_select_long_press ();
	void button_page_left ();
	void button_page_right ();
	void button_octave_up ();
	void button_octave_down ();

	void start_shift ();
	void end_shift ();
	void start_select ();
	void end_select ();

	/* encoders */

	void strip_vpot (int, int);
	void other_vpot (int, int);
	void strip_vpot_touch (int, bool);
	void other_vpot_touch (int, bool);

	/* widgets */

	Cairo::RefPtr<Cairo::Context> context;
	Glib::RefPtr<Pango::Layout> tc_clock_layout;
	Glib::RefPtr<Pango::Layout> bbt_clock_layout;
	Glib::RefPtr<Pango::Layout> upper_layout[8];
	Glib::RefPtr<Pango::Layout> mid_layout[8];
	Glib::RefPtr<Pango::Layout> lower_layout[8];

	void splash ();
	ARDOUR::microseconds_t splash_start;

	/* stripables */

	int32_t bank_start;
	PBD::ScopedConnectionList stripable_connections;
	boost::shared_ptr<ARDOUR::Stripable> stripable[8];
	boost::shared_ptr<ARDOUR::Stripable> master;
	boost::shared_ptr<ARDOUR::Stripable> monitor;

	void solo_change (int);
	void mute_change (int);
	void stripable_property_change (PBD::PropertyChange const& what_changed, int which);

	void switch_bank (uint32_t base);

	bool pad_filter (ARDOUR::MidiBuffer& in, ARDOUR::MidiBuffer& out) const;

	boost::weak_ptr<ARDOUR::Stripable> first_selected_stripable;

	PBD::ScopedConnection port_reg_connection;
	void port_registration_handler ();

	enum ConnectionState {
		InputConnected = 0x1,
		OutputConnected = 0x2
	};

	int connection_state;
	bool connection_handler (boost::weak_ptr<ARDOUR::Port>, std::string name1, boost::weak_ptr<ARDOUR::Port>, std::string name2, bool yn);
	PBD::ScopedConnection port_connection;

	/* GUI */

	mutable void *gui;
	void build_gui ();

	/* pad mapping */

	uint8_t pad_table[8][8];
	void build_pad_table();
	int octave_shift;
};


} /* namespace */

#endif /* __ardour_push2_h__ */
