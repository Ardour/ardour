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


#include <glibmm-2.4/glibmm/main.h>
#include <boost/optional.hpp>

#include "pbd/abstract_ui.cc" // instantiate template
#include "pbd/controllable.h"
#include "pbd/i18n.h"

#include "ardour/audioengine.h"
#include "ardour/debug.h"
#include "ardour/filesystem_paths.h"
#include "ardour/meter.h"
#include "ardour/monitor_control.h"
#include "ardour/phase_control.h"
#include "ardour/readonly_control.h"
#include "ardour/session.h"
#include "ardour/stripable.h"
#include "ardour/track.h"
#include "ardour/vca_manager.h"

#include "console1.h"
#include "c1_control.h"
#include "c1_gui.h"

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
	port_setup ();
}

Console1::~Console1 ()
{
	all_lights_out ();

	MIDISurface::drop ();

	tear_down_gui ();

	/* stop event loop */
	DEBUG_TRACE (DEBUG::Console1, "BaseUI::quit ()\n");

	BaseUI::quit ();
}

void
Console1::all_lights_out ()
{
	for (ButtonMap::iterator b = buttons.begin (); b != buttons.end (); ++b) {
		b->second.set_led_state (false);
	}
}

int
Console1::set_active (bool yn)
{
	DEBUG_TRACE (DEBUG::Console1, string_compose ("Console1::set_active init with yn: '%1'\n", yn));

	if (yn == active ()) {
		return 0;
	}

	if (yn) {

		/* start event loop */

		DEBUG_TRACE (DEBUG::Console1, "Console1::set_active\n");

		BaseUI::run ();

		connect_session_signals ();

	} else {
		/* Control Protocol Manager never calls us with false, but
		 * insteads destroys us.
		 */
	}

	ControlProtocol::set_active (yn);

	DEBUG_TRACE (DEBUG::Console1, string_compose ("Console1::set_active done with yn: '%1'\n", yn));

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

int
Console1::begin_using_device ()
{
	DEBUG_TRACE (DEBUG::Console1, "sending device inquiry message...\n");

	if (MIDISurface::begin_using_device ()) {
		return -1;
	}
	/*
	  with this sysex command we can enter the 'native mode'
	  But there's no need to do so
	  f0 7d 20 00 00 00 01 00 7f 49 6f 6c 73 00 f7
	*/

	load_mappings ();
	setup_controls ();

	/*
	Connection to the blink-timer
	*/
	Glib::RefPtr<Glib::TimeoutSource> blink_timeout = Glib::TimeoutSource::create (200); // milliseconds
	blink_connection = blink_timeout->connect (sigc::mem_fun (*this, &Console1::blinker));
	blink_timeout->attach (main_loop ()->get_context ());

	/* Connection to the peridic timer for meters */
	Glib::RefPtr<Glib::TimeoutSource> periodic_timer = Glib::TimeoutSource::create (100);
	periodic_connection = periodic_timer->connect (sigc::mem_fun (*this, &Console1::periodic));
	periodic_timer->attach (main_loop ()->get_context ());

	DEBUG_TRACE (DEBUG::Console1, "************** begin_using_device() ********************\n");
	create_strip_invetory ();
	connect_internal_signals ();
	map_shift (false);
	return 0;
}

void
Console1::connect_session_signals ()
{
	DEBUG_TRACE (DEBUG::Console1, "connect_session_signals\n");
	// receive routes added
	session->RouteAdded.connect (
	  session_connections, MISSING_INVALIDATOR, boost::bind (&Console1::create_strip_invetory, this), this);
	// receive VCAs added
	session->vca_manager ().VCAAdded.connect (
	  session_connections, MISSING_INVALIDATOR, boost::bind (&Console1::create_strip_invetory, this), this);

	// receive record state toggled
	// session->RecordStateChanged.connect(session_connections,
	// MISSING_INVALIDATOR, boost::bind
	// (&MIDISurface::notify_record_state_changed, this), this); receive
	// transport
	session->TransportStateChange.connect (
	  session_connections, MISSING_INVALIDATOR, boost::bind (&Console1::notify_transport_state_changed, this), this);
	// session->TransportLooped.connect (session_connections,
	// MISSING_INVALIDATOR, boost::bind
	// (&MIDISurface::notify_loop_state_changed, this), this); receive punch-in
	// and punch-out
	Config->ParameterChanged.connect (
	  session_connections, MISSING_INVALIDATOR, boost::bind (&Console1::notify_parameter_changed, this, _1), this);
	session->config.ParameterChanged.connect (
	  session_connections, MISSING_INVALIDATOR, boost::bind (&Console1::notify_parameter_changed, this, _1), this);
	// receive rude solo changed
	session->SoloActive.connect (
	  session_connections, MISSING_INVALIDATOR, boost::bind (&Console1::notify_solo_active_changed, this, _1), this);
	// window.signal_window_state_event().connect (sigc::bind (sigc::mem_fun (*this,
	// &ARDOUR_UI::tabbed_window_state_event_handler), owner));
}

void
Console1::connect_internal_signals ()
{
	BankChange.connect (console1_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_bank, this), this);
	ShiftChange.connect (console1_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_shift, this, _1), this);
	PluginStateChange.connect (
	  console1_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_plugin_state, this, _1), this);
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
		ControllerButton track_select_button (
		  *this,
		  ControllerID (FOCUS1 + i),
		  boost::function<void (uint32_t)> (boost::bind (&Console1::select, this, i)),
		  0,
		  boost::function<void (uint32_t)> (boost::bind (&Console1::select_plugin, this, i)));
	}

	ControllerButton shift_button (
	  *this, ControllerID::PRESET, boost::function<void (uint32_t)> (boost::bind (&Console1::shift, this, _1)));

	ControllerButton plugin_state_button (
	  *this,
	  ControllerID::TRACK_GROUP,
	  boost::function<void (uint32_t)> (boost::bind (&Console1::plugin_state, this, _1)));

	ControllerButton rude_solo (
	  *this, ControllerID::DISPLAY_ON, boost::function<void (uint32_t)> (boost::bind (&Console1::rude_solo, this, _1)));
	ControllerButton zoom_button (
	  *this, ControllerID::MODE, boost::function<void (uint32_t)> (boost::bind (&Console1::zoom, this, _1)));
	MultiStateButton view (*this,
	                       ControllerID::EXTERNAL_SIDECHAIN,
	                       std::vector<uint32_t>{ 0, 63, 127 },
	                       boost::function<void (uint32_t)> (boost::bind (&Console1::window, this, _1)));

	ControllerButton bank_up_button (
	  *this, ControllerID::PAGE_UP, boost::function<void (uint32_t)> (boost::bind (&Console1::bank, this, true)));
	ControllerButton bank_down_button (
	  *this, ControllerID::PAGE_DOWN, boost::function<void (uint32_t)> (boost::bind (&Console1::bank, this, false)));

	ControllerButton mute_button (
	  *this, ControllerID::MUTE, boost::function<void (uint32_t)> (boost::bind (&Console1::mute, this, _1)));
	ControllerButton solo_button (
	  *this, ControllerID::SOLO, boost::function<void (uint32_t)> (boost::bind (&Console1::solo, this, _1)));
	ControllerButton phase_button (
	  *this, ControllerID::PHASE_INV, boost::function<void (uint32_t)> (boost::bind (&Console1::phase, this, _1)));

	/*
	Console 1: Input Gain - Ardour / Mixbus: Trim
	*/
	Encoder trim_encoder (
	  *this, ControllerID::GAIN, boost::function<void (uint32_t)> (boost::bind (&Console1::trim, this, _1)));

	/*
	Console 1: Volume - Ardour / Mixbus: Gain
	*/
	Encoder gain_encoder (
	  *this, ControllerID::VOLUME, boost::function<void (uint32_t)> (boost::bind (&Console1::gain, this, _1)));

	Encoder pan_encoder (
	  *this, ControllerID::PAN, boost::function<void (uint32_t)> (boost::bind (&Console1::pan, this, _1)));

	/* Filter Section*/
	ControllerButton filter_button (*this,
	                                ControllerID::FILTER_TO_COMPRESSORS,
	                                boost::function<void (uint32_t)> (boost::bind (&Console1::filter, this, _1)));
	Encoder low_cut_encoder (
	  *this, ControllerID::LOW_CUT, boost::function<void (uint32_t)> (boost::bind (&Console1::low_cut, this, _1)));
	Encoder high_cut_encoder (
	  *this, ControllerID::HIGH_CUT, boost::function<void (uint32_t)> (boost::bind (&Console1::high_cut, this, _1)));

	/* Gate Section */
	ControllerButton gate_on_off (
	  *this, ControllerID::SHAPE, boost::function<void (uint32_t)> (boost::bind (&Console1::gate, this, _1)));
	ControllerButton gate_scf_listen (
	  *this,
	  ControllerID::HARD_GATE,
	  boost::function<void (uint32_t)> (boost::bind (&Console1::gate_scf, this, _1)),
	  boost::function<void (uint32_t)> (boost::bind (&Console1::gate_listen, this, _1)));
	Encoder gate_thresh_encoder (*this,
	                             ControllerID::SHAPE_GATE,
	                             boost::function<void (uint32_t)> (boost::bind (&Console1::gate_thresh, this, _1)));
	Encoder gate_release_encoder (*this,
	                              ControllerID::SHAPE_RELEASE,
	                              boost::function<void (uint32_t)> (boost::bind (&Console1::gate_release, this, _1)),
	                              boost::function<void (uint32_t)> (boost::bind (&Console1::gate_hyst, this, _1)));
	Encoder gate_attack_encoder (*this,
	                             ControllerID::SHAPE_SUSTAIN,
	                             boost::function<void (uint32_t)> (boost::bind (&Console1::gate_attack, this, _1)),
	                             boost::function<void (uint32_t)> (boost::bind (&Console1::gate_hold, this, _1)));
	Encoder gate_depth_encoder (*this,
	                            ControllerID::SHAPE_PUNCH,
	                            boost::function<void (uint32_t)> (boost::bind (&Console1::gate_depth, this, _1)),
	                            boost::function<void (uint32_t)> (boost::bind (&Console1::gate_filter_freq, this, _1)));

	Meter gate_meter (*this, ControllerID::SHAPE_METER, boost::function<void ()> ([] () {}));

	/* EQ Section */
	ControllerButton eq_on_off (
	  *this, ControllerID::EQ, boost::function<void (uint32_t)> (boost::bind (&Console1::eq, this, _1)));

	for (uint32_t i = 0; i < 4; ++i) {
		Encoder low_freq_encoder (
		  *this,
		  eq_freq_controller_for_band (i),
		  boost::function<void (uint32_t)> (boost::bind (&Console1::eq_freq, this, i, _1)),
		  boost::function<void (uint32_t)> (boost::bind (&Console1::mb_send_level, this, i, _1)));
		Encoder low_gain_encoder (
		  *this,
		  eq_gain_controller_for_band (i),
		  boost::function<void (uint32_t)> (boost::bind (&Console1::eq_gain, this, i, _1)),
		  boost::function<void (uint32_t)> (boost::bind (&Console1::mb_send_level, this, i + 4, _1)));
	}
	Encoder low_mid_shape_encoder (
	  *this,
	  ControllerID::LOW_MID_SHAPE,
	  boost::function<void (uint32_t)> (boost::bind (&Console1::mb_send_level, this, 10, _1)),
	  boost::function<void (uint32_t)> (boost::bind (&Console1::mb_send_level, this, 8, _1)));
	Encoder high_mid_shape_encoder (
	  *this,
	  ControllerID::HIGH_MID_SHAPE,
	  boost::function<void (uint32_t)> (boost::bind (&Console1::mb_send_level, this, 11, _1)),
	  boost::function<void (uint32_t)> (boost::bind (&Console1::mb_send_level, this, 9, _1)));

	ControllerButton eq_low_shape (*this,
	                               ControllerID::LOW_SHAPE,
	                               boost::function<void (uint32_t)> (boost::bind (&Console1::eq_low_shape, this, _1)));
	ControllerButton eq_high_shape (
	  *this,
	  ControllerID::HIGH_SHAPE,
	  boost::function<void (uint32_t)> (boost::bind (&Console1::eq_high_shape, this, _1)));

	Encoder drive_encoder (
	  *this, ControllerID::CHARACTER, boost::function<void (uint32_t)> (boost::bind (&Console1::drive, this, _1)));

	/* Compressor Section */
	ControllerButton comp_on_off (
	  *this, ControllerID::COMP, boost::function<void (uint32_t)> (boost::bind (&Console1::comp, this, _1)));
	MultiStateButton comp_mode (*this,
	                            ControllerID::ORDER,
	                            std::vector<uint32_t>{ 0, 63, 127 },
	                            boost::function<void (uint32_t)> (boost::bind (&Console1::comp_mode, this, _1)));

	Encoder comp_thresh_encoder (*this,
	                             ControllerID::COMP_THRESH,
	                             boost::function<void (uint32_t)> (boost::bind (&Console1::comp_thresh, this, _1)));
	Encoder comp_attack_encoder (*this,
	                             ControllerID::COMP_ATTACK,
	                             boost::function<void (uint32_t)> (boost::bind (&Console1::comp_attack, this, _1)));
	Encoder comp_release_encoder (*this,
	                              ControllerID::COMP_RELEASE,
	                              boost::function<void (uint32_t)> (boost::bind (&Console1::comp_release, this, _1)));
	Encoder comp_ratio_encoder (*this,
	                            ControllerID::COMP_RATIO,
	                            boost::function<void (uint32_t)> (boost::bind (&Console1::comp_ratio, this, _1)));
	Encoder comp_makeup_encoder (
	  *this, ControllerID::COMP_PAR, boost::function<void (uint32_t)> (boost::bind (&Console1::comp_makeup, this, _1)));
	Encoder comp_emph_encoder (
	  *this, ControllerID::DRIVE, boost::function<void (uint32_t)> (boost::bind (&Console1::comp_emph, this, _1)));

	Meter compressor_meter (*this, ControllerID::COMP_METER, boost::function<void ()> ([] () {}));

	/* Output Section */
	Meter output_meter_l (*this, ControllerID::OUTPUT_METER_L, boost::function<void ()> ([] () {}));
	Meter output_meter_r (*this, ControllerID::OUTPUT_METER_R, boost::function<void ()> ([] () {}));
}

int
Console1::stop_using_device ()
{
	DEBUG_TRACE (DEBUG::Console1, "stop_using_device()\n");

	blink_connection.disconnect ();
	periodic_connection.disconnect ();
	stripable_connections.drop_connections ();
	return 0;
}

void
Console1::handle_midi_controller_message (MIDI::Parser&, MIDI::EventTwoBytes* tb)
{
	uint32_t controller_number = static_cast<uint32_t> (tb->controller_number);
	uint32_t value = static_cast<uint32_t> (tb->value);

	DEBUG_TRACE (DEBUG::Console1,
	             string_compose ("handle_midi_controller_message cn: '%1' val: '%2'\n", controller_number, value));
	try {
		Encoder e = get_encoder (ControllerID (controller_number));
		if (shift_state && e.shift_action) {
			e.shift_action (value);
		} else {
			e.action (value);
		}
		return;
	} catch (ControlNotFoundException& e) {
		DEBUG_TRACE (DEBUG::Console1,
		             string_compose ("handle_midi_controller_message: encoder not found cn: "
		                             "'%1' val: '%2'\n",
		                             controller_number,
		                             value));
	}

	try {
		ControllerButton& b = get_button (ControllerID (controller_number));
		if (in_plugin_state && b.plugin_action) {
			DEBUG_TRACE (DEBUG::Console1, "Executing plugin_action\n");
			b.plugin_action (value);
		} else if (shift_state && b.shift_action) {
			DEBUG_TRACE (DEBUG::Console1, "Executing shift_action\n");
			b.shift_action (value);
		} else {
			DEBUG_TRACE (DEBUG::Console1, "Executing action\n");
			b.action (value);
		}
		return;
	} catch (ControlNotFoundException& e) {
		DEBUG_TRACE (DEBUG::Console1,
		             string_compose ("handle_midi_controller_message: button not found cn: "
		                             "'%1' val: '%2'\n",
		                             controller_number,
		                             value));
	}

	try {
		MultiStateButton mb = get_mbutton (ControllerID (controller_number));
		if (shift_state && mb.shift_action) {
			mb.shift_action (value);
		} else {
			mb.action (value);
		}

		return;
	} catch (ControlNotFoundException& e) {
		DEBUG_TRACE (DEBUG::Console1,
		             string_compose ("handle_midi_controller_message: mbutton not found cn: "
		                             "'%1' val: '%2'\n",
		                             controller_number,
		                             value));
	}
}

void
Console1::tabbed_window_state_event_handler (GdkEventWindowState* ev, void* object)
{
	DEBUG_TRACE (DEBUG::Console1, string_compose ("tabbed_window_state_event_handler: %1\n", ev->type));
}

void
Console1::notify_solo_active_changed (bool state)
{
	DEBUG_TRACE (DEBUG::Console1, "notify_active_solo_changed() \n");
	try {
		get_button (ControllerID::DISPLAY_ON).set_led_value (state ? 127 : 0);
	} catch (ControlNotFoundException& e) {
		DEBUG_TRACE (DEBUG::Console1, "button not found");
	}
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
	set_current_stripable (first_selected_stripable ());
}

void
Console1::drop_current_stripable ()
{
	if (_current_stripable) {
		if (_current_stripable == session->monitor_out ()) {
			set_current_stripable (session->master_out ());
		} else {
			set_current_stripable (std::shared_ptr<Stripable> ());
		}
	}
}

void
Console1::set_current_stripable (std::shared_ptr<Stripable> r)
{
	DEBUG_TRACE (DEBUG::Console1, "set_current_stripable \n");
	stripable_connections.drop_connections ();

	_current_stripable = r;

	if (_current_stripable) {
		DEBUG_TRACE (DEBUG::Console1, "current_stripable found:  \n");

		PresentationInfo pi = _current_stripable->presentation_info ();

		DEBUG_TRACE (DEBUG::Console1, string_compose ("current_stripable %1 - %2\n", pi.order (), pi.flags ()));

		gate_redux_meter = _current_stripable->gate_redux_controllable ();
		comp_redux_meter = _current_stripable->comp_redux_controllable ();

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
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_pan, this), this);

		_current_stripable->DropReferences.connect (
		  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::drop_current_stripable, this), this);

		_current_stripable->mute_control ()->Changed.connect (
		  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_mute, this), this);

		_current_stripable->solo_control ()->Changed.connect (
		  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_solo, this), this);

		_current_stripable->phase_control ()->Changed.connect (
		  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_phase, this), this);

		// Rec Enabled
		std::shared_ptr<Track> t = std::dynamic_pointer_cast<Track> (_current_stripable);
		if (t) {
			t->rec_enable_control ()->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_recenable, this), this);
		}

		// Monitor
		if (_current_stripable->monitoring_control ()) {
			_current_stripable->monitoring_control ()->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_monitoring, this), this);
		}

		// Trim
		std::shared_ptr<AutomationControl> trim_control = _current_stripable->trim_control ();
		if (trim_control) {
			trim_control->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_trim, this), this);
		}
		// Gain
		std::shared_ptr<AutomationControl> gain_control = _current_stripable->gain_control ();
		if (gain_control) {
			gain_control->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_gain, this), this);

			// control->alist()->automation_state_changed.connect
			// (stripable_connections, MISSING_INVALIDATOR, boost::bind
			// (&Console1::map_auto, this), this);
		}

		// Filter Section
		if (_current_stripable->filter_enable_controllable (true)) {
			_current_stripable->filter_enable_controllable (true)->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_filter, this), this);
		}

		if (_current_stripable->filter_freq_controllable (true)) {
			_current_stripable->filter_freq_controllable (true)->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_low_cut, this), this);
		}

		if (_current_stripable->filter_freq_controllable (false)) {
			_current_stripable->filter_freq_controllable (false)->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_high_cut, this), this);
		}

		// Gate Section
		if (_current_stripable->gate_enable_controllable ()) {
			_current_stripable->gate_enable_controllable ()->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_gate, this), this);
		}

		if (_current_stripable->gate_key_filter_enable_controllable ()) {
			_current_stripable->gate_key_filter_enable_controllable ()->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_gate_scf, this), this);
		}

		if (_current_stripable->gate_key_listen_controllable ()) {
			_current_stripable->gate_key_listen_controllable ()->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_gate_listen, this), this);
		}

		if (_current_stripable->gate_threshold_controllable ()) {
			_current_stripable->gate_threshold_controllable ()->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_gate_thresh, this), this);
		}

		if (_current_stripable->gate_depth_controllable ()) {
			_current_stripable->gate_depth_controllable ()->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_gate_depth, this), this);
		}

		if (_current_stripable->gate_release_controllable ()) {
			_current_stripable->gate_release_controllable ()->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_gate_release, this), this);
		}

		if (_current_stripable->gate_attack_controllable ()) {
			_current_stripable->gate_attack_controllable ()->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_gate_attack, this), this);
		}

		if (_current_stripable->gate_hysteresis_controllable ()) {
			_current_stripable->gate_hysteresis_controllable ()->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_gate_hyst, this), this);
		}

		if (_current_stripable->gate_hold_controllable ()) {
			_current_stripable->gate_hold_controllable ()->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_gate_hold, this), this);
		}

		if (_current_stripable->gate_key_filter_freq_controllable ()) {
			_current_stripable->gate_key_filter_freq_controllable ()->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_gate_filter_freq, this), this);
		}

		// EQ Section
		if (_current_stripable->eq_enable_controllable ()) {
			_current_stripable->eq_enable_controllable ()->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_eq, this), this);
		}

		for (uint32_t i = 0; i < _current_stripable->eq_band_cnt (); ++i) {
			if (_current_stripable->eq_freq_controllable (i)) {
				_current_stripable->eq_freq_controllable (i)->Changed.connect (
				  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_eq_freq, this, i), this);
			}
			if (_current_stripable->eq_gain_controllable (i)) {
				_current_stripable->eq_gain_controllable (i)->Changed.connect (
				  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_eq_gain, this, i), this);
			}
		}

		if (_current_stripable->eq_shape_controllable (0)) {
			_current_stripable->eq_shape_controllable (0)->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_eq_low_shape, this), this);
		}

		if (_current_stripable->eq_shape_controllable (3)) {
			_current_stripable->eq_shape_controllable (3)->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_eq_high_shape, this), this);
		}

		// Drive
		if (_current_stripable->tape_drive_controllable ()) {
			_current_stripable->tape_drive_controllable ()->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_drive, this), this);
		}

		// Mixbus Sends
		for (uint32_t i = 0; i < 12; ++i) {
			if (_current_stripable->send_level_controllable (i)) {
				_current_stripable->send_level_controllable (i)->Changed.connect (
				  stripable_connections,
				  MISSING_INVALIDATOR,
				  boost::bind (&Console1::map_mb_send_level, this, i),
				  this);
			}
		}

		// Comp Section
		if (_current_stripable->comp_enable_controllable ()) {
			_current_stripable->comp_enable_controllable ()->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_comp, this), this);
		}

		if (_current_stripable->comp_mode_controllable ()) {
			_current_stripable->comp_mode_controllable ()->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_comp_mode, this), this);
		}

		if (_current_stripable->comp_threshold_controllable ()) {
			_current_stripable->comp_threshold_controllable ()->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_comp_thresh, this), this);
		}

		if (_current_stripable->comp_attack_controllable ()) {
			_current_stripable->comp_attack_controllable ()->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_comp_attack, this), this);
		}

		if (_current_stripable->comp_release_controllable ()) {
			_current_stripable->comp_release_controllable ()->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_comp_release, this), this);
		}

		if (_current_stripable->comp_ratio_controllable ()) {
			_current_stripable->comp_ratio_controllable ()->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_comp_ratio, this), this);
		}

		if (_current_stripable->comp_makeup_controllable ()) {
			_current_stripable->comp_makeup_controllable ()->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_comp_makeup, this), this);
		}

		if (_current_stripable->comp_key_filter_freq_controllable ()) {
			_current_stripable->comp_key_filter_freq_controllable ()->Changed.connect (
			  stripable_connections, MISSING_INVALIDATOR, boost::bind (&Console1::map_comp_emph, this), this);
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

	map_stripable_state ();
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

		// EQ Section
		map_eq ();
		for (uint32_t i = 0; i < _current_stripable->eq_band_cnt (); ++i) {
			map_eq_freq (i);
			map_eq_gain (i);
		}
		map_eq_low_shape ();
		map_eq_high_shape ();

		for (int i = 0; i < 12; ++i) {
			map_mb_send_level (i);
		}

		// Drive
		map_drive ();

		// Comp Section
		map_comp ();
		map_comp_mode ();
		map_comp_thresh ();
		map_comp_attack ();
		map_comp_release ();
		map_comp_ratio ();
		map_comp_makeup ();
		map_comp_emph ();

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
	blinkers.remove (id);
	get_button (id).set_led_state (false);
}

void
Console1::start_blinking (ControllerID id)
{
	blinkers.push_back (id);
	get_button (id).set_led_state (true);
}

bool
Console1::blinker ()
{
	blink_state = !blink_state;

	for (Blinkers::iterator b = blinkers.begin (); b != blinkers.end (); b++) {
		try {
			get_button (*b).set_led_state (blink_state);
		} catch (ControlNotFoundException& e) {
			DEBUG_TRACE (DEBUG::Console1, "Blinking Button not found ...\n");
		}
	}

	return true;
}

ControllerButton&
Console1::get_button (ControllerID id) const
{
	ButtonMap::const_iterator b = buttons.find (id);
	if (b == buttons.end ())
		throw (ControlNotFoundException ());
	return const_cast<ControllerButton&> (b->second);
}

Meter&
Console1::get_meter (ControllerID id) const
{
	MeterMap::const_iterator m = meters.find (id);
	if (m == meters.end ())
		throw (ControlNotFoundException ());
	return const_cast<Meter&> (m->second);
}

Encoder&
Console1::get_encoder (ControllerID id) const
{
	EncoderMap::const_iterator m = encoders.find (id);
	if (m == encoders.end ())
		throw (ControlNotFoundException ());
	return const_cast<Encoder&> (m->second);
}

MultiStateButton&
Console1::get_mbutton (ControllerID id) const
{
	MultiStateButtonMap::const_iterator m = multi_buttons.find (id);
	if (m == multi_buttons.end ())
		throw (ControlNotFoundException ());
	return const_cast<MultiStateButton&> (m->second);
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
				if (val_l != last_output_meter_l) {
					get_meter (OUTPUT_METER_L).set_value (val_l);
					last_output_meter_l = val_l;
				}
				if (val_r != last_output_meter_r) {
					get_meter (OUTPUT_METER_R).set_value (val_r);
					last_output_meter_r = val_r;
				}
			} catch (ControlNotFoundException& e) {
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
				if (val != last_gate_meter) {
					get_meter (SHAPE_METER).set_value (val);
					last_gate_meter = val;
				}
			} catch (ControlNotFoundException& e) {
				DEBUG_TRACE (DEBUG::Console1, "Meter not found ...\n");
			}
		}
		if (comp_redux_meter) {
			uint32_t val;
			if (!show) {
				val = 127;
			} else {
				float rx = comp_redux_meter->get_parameter () * 127.f;
				val = pow (3.3 + 0.11 * rx, 4);
				val = std::min (127.f, std::max (0.f, rx));
			}
			try {
				if (val != last_comp_redux) {
					last_comp_redux = val;
					val = val * 0.6 + last_comp_redux * 0.4;
					get_meter (COMP_METER).set_value (val);
				}
			} catch (ControlNotFoundException& e) {
				DEBUG_TRACE (DEBUG::Console1, "Meter not found ...\n");
			}
		}
	}
}

float
Console1::calculate_meter (float dB)
{
	return pow ((8.7 + 0.18 * dB), 2.1);
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
Console1::create_strip_invetory ()
{
	DEBUG_TRACE (DEBUG::Console1, "create_strip_invetory()\n");
	StripableList sl;
	boost::optional<order_t> master_order;
	strip_inventory.clear ();
	session->get_stripables (sl);
	uint32_t index = 0;
	for (const auto& s : sl) {
		PresentationInfo pi = s->presentation_info ();
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
			             string_compose ("monitor strip found at index %1, order %2\n", index, pi.order ()));
			continue;
		}
		strip_inventory.insert (std::make_pair (index, pi.order ()));
		DEBUG_TRACE (DEBUG::Console1, string_compose ("insert strip at index %1, order %2\n", index, pi.order ()));
		++index;
	}
	if (master_order) {
		strip_inventory.insert (std::make_pair (index, master_order.value ()));
	}
	DEBUG_TRACE (DEBUG::Console1,
	             string_compose ("create_strip_invetory - inventory size %1\n", strip_inventory.size ()));
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
#ifdef MIXBUS
	set_rid_selection (index + 1);
#else
	set_rid_selection (index + 2);
#endif
}
