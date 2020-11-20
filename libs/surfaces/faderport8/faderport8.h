/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef ardour_surface_faderport8_h
#define ardour_surface_faderport8_h

// allow to undo "mute clear", "solo clear"
// eventually this should use some libardour mixer history/undo
#define FP8_MUTESOLO_UNDO

#include <list>
#include <map>
#include <glibmm/threads.h>

#define ABSTRACT_UI_EXPORTS
#include "pbd/abstract_ui.h"
#include "pbd/properties.h"
#include "pbd/controllable.h"

#include "ardour/types.h"
#include "ardour/async_midi_port.h"
#include "ardour/midi_port.h"

#include "control_protocol/control_protocol.h"

#include "fp8_base.h"
#include "fp8_controls.h"

namespace MIDI {
	class Parser;
}

namespace ARDOUR {
	class Bundle;
	class Session;
	class Processor;
	class PluginInsert;
}

namespace ArdourSurface { namespace FP_NAMESPACE {

struct FaderPort8Request : public BaseUI::BaseRequestObject
{
	public:
		FaderPort8Request () {}
		~FaderPort8Request () {}
};

class FaderPort8 : public FP8Base, public ARDOUR::ControlProtocol, public AbstractUI<FaderPort8Request>
{
public:
	FaderPort8 (ARDOUR::Session&);
	virtual ~FaderPort8();

	int set_active (bool yn);

	/* we probe for a device when our ports are connected. Before that,
	 * there's no way to know if the device exists or not.
	 */
	static bool  probe() { return true; }
	static void* request_factory (uint32_t);

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	/* configuration GUI */
	bool  has_editor () const { return true; }
	void* get_gui () const;
	void  tear_down_gui ();
	PBD::Signal0<void> ConnectionChange;

	void set_button_action (FP8Controls::ButtonId, bool, std::string const&);
	std::string get_button_action (FP8Controls::ButtonId, bool);
	FP8Controls const& control () const { return  _ctrls; }

	void set_clock_mode (uint32_t m) { _clock_mode = m; }
	void set_scribble_mode (uint32_t m) { _scribble_mode = m; }
	void set_two_line_text (bool yn) { _two_line_text = yn; }
	void set_auto_pluginui (bool yn) { _auto_pluginui = yn; }

	uint32_t clock_mode () const { return _clock_mode; }
	uint32_t scribble_mode () const { return _scribble_mode; }
	bool twolinetext () const { return _two_line_text; }
	bool auto_pluginui () const { return _auto_pluginui; }

	void stop ();
	void do_request (FaderPort8Request*);
	void thread_init ();

	boost::shared_ptr<ARDOUR::Port> input_port() const { return _input_port; }
	boost::shared_ptr<ARDOUR::Port> output_port() const { return _output_port; }
	std::list<boost::shared_ptr<ARDOUR::Bundle> > bundles ();

	size_t tx_midi (std::vector<uint8_t> const&) const;

private:
	void close ();

	void start_midi_handling ();
	void stop_midi_handling ();

	/* I/O Ports */
	PBD::ScopedConnectionList port_connections;
	boost::shared_ptr<ARDOUR::AsyncMIDIPort> _input_port;
	boost::shared_ptr<ARDOUR::AsyncMIDIPort> _output_port;
	boost::shared_ptr<ARDOUR::Bundle>        _input_bundle;
	boost::shared_ptr<ARDOUR::Bundle>        _output_bundle;

	bool midi_input_handler (Glib::IOCondition ioc, boost::weak_ptr<ARDOUR::AsyncMIDIPort> port);

	bool connection_handler (std::string name1, std::string name2);
	void engine_reset ();

	enum ConnectionState {
		InputConnected = 0x1,
		OutputConnected = 0x2
	};

	void connected ();
	void disconnected ();
	int  _connection_state;
	bool _device_active;

	/* MIDI input message handling */
	void sysex_handler (MIDI::Parser &p, MIDI::byte *, size_t);
	void polypressure_handler (MIDI::Parser &, MIDI::EventTwoBytes* tb);
	void pitchbend_handler (MIDI::Parser &, uint8_t chan, MIDI::pitchbend_t pb);
	void controller_handler (MIDI::Parser &, MIDI::EventTwoBytes* tb);
	void note_on_handler (MIDI::Parser &, MIDI::EventTwoBytes* tb);
	void note_off_handler (MIDI::Parser &, MIDI::EventTwoBytes* tb);
	PBD::ScopedConnectionList midi_connections;

	/* ***************************************************************************
	 * Control Elements
	 */
	FP8Controls _ctrls;
	void notify_stripable_added_or_removed ();
	void notify_fader_mode_changed ();
	void filter_stripables (ARDOUR::StripableList& strips) const;
	void assign_stripables (bool select_only = false);
	void set_periodic_display_mode (FP8Strip::DisplayMode);

	void assign_strips ();
	void bank (bool down, bool page);
	void move_selected_into_view ();
	void select_prev_next (bool next);

	void assign_sends ();
	void spill_plugins ();
	void assign_processor_ctrls ();
	bool assign_plugin_presets (boost::shared_ptr<ARDOUR::PluginInsert>);
	void build_well_known_processor_ctrls (boost::shared_ptr<ARDOUR::Stripable>, bool);
	void preset_changed ();
	void select_plugin (int num);
	void select_plugin_preset (size_t num);

	void toggle_preset_param_mode ();
	void bank_param (bool down, bool page);
	/* bank offsets */
	int  get_channel_off (FP8Types::MixMode m) const { return _channel_off [m]; }
	void set_channel_off (FP8Types::MixMode m, int off) {_channel_off [m] = off ; }

	int _channel_off[FP8Types::MixModeMax + 1];
	int _plugin_off;
	int _parameter_off;

	/* plugin + send mode stripable
	 *
	 * This is used when parameters of one strip are assigned to
	 * individual FP8Strip controls (Edit Send, Edit Plugins).
	 *
	 * When there's one stripable per FP8Strip, FP8Strip itself keeps
	 * track of the object lifetime and these are NULL.
	 */
	PBD::ScopedConnectionList processor_connections;

	PBD::ScopedConnectionList assigned_stripable_connections;
	typedef std::map<boost::shared_ptr<ARDOUR::Stripable>, uint8_t> StripAssignmentMap;
	StripAssignmentMap _assigned_strips;

	void drop_ctrl_connections ();

	void select_strip (boost::weak_ptr<ARDOUR::Stripable>);

	void notify_pi_property_changed (const PBD::PropertyChange&);
	void notify_stripable_property_changed (boost::weak_ptr<ARDOUR::Stripable>, const PBD::PropertyChange&);
	void stripable_selection_changed ();
	void subscribe_to_strip_signals ();

	PBD::ScopedConnection selection_connection;
	PBD::ScopedConnectionList route_state_connections;
	PBD::ScopedConnectionList modechange_connections;
	/* **************************************************************************/
	struct ProcessorCtrl {
		ProcessorCtrl (std::string const &n, boost::shared_ptr<ARDOUR::AutomationControl> c)
		 : name (n)
		 , ac (c)
		{}
		std::string name;
		boost::shared_ptr<ARDOUR::AutomationControl> ac;

		inline bool operator< (const ProcessorCtrl& other) const;
	};

	std::list <ProcessorCtrl> _proc_params;
	boost::weak_ptr<ARDOUR::PluginInsert> _plugin_insert;
	bool _show_presets;
	int _showing_well_known;
	/* **************************************************************************/

	/* periodic updates, parameter poll */
	sigc::connection _periodic_connection;
	bool periodic ();
	std::string _timecode;
	std::string _musical_time;
	std::string const& timecode () const { return _timecode; }
	std::string const& musical_time () const { return _musical_time; }

	int _timer_divider;

	bool show_meters () const { return _scribble_mode & 1; }
	bool show_panner () const { return _scribble_mode & 2; }

	/* sync button blink -- the FP's blink mode does not work */
	sigc::connection _blink_connection;
	bool _blink_onoff;
	bool blink_it ();

	/* shift key */
	sigc::connection _shift_connection;
	bool _shift_lock;
	int _shift_pressed;
	bool shift_timeout () { _shift_lock = true; return false; }
	bool shift_mod () const { return _shift_lock || (_shift_pressed > 0); }

	/* GUI */
	void build_gui ();
	mutable void *gui;

	/* setup callbacks & actions */
	void connect_session_signals ();
	void setup_actions ();
	void send_session_state ();

	/* callbacks */
	PBD::ScopedConnectionList session_connections;
	void notify_parameter_changed (std::string);
	void notify_record_state_changed ();
	void notify_transport_state_changed ();
	void notify_loop_state_changed ();
	void notify_snap_change ();
	void notify_session_dirty_changed ();
	void notify_history_changed ();
	void notify_solo_changed ();
	void notify_mute_changed ();
	void notify_route_state_changed ();
	void notify_plugin_active_changed ();

	/* actions */
	PBD::ScopedConnectionList button_connections;
	void button_play ();
	void button_stop ();
	void button_record ();
	void button_loop ();
	void button_metronom ();
	void button_bypass ();
	void button_open ();
	void button_link ();
	void button_lock ();
	void button_varispeed (bool);
#ifdef FP8_MUTESOLO_UNDO
	void button_solo_clear ();
#endif
	void button_mute_clear ();
	void button_arm (bool);
	void button_arm_all ();
	void button_automation (ARDOUR::AutoState);
	void button_prev_next (bool);
	void button_action (const std::string& group, const std::string& item);

	void button_chanlock (); /* FP2 only */
	void button_flip (); /* FP2 only */

	void button_encoder ();
	void button_parameter ();
	void encoder_navigate (bool, int);
	void encoder_parameter (bool, int);

	/* mute undo history */
#ifdef FP8_MUTESOLO_UNDO
	std::vector <boost::weak_ptr<ARDOUR::AutomationControl> > _mute_state;
	std::vector <boost::weak_ptr<ARDOUR::AutomationControl> > _solo_state;
#endif

	/* Encoder handlers */
	void handle_encoder_pan (int steps);
	void handle_encoder_link (int steps);

	/* Control Link */
	void stop_link ();
	void start_link ();
	void lock_link ();
	void unlock_link (bool drop = false);
	void nofity_focus_control (boost::weak_ptr<PBD::Controllable>);
	PBD::ScopedConnection link_connection;
	PBD::ScopedConnection link_locked_connection;
	boost::weak_ptr<PBD::Controllable> _link_control;
	bool _link_enabled;
	bool _link_locked; // can only be true if _link_enabled

	bool _chan_locked; /* FP2 only */

	/* user prefs */
	uint32_t _clock_mode;
	uint32_t _scribble_mode;
	bool     _two_line_text;
	bool     _auto_pluginui;

	/* user bound actions */
	void button_user (bool, FP8Controls::ButtonId);

	enum ActionType {
		Unset,
		NamedAction,
		// InternalFunction, // unused
	};

	struct UserAction {
		UserAction () : _type (Unset) {}

		ActionType _type;
		std::string _action_name;
		//boost::function<void()> function; // unused

		void clear ()
		{
			_type = Unset;
			_action_name.clear();
		}

		void assign_action (std::string const& action_name)
		{
			if (action_name.empty ()) {
				_type = Unset;
				_action_name.clear();
			} else {
				_type = NamedAction;
				_action_name = action_name;
			}
		}

		bool empty () const
		{
			return _type == Unset;
		}

		void call (FaderPort8& _base) const
		{
			switch (_type) {
				case NamedAction:
					_base.access_action (_action_name);
					break;
				default:
					break;
			}
		}
	};

	struct ButtonAction {
		UserAction on_press;
		UserAction on_release;

		UserAction& action (bool press)
		{
			return press ? on_press : on_release;
		}

		UserAction const& action (bool press) const
		{
			return press ? on_press : on_release;
		}

		void call (FaderPort8& _base, bool press) const
		{
			action (press).call (_base);
		}
		bool empty () const
		{
			return on_press.empty () && on_release.empty();
		}
	};

	typedef std::map<FP8Controls::ButtonId, ButtonAction> UserActionMap;
	UserActionMap _user_action_map;
};

} } /* namespace */

#endif /* ardour_surface_faderport8_h */
