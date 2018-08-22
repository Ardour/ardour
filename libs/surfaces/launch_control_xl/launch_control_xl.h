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

#ifndef __ardour_launch_control_h__
#define __ardour_launch_control_h__

#include <vector>
#include <map>
#include <stack>
#include <list>
#include <set>

#define ABSTRACT_UI_EXPORTS
#include "pbd/abstract_ui.h"

#include "midi++/types.h"

#include "ardour/mode.h"
#include "ardour/types.h"

#include "control_protocol/control_protocol.h"
#include "control_protocol/types.h"

#include "midi_byte_array.h"

namespace MIDI {
class Parser;
class Port;
} // namespace MIDI

namespace ARDOUR {
class AsyncMIDIPort;
class Port;
class MidiBuffer;
class MidiTrack;
} // namespace ARDOUR

namespace ArdourSurface {


struct LaunchControlRequest : public BaseUI::BaseRequestObject {
	public:
		LaunchControlRequest() {}
		~LaunchControlRequest() {}
};

class LCXLGUI;
class LaunchControlMenu;

class LaunchControlXL : public ARDOUR::ControlProtocol,
                        public AbstractUI<LaunchControlRequest> {
public:
	enum TrackMode {
		TrackMute,
		TrackSolo,
		TrackRecord
	};

	enum ButtonID {
		Focus1 = 0,
		Focus2,
		Focus3,
		Focus4,
		Focus5,
		Focus6,
		Focus7,
		Focus8,
		Control1,
		Control2,
		Control3,
		Control4,
		Control5,
		Control6,
		Control7,
		Control8,
		Device,
		Mute,
		Solo,
		Record,
		SelectUp,
		SelectDown,
		SelectLeft,
		SelectRight
	};

	enum FaderID {
		Fader1 = 0,
		Fader2,
		Fader3,
		Fader4,
		Fader5,
		Fader6,
		Fader7,
		Fader8
	};

	enum KnobID {
		SendA1 = 0,
		SendA2,
		SendA3,
		SendA4,
		SendA5,
		SendA6,
		SendA7,
		SendA8,
		SendB1,
		SendB2,
		SendB3,
		SendB4,
		SendB5,
		SendB6,
		SendB7,
		SendB8,
		Pan1,
		Pan2,
		Pan3,
		Pan4,
		Pan5,
		Pan6,
		Pan7,
		Pan8
	};

	enum LEDFlag { Normal = 0xC, Blink = 0x8, DoubleBuffering = 0x0 };

	enum LEDColor { Off=0, RedLow = 1, RedFull = 3, GreenLow = 16, GreenFull = 48, YellowLow = 34, YellowFull = 51, AmberLow = 18, AmberFull = 35};


	struct Controller {
		Controller(uint8_t cn,  uint8_t val = 0) : _controller_number(cn), _value(val)  {}

		uint8_t  controller_number() const { return _controller_number; }
		uint8_t value() const { return _value; }
		void set_value(uint8_t val) { _value = val; }

		protected:
		uint8_t _controller_number;
		uint8_t _value;
	};

	struct LED {
		LED(uint8_t i, LEDColor c, LaunchControlXL& l) : _index(i), _color(c), _flag(Normal), lcxl(&l)  {}
		LED(uint8_t i, LEDColor c, LEDFlag f, LaunchControlXL& lcxl) : _index(i), _color(c), _flag(f) {}
		virtual ~LED() {}

		LEDColor color() const { return _color; }
		LEDFlag flag() const { return _flag; }
		uint8_t index() const { return _index; }
		void set_flag(LEDFlag f) { _flag = f; }

		virtual MidiByteArray state_msg(bool light) const = 0;

		protected:
		uint8_t _index;
		LEDColor _color;
		LEDFlag _flag;
		MidiByteArray _state_msg;
		LaunchControlXL* lcxl;
	};

	struct MultiColorLED : public LED {
		MultiColorLED (uint8_t i, LEDColor c, LaunchControlXL& l) : LED(i, c, l) {}
		MultiColorLED (uint8_t i, LEDColor c, LEDFlag f, LaunchControlXL& l)
			: LED(i, c, f, l) {}

		void set_color(LEDColor c) { _color = c; }
	};

	struct Button {
		Button(ButtonID id)
			: press_method(&LaunchControlXL::relax)
			, release_method(&LaunchControlXL::relax)
			, long_press_method(&LaunchControlXL::relax), _id(id) {}

		Button(ButtonID id, void (LaunchControlXL::*press)())
			: press_method(press)
			, release_method(&LaunchControlXL::relax)
			, long_press_method(&LaunchControlXL::relax), _id(id) {}

		Button(ButtonID id, void (LaunchControlXL::*press)(), void (LaunchControlXL::*release)())
			: press_method(press), release_method(release)
			, long_press_method(&LaunchControlXL::relax), _id(id) {}

		Button(ButtonID id, void (LaunchControlXL::*press)(), void (LaunchControlXL::*release)(), void (LaunchControlXL::*long_press)())
			: press_method(press), release_method(release)
			, long_press_method(long_press), _id(id) {}

		virtual ~Button() {}

		ButtonID id() const { return _id; }

		void (LaunchControlXL::*press_method)();
		void (LaunchControlXL::*release_method)();
		void (LaunchControlXL::*long_press_method)();

		sigc::connection timeout_connection;

		protected:
		ButtonID _id;
	};

	struct ControllerButton : public Button {

		ControllerButton(ButtonID id, uint8_t cn,
				void (LaunchControlXL::*press)())
			: Button(id, press), _controller_number(cn) {}

		ControllerButton(ButtonID id, uint8_t cn,
				void (LaunchControlXL::*press)(),
				void (LaunchControlXL::*release)())
			: Button(id, press, release), _controller_number(cn) {}


		uint8_t controller_number() const { return _controller_number; }

		private:
		uint8_t _controller_number;
	};

	struct NoteButton : public Button {

		NoteButton(ButtonID id, uint8_t cn, void (LaunchControlXL::*press)())
			: Button(id, press), _note_number(cn) {}

		NoteButton(ButtonID id, uint8_t cn,
				void (LaunchControlXL::*press)(),
				void (LaunchControlXL::*release)())
			: Button(id, press, release), _note_number(cn) {}
		NoteButton(ButtonID id, uint8_t cn,
				void (LaunchControlXL::*press)(),
				void (LaunchControlXL::*release)(),
				void (LaunchControlXL::*release_long)())
			: Button(id, press, release, release_long), _note_number(cn) {}

		uint8_t note_number() const { return _note_number; }

		private:
		uint8_t _note_number;
	};

	struct TrackButton : public NoteButton, public MultiColorLED {
		TrackButton(ButtonID id, uint8_t nn, uint8_t index, LEDColor color,
				void (LaunchControlXL::*press)(), LaunchControlXL& l)
			: NoteButton(id, nn, press), MultiColorLED(index, color, l) {}

		TrackButton(ButtonID id, uint8_t nn, uint8_t index, LEDColor color,
				void (LaunchControlXL::*press)(),
				void (LaunchControlXL::*release)(),
				LaunchControlXL& l)
			: NoteButton(id, nn, press, release), MultiColorLED(index, color, l) {}

		MidiByteArray state_msg(bool light = true) const;
	};

	struct SelectButton : public ControllerButton, public LED {
		SelectButton(ButtonID id, uint8_t cn, uint8_t index, void (LaunchControlXL::*press)(), LaunchControlXL& l)
			: ControllerButton(id, cn, press), LED(index, RedFull, l) {}

		MidiByteArray state_msg(bool light) const;
	};

	struct TrackStateButton : public NoteButton, public LED {
		TrackStateButton(ButtonID id, uint8_t nn, uint8_t index, void (LaunchControlXL::*press)(), LaunchControlXL& l)
			: NoteButton(id, nn, press)
			, LED(index, YellowLow, l) {}

		TrackStateButton(ButtonID id, uint8_t nn, uint8_t index, void (LaunchControlXL::*press)(),
				void (LaunchControlXL::*release)(),
				LaunchControlXL& l)
			: NoteButton(id, nn, press, release)
			, LED(index, YellowLow, l) {}

		TrackStateButton(ButtonID id, uint8_t nn, uint8_t index, void (LaunchControlXL::*press)(),
				void (LaunchControlXL::*release)(),
				void (LaunchControlXL::*release_long)(),
				LaunchControlXL& l)
			: NoteButton(id, nn, press, release, release_long)
			, LED(index, YellowLow, l) {}

		MidiByteArray state_msg(bool light) const;
	};

	struct Fader : public Controller {
		Fader(FaderID id, uint8_t cn)
			: Controller(cn, 0), _id(id) {} // minimal value

		FaderID id() const { return _id; }

		void controller_changed(Controller* controller);

		private:
		FaderID _id;
	};

	struct Knob : public Controller, public MultiColorLED {
		Knob(KnobID id, uint8_t cn, uint8_t index, LaunchControlXL& l)
			: Controller(cn, 64)
			, MultiColorLED(index, Off, l)
			, _id(id) {} // knob 50/50 value

		KnobID id() const { return _id; }

		MidiByteArray state_msg(bool light = true) const;

		private:
		KnobID _id;
	};

public:
	LaunchControlXL(ARDOUR::Session &);
	~LaunchControlXL();


	static bool probe();
	static void *request_factory(uint32_t);

	std::list<boost::shared_ptr<ARDOUR::Bundle> > bundles();

	bool has_editor() const { return true; }
	void *get_gui() const;
	void tear_down_gui();

	bool use_fader8master = false;

	int set_active(bool yn);
	XMLNode &get_state();
	int set_state(const XMLNode &node, int version);

	PBD::Signal0<void> ConnectionChange;

	boost::shared_ptr<ARDOUR::Port> input_port();
	boost::shared_ptr<ARDOUR::Port> output_port();

	Button *button_by_id(ButtonID);

	static std::string button_name_by_id(ButtonID);
	static std::string knob_name_by_id(KnobID);
	static std::string fader_name_by_id(FaderID);

	void write(const MidiByteArray &);
	void reset(uint8_t chan);
	void set_fader8master (bool yn);

	TrackMode track_mode() const { return _track_mode; }
	void set_track_mode(TrackMode mode);

	uint8_t template_number() const { return _template_number; }

private:
	bool in_use;
	TrackMode _track_mode;
	uint8_t _template_number;

	void do_request(LaunchControlRequest *);

	int begin_using_device();
	int stop_using_device();
	int ports_acquire();
	void ports_release();
	void run_event_loop();
	void stop_event_loop();

	void relax() {}

	/* map of NoteButtons by NoteNumber */
	typedef std::map<int, NoteButton *> NNNoteButtonMap;
	NNNoteButtonMap nn_note_button_map;
	/* map of NoteButtons by ButtonID */
	typedef std::map<ButtonID, NoteButton *> IDNoteButtonMap;
	IDNoteButtonMap id_note_button_map;
	/* map of ControllerNoteButtons by CC */
	typedef std::map<int, ControllerButton *> CCControllerButtonMap;
	CCControllerButtonMap cc_controller_button_map;
	/* map of ControllerButtons by ButtonID */
	typedef std::map<ButtonID, ControllerButton *> IDControllerButtonMap;
	IDControllerButtonMap id_controller_button_map;


	/* map of Fader by CC */
	typedef std::map<int, Fader *> CCFaderMap;
	CCFaderMap cc_fader_map;
	/* map of Fader by FaderID */
	typedef std::map<FaderID, Fader *> IDFaderMap;
	IDFaderMap id_fader_map;

	/* map of Knob by CC */
	typedef std::map<int, Knob *> CCKnobMap;
	CCKnobMap cc_knob_map;
	/* map of Knob by KnobID */
	typedef std::map<KnobID, Knob *> IDKnobMap;
	IDKnobMap id_knob_map;

	std::set<ButtonID> buttons_down;
	std::set<ButtonID> consumed;

	bool button_long_press_timeout(ButtonID id, Button *button);
	void start_press_timeout(Button *, ButtonID);

	void init_buttons(bool startup);

	void switch_template(uint8_t t);

	void build_maps();

	// Bundle to represent our input ports
	boost::shared_ptr<ARDOUR::Bundle> _input_bundle;
	// Bundle to represent our output ports
	boost::shared_ptr<ARDOUR::Bundle> _output_bundle;

	MIDI::Port *_input_port;
	MIDI::Port *_output_port;
	boost::shared_ptr<ARDOUR::Port> _async_in;
	boost::shared_ptr<ARDOUR::Port> _async_out;

	void connect_to_parser();
	void handle_button_message(Button* button, MIDI::EventTwoBytes *);
	void handle_fader_message(Fader* fader);
	void handle_knob_message(Knob* knob);

	bool check_pick_up(Controller* controller, boost::shared_ptr<ARDOUR::AutomationControl> ac);

	void handle_midi_controller_message(MIDI::Parser &, MIDI::EventTwoBytes *, MIDI::channel_t chan);
	void handle_midi_note_on_message(MIDI::Parser &, MIDI::EventTwoBytes *, MIDI::channel_t chan);
	void handle_midi_note_off_message(MIDI::Parser &, MIDI::EventTwoBytes *, MIDI::channel_t chan);
	void handle_midi_sysex(MIDI::Parser &, MIDI::byte *, size_t count);

	bool midi_input_handler(Glib::IOCondition ioc, MIDI::Port *port);

	void thread_init();

	PBD::ScopedConnectionList session_connections;
	void connect_session_signals();
	void notify_transport_state_changed();
	void notify_loop_state_changed();
	void notify_parameter_changed(std::string);

	/* Knob methods */

	Knob** knobs_by_column(uint8_t col, Knob** knob_col);
	void update_knob_led(uint8_t n);

	/* Button methods */

	TrackButton* track_button_by_range(uint8_t n, uint8_t first, uint8_t middle);
	TrackButton* focus_button_by_column(uint8_t col) { return track_button_by_range(col, 41, 57) ; }
	TrackButton* control_button_by_column(uint8_t col) { return track_button_by_range(col, 73, 89) ; }


	void button_device();
	void button_device_long_press();
	void button_track_mode(TrackMode state);
	void button_mute();
	void button_solo();
	void button_record();
	void button_select_up();
	void button_select_down();
	void button_select_left();
	void button_select_right();

	void button_track_focus(uint8_t n);
	void button_track_control(uint8_t n);

	boost::shared_ptr<ARDOUR::AutomationControl> get_ac_by_state(uint8_t n);
	void update_track_focus_led(uint8_t n);
	void update_track_control_led(uint8_t n);

	void button_track_focus_1() { button_track_focus(0); }
	void button_track_focus_2() { button_track_focus(1); }
	void button_track_focus_3() { button_track_focus(2); }
	void button_track_focus_4() { button_track_focus(3); }
	void button_track_focus_5() { button_track_focus(4); }
	void button_track_focus_6() { button_track_focus(5); }
	void button_track_focus_7() { button_track_focus(6); }
	void button_track_focus_8() { button_track_focus(7); }

	void button_track_control_1() { button_track_control(0); }
	void button_track_control_2() { button_track_control(1); }
	void button_track_control_3() { button_track_control(2); }
	void button_track_control_4() { button_track_control(3); }
	void button_track_control_5() { button_track_control(4); }
	void button_track_control_6() { button_track_control(5); }
	void button_track_control_7() { button_track_control(6); }
	void button_track_control_8() { button_track_control(7); }

	/* stripables */

	int32_t bank_start;
	PBD::ScopedConnectionList stripable_connections;
	boost::shared_ptr<ARDOUR::Stripable> stripable[8];

	void stripables_added ();

	void stripable_property_change (PBD::PropertyChange const& what_changed, uint32_t which);

	void switch_bank (uint32_t base);

	void solo_changed (uint32_t n) { solo_mute_rec_changed(n); }
	void mute_changed (uint32_t n) { solo_mute_rec_changed(n); }
	void rec_changed (uint32_t n) { solo_mute_rec_changed(n); }
	void solo_mute_rec_changed (uint32_t n);

	/* special Stripable */

	boost::shared_ptr<ARDOUR::Stripable> master;

	PBD::ScopedConnection port_reg_connection;
	void port_registration_handler();

	enum ConnectionState { InputConnected = 0x1, OutputConnected = 0x2 };

	int connection_state;
	bool connection_handler(boost::weak_ptr<ARDOUR::Port>, std::string name1,
			boost::weak_ptr<ARDOUR::Port>, std::string name2,
			bool yn);
	PBD::ScopedConnection port_connection;
	void connected();

	/* GUI */

	mutable LCXLGUI *gui;
	void build_gui();

	void stripable_selection_changed();

	bool in_range_select;
												};


} // namespace ArdourSurface

#endif /* __ardour_launch_control_h__ */
