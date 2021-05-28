/*
 * Copyright (C) 2018 Jan Lentfer <jan.lentfer@web.de>
 * Copyright (C) 2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2018 Térence Clastres <t.clastres@gmail.com>
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
                        public AbstractUI<LaunchControlRequest>
{
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

	enum DeviceStatus {
		dev_nonexistant = 0,
		dev_inactive,
		dev_active
	};

	enum LEDFlag { Normal = 0xC, Blink = 0x8, DoubleBuffering = 0x0 };
	enum LEDColor { Off=0, RedLow = 1, RedFull = 3, GreenLow = 16, GreenFull = 48, YellowLow = 34, YellowFull = 51, AmberLow = 18, AmberFull = 35};


#ifdef MIXBUS
	enum CompParam {
		CompMakeup,
		CompMode,
		CompSpeed
	};
#endif

	struct Controller {
		Controller(uint8_t cn,  uint8_t val, boost::function<void ()> action)
		: _controller_number(cn)
		, _value(val)
		, action_method(action) {}

		uint8_t  controller_number() const { return _controller_number; }
		uint8_t value() const { return _value; }
		void set_value(uint8_t val) { _value = val; }

		protected:
		uint8_t _controller_number;
		uint8_t _value;

		public:
		boost::function<void ()> action_method;
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
		Button(ButtonID id, boost::function<void ()> press, boost::function<void ()> release,
				boost::function<void ()> long_press)
			: press_method(press)
			, release_method(release)
			, long_press_method(long_press),
			_id(id) {}

		virtual ~Button() {}

		ButtonID id() const { return _id; }

		boost::function<void ()> press_method;
		boost::function<void ()> release_method;
		boost::function<void ()> long_press_method;

		sigc::connection timeout_connection;

		protected:
		ButtonID _id;
	};

	struct ControllerButton : public Button {
		ControllerButton(ButtonID id, uint8_t cn,
				boost::function<void ()> press,
				boost::function<void ()> release,
				boost::function<void ()> long_release)
			: Button(id, press, release, long_release), _controller_number(cn) {}



		uint8_t controller_number() const { return _controller_number; }

		private:
		uint8_t _controller_number;
	};

	struct NoteButton : public Button {
		NoteButton(ButtonID id, uint8_t cn,
				boost::function<void ()> press,
				boost::function<void ()> release,
				boost::function<void ()> release_long)
			: Button(id, press, release, release_long), _note_number(cn) {}

		uint8_t note_number() const { return _note_number; }

		private:
		uint8_t _note_number;
	};

	struct TrackButton : public NoteButton, public MultiColorLED {
		TrackButton(ButtonID id, uint8_t nn, uint8_t index, LEDColor c_on, LEDColor c_off,
				boost::function<void ()> press,
				boost::function<void ()> release,
				boost::function<void ()> release_long,
				boost::function<uint8_t ()> check,
				LaunchControlXL& l)
			: NoteButton(id, nn, press, release, release_long)
			, MultiColorLED(index, Off, l)
			, check_method(check)
			, _color_enabled (c_on)
			, _color_disabled (c_off) {}




		LEDColor color_enabled() const { return _color_enabled; }
		LEDColor color_disabled() const { return _color_disabled; }
		void set_color_enabled (LEDColor c_on) { _color_enabled = c_on; }
		void set_color_disabled (LEDColor c_off) { _color_disabled = c_off; }
		boost::function<uint8_t ()> check_method;


		MidiByteArray state_msg(bool light = true) const;

		private:
		LEDColor _color_enabled;
		LEDColor _color_disabled;
	};

	struct SelectButton : public ControllerButton, public LED {
		SelectButton(ButtonID id, uint8_t cn, uint8_t index,
				boost::function<void ()> press,
				boost::function<void ()> release,
				boost::function<void ()> long_release,
				LaunchControlXL& l)
			: ControllerButton(id, cn, press, release, long_release), LED(index, RedFull, l) {}


		MidiByteArray state_msg(bool light) const;
	};

	struct TrackStateButton : public NoteButton, public LED {
		TrackStateButton(ButtonID id, uint8_t nn, uint8_t index,
				boost::function<void ()> press,
				boost::function<void ()> release,
				boost::function<void ()> release_long,
				LaunchControlXL& l)
			: NoteButton(id, nn, press, release, release_long)
			, LED(index, YellowLow, l) {}

		MidiByteArray state_msg(bool light) const;
	};

	struct Fader : public Controller {
		Fader(FaderID id, uint8_t cn, boost::function<void ()> action)
			: Controller(cn, 0, action), _id(id) {} // minimal value

		FaderID id() const { return _id; }

		void controller_changed(Controller* controller);

		private:
		FaderID _id;
	};

	struct Knob : public Controller, public MultiColorLED {
		Knob(KnobID id, uint8_t cn, uint8_t index, LEDColor c_on, LEDColor c_off, boost::function<void ()> action,
			LaunchControlXL &l)
			: Controller(cn, 64, action)
			, MultiColorLED(index, Off, l)
			, _id(id)
			, _color_enabled (c_on)
			, _color_disabled (c_off) {} // knob 50/50 value

		Knob(KnobID id, uint8_t cn, uint8_t index, LEDColor c_on, LEDColor c_off, boost::function<void ()> action,
			boost::function<uint8_t ()> check, LaunchControlXL &l)
			: Controller(cn, 64, action)
			, MultiColorLED(index, Off, l)
			, check_method(check)
			, _id(id)
			, _color_enabled (c_on)
			, _color_disabled (c_off) {} // knob 50/50 value



		KnobID id() const { return _id; }
		LEDColor color_enabled() const { return _color_enabled; }
		LEDColor color_disabled() const { return _color_disabled; }
		boost::function<uint8_t ()> check_method;

		MidiByteArray state_msg(bool light = true) const;

		private:
		KnobID _id;
		LEDColor _color_enabled;
		LEDColor _color_disabled;
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

	int get_amount_of_tracks();

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
	bool fader8master () const { return _fader8master; }

	void set_refresh_leds_flag (bool yn);
	bool refresh_leds_flag () const { return _refresh_leds_flag; }

	void set_device_mode (bool yn);
	bool device_mode () const { return _device_mode; }

#ifdef MIXBUS32C
	void set_ctrllowersends (bool yn);
	bool ctrllowersends () const { return _ctrllowersends; }

	void store_fss_type();
	bool fss_is_mixbus() const { return _fss_is_mixbus; }
#endif
	TrackMode track_mode() const { return _track_mode; }
	void set_track_mode(TrackMode mode);

	uint8_t template_number() const { return _template_number; }

	void set_send_bank (int offset);
	void send_bank_switch(bool up);
	int send_bank_base () const { return _send_bank_base; }

private:
	bool in_use;
	TrackMode _track_mode;
	uint8_t _template_number;

	bool _fader8master;
	bool _device_mode;
#ifdef MIXBUS32C
	bool _ctrllowersends;
	bool _fss_is_mixbus;
#endif
	bool _refresh_leds_flag;

	int _send_bank_base;

	void do_request(LaunchControlRequest *);

	int begin_using_device();
	int stop_using_device();
	int ports_acquire();
	void ports_release();
	void run_event_loop();
	void stop_event_loop();

	void relax() {}

	/* map of NoteButtons by NoteNumber */
	typedef std::map<int, boost::shared_ptr<NoteButton> > NNNoteButtonMap;
	NNNoteButtonMap nn_note_button_map;
	/* map of NoteButtons by ButtonID */
	typedef std::map<ButtonID, boost::shared_ptr<NoteButton> > IDNoteButtonMap;
	IDNoteButtonMap id_note_button_map;
	/* map of ControllerNoteButtons by CC */
	typedef std::map<int, boost::shared_ptr<ControllerButton> > CCControllerButtonMap;
	CCControllerButtonMap cc_controller_button_map;
	/* map of ControllerButtons by ButtonID */
	typedef std::map<ButtonID, boost::shared_ptr<ControllerButton> > IDControllerButtonMap;
	IDControllerButtonMap id_controller_button_map;


	/* map of Fader by CC */
	typedef std::map<int, boost::shared_ptr<Fader> > CCFaderMap;
	CCFaderMap cc_fader_map;
	/* map of Fader by FaderID */
	typedef std::map<FaderID, boost::shared_ptr<Fader> > IDFaderMap;
	IDFaderMap id_fader_map;

	/* map of Knob by CC */
	typedef std::map<int, boost::shared_ptr<Knob> > CCKnobMap;
	CCKnobMap cc_knob_map;
	/* map of Knob by KnobID */
	typedef std::map<KnobID, boost::shared_ptr<Knob> > IDKnobMap;
	IDKnobMap id_knob_map;

	std::set<ButtonID> buttons_down;
	std::set<ButtonID> consumed;

	bool button_long_press_timeout(ButtonID id, boost::shared_ptr<Button> button);
	void start_press_timeout(boost::shared_ptr<Button> , ButtonID);

	void init_buttons();
	void init_buttons(bool startup);
	void init_buttons (ButtonID buttons[], uint8_t i);
	void init_knobs();
	void init_knobs(KnobID knobs[], uint8_t i);
	void init_knobs_and_buttons();

	void init_device_mode();
	void init_dm_callbacks();

	void switch_template(uint8_t t);
	void filter_stripables (ARDOUR::StripableList& strips) const;

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
	void handle_button_message(boost::shared_ptr<Button> button, MIDI::EventTwoBytes *);

	bool check_pick_up(boost::shared_ptr<Controller> controller, boost::shared_ptr<ARDOUR::AutomationControl> ac, bool rotary = false);

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
	boost::shared_ptr<Knob> knob_by_id(KnobID id);
	boost::shared_ptr<Knob>* knobs_by_column(uint8_t col, boost::shared_ptr<Knob>* knob_col);
	void update_knob_led_by_strip(uint8_t n);
	void update_knob_led_by_id(uint8_t id, LEDColor color);

	void knob_sendA(uint8_t n);
	void knob_sendB(uint8_t n);
	void knob_pan(uint8_t n);

	uint8_t dm_check_dummy(DeviceStatus ds);

	void dm_fader(FaderID id);
	uint8_t dm_check_pan_azi ();
	void dm_pan_azi(KnobID k);
	uint8_t dm_check_pan_width();
	void dm_pan_width (KnobID k);
	uint8_t dm_check_trim ();
	void dm_trim(KnobID k);
	uint8_t dm_mute_enabled();
	void dm_mute_switch();
	uint8_t dm_solo_enabled();
	void dm_solo_switch();
	uint8_t dm_recenable_enabled();
	void dm_recenable_switch();
	void dm_select_prev_strip();
	void dm_select_next_strip();

#ifdef MIXBUS
	void dm_mb_eq_switch();
	void dm_mb_eq (KnobID k, bool gain, uint8_t band);
	uint8_t dm_mb_eq_freq_enabled();
	uint8_t dm_mb_eq_gain_enabled(uint8_t band);
	void dm_mb_eq_shape_switch(uint8_t band);
	uint8_t dm_mb_eq_shape_enabled(uint8_t band);
	uint8_t dm_mb_flt_enabled();
	void dm_mb_flt_frq (KnobID k, bool hpf);
	void dm_mb_flt_switch();
	void dm_mb_send_enabled(KnobID k);
	uint8_t dm_mb_check_send_knob(KnobID k);
	uint8_t dm_mb_check_send_button(uint8_t s);
	void dm_mb_sends (KnobID k);
	void dm_mb_send_switch (ButtonID b);
	uint8_t dm_mb_comp_enabled();
	void dm_mb_comp_switch();
	void dm_mb_comp (KnobID k, CompParam c);
	void dm_mb_comp_thresh (FaderID id);
	uint8_t dm_mb_has_tapedrive();
	void dm_mb_tapedrive (KnobID k);
	uint8_t dm_mb_master_assign_enabled();
	void dm_mb_master_assign_switch();
#endif

	/* Fader methods */
	void fader(uint8_t n);


	/* Button methods */
	boost::shared_ptr<TrackButton> track_button_by_range(uint8_t n, uint8_t first, uint8_t middle);
	boost::shared_ptr<TrackButton> focus_button_by_column(uint8_t col) { return track_button_by_range(col, 41, 57) ; }
	boost::shared_ptr<TrackButton> control_button_by_column(uint8_t col) { return track_button_by_range(col, 73, 89) ; }


	void button_device();
	void button_device_long_press();
	void button_track_mode(TrackMode state);
	void button_mute();
	void button_mute_long_press();
	void button_solo();
	void button_solo_long_press();
	void button_record();
	void button_select_up();
	void button_select_down();
	void button_select_left();
	void button_select_right();

	void button_track_focus(uint8_t n);
	void button_press_track_control(uint8_t n);
	void button_release_track_control(uint8_t n);

	boost::shared_ptr<ARDOUR::AutomationControl> get_ac_by_state(uint8_t n);
	void update_track_focus_led(uint8_t n);
	void update_track_control_led(uint8_t n);

	void send_bank_switch_0() { send_bank_switch(0); }
	void send_bank_switch_1() { send_bank_switch(1); }

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
	void solo_iso_changed (uint32_t n);
	void solo_iso_led_bank ();
#ifdef MIXBUS
	void master_send_changed (uint32_t n);
	void master_send_led_bank ();
#endif

	void solo_mute_rec_changed (uint32_t n);

	/* special Stripable */

	boost::shared_ptr<ARDOUR::Stripable> master;

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
