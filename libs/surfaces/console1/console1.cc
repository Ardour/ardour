/*
 * Copyright (C) 2026 Holger Dehnhardt <holger@dehnhardt.org>
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

#include <algorithm>
#include <cmath>
#include <optional>

#include "pbd/controllable.h"
#include "pbd/i18n.h"

#include "ardour/audioengine.h"
#include "ardour/debug.h"
#include "ardour/filesystem_paths.h"
#include "ardour/meter.h"
#include "ardour/monitor_control.h"
#include "ardour/phase_control.h"
#include "ardour/readonly_control.h"
#include "ardour/route.h"
#include "ardour/selection.h"
#include "ardour/session.h"
#include "ardour/stripable.h"
#include "ardour/track.h"
#include "ardour/vca_manager.h"
#include "ardour/well_known_enum.h"

#include "console1.h"
#include "c1_control.h"
#include "c1_gui.h"

#include "pbd/abstract_ui.inc.cc" // instantiate template

using namespace ARDOUR;
using namespace ArdourSurface;
using namespace PBD;
using namespace Glib;
using namespace std;

Console1::Console1 (Session& s)
  : MIDISurface (s, X_ ("Softube Console1"), X_ ("Console1"), false)
  , gui (0)
  , blink_state (false)
  , rec_enable_state (false)
{
	run_event_loop ();
	port_setup ();
}

Console1::~Console1 ()
{
	all_lights_out ();

	tear_down_gui ();

	stop_event_loop ();
	MIDISurface::drop ();

	for (const auto& b : buttons) {
		delete b.second;
	}
	for (const auto& e : encoders) {
		delete e.second;
	}
	for (const auto& m : meters) {
		delete m.second;
	}
	for (const auto& mb : multi_buttons) {
		delete mb.second;
	}
}

void
Console1::all_lights_out ()
{
	for (ButtonMap::iterator b = buttons.begin (); b != buttons.end (); ++b) {
		b->second->set_led_state (false);
	}
}

int
Console1::set_active (bool yn)
{
	DEBUG_TRACE (DEBUG::Console1, string_compose ("Console1::set_active init with yn: '%1'\n", yn));

	if (yn == active ()) {
		return 0;
	}

	ControlProtocol::set_active (yn);

	return 0;
}

std::string
Console1::input_port_name () const
{
#ifdef __APPLE__
	/* the origin of the numeric magic identifiers is known only to Ableton
	   and may change in time. This is part of how CoreMIDI works.
	*/
	return X_ ("system:midi_capture_2849385499");
#else
	return X_ ("Console1 Recv");
#endif
}

XMLNode&
Console1::get_state () const
{
	XMLNode& node = MIDISurface::get_state ();
	node.set_property ("swap-solo-mute", swap_solo_mute);
	node.set_property ("band-q-as-send", band_q_as_send);
	node.set_property ("create-mapping-stubs", create_mapping_stubs);
	return node;
}

int
Console1::set_state (const XMLNode& node, int version)
{
	MIDISurface::set_state (node, version);
	std::string tmp;
	node.get_property ("swap-solo-mute", tmp);
	swap_solo_mute = (tmp == "1");
	if (node.property("band-q-as-send")) {
		node.get_property("band-q-as-send", tmp);
	} else {
		tmp = "1";
	}
	band_q_as_send = (tmp == "1");
	node.get_property("create-mapping-stubs", tmp);
	create_mapping_stubs = (tmp == "1");
	return 0;
}

std::string
Console1::output_port_name () const
{
#ifdef __APPLE__
	/* the origin of the numeric magic identifiers is known only to Ableton
	   and may change in time. This is part of how CoreMIDI works.
	*/
	return X_ ("system:midi_playback_1721623007");
#else
	return X_ ("Console1 Send");
#endif
}

void
Console1::run_event_loop ()
{
  DEBUG_TRACE (DEBUG::Launchpad, "start event loop\n");
  BaseUI::run ();
}

void
Console1::stop_event_loop ()
{
  DEBUG_TRACE (DEBUG::Launchpad, "stop event loop\n");
  BaseUI::quit ();
}

int
Console1::begin_using_device ()
{
	DEBUG_TRACE (DEBUG::Console1, "sending device inquiry message...\n");

	/*
	  with this sysex command we can enter the 'native mode'
	  But there's no need to do so
	  f0 7d 20 00 00 00 01 00 7f 49 6f 6c 73 00 f7
	*/

	load_mappings ();
	if (!controls_initialized) {
		setup_controls ();
		controls_initialized = true;
	}

	/* Connection to the blink-timer */
	Glib::RefPtr<Glib::TimeoutSource> blink_timeout = Glib::TimeoutSource::create (200); // milliseconds
	blink_connection = blink_timeout->connect (sigc::mem_fun (*this, &Console1::blinker));
	blink_timeout->attach (main_loop ()->get_context ());

	/* Connection to the peridic timer for meters */
	Glib::RefPtr<Glib::TimeoutSource> periodic_timer = Glib::TimeoutSource::create (100);
	periodic_connection = periodic_timer->connect (sigc::mem_fun (*this, &Console1::periodic));
	periodic_timer->attach (main_loop ()->get_context ());
	connect_internal_signals ();
	create_strip_inventory ();

	DEBUG_TRACE (DEBUG::Console1, "************** begin_using_device() ********************\n");

	int ret = MIDISurface::begin_using_device ();
	if (ret == 0) {
		if (session->loading ()) {
			/* the GUI restores its own selection while loading, so wait until that is done */
			session->SessionLoaded.connect (
			  session_connections, MISSING_INVALIDATOR, std::bind (&Console1::schedule_initial_selection, this), this);
		} else {
			schedule_initial_selection ();
		}
	}
	return ret;
}

void
Console1::schedule_initial_selection ()
{
	/* The GUI ignores selection changes until its editor and mixer have been attached to the
	 * session, which happens after SessionLoaded. There is no signal for that, so give it a moment. */
	initial_selection_connection.disconnect ();
	Glib::RefPtr<Glib::TimeoutSource> t = Glib::TimeoutSource::create (1000);
	initial_selection_connection = t->connect ([this] () {
		select_initial_stripable ();
		return false;
	});
	t->attach (main_loop ()->get_context ());
}

void
Console1::select_initial_stripable ()
{
	if (!session || !_in_use) {
		return;
	}
	std::shared_ptr<Stripable> s = ControlProtocol::first_selected_stripable ();
	if (s) {
		if (s != _current_stripable) {
			set_current_stripable (s);
		}
		/* make sure the GUI shows the selection that was made before it was ready */
		PBD::PropertyChange pc;
		pc.add (ARDOUR::Properties::selected);
		PresentationInfo::send_static_change (pc);
		return;
	}
	if (strip_inventory.empty ()) {
		return;
	}
	select_rid_by_index (0);
}

int
Console1::stop_using_device ()
{
	DEBUG_TRACE (DEBUG::Console1, "stop_using_device()\n");
	blink_connection.disconnect ();
	periodic_connection.disconnect ();
	initial_selection_connection.disconnect ();
	stripable_connections.drop_connections ();
	gate_connections.drop_connections ();
	eq_connections.drop_connections ();
	comp_connections.drop_connections ();
	console1_connections.drop_connections ();
	MIDISurface::stop_using_device ();
	return 0;
}

void
Console1::connect_session_signals ()
{
	DEBUG_TRACE (DEBUG::Console1, "connect_session_signals\n");
	// receive routes added
	session->RouteAdded.connect (
	  session_connections, MISSING_INVALIDATOR, std::bind (&Console1::create_strip_inventory, this), this);
	// receive VCAs added
	session->vca_manager ().VCAAdded.connect (
	  session_connections, MISSING_INVALIDATOR, std::bind (&Console1::create_strip_inventory, this), this);

	// receive record state toggled
	// session->RecordStateChanged.connect(session_connections,
	// MISSING_INVALIDATOR, std::bind
	// (&MIDISurface::notify_record_state_changed, this), this); receive
	// transport
	session->TransportStateChange.connect (
	  session_connections, MISSING_INVALIDATOR, std::bind (&Console1::notify_transport_state_changed, this), this);
	// session->TransportLooped.connect (session_connections,
	// MISSING_INVALIDATOR, std::bind
	// (&MIDISurface::notify_loop_state_changed, this), this); receive punch-in
	// and punch-out
	Config->ParameterChanged.connect (
	  session_connections, MISSING_INVALIDATOR, std::bind (&Console1::notify_parameter_changed, this, _1), this);
	session->config.ParameterChanged.connect (
	  session_connections, MISSING_INVALIDATOR, std::bind (&Console1::notify_parameter_changed, this, _1), this);
	// receive rude solo changed
	session->SoloActive.connect (
	  session_connections, MISSING_INVALIDATOR, std::bind (&Console1::notify_solo_active_changed, this, _1), this);
	session->MonitorBusAddedOrRemoved.connect (
	  session_connections, MISSING_INVALIDATOR, std::bind (&Console1::master_monitor_has_changed, this), this);
	session->MonitorChanged.connect (
	  session_connections, MISSING_INVALIDATOR, std::bind (&Console1::master_monitor_has_changed, this), this);
	session->RouteAdded.connect (
	  session_connections, MISSING_INVALIDATOR, std::bind (&Console1::strip_inventory_changed, this, _1), this);
}

void
Console1::connect_internal_signals ()
{
	DEBUG_TRACE (DEBUG::Console1, "connect_internal_signals\n");
	BankChange.connect (console1_connections, MISSING_INVALIDATOR, std::bind (&Console1::map_bank, this), this);
	ShiftChange.connect (console1_connections, MISSING_INVALIDATOR, std::bind (&Console1::map_shift, this, _1), this);
	EQBandQBindingChange.connect (console1_connections, MISSING_INVALIDATOR, std::bind(&Console1::eqBandQChangeMapping, this, _1), this);
	PluginStateChange.connect(console1_connections, MISSING_INVALIDATOR, std::bind(&Console1::map_plugin_state, this, _1), this);
	GotoView.connect (
			console1_connections,
			MISSING_INVALIDATOR,
			[] (uint32_t val) { DEBUG_TRACE (DEBUG::Console1, string_compose ("GotooView: %1\n", val)); },
			this);
	VerticalZoomInSelected.connect (
			console1_connections, MISSING_INVALIDATOR, [] () { DEBUG_TRACE (DEBUG::Console1, "VerticalZoomIn\n"); }, this);
	VerticalZoomOutSelected.connect (
			console1_connections, MISSING_INVALIDATOR, [] () { DEBUG_TRACE (DEBUG::Console1, "VerticalZoomOut\n"); }, this);

}

void
Console1::setup_controls ()
{

	for (uint32_t i = 0; i < 20; ++i) {
		new ControllerButton (this,
				ControllerID (FOCUS1 + i),
				std::function<void (uint32_t)> (std::bind (&Console1::select, this, i)),
				0,
				std::function<void (uint32_t)> (std::bind (&Console1::select_plugin, this, i)));
	}

	new ControllerButton (
			this, ControllerID::PRESET, std::function<void (uint32_t)> (std::bind (&Console1::shift, this, _1)));

	new ControllerButton (this,
			ControllerID::TRACK_GROUP,
			std::function<void (uint32_t)> (std::bind (&Console1::plugin_state, this, _1)));

	new ControllerButton (
			this, ControllerID::DISPLAY_ON, std::function<void (uint32_t)> (std::bind (&Console1::rude_solo, this, _1)));
	new ControllerButton (
			this, ControllerID::MODE, std::function<void (uint32_t)> (std::bind (&Console1::zoom, this, _1)));
	new MultiStateButton (this,
			ControllerID::EXTERNAL_SIDECHAIN,
			std::vector<uint32_t>{ 0, 63, 127 },
			std::function<void (uint32_t)> (std::bind (&Console1::window, this, _1)));

	new ControllerButton (
			this, ControllerID::PAGE_UP, std::function<void (uint32_t)> (std::bind (&Console1::bank, this, true)));
	new ControllerButton (
			this, ControllerID::PAGE_DOWN, std::function<void (uint32_t)> (std::bind (&Console1::bank, this, false)));

	new ControllerButton (this,
			swap_solo_mute ? ControllerID::SOLO : ControllerID::MUTE,
			std::function<void (uint32_t)> (std::bind (&Console1::mute, this, _1)));
	new ControllerButton (this,
			swap_solo_mute ? ControllerID::MUTE : ControllerID::SOLO,
			std::function<void (uint32_t)> (std::bind (&Console1::solo, this, _1)));

	new ControllerButton (
			this, ControllerID::PHASE_INV, std::function<void (uint32_t)> (std::bind (&Console1::phase, this, _1)));

	/* Console 1: Input Gain - Ardour / Mixbus: Trim */
	new Encoder (this, ControllerID::GAIN, std::function<void (uint32_t)> (std::bind (&Console1::trim, this, _1)));

	/* Console 1: Volume - Ardour / Mixbus: Gain */
	new Encoder (
			this, ControllerID::VOLUME, std::function<void (uint32_t)> (std::bind (&Console1::gain, this, _1)));

	new Encoder (this, ControllerID::PAN, std::function<void (uint32_t)> (std::bind (&Console1::pan, this, _1)));

	/* Filter Section*/
	new ControllerButton (this,
			ControllerID::FILTER_TO_COMPRESSORS,
			std::function<void (uint32_t)> (std::bind (&Console1::filter, this, _1)));
	new Encoder (
			this, ControllerID::LOW_CUT, std::function<void (uint32_t)> (std::bind (&Console1::low_cut, this, _1)));
	new Encoder (
			this, ControllerID::HIGH_CUT, std::function<void (uint32_t)> (std::bind (&Console1::high_cut, this, _1)));

	/* Gate Section */
	new ControllerButton (
			this, ControllerID::SHAPE, std::function<void (uint32_t)> (std::bind (&Console1::gate, this, _1)));
	new ControllerButton (this,
			ControllerID::HARD_GATE,
			std::function<void (uint32_t)> (std::bind (&Console1::gate_scf, this, _1)),
			std::function<void (uint32_t)> (std::bind (&Console1::gate_listen, this, _1)));
	new Encoder (this,
			ControllerID::SHAPE_GATE,
			std::function<void (uint32_t)> (std::bind (&Console1::gate_thresh, this, _1)));
	new Encoder (this,
			ControllerID::SHAPE_RELEASE,
			std::function<void (uint32_t)> (std::bind (&Console1::gate_release, this, _1)),
			std::function<void (uint32_t)> (std::bind (&Console1::gate_hyst, this, _1)));
	new Encoder (this,
			ControllerID::SHAPE_SUSTAIN,
			std::function<void (uint32_t)> (std::bind (&Console1::gate_attack, this, _1)),
			std::function<void (uint32_t)> (std::bind (&Console1::gate_hold, this, _1)));
	new Encoder (this,
			ControllerID::SHAPE_PUNCH,
			std::function<void (uint32_t)> (std::bind (&Console1::gate_depth, this, _1)),
			std::function<void (uint32_t)> (std::bind (&Console1::gate_filter_freq, this, _1)));

	new Meter (this, ControllerID::SHAPE_METER, std::function<void ()> ([] () {}));

	/* EQ Section */
	new ControllerButton (
			this, ControllerID::EQ, std::function<void (uint32_t)> (std::bind (&Console1::eq, this, _1)));

	for (uint32_t i = 0; i < 4; ++i) {
		new Encoder (this,
				eq_freq_controller_for_band (i),
				std::function<void (uint32_t)> (std::bind (&Console1::eq_freq, this, i, _1)),
				std::function<void (uint32_t)> (std::bind (&Console1::mb_send_level, this, i, _1)));
		new Encoder (this,
				eq_gain_controller_for_band (i),
				std::function<void (uint32_t)> (std::bind (&Console1::eq_gain, this, i, _1)),
				std::function<void (uint32_t)> (std::bind (&Console1::mb_send_level, this, i + 4, _1)));
	}


	if (band_q_as_send) {
		new Encoder (this,
				ControllerID::LOW_MID_SHAPE,
				std::function<void (uint32_t)> (std::bind (&Console1::mb_send_level, this, 10, _1)),
				std::function<void (uint32_t)> (std::bind (&Console1::mb_send_level, this, 8, _1)));
		new Encoder (this,
				ControllerID::HIGH_MID_SHAPE,
				std::function<void (uint32_t)> (std::bind (&Console1::mb_send_level, this, 11, _1)),
				std::function<void (uint32_t)> (std::bind (&Console1::mb_send_level, this, 9, _1)));
	} else {
		new Encoder (this,
				ControllerID::LOW_MID_SHAPE,
				std::function<void (uint32_t)> (std::bind (&Console1::eq_band_q, this, 1, _1)),
				std::function<void (uint32_t)> (std::bind (&Console1::mb_send_level, this, 8, _1)));
		new Encoder (this,
				ControllerID::HIGH_MID_SHAPE,
				std::function<void (uint32_t)> (std::bind (&Console1::eq_band_q, this, 2, _1)),
				std::function<void (uint32_t)> (std::bind (&Console1::mb_send_level, this, 9, _1)));
	}

	new ControllerButton (this,
			ControllerID::LOW_SHAPE,
			std::function<void (uint32_t)> (std::bind (&Console1::eq_low_shape, this, _1)));
	new ControllerButton (this,
			ControllerID::HIGH_SHAPE,
			std::function<void (uint32_t)> (std::bind (&Console1::eq_high_shape, this, _1)));

	new Encoder (
			this, ControllerID::CHARACTER, std::function<void (uint32_t)> (std::bind (&Console1::drive, this, _1)));

	/* Compressor Section */
	new ControllerButton (
			this, ControllerID::COMP, std::function<void (uint32_t)> (std::bind (&Console1::comp, this, _1)));
	new MultiStateButton (this,
			ControllerID::ORDER,
			std::vector<uint32_t>{ 0, 63, 127 },
			std::function<void (uint32_t)> (std::bind (&Console1::comp_mode, this, _1)));

	new Encoder (this,
			ControllerID::COMP_THRESH,
			std::function<void (uint32_t)> (std::bind (&Console1::comp_thresh, this, _1)));
	new Encoder (this,
			ControllerID::COMP_ATTACK,
			std::function<void (uint32_t)> (std::bind (&Console1::comp_attack, this, _1)));
	new Encoder (this,
			ControllerID::COMP_RELEASE,
			std::function<void (uint32_t)> (std::bind (&Console1::comp_release, this, _1)));
	new Encoder (
			this, ControllerID::COMP_RATIO, std::function<void (uint32_t)> (std::bind (&Console1::comp_ratio, this, _1)));
	new Encoder (
			this, ControllerID::COMP_PAR, std::function<void (uint32_t)> (std::bind (&Console1::comp_makeup, this, _1)));
	new Encoder (
			this, ControllerID::DRIVE, std::function<void (uint32_t)> (std::bind (&Console1::comp_emph, this, _1)));

	new Meter (this, ControllerID::COMP_METER, std::function<void ()> ([] () {}));

	/* Output Section */
	new Meter (this, ControllerID::OUTPUT_METER_L, std::function<void ()> ([] () {}));
	new Meter (this, ControllerID::OUTPUT_METER_R, std::function<void ()> ([] () {}));
}

void
Console1::handle_midi_controller_message (MIDI::Parser&, MIDI::EventTwoBytes* tb)
{
	uint32_t controller_number = static_cast<uint32_t> (tb->controller_number);
	uint32_t value = static_cast<uint32_t> (tb->value);
DEBUG_TRACE (DEBUG::Console1,
	             string_compose ("handle_midi_controller_message cn: '%1' val: '%2'\n", controller_number, value));
	if (midi_assign_mode && controller_number != ControllerID::PRESET) {
		SendControllerNumber (controller_number, shift_state);
		return;
	}
	Encoder* e = nullptr;
	try {
		e = get_encoder (ControllerID (controller_number));
	} catch (ControlNotFoundException const&) {
		DEBUG_TRACE (DEBUG::Console1,
		             string_compose ("handle_midi_controller_message: encoder not found cn: "
		                             "'%1' val: '%2'\n",
		                             controller_number,
		                             value));
	}
	if (e) {
		if (in_plugin_state) {
			if (shift_state && e->plugin_shift_action) {
				e->plugin_shift_action (value);
			} else if (e->plugin_action) {
				e->plugin_action (value);
			}
			return;
		} else if (shift_state && e->shift_action) {
			e->shift_action (value);
		} else {
			e->action (value);
		}
		return;
	}

	ControllerButton* b = nullptr;
	try {
		b = get_button (ControllerID (controller_number));
	} catch (ControlNotFoundException const&) {
		DEBUG_TRACE (DEBUG::Console1,
		             string_compose ("handle_midi_controller_message: button not found cn: "
		                             "'%1' val: '%2'\n",
		                             controller_number,
		                             value));
	}
	if (b) {
		if (in_plugin_state && (b->id () == ControllerID::TRACK_GROUP || b->id () == ControllerID::PRESET)) {
			b->action (value);
			return;
		}
		if (in_plugin_state) {
			if (shift_state && b->plugin_shift_action) {
				b->plugin_shift_action (value);
			} else if (b->plugin_action) {
				b->plugin_action (value);
			}
			return;
		} else if (shift_state && b->shift_action) {
			b->shift_action (value);
		} else {
			b->action (value);
		}
		return;
	}

	MultiStateButton* mb = nullptr;
	try {
		mb = get_mbutton (ControllerID (controller_number));
	} catch (ControlNotFoundException const&) {
		DEBUG_TRACE (DEBUG::Console1,
		             string_compose ("handle_midi_controller_message: mbutton not found cn: "
		                             "'%1' val: '%2'\n",
		                             controller_number,
		                             value));
	}
	if (mb) {
		if (in_plugin_state) {
			if (shift_state && mb->plugin_shift_action) {
				mb->plugin_shift_action (value);
			} else if (mb->plugin_action) {
				mb->plugin_action (value);
			}
			return;
		} else if (shift_state && mb->shift_action) {
			mb->shift_action (value);
		} else {
			mb->action (value);
		}

		return;
	}
}

void
Console1::notify_solo_active_changed (bool state)
{
	DEBUG_TRACE (DEBUG::Console1, "notify_active_solo_changed() \n");
	try {
		get_button (ControllerID::DISPLAY_ON)->set_led_value (state ? 127 : 0);
	} catch (ControlNotFoundException const&) {
		DEBUG_TRACE (DEBUG::Console1, "button not found");
	}
}

void
Console1::band_q_usage_changed( )
{
    Encoder *e = get_encoder (ControllerID (EQ_BandQ) );
	DEBUG_TRACE (DEBUG::Console1, string_compose ("notify_parameter_changed: %1\n", e->id()));
}

void
Console1::notify_parameter_changed (std::string s)
{
	DEBUG_TRACE (DEBUG::Console1, string_compose ("notify_parameter_changed: %1\n", s));
}

void
Console1::notify_transport_state_changed ()
{
	DEBUG_TRACE (DEBUG::Console1, "transport_state_changed() \n");
	rolling = session->transport_state_rolling ();
}

void
Console1::stripable_selection_changed ()
{
	DEBUG_TRACE (DEBUG::Console1, "stripable_selection_changed \n");
	if (!_in_use)
		return;

	std::shared_ptr<Stripable> r = ControlProtocol::first_selected_stripable ();
	if (r && r != _current_stripable) {
		set_current_stripable (r);
	}
}

void
Console1::drop_current_stripable ()
{
	DEBUG_TRACE (DEBUG::Console1, "drop_current_stripable \n");
	if (_current_stripable) {
		if (_current_stripable == session->monitor_out ()) {
			set_current_stripable (session->master_out ());
		} else {
			set_current_stripable (_current_stripable);
		}
	} else {
		set_current_stripable (std::shared_ptr<Stripable> ());
	}
}

void
Console1::set_current_stripable (std::shared_ptr<Stripable> r)
{
	DEBUG_TRACE (DEBUG::Console1, "set_current_stripable \n");
	stripable_connections.drop_connections ();
	gate_connections.drop_connections ();
	eq_connections.drop_connections ();
	comp_connections.drop_connections ();

	_current_stripable = r;

	if (_current_stripable) {
		DEBUG_TRACE (DEBUG::Console1, "current_stripable found:  \n");

		r->MappedControlsChanged.connect (stripable_connections, MISSING_INVALIDATOR, std::bind (&Console1::mapped_controls_changed, this), this);

		current_plugin_index = -1;
		plugin_meters.clear ();
		last_output_meter_l = last_output_meter_r = last_gate_meter = last_comp_redux = 0xffffffff;

		PresentationInfo pi = _current_stripable->presentation_info ();

		DEBUG_TRACE (DEBUG::Console1, string_compose ("current_stripable %1 - %2\n", pi.order (), pi.flags ()));

		gate_redux_meter = _current_stripable->mapped_output (Gate_Redux);
		comp_redux_meter = _current_stripable->mapped_output (Comp_Redux);

		/*
		Support all types of pan controls / find first available control
		*/
		if (_current_stripable->pan_azimuth_control ())
			current_pan_control = _current_stripable->pan_azimuth_control ();
		else if (_current_stripable->pan_elevation_control ())
			current_pan_control = _current_stripable->pan_azimuth_control ();
		else if (_current_stripable->pan_width_control ())
			current_pan_control = _current_stripable->pan_width_control ();
		else if (_current_stripable->pan_frontback_control ())
			current_pan_control = _current_stripable->pan_frontback_control ();
		else if (_current_stripable->pan_lfe_control ())
			current_pan_control = _current_stripable->pan_lfe_control ();
		else
			current_pan_control = nullptr;

		std::shared_ptr<AutomationControl> pan_control = current_pan_control;
		if (pan_control)
			pan_control->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, std::bind (&Console1::map_pan, this), this);

		_current_stripable->DropReferences.connect (
		  stripable_connections, MISSING_INVALIDATOR, std::bind (&Console1::drop_current_stripable, this), this);

		if (_current_stripable->mute_control ()) {
			_current_stripable->mute_control ()->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, std::bind (&Console1::map_mute, this), this);
		}

		if (_current_stripable->solo_control ()) {
			_current_stripable->solo_control ()->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, std::bind (&Console1::map_solo, this), this);
		}

		if (_current_stripable->phase_control ()) {
			_current_stripable->phase_control ()->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, std::bind (&Console1::map_phase, this), this);
		}

		// Rec Enabled
		std::shared_ptr<Track> t = std::dynamic_pointer_cast<Track> (_current_stripable);
		if (t) {
			t->rec_enable_control ()->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, std::bind (&Console1::map_recenable, this), this);
		}

		// Monitor
		if (_current_stripable->monitoring_control ()) {
			_current_stripable->monitoring_control ()->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, std::bind (&Console1::map_monitoring, this), this);
		}

		// Trim
		std::shared_ptr<AutomationControl> trim_control = _current_stripable->trim_control ();
		if (trim_control) {
			trim_control->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, std::bind (&Console1::map_trim, this), this);
		}
		// Gain
		std::shared_ptr<AutomationControl> gain_control = _current_stripable->gain_control ();
		if (gain_control) {
			gain_control->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, std::bind (&Console1::map_gain, this), this);

			// control->alist()->automation_state_changed.connect
			// (stripable_connections, MISSING_INVALIDATOR, std::bind
			// (&Console1::map_auto, this), this);
		}

		// Filter Section
		if (_current_stripable->mapped_control (HPF_Enable)) {
			_current_stripable->mapped_control (HPF_Enable)->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, std::bind (&Console1::map_filter, this), this);
		}

		if (_current_stripable->mapped_control (HPF_Freq)) {
			_current_stripable->mapped_control (HPF_Freq)->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, std::bind (&Console1::map_low_cut, this), this);
		}

		if (_current_stripable->mapped_control (LPF_Freq)) {
			_current_stripable->mapped_control (LPF_Freq)->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, std::bind (&Console1::map_high_cut, this), this);
		}

		// Gate, EQ and Comp can switch their type, so their listeners are managed separately
		connect_gate_controls ();
		connect_eq_controls ();
		connect_comp_controls ();

		// Drive
		if (_current_stripable->mapped_control (TapeDrive_Drive)) {
			_current_stripable->mapped_control (TapeDrive_Drive)->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, std::bind (&Console1::map_drive, this), this);
		}

		// Mixbus Sends
		for (uint32_t i = 0; i < (band_q_as_send ? 12 : 10); ++i) {
			if (_current_stripable->send_level_controllable (i)) {
				_current_stripable->send_level_controllable (i)->Changed.connect (
				  stripable_connections,
				  MISSING_INVALIDATOR,
				  std::bind (&Console1::map_mb_send_level, this, i),
				  this);
			}
		}

		uint32_t index = get_index_by_inventory_order (pi.order ());
		current_strippable_index = index % bank_size;
		uint32_t bank = index / bank_size;
		if (bank != current_bank) {
			current_bank = bank;
			BankChange ();
		}
		DEBUG_TRACE (
		  DEBUG::Console1,
		  string_compose (
		    "current_stripable: rid %1, bank %2, index %3 \n", index, current_bank, current_strippable_index));

	} else {
		gate_redux_meter = 0;
		comp_redux_meter = 0;
	}

	// ToDo: subscribe to the fader automation modes so we can light the LEDs
	// map_shift() also maps the complete stripable state
	map_shift (shift_state);
	LabelsChanged ();
}

void
Console1::connect_gate_controls ()
{
	gate_connections.drop_connections ();
	if (!_current_stripable) {
		return;
	}
	auto hook = [this] (std::shared_ptr<AutomationControl> c, std::function<void ()> f) {
		if (c) {
			c->Changed.connect (gate_connections, MISSING_INVALIDATOR, [f] (bool, PBD::Controllable::GroupControlDisposition) { f (); }, this);
		}
	};
	std::shared_ptr<Stripable> s = _current_stripable;

	hook (s->mapped_control (Gate_Enable), std::bind (&Console1::map_gate, this));
	hook (s->mapped_control (Gate_KeyFilterEnable), std::bind (&Console1::map_gate_scf, this));
	hook (s->mapped_control (Gate_FastAttack), std::bind (&Console1::map_gate_scf, this));
	hook (s->mapped_control (Gate_Mode), std::bind (&Console1::map_gate_listen, this));
	hook (s->mapped_control (Gate_KeyListen), std::bind (&Console1::map_gate_listen, this));
	hook (s->mapped_control (Gate_Threshold), std::bind (&Console1::map_gate_thresh, this));
	hook (s->mapped_control (Gate_Depth), std::bind (&Console1::map_gate_depth, this));
	hook (s->mapped_control (Gate_Release), std::bind (&Console1::map_gate_release, this));
	hook (s->mapped_control (Gate_Attack), std::bind (&Console1::map_gate_attack, this));
	hook (s->mapped_control (Gate_Hysteresis), std::bind (&Console1::map_gate_hyst, this));
	hook (s->mapped_control (Gate_Hold), std::bind (&Console1::map_gate_hold, this));
	hook (s->mapped_control (Gate_Hold), std::bind (&Console1::map_gate_attack, this));
	hook (s->mapped_control (Gate_KeyFilterFreq), std::bind (&Console1::map_gate_filter_freq, this));
}

void
Console1::connect_eq_controls ()
{
	eq_connections.drop_connections ();
	if (!_current_stripable) {
		return;
	}
	auto hook = [this] (std::shared_ptr<AutomationControl> c, std::function<void ()> f) {
		if (c) {
			c->Changed.connect (eq_connections, MISSING_INVALIDATOR, [f] (bool, PBD::Controllable::GroupControlDisposition) { f (); }, this);
		}
	};
	std::shared_ptr<Stripable> s = _current_stripable;

	hook (s->mapped_control (EQ_Enable), std::bind (&Console1::map_eq, this));
	for (uint32_t i = 0; i < s->eq_band_cnt (); ++i) {
		hook (s->mapped_control (EQ_BandFreq, i), std::bind (&Console1::map_eq_freq, this, i));
		hook (s->mapped_control (EQ_BandGain, i), std::bind (&Console1::map_eq_gain, this, i));
	}
	if (!band_q_as_send) {
		hook (s->mapped_control (EQ_BandQ, 1), std::bind (&Console1::map_eq_band_q, this, 1));
		hook (s->mapped_control (EQ_BandQ, 2), std::bind (&Console1::map_eq_band_q, this, 2));
	}
	hook (s->mapped_control (EQ_BandShape, 0), std::bind (&Console1::map_eq_low_shape, this));
	hook (s->mapped_control (EQ_BandShape, 3), std::bind (&Console1::map_eq_high_shape, this));
}

void
Console1::connect_comp_controls ()
{
	comp_connections.drop_connections ();
	if (!_current_stripable) {
		return;
	}
	auto hook = [this] (std::shared_ptr<AutomationControl> c, std::function<void ()> f) {
		if (c) {
			c->Changed.connect (comp_connections, MISSING_INVALIDATOR, [f] (bool, PBD::Controllable::GroupControlDisposition) { f (); }, this);
		}
	};
	std::shared_ptr<Stripable> s = _current_stripable;

	hook (s->mapped_control (Comp_Enable), std::bind (&Console1::map_comp, this));
	hook (s->mapped_control (Comp_Mode), std::bind (&Console1::map_comp_mode, this));
	hook (s->mapped_control (Comp_Threshold), std::bind (&Console1::map_comp_thresh, this));
	hook (s->mapped_control (Comp_Attack), std::bind (&Console1::map_comp_attack, this));
	hook (s->mapped_control (Comp_FastAttack), std::bind (&Console1::map_comp_attack, this));
	hook (s->mapped_control (Comp_RMSPeak), std::bind (&Console1::map_comp_attack, this));
	hook (s->mapped_control (Comp_Release), std::bind (&Console1::map_comp_release, this));
	hook (s->mapped_control (Comp_Ratio), std::bind (&Console1::map_comp_ratio, this));
	hook (s->mapped_control (Comp_Makeup), std::bind (&Console1::map_comp_makeup, this));
	hook (s->mapped_control (Comp_Mix), std::bind (&Console1::map_comp_makeup, this));
	hook (s->mapped_control (Comp_KeyFilterFreq), std::bind (&Console1::map_comp_emph, this));
}

void
Console1::mapped_controls_changed ()
{
	/* A gate, EQ or compressor type switch replaces the mapped controls of
	 * that section only, so just those listeners and mappings are rebuilt. */
	if (!_current_stripable) {
		return;
	}
	DEBUG_TRACE (DEBUG::Console1, "mapped_controls_changed\n");
#ifdef MIXBUS
	if (std::shared_ptr<ARDOUR::Route> rt = std::dynamic_pointer_cast<ARDOUR::Route> (_current_stripable)) {
		DEBUG_TRACE (DEBUG::Console1, string_compose ("Remapping Gate (%1), EQ (%2) and Comp (%3) after type change\n",
		             rt->mixbus_gate_type () == ARDOUR::Route::GateSSL ? "SSL" : "Harrison",
		             rt->mixbus_eq_type () == ARDOUR::Route::EQType9kJ ? "SSL" : "Harrison",
		             rt->mixbus_comp_type () == ARDOUR::Route::CompSSL ? "SSL" : "Harrison"));
	}
#endif

	gate_redux_meter = _current_stripable->mapped_output (Gate_Redux);
	comp_redux_meter = _current_stripable->mapped_output (Comp_Redux);

	connect_gate_controls ();
	connect_eq_controls ();
	connect_comp_controls ();

	if (!in_plugin_state) {
		map_gate_section ();
		map_eq_section ();
		map_comp_section ();
	}
	LabelsChanged ();
}

void
Console1::map_gate_section ()
{
	map_gate ();
	map_gate_scf ();
	map_gate_listen ();
	map_gate_thresh ();
	map_gate_attack ();
	map_gate_release ();
	map_gate_depth ();
	map_gate_hyst ();
	map_gate_hold ();
	map_gate_filter_freq ();
}

void
Console1::map_eq_section ()
{
	map_eq ();
	map_eq_mode ();
	for (uint32_t i = 0; i < _current_stripable->eq_band_cnt (); ++i) {
		map_eq_freq (i);
		map_eq_gain (i);
		if ((!switch_eq_q_dials) && i > 0 && i < 3) {
			map_eq_band_q (i);
		}
	}
	map_eq_low_shape ();
	map_eq_high_shape ();
}

void
Console1::map_comp_section ()
{
	map_comp ();
	map_comp_mode ();
	map_comp_thresh ();
	map_comp_attack ();
	map_comp_release ();
	map_comp_ratio ();
	map_comp_makeup ();
	map_comp_emph ();
}

void
Console1::map_stripable_state ()
{
	if (!_current_stripable) {
		stop_blinking (MUTE);
		stop_blinking (SOLO);
		stop_blinking (PHASE_INV);
	} else {
		map_select ();

		map_bank ();
		map_gain ();
		map_pan ();
		map_phase ();
		map_recenable ();
		map_solo ();
		map_trim ();

		// Filter Section
		map_filter ();
		map_low_cut ();
		map_high_cut ();

		// Gate Section
		map_gate_section ();

		// EQ Section
		map_eq_section ();

		for (int i = 0; i < 12; ++i) {
			map_mb_send_level (i);
		}

		// Drive
		map_drive ();

		// Comp Section
		map_comp_section ();

		if (_current_stripable == session->monitor_out ()) {
			// map_cut();
		} else {
			map_mute ();
		}
	}
}

void
Console1::stop_blinking (ControllerID id)
{
	try {
		blinkers.remove (id);
		get_button (id)->set_led_state (false);
	} catch (ControlNotFoundException const&) {
		DEBUG_TRACE (DEBUG::Console1, "Button to stop blinking not found ...\n");
	}
}

void
Console1::start_blinking (ControllerID id)
{
	try {
		blinkers.push_back (id);
		get_button (id)->set_led_state (true);
	} catch (ControlNotFoundException const&) {
		DEBUG_TRACE (DEBUG::Console1, "Button to start blinking not found ...\n");
	}
}

bool
Console1::blinker ()
{
	blink_state = !blink_state;

	for (Blinkers::iterator b = blinkers.begin (); b != blinkers.end (); b++) {
		try {
			get_button (*b)->set_led_state (blink_state);
		} catch (ControlNotFoundException const&) {
			DEBUG_TRACE (DEBUG::Console1, "Blinking Button not found ...\n");
		}
	}

	return true;
}

ControllerButton*
Console1::get_button (ControllerID id) const
{
	ButtonMap::const_iterator b = buttons.find (id);
	if (b == buttons.end ())
		throw (ControlNotFoundException ());
	return const_cast<ControllerButton*> (b->second);
}

Meter*
Console1::get_meter (ControllerID id) const
{
	MeterMap::const_iterator m = meters.find (id);
	if (m == meters.end ())
		throw (ControlNotFoundException ());
	return const_cast<Meter*> (m->second);
}

Encoder*
Console1::get_encoder (ControllerID id) const
{
	EncoderMap::const_iterator m = encoders.find (id);
	if (m == encoders.end ())
		throw (ControlNotFoundException ());
	return const_cast<Encoder*> (m->second);
}

MultiStateButton*
Console1::get_mbutton (ControllerID id) const
{
	MultiStateButtonMap::const_iterator m = multi_buttons.find (id);
	if (m == multi_buttons.end ())
		throw (ControlNotFoundException ());
	return const_cast<MultiStateButton*> (m->second);
}

ControllerID
Console1::get_send_controllerid (uint32_t n)
{
	SendControllerMap::const_iterator s = send_controllers.find (n);
	if (s != send_controllers.end ())
		return s->second;
	else
		return CONTROLLER_NONE;
}

bool
Console1::periodic ()
{
	periodic_update_meter ();
	update_plugin_meters ();
	return true;
}

void
Console1::periodic_update_meter ()
{
	if (_current_stripable) {
		bool show = (rolling || !strip_recenabled || (monitor_state & ARDOUR::MonitorState::MonitoringInput));
		if (_current_stripable->peak_meter ()) {
			uint32_t val_l, val_r;
			if (!show) {
				val_l = val_r = 0;
			} else {
				uint32_t chan_count = _current_stripable->peak_meter ()->input_streams ().n_total ();
				float dB = _current_stripable->peak_meter ()->meter_level (0, MeterMCP);
				val_l = val_r = calculate_meter (dB);
				if (chan_count > 1) {
					dB = _current_stripable->peak_meter ()->meter_level (1, MeterMCP);
					val_r = calculate_meter (dB);
				}
			}
			try {
				if (val_l != last_output_meter_l && !plugin_meter_active (OUTPUT_METER_L)) {
					get_meter (OUTPUT_METER_L)->set_value (val_l);
					last_output_meter_l = val_l;
				}
				if (val_r != last_output_meter_r && !plugin_meter_active (OUTPUT_METER_R)) {
					get_meter (OUTPUT_METER_R)->set_value (val_r);
					last_output_meter_r = val_r;
				}
			} catch (ControlNotFoundException const&) {
				DEBUG_TRACE (DEBUG::Console1, "Meter not found ...\n");
			}
		}
		if (gate_redux_meter) {
			uint32_t val;
			if (!show) {
				val = 127;
			} else {
				float dB = gate_redux_meter->get_parameter ();
				val = 127 * dB;
			}
			try {
				if (val != last_gate_meter && !plugin_meter_active (SHAPE_METER)) {
					get_meter (SHAPE_METER)->set_value (val);
					last_gate_meter = val;
				}
			} catch (ControlNotFoundException const&) {
				DEBUG_TRACE (DEBUG::Console1, "Meter not found ...\n");
			}
		}
		if (comp_redux_meter) {
			uint32_t val;
			if (!show) {
				val = 127;
			} else {
				float rx = comp_redux_meter->get_parameter () * 127.f;
				val = static_cast<uint32_t> (std::min (127.f, std::max (0.f, rx)));
			}
			float smoothed = val * 0.6f + smoothed_comp_redux * 0.4f;
			smoothed_comp_redux = smoothed;
			uint32_t output = static_cast<uint32_t> (std::lround (smoothed));
			try {
				if (output != last_comp_redux && !plugin_meter_active (COMP_METER)) {
					last_comp_redux = output;
					get_meter (COMP_METER)->set_value (output);
				}
			} catch (ControlNotFoundException const&) {
				DEBUG_TRACE (DEBUG::Console1, "Meter not found ...\n");
			}
		}
	}
}

bool
Console1::plugin_meter_active (ControllerID id) const
{
	if (!in_plugin_state) {
		return false;
	}
	for (const auto& pm : plugin_meters) {
		if (pm.id == id) {
			return true;
		}
	}
	return false;
}

void
Console1::update_plugin_meters ()
{
	if (!in_plugin_state) {
		return;
	}
	for (auto& pm : plugin_meters) {
		double v = pm.descriptor.to_interface (pm.control->get_parameter (), true);
		v = std::min (1.0, std::max (0.0, v));
		/* the reduction meters rest at full scale */
		if (pm.id == COMP_METER || pm.id == SHAPE_METER) {
			v = 1.0 - v;
		}
		uint32_t val = static_cast<uint32_t> (std::lround (v * 127.0));
		if (val == pm.last_value) {
			continue;
		}
		pm.last_value = val;
		try {
			get_meter (pm.id)->set_value (val);
		} catch (ControlNotFoundException const&) {
		}
	}
}

float
Console1::calculate_meter (float dB)
{
	if (!std::isfinite (dB)) {
		return dB > 0.f ? 127.f : 0.f;
	}
	float level = 8.7f + 0.18f * dB;
	if (level <= 0.f) {
		return 0.f;
	}
	return std::min (127.f, std::pow (level, 2.1f));
}

uint32_t
Console1::control_to_midi (Controllable controllable, float val, uint32_t max_value_for_type)
{
	if (!controllable) {
		return 0;
	}

	if (controllable->is_gain_like ()) {
		return controllable->internal_to_interface (val) * max_value_for_type;
	}

	float control_min = controllable->lower ();
	float control_max = controllable->upper ();
	float control_range = control_max - control_min;

	if (controllable->is_toggle ()) {
		if (val >= (control_min + (control_range / 2.0f))) {
			return max_value_for_type;
		} else {
			return 0;
		}
	} else {
		std::shared_ptr<AutomationControl> actl = std::dynamic_pointer_cast<AutomationControl> (controllable);
		if (actl) {
			control_min = actl->internal_to_interface (control_min);
			control_max = actl->internal_to_interface (control_max);
			control_range = control_max - control_min;
			val = actl->internal_to_interface (val);
		}
	}
	// fiddle value of max so value doesn't jump from 125 to 127 for 1.0
	// otherwise decrement won't work.
	return (val - control_min) / control_range * (max_value_for_type - 1);
}

float
Console1::midi_to_control (Controllable controllable, uint32_t val, uint32_t max_value_for_type)
{
	if (!controllable) {
		return 0;
	}
	/* fiddle with MIDI value so that we get an odd number of integer steps
	        and can thus represent "middle" precisely as 0.5. this maps to
	        the range 0..+1.0 (0 to 126)
	*/

	float fv = (val == 0 ? 0 : float (val - 1) / (max_value_for_type - 1));

	if (controllable->is_gain_like ()) {
		return controllable->interface_to_internal (fv);
	}
	DEBUG_TRACE (DEBUG::GenericMidi, string_compose ("Raw value %1 float %2\n", val, fv));

	float control_min = controllable->lower ();
	float control_max = controllable->upper ();
	float control_range = control_max - control_min;
	DEBUG_TRACE (DEBUG::GenericMidi,
	             string_compose ("Min %1 Max %2 Range %3\n", control_min, control_max, control_range));

	std::shared_ptr<AutomationControl> actl = std::dynamic_pointer_cast<AutomationControl> (controllable);
	if (actl) {
		if (fv == 0.f)
			return control_min;
		if (fv == 1.f)
			return control_max;
		control_min = actl->internal_to_interface (control_min);
		control_max = actl->internal_to_interface (control_max);
		control_range = control_max - control_min;
		return actl->interface_to_internal ((fv * control_range) + control_min);
	}
	return (fv * control_range) + control_min;
}

void
Console1::create_strip_inventory ()
{
	DEBUG_TRACE (DEBUG::Console1, "create_strip_inventory()\n");
	std::optional<order_t> master_order;
	strip_inventory.clear ();
	StripableList sl = session->get_stripables ();
	uint32_t index = 0;
	for (const auto& s : sl) {
		PresentationInfo pi = s->presentation_info ();
		DEBUG_TRACE (DEBUG::Console1, string_compose ("%1: ", s->name ()));
		if (pi.flags () & ARDOUR::PresentationInfo::Hidden) {
			DEBUG_TRACE (DEBUG::Console1, string_compose ("strip hidden: index %1, order %2\n", index, pi.order ()));
			continue;
		}
		if (pi.flags () & ARDOUR::PresentationInfo::MasterOut) {
			master_order = pi.order ();
			DEBUG_TRACE (DEBUG::Console1,
			             string_compose ("master strip found at index %1, order %2\n", index, pi.order ()));
			continue;
		}
		if (pi.flags () & ARDOUR::PresentationInfo::MonitorOut) {
			DEBUG_TRACE (DEBUG::Console1,
			             string_compose ("monitor strip found at index %1, order %2 - ignoring\n", index, pi.order ()));
			continue;
		}
		if (pi.flags () & ARDOUR::PresentationInfo::FoldbackBus) {
			DEBUG_TRACE (DEBUG::Console1,
			             string_compose ("foldback bus found at index %1, order %2\n", index, pi.order ()));
			continue;
		}
		strip_inventory.insert (std::make_pair (index, pi.order ()));
		DEBUG_TRACE (DEBUG::Console1, string_compose ("insert strip at index %1, order %2\n", index, pi.order ()));
		++index;
	}
	if (master_order) {
		master_index = index;
		strip_inventory.insert (std::make_pair (index, master_order.value ()));
	}
	max_strip_index = index;
	DEBUG_TRACE (DEBUG::Console1,
	             string_compose ("create_strip_inventory - inventory size %1\n", strip_inventory.size ()));
}

order_t
Console1::get_inventory_order_by_index (uint32_t index)
{
	StripInventoryMap::const_iterator s = strip_inventory.find (index);
	if (s == strip_inventory.end ())
		throw (ControlNotFoundException ());
	return s->second;
}

uint32_t
Console1::get_index_by_inventory_order (order_t order)
{
	for (std::pair<uint32_t, order_t> i : strip_inventory) {
		if (i.second == order) {
			return i.first;
		}
	}
	return 0;
}

void
Console1::select_rid_by_index (uint32_t index)
{
	bool success = true;
	DEBUG_TRACE (DEBUG::Console1, "select_rid_by_index()\n");
	int offset = session->monitor_out () ? 1 : 0;
	DEBUG_TRACE (DEBUG::Console1, string_compose ("offset %1\n", offset));
	uint_fast32_t rid = 0;
#ifdef MIXBUS
	rid = index + offset;
#else
	if (index == master_index) {
		rid = 1;
	} else {
		rid = index + 1 + offset;
	}
#endif
	DEBUG_TRACE (DEBUG::Console1, string_compose ("rid %1\n", rid));
	if (rid > ( max_strip_index + 1 + offset )) {
		success =  false;
	}
	std::shared_ptr<Stripable> s = session->get_remote_nth_stripable (rid, PresentationInfo::MixerStripables);
	if (s && s == _current_stripable) {
		/* no selection change will be signalled, but the button toggled its own LED */
		map_select ();
		return;
	}
	if (s) {
		session->selection ().select_stripable_and_maybe_group (s, SelectionSet, true, false, 0);
	} else {
		success = false;
	}
	if (!success) {
		map_select ();
	}
}

void
Console1::master_monitor_has_changed ()
{
	DEBUG_TRACE (DEBUG::Console1, "master_monitor_has_changed()\n");
	bool monitor_active = session->monitor_active ();
	DEBUG_TRACE (DEBUG::Console1, string_compose ("master_monitor_has_changed - monitor active %1\n", monitor_active));
	create_strip_inventory ();
}
