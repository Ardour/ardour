/*
 * Copyright (C) 2023 Holger Dehnhardt <holger@dehnhardt.org>
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

#ifndef ardour_surface_console1_h
#define ardour_surface_console1_h

#include <list>
#include <map>
#include <set>

#include <gtkmm/treemodel.h>
#include <gtkmm/liststore.h>

#include <glibmm/threads.h>

#define ABSTRACT_UI_EXPORTS
#include "pbd/abstract_ui.h"

#include "ardour/presentation_info.h"

#include "control_protocol/control_protocol.h"
#include "control_protocol/types.h"

#include "midi_surface/midi_byte_array.h"
#include "midi_surface/midi_surface.h"

namespace MIDI {
class Parser;
class Port;
}

namespace ARDOUR {
class AsyncMIDIPort;
class Bundle;
class Port;
class Plugin;
class Processor;
class ReadOnlyControl;
class Route;
class Session;
class MidiPort;
}

namespace PBD {
class Controllable;
}

class MIDIControllable;
class MIDIFunction;
class MIDIAction;

namespace ArdourSurface {

class C1GUI;

// XXX TODO: these classes should not be in the ArdourSurface namespace
// which is shared with all other ctrl surfaces.
//
// ArdourSurface::Meter etc may cause conflicts.
// best add a C1 prefix, or additional namespace

class Controller;
class ControllerButton;
class MultiStateButton;
class Meter;
class Encoder;

// 'using' in header files is frowned upon.
// it may cause conflicts or result in amgibuities
using Controllable = std::shared_ptr<PBD::Controllable>;
using order_t = ARDOUR::PresentationInfo::order_t;

class ControlNotFoundException : public std::exception
{
public:
	ControlNotFoundException () {}
};

class Console1 : public MIDISurface
{

	friend Controller;
	friend ControllerButton;
	friend MultiStateButton;
	friend Meter;
	friend Encoder;

public:
	Console1 (ARDOUR::Session&);
	~Console1 ();

	void map_p ();

	int set_active (bool yn) override;

	bool has_editor () const override { return true; }
	void* get_gui () const override;
	void tear_down_gui () override;

	std::string input_port_name () const override;
	std::string output_port_name () const override;
	uint32_t load_mappings ();

	XMLNode& get_state () const override;
	int set_state (const XMLNode&, int version) override;

	bool swap_solo_mute;
	bool create_mapping_stubs;

	PBD::Signal0<void> ConnectionChange;

	/* Timer Events */
	PBD::Signal1<void, bool> BlinkIt;
	PBD::Signal0<void> Periodic;

	/* Local Signals */
	PBD::Signal0<void> BankChange;
	PBD::Signal1<void, bool> ShiftChange;
	PBD::Signal1<void, bool> PluginStateChange;

	enum ControllerID
	{
		CONTROLLER_NONE = 0,
		VOLUME = 7,
		PAN = 10,
		MUTE = 12,
		SOLO = 13,
		ORDER = 14,
		DRIVE = 15,
		EXTERNAL_SIDECHAIN = 17,
		CHARACTER = 18,
		FOCUS1 = 21,
		FOCUS2,
		FOCUS3,
		FOCUS4,
		FOCUS5,
		FOCUS6,
		FOCUS7,
		FOCUS8,
		FOCUS9,
		FOCUS10,
		FOCUS11,
		FOCUS12,
		FOCUS13,
		FOCUS14,
		FOCUS15,
		FOCUS16,
		FOCUS17,
		FOCUS18,
		FOCUS19,
		FOCUS20 = 40,
		COMP = 46,
		COMP_THRESH = 47,
		COMP_RELEASE = 48,
		COMP_RATIO = 49,
		COMP_PAR = 50,
		COMP_ATTACK = 51,
		SHAPE = 53,
		SHAPE_GATE = 54,
		SHAPE_SUSTAIN = 55,
		SHAPE_RELEASE = 56,
		SHAPE_PUNCH = 57,
		PRESET = 58,
		HARD_GATE = 59,
		FILTER_TO_COMPRESSORS = 61,
		HIGH_SHAPE = 65,
		EQ = 80,
		HIGH_GAIN = 82,
		HIGH_FREQ = 83,
		HIGH_MID_GAIN = 85,
		HIGH_MID_FREQ = 86,
		HIGH_MID_SHAPE = 87,
		LOW_MID_GAIN = 88,
		LOW_MID_FREQ = 89,
		LOW_MID_SHAPE = 90,
		LOW_GAIN = 91,
		LOW_FREQ = 92,
		LOW_SHAPE = 93,
		PAGE_UP = 96,
		PAGE_DOWN = 97,
		DISPLAY_ON = 102,
		LOW_CUT = 103,
		MODE = 104,
		HIGH_CUT = 105,
		GAIN = 107,
		PHASE_INV = 108,
		INPUT_METER_L = 110,
		INPUT_METER_R = 111,
		OUTPUT_METER_L = 112,
		OUTPUT_METER_R = 113,
		SHAPE_METER = 114,
		COMP_METER = 115,
		TRACK_COPY = 120,
		TRACK_GROUP = 123,

	};

	using ControllerMap = std::map<std::string, ControllerID>;

	ControllerMap controllerMap{ { "CONTROLLER_NONE", ControllerID::CONTROLLER_NONE },
		                         { "VOLUME", ControllerID::VOLUME },
		                         { "PAN", ControllerID::PAN },
		                         { "MUTE", ControllerID::MUTE },
		                         { "SOLO", ControllerID::SOLO },
		                         { "ORDER", ControllerID::ORDER },
		                         { "DRIVE", ControllerID::DRIVE },
		                         { "EXTERNAL_SIDECHAIN", ControllerID::EXTERNAL_SIDECHAIN },
		                         { "CHARACTER", ControllerID::CHARACTER },
		                         { "FOCUS1", ControllerID::FOCUS1 },
		                         { "FOCUS2", ControllerID::FOCUS2 },
		                         { "FOCUS3", ControllerID::FOCUS3 },
		                         { "FOCUS4", ControllerID::FOCUS4 },
		                         { "FOCUS5", ControllerID::FOCUS5 },
		                         { "FOCUS6", ControllerID::FOCUS6 },
		                         { "FOCUS7", ControllerID::FOCUS7 },
		                         { "FOCUS8", ControllerID::FOCUS8 },
		                         { "FOCUS9", ControllerID::FOCUS9 },
		                         { "FOCUS10", ControllerID::FOCUS10 },
		                         { "FOCUS11", ControllerID::FOCUS11 },
		                         { "FOCUS12", ControllerID::FOCUS12 },
		                         { "FOCUS13", ControllerID::FOCUS13 },
		                         { "FOCUS14", ControllerID::FOCUS14 },
		                         { "FOCUS15", ControllerID::FOCUS15 },
		                         { "FOCUS16", ControllerID::FOCUS16 },
		                         { "FOCUS17", ControllerID::FOCUS17 },
		                         { "FOCUS18", ControllerID::FOCUS18 },
		                         { "FOCUS19", ControllerID::FOCUS19 },
		                         { "FOCUS20", ControllerID::FOCUS20 },
		                         { "COMP", ControllerID::COMP },
		                         { "COMP_THRESH", ControllerID::COMP_THRESH },
		                         { "COMP_RELEASE", ControllerID::COMP_RELEASE },
		                         { "COMP_RATIO", ControllerID::COMP_RATIO },
		                         { "COMP_PAR", ControllerID::COMP_PAR },
		                         { "COMP_ATTACK", ControllerID::COMP_ATTACK },
		                         { "SHAPE", ControllerID::SHAPE },
		                         { "SHAPE_GATE", ControllerID::SHAPE_GATE },
		                         { "SHAPE_SUSTAIN", ControllerID::SHAPE_SUSTAIN },
		                         { "SHAPE_RELEASE", ControllerID::SHAPE_RELEASE },
		                         { "SHAPE_PUNCH", ControllerID::SHAPE_PUNCH },
		                         { "PRESET", ControllerID::PRESET },
		                         { "HARD_GATE", ControllerID::HARD_GATE },
		                         { "FILTER_TO_COMPRESSORS", ControllerID::FILTER_TO_COMPRESSORS },
		                         { "HIGH_SHAPE", ControllerID::HIGH_SHAPE },
		                         { "EQ", ControllerID::EQ },
		                         { "HIGH_GAIN", ControllerID::HIGH_GAIN },
		                         { "HIGH_FREQ", ControllerID::HIGH_FREQ },
		                         { "HIGH_MID_GAIN", ControllerID::HIGH_MID_GAIN },
		                         { "HIGH_MID_FREQ", ControllerID::HIGH_MID_FREQ },
		                         { "HIGH_MID_SHAPE", ControllerID::HIGH_MID_SHAPE },
		                         { "LOW_MID_GAIN", ControllerID::LOW_MID_GAIN },
		                         { "LOW_MID_FREQ", ControllerID::LOW_MID_FREQ },
		                         { "LOW_MID_SHAPE", ControllerID::LOW_MID_SHAPE },
		                         { "LOW_GAIN", ControllerID::LOW_GAIN },
		                         { "LOW_FREQ", ControllerID::LOW_FREQ },
		                         { "LOW_SHAPE", ControllerID::LOW_SHAPE },
		                         { "PAGE_UP", ControllerID::PAGE_UP },
		                         { "PAGE_DOWN", ControllerID::PAGE_DOWN },
		                         { "DISPLAY_ON", ControllerID::DISPLAY_ON },
		                         { "LOW_CUT", ControllerID::LOW_CUT },
		                         { "MODE", ControllerID::MODE },
		                         { "HIGH_CUT", ControllerID::HIGH_CUT },
		                         { "GAIN", ControllerID::GAIN },
		                         { "PHASE_INV", ControllerID::PHASE_INV },
		                         { "INPUT_METER_L", ControllerID::INPUT_METER_L },
		                         { "INPUT_METER_R", ControllerID::INPUT_METER_R },
		                         { "OUTPUT_METER_L", ControllerID::OUTPUT_METER_L },
		                         { "OUTPUT_METER_R", ControllerID::OUTPUT_METER_R },
		                         { "SHAPE_METER", ControllerID::SHAPE_METER },
		                         { "COMP_METER", ControllerID::COMP_METER },
		                         { "TRACK_COPY", ControllerID::TRACK_COPY },
		                         { "TRACK_GROUP", ControllerID::TRACK_GROUP } };

    struct PluginControllerColumns : public Gtk::TreeModel::ColumnRecord {
		PluginControllerColumns () { 
			add (controllerName);
            add (controllerId);
		}
		Gtk::TreeModelColumn<std::string> controllerName;
		Gtk::TreeModelColumn<int>         controllerId; 
	};
	PluginControllerColumns plugin_controller_columns;
	Glib::RefPtr<Gtk::ListStore> plugin_controller_model;
	const std::string findControllerNameById (const ControllerID id);

  private:
	std::string config_dir_name = "c1mappings";
	/* GUI */
	mutable C1GUI* gui;
	void build_gui ();

	bool mappings_loaded = false;
	bool controls_model_loaded = false;

	/* Configuration */
	const uint32_t bank_size = 20;

	// Shift button
	bool shift_state = false;
	bool in_plugin_state = false;

	bool rolling = false;
	uint32_t current_bank = 0;
	uint32_t current_strippable_index = 0;

	uint32_t max_strip_index = 0;
	uint32_t master_index = 0;

	int32_t current_plugin_index = -1;
#ifdef MIXBUS
	int32_t selected_intern_plugin_index = -1;
#endif

	std::shared_ptr<ARDOUR::AutomationControl> current_pan_control = nullptr;

	std::shared_ptr<ARDOUR::Stripable> _current_stripable;
	std::weak_ptr<ARDOUR::Stripable> pre_master_stripable;
	std::weak_ptr<ARDOUR::Stripable> pre_monitor_stripable;

	void create_encoder (ControllerID id,
	                     boost::function<void (uint32_t)> action,
	                     boost::function<void (uint32_t)> shift_action = 0);

	void setup_controls ();

	bool strip_recenabled = false;
	ARDOUR::MonitorState monitor_state = ARDOUR::MonitorState::MonitoringSilence;

	int begin_using_device () override;
	int stop_using_device () override;

	int device_acquire () override { return 0; }
	void device_release () override {}

	void connect_session_signals () override;
	void connect_internal_signals ();

	void run_event_loop ();
	void stop_event_loop ();

	/* MIDI-Message handler - we only have controller messages */
	void handle_midi_controller_message (MIDI::Parser&, MIDI::EventTwoBytes* tb) override;

	void master_monitor_has_changed ();

	/* Strip inventory */
	typedef std::map<uint32_t, order_t> StripInventoryMap;

	StripInventoryMap strip_inventory;

	void create_strip_inventory ();

	void strip_inventory_changed (ARDOUR::RouteList&) {
		create_strip_inventory ();
	}

	order_t get_inventory_order_by_index (const uint32_t index);
	uint32_t get_index_by_inventory_order (order_t order);

	void select_rid_by_index (const uint32_t index);

	/* Controller Maps*/
	typedef std::map<ControllerID, ArdourSurface::ControllerButton*> ButtonMap;
	typedef std::map<ControllerID, ArdourSurface::MultiStateButton*> MultiStateButtonMap;
	typedef std::map<ControllerID, ArdourSurface::Meter*> MeterMap;
	typedef std::map<ControllerID, ArdourSurface::Encoder*> EncoderMap;

	ButtonMap buttons;
	ControllerButton* get_button (ControllerID) const;

	MultiStateButtonMap multi_buttons;
	MultiStateButton* get_mbutton (ControllerID id) const;

	MeterMap meters;
	Meter* get_meter (ControllerID) const;

	EncoderMap encoders;
	Encoder* get_encoder (ControllerID) const;

	typedef std::map<uint32_t, ControllerID> SendControllerMap;
	SendControllerMap send_controllers{ { 0, LOW_FREQ },       { 1, LOW_MID_FREQ },   { 2, HIGH_MID_FREQ },
		                                { 3, HIGH_FREQ },      { 4, LOW_GAIN },       { 5, LOW_MID_GAIN },
		                                { 6, HIGH_MID_GAIN },  { 7, HIGH_GAIN },      { 8, LOW_MID_SHAPE },
		                                { 9, HIGH_MID_SHAPE }, { 10, LOW_MID_SHAPE }, { 11, HIGH_MID_SHAPE } };

	ControllerID get_send_controllerid (uint32_t);

	/* */
	void all_lights_out ();

	void notify_transport_state_changed () override;
	void notify_solo_active_changed (bool) override;

	sigc::connection periodic_connection;

	bool periodic ();
	void periodic_update_meter ();

	// Meter Handlig
	uint32_t last_output_meter_l = 0;
	uint32_t last_output_meter_r = 0;

	std::shared_ptr<ARDOUR::ReadOnlyControl> gate_redux_meter = 0;
	uint32_t last_gate_meter = 0;

	std::shared_ptr<ARDOUR::ReadOnlyControl> comp_redux_meter = 0;
	uint32_t last_comp_redux = 0;

	sigc::connection blink_connection;
	typedef std::list<ControllerID> Blinkers;
	Blinkers blinkers;
	bool blink_state;
	bool blinker ();
	void start_blinking (ControllerID);
	void stop_blinking (ControllerID);

	void set_current_stripable (std::shared_ptr<ARDOUR::Stripable>);
	void drop_current_stripable ();
	/*void use_master ();
	void use_monitor ();*/
	void stripable_selection_changed () override;
	/*PBD::ScopedConnection selection_connection;*/
	PBD::ScopedConnectionList stripable_connections;
	PBD::ScopedConnectionList console1_connections;
	PBD::ScopedConnectionList plugin_connections;

	void map_stripable_state ();

	void notify_parameter_changed (std::string) override;

	/* operations (defined in c1_operations.cc) */

	void bank (bool up);
	void drive (uint32_t value);
	void gain (const uint32_t value);
	void mute (const uint32_t);
	void pan (const uint32_t value);
	void phase (const uint32_t);
	void rude_solo (const uint32_t);
	void select (const uint32_t i);
	void shift (const uint32_t);
	void plugin_state (const uint32_t);
	void solo (const uint32_t);
	void trim (const uint32_t value);
	void window (const uint32_t value);
	void zoom (const uint32_t value);

	// Filter Section
	void filter (const uint32_t value);
	void low_cut (const uint32_t value);
	void high_cut (const uint32_t value);

	// Gate Section
	void gate (const uint32_t value);
	void gate_scf (const uint32_t value);
	void gate_listen (const uint32_t value);
	void gate_thresh (const uint32_t value);
	void gate_depth (const uint32_t value);
	void gate_release (const uint32_t value);
	void gate_attack (const uint32_t value);
	void gate_hyst (const uint32_t value);
	void gate_hold (const uint32_t value);
	void gate_filter_freq (const uint32_t value);

	// EQ section
	void eq (const uint32_t);
	void eq_freq (const uint32_t band, uint32_t value);
	void eq_gain (const uint32_t band, uint32_t value);
	void eq_high_shape (const uint32_t value);
	void eq_low_shape (const uint32_t value);

	ControllerID eq_freq_controller_for_band (const uint32_t band)
	{
		ControllerID eq_freq_id;
		switch (band) {
			case 0:
				eq_freq_id = ControllerID::LOW_FREQ;
				break;
			case 1:
				eq_freq_id = ControllerID::LOW_MID_FREQ;
				break;
			case 2:
				eq_freq_id = ControllerID::HIGH_MID_FREQ;
				break;
			case 3:
				eq_freq_id = ControllerID::HIGH_FREQ;
				break;
		}
		return eq_freq_id;
	}

	ControllerID eq_gain_controller_for_band (const uint32_t band)
	{
		ControllerID eq_gain_id;
		switch (band) {
			case 0:
				eq_gain_id = ControllerID::LOW_GAIN;
				break;
			case 1:
				eq_gain_id = ControllerID::LOW_MID_GAIN;
				break;
			case 2:
				eq_gain_id = ControllerID::HIGH_MID_GAIN;
				break;
			case 3:
				eq_gain_id = ControllerID::HIGH_GAIN;
				break;
		}
		return eq_gain_id;
	}

	// Mixbus sends
	void mb_send_level (const uint32_t n, const uint32_t value);

	// Comp Section
	void comp (const uint32_t value);
	void comp_mode (const uint32_t value);
	void comp_thresh (const uint32_t value);
	void comp_attack (const uint32_t value);
	void comp_release (const uint32_t value);
	void comp_ratio (const uint32_t value);
	void comp_makeup (const uint32_t value);
	void comp_emph (const uint32_t value);

	bool map_encoder (ControllerID controllerID);
	void map_encoder (ControllerID controllerID, std::shared_ptr<ARDOUR::AutomationControl> control);

	void map_bank ();
	void map_drive ();
	void map_gain ();
	void map_monitoring ();
	void map_mute ();
	void map_pan ();
	void map_phase ();
	void map_recenable ();
	void map_select ();
	void map_shift (bool shift);
	void map_plugin_state (bool state);
	void map_solo ();
	void map_trim ();

	// Filter Section
	void map_filter ();
	void map_low_cut ();
	void map_high_cut ();

	// Gate Section
	void map_gate ();
	void map_gate_scf ();
	void map_gate_listen ();
	void map_gate_thresh ();
	void map_gate_depth ();
	void map_gate_release ();
	void map_gate_attack ();
	void map_gate_hyst ();
	void map_gate_hold ();
	void map_gate_filter_freq ();

	// EQ section
	void map_eq ();
	void map_eq_freq (const uint32_t band);
	void map_eq_gain (const uint32_t band);
	void map_eq_low_shape ();
	void map_eq_high_shape ();

	// MB Sends
	void map_mb_send_level (const uint32_t n);

	// Comp Section
	void map_comp ();
	void map_comp_mode ();
	void map_comp_thresh ();
	void map_comp_attack ();
	void map_comp_release ();
	void map_comp_ratio ();
	void map_comp_makeup ();
	void map_comp_emph ();

	bool rec_enable_state;

	float calculate_meter (float dB);
	uint32_t control_to_midi (Controllable controllable, float val, uint32_t max_value_for_type = 127);
	float midi_to_control (Controllable controllable, uint32_t val, uint32_t max_value_for_type = 127);

	struct PluginParameterMapping
	{
		int paramIndex;
		bool shift = false;
		bool is_switch = false;
		std::string name;
		ControllerID controllerId;
	};

	using ParameterMap = std::map<uint32_t, PluginParameterMapping>;

	/* plugin handling */
	bool ensure_config_dir ();
	bool load_mapping (XMLNode* fin);
	void create_mapping (const std::shared_ptr<ARDOUR::Processor> proc, const std::shared_ptr<ARDOUR::Plugin> plugin);

	bool spill_plugins (const int32_t plugin_index);

	/* plugin operations */
	void remove_plugin_operations ();
	std::shared_ptr<ARDOUR::Processor> find_plugin (const int32_t plugin_index);
	bool select_plugin (const int32_t plugin_index);

	bool map_select_plugin (const int32_t plugin_index);


  public:
	struct PluginMapping
	{
		std::string id;
		std::string name;
		bool configured;
		ParameterMap parameters;
	};
	using PluginMappingMap = std::map<std::string, PluginMapping>;
	PluginMappingMap pluginMappingMap;

	PluginMappingMap getPluginMappingMap () { return pluginMappingMap; }
    Glib::RefPtr<Gtk::ListStore> getPluginControllerModel();
	void write_plugin_mapping (PluginMapping &mapping);
};
}
#endif /* ardour_surface_console1_h */
