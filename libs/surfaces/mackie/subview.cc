/*
 * Copyright (C) 2006-2007 John Anderson
 * Copyright (C) 2007-2010 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2016 Len Ovens <len@ovenwerks.net>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016-2018 Ben Loftis <ben@harrisonconsoles.com>
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
 

#include "ardour/debug.h"
#include "ardour/monitor_control.h"
#include "ardour/phase_control.h"
#include "ardour/route.h"
#include "ardour/solo_isolate_control.h"
#include "ardour/stripable.h"
#include "ardour/track.h"

#include "mackie_control_protocol.h"
#include "pot.h"
#include "strip.h"
#include "subview.h"
#include "surface.h"
 
using namespace ARDOUR;
using namespace ArdourSurface;
using namespace Mackie;
using namespace PBD;
	
#define ui_context() MackieControlProtocol::instance() /* a UICallback-derived object that specifies the event loop for signal handling */

SubviewFactory* SubviewFactory::_instance = 0;

SubviewFactory* SubviewFactory::instance() {
	if (!_instance) {
		_instance = new SubviewFactory();
	}
	return _instance;
}

SubviewFactory::SubviewFactory() {};

boost::shared_ptr<Subview> SubviewFactory::create_subview(
		SubViewMode svm, 
		MackieControlProtocol& mcp, 
		boost::shared_ptr<ARDOUR::Stripable> subview_stripable) 
{
	switch (svm) {
		case SubViewMode::EQ:
			return boost::make_shared<EQSubview>(mcp, subview_stripable);
		case SubViewMode::Dynamics:
			return boost::make_shared<DynamicsSubview>(mcp, subview_stripable);
		case SubViewMode::Sends:
			return boost::make_shared<SendsSubview>(mcp, subview_stripable);
		case SubViewMode::TrackView:
			return boost::make_shared<TrackViewSubview>(mcp, subview_stripable);
		case SubViewMode::PluginSelect:
			return boost::make_shared<PluginSelectSubview>(mcp, subview_stripable);
		case SubViewMode::PluginEdit:
			return boost::make_shared<PluginEditSubview>(mcp, subview_stripable);
		case SubViewMode::None:
		default:
			return boost::make_shared<NoneSubview>(mcp, subview_stripable);
	}
}


Subview::Subview(MackieControlProtocol& mcp, boost::shared_ptr<ARDOUR::Stripable> subview_stripable)
	: _mcp(mcp)
	, _subview_stripable(subview_stripable)
{
	init_strip_vectors();
}

Subview::~Subview() {}

bool
Subview::subview_mode_would_be_ok (SubViewMode mode, boost::shared_ptr<Stripable> r, std::string& reason_why_not)
{
	switch (mode) {
	case SubViewMode::None:
		return NoneSubview::subview_mode_would_be_ok(r, reason_why_not);
	case SubViewMode::Sends:
		return SendsSubview::subview_mode_would_be_ok(r, reason_why_not);
	case SubViewMode::EQ:
		return EQSubview::subview_mode_would_be_ok(r, reason_why_not);
	case SubViewMode::Dynamics:
		return DynamicsSubview::subview_mode_would_be_ok(r, reason_why_not);
	case SubViewMode::TrackView:
		return TrackViewSubview::subview_mode_would_be_ok(r, reason_why_not);
	case SubViewMode::PluginSelect:
		return PluginSelectSubview::subview_mode_would_be_ok(r, reason_why_not);
	case SubViewMode::PluginEdit:
		return PluginEditSubview::subview_mode_would_be_ok(r, reason_why_not);
	}

	return false;
}

void
Subview::notify_subview_stripable_deleted ()
{
	_subview_stripable.reset ();
}

void
Subview::init_strip_vectors()
{
	_strips_over_all_surfaces.resize(_mcp.n_strips(), 0);
	_strip_vpots_over_all_surfaces.resize(_mcp.n_strips(), 0);
	_strip_pending_displays_over_all_surfaces.resize(_mcp.n_strips(), 0);
}

void
Subview::store_pointers(Strip* strip, Pot* vpot, std::string* pending_display, uint32_t global_strip_position) 
{
	if (global_strip_position >= _strips_over_all_surfaces.size() ||
		global_strip_position >= _strip_vpots_over_all_surfaces.size() ||
		global_strip_position >= _strip_pending_displays_over_all_surfaces.size())
	{
		return;
	}
	
	std::cout << "-> Strip[" << global_strip_position << "] = " << strip << std::endl;
	
	_strips_over_all_surfaces[global_strip_position] = strip;
	_strip_vpots_over_all_surfaces[global_strip_position] = vpot;
	_strip_pending_displays_over_all_surfaces[global_strip_position] = pending_display;
}

bool
Subview::retrieve_pointers(Strip** strip, Pot** vpot, std::string** pending_display, uint32_t global_strip_position)
{
	if (global_strip_position >= _strips_over_all_surfaces.size() ||
		global_strip_position >= _strip_vpots_over_all_surfaces.size() ||
		global_strip_position >= _strip_pending_displays_over_all_surfaces.size()) 
	{
		return false;
	}
	
	std::cout << "<- Strip[" << global_strip_position << "] = " << _strips_over_all_surfaces[global_strip_position] << std::endl;
	
	*strip = _strips_over_all_surfaces[global_strip_position];
	*vpot = _strip_vpots_over_all_surfaces[global_strip_position];
	*pending_display = _strip_pending_displays_over_all_surfaces[global_strip_position];
	return true;
}



NoneSubview::NoneSubview(MackieControlProtocol& mcp, boost::shared_ptr<ARDOUR::Stripable> subview_stripable) 
	: Subview(mcp, subview_stripable)
{}

NoneSubview::~NoneSubview() 
{}

bool NoneSubview::subview_mode_would_be_ok (boost::shared_ptr<ARDOUR::Stripable> r, std::string& reason_why_not) 
{
	return true;
}

void NoneSubview::update_global_buttons() 
{
	_mcp.update_global_button (Button::Send, off);
	_mcp.update_global_button (Button::Plugin, off);
	_mcp.update_global_button (Button::Eq, off);
	_mcp.update_global_button (Button::Dyn, off);
	_mcp.update_global_button (Button::Track, off);
	_mcp.update_global_button (Button::Pan, on);
}

void NoneSubview::setup_vpot(
		Strip* strip,
		Pot* vpot, 
		std::string pending_display[2])
{
	
}



EQSubview::EQSubview(MackieControlProtocol& mcp, boost::shared_ptr<ARDOUR::Stripable> subview_stripable) 
	: Subview(mcp, subview_stripable)
{}

EQSubview::~EQSubview() 
{}

bool EQSubview::subview_mode_would_be_ok (boost::shared_ptr<ARDOUR::Stripable> r, std::string& reason_why_not) 
{
	if (r && r->eq_band_cnt() > 0) {
		return true;
	} 
	
	reason_why_not = "no EQ in the track/bus";
	return false;
}

void EQSubview::update_global_buttons() 
{
	_mcp.update_global_button (Button::Send, off);
	_mcp.update_global_button (Button::Plugin, off);
	_mcp.update_global_button (Button::Eq, on);
	_mcp.update_global_button (Button::Dyn, off);
	_mcp.update_global_button (Button::Track, off);
	_mcp.update_global_button (Button::Pan, off);
}

void EQSubview::setup_vpot(
		Strip* strip,
		Pot* vpot, 
		std::string pending_display[2])
{
	const uint32_t global_strip_position = _mcp.global_index (*strip);
	store_pointers(strip, vpot, pending_display, global_strip_position);
		
	if (!_subview_stripable) {
		return;
	}
	
	
	boost::shared_ptr<AutomationControl> pc;
	std::string pot_id;

#ifdef MIXBUS
	int eq_band = -1;
	std::string band_name;
	if (_subview_stripable->is_input_strip ()) {

#ifdef MIXBUS32C
		switch (global_strip_position) {
			case 0:
			case 2:
			case 4:
			case 6:
				eq_band = global_strip_position / 2;
				pc = _subview_stripable->eq_freq_controllable (eq_band);
				band_name = _subview_stripable->eq_band_name (eq_band);
				pot_id = band_name + "Freq";
				break;
			case 1:
			case 3:
			case 5:
			case 7:
				eq_band = global_strip_position / 2;
				pc = _subview_stripable->eq_gain_controllable (eq_band);
				band_name = _subview_stripable->eq_band_name (eq_band);
				pot_id = band_name + "Gain";
				break;
			case 8: 
				pc = _subview_stripable->eq_shape_controllable(0);  //low band "bell" button
				band_name = "lo";
				pot_id = band_name + " Shp";
				break;
			case 9:
				pc = _subview_stripable->eq_shape_controllable(3);  //high band "bell" button
				band_name = "hi";
				pot_id = band_name + " Shp";
				break;
			case 10:
				pc = _subview_stripable->eq_enable_controllable();
				pot_id = "EQ";
				break;
		}

#else  //regular Mixbus channel EQ

		switch (global_strip_position) {
			case 0:
			case 2:
			case 4:
				eq_band = global_strip_position / 2;
				pc = _subview_stripable->eq_gain_controllable (eq_band);
				band_name = _subview_stripable->eq_band_name (eq_band);
				pot_id = band_name + "Gain";
				break;
			case 1:
			case 3:
			case 5:
				eq_band = global_strip_position / 2;
				pc = _subview_stripable->eq_freq_controllable (eq_band);
				band_name = _subview_stripable->eq_band_name (eq_band);
				pot_id = band_name + "Freq";
				break;
			case 6:
				pc = _subview_stripable->eq_enable_controllable();
				pot_id = "EQ";
				break;
			case 7:
				pc = _subview_stripable->filter_freq_controllable(true);
				pot_id = "HP Freq";
				break;
		}

#endif

	} else {  //mixbus or master bus ( these are currently the same for MB & 32C )
		switch (global_strip_position) {
			case 0:
			case 1:
			case 2:
				eq_band = global_strip_position;
				pc = _subview_stripable->eq_gain_controllable (eq_band);
				band_name = _subview_stripable->eq_band_name (eq_band);
				pot_id = band_name + "Gain";
				break;
		}
	}
#endif

	//If a controllable was found, connect it up, and put the labels in the display.
	if (pc) {
		pc->Changed.connect (_subview_connections, MISSING_INVALIDATOR, boost::bind (&EQSubview::notify_change, this, boost::weak_ptr<AutomationControl>(pc), global_strip_position, false), ui_context());
		vpot->set_control (pc);

		if (!pot_id.empty()) {
			pending_display[0] = pot_id;
		} else {
			pending_display[0] = std::string();
		}
		
	} else {  //no controllable was found;  just clear this knob
		vpot->set_control (boost::shared_ptr<AutomationControl>());
		pending_display[0] = std::string();
		pending_display[1] = std::string();
	}
	
	notify_change (boost::weak_ptr<AutomationControl>(pc), global_strip_position, true);
}

void EQSubview::notify_change (boost::weak_ptr<ARDOUR::AutomationControl> pc, uint32_t global_strip_position, bool force) 
{
	if (!_subview_stripable) {
		return;
	}
	
	Strip* strip = 0;
	Pot* vpot = 0;
	std::string* pending_display = 0;
	if (!retrieve_pointers(&strip, &vpot, &pending_display, global_strip_position))
	{
		return;
	}
	
	if (!strip || !vpot || !pending_display)
	{
		return;
	}

	boost::shared_ptr<AutomationControl> control = pc.lock ();
	if (control) {
		float val = control->get_value();
		//do_parameter_display (control->desc(), val, true);
		{
			bool screen_hold = true;
			pending_display[1] = Strip::format_paramater_for_display(
					control->desc(), 
					val, 
					strip->stripable(), 
					screen_hold
				);
	
			if (screen_hold) {
				/* we just queued up a parameter to be displayed.
					1 second from now, switch back to vpot mode display.
				*/
				strip->block_vpot_mode_display_for (1000);
			}
		}
		
		/* update pot/encoder */
		strip->surface()->write (vpot->set (control->internal_to_interface (val), true, Pot::wrap));
	}
}



DynamicsSubview::DynamicsSubview(MackieControlProtocol& mcp, boost::shared_ptr<ARDOUR::Stripable> subview_stripable) 
	: Subview(mcp, subview_stripable)
{}

DynamicsSubview::~DynamicsSubview() 
{}

bool DynamicsSubview::subview_mode_would_be_ok (boost::shared_ptr<ARDOUR::Stripable> r, std::string& reason_why_not) 
{
	if (r && r->comp_enable_controllable()) {
		return true;
	}
	
	reason_why_not = "no dynamics in selected track/bus";
	return false;
}

void DynamicsSubview::update_global_buttons() 
{
	_mcp.update_global_button (Button::Send, off);
	_mcp.update_global_button (Button::Plugin, off);
	_mcp.update_global_button (Button::Eq, off);
	_mcp.update_global_button (Button::Dyn, on);
	_mcp.update_global_button (Button::Track, off);
	_mcp.update_global_button (Button::Pan, off);
}

void DynamicsSubview::setup_vpot(
		Strip* strip,
		Pot* vpot, 
		std::string pending_display[2])
{
	const uint32_t global_strip_position = _mcp.global_index (*strip);
	store_pointers(strip, vpot, pending_display, global_strip_position);
	
	if (!_subview_stripable) {
		return;
	}
	
	boost::shared_ptr<AutomationControl> tc = _subview_stripable->comp_threshold_controllable ();
	boost::shared_ptr<AutomationControl> sc = _subview_stripable->comp_speed_controllable ();
	boost::shared_ptr<AutomationControl> mc = _subview_stripable->comp_mode_controllable ();
	boost::shared_ptr<AutomationControl> kc = _subview_stripable->comp_makeup_controllable ();
	boost::shared_ptr<AutomationControl> ec = _subview_stripable->comp_enable_controllable ();

#ifdef MIXBUS32C	//Mixbus32C needs to spill the filter controls into the comp section
	boost::shared_ptr<AutomationControl> hpfc = _subview_stripable->filter_freq_controllable (true);
	boost::shared_ptr<AutomationControl> lpfc = _subview_stripable->filter_freq_controllable (false);
	boost::shared_ptr<AutomationControl> fec = _subview_stripable->filter_enable_controllable (true); // shared HP/LP
#endif

	/* we will control the global_strip_position-th available parameter, from the list in the
	 * order shown above.
	 */

	std::vector<std::pair<boost::shared_ptr<AutomationControl>, std::string > > available;
	std::vector<AutomationType> params;

	if (tc) { available.push_back (std::make_pair (tc, "Thresh")); }
	if (sc) { available.push_back (std::make_pair (sc, mc ? _subview_stripable->comp_speed_name (mc->get_value()) : "Speed")); }
	if (mc) { available.push_back (std::make_pair (mc, "Mode")); }
	if (kc) { available.push_back (std::make_pair (kc, "Makeup")); }
	if (ec) { available.push_back (std::make_pair (ec, "on/off")); }

#ifdef MIXBUS32C	//Mixbus32C needs to spill the filter controls into the comp section
	if (hpfc) { available.push_back (std::make_pair (hpfc, "HPF")); }
	if (lpfc) { available.push_back (std::make_pair (lpfc, "LPF")); }
	if (fec)  { available.push_back (std::make_pair (fec, "FiltIn")); }
#endif

	if (global_strip_position >= available.size()) {
		/* this knob is not needed to control the available parameters */
		vpot->set_control (boost::shared_ptr<AutomationControl>());
		pending_display[0] = std::string();
		pending_display[1] = std::string();
		return;
	}

	boost::shared_ptr<AutomationControl> pc;

	pc = available[global_strip_position].first;
	std::string pot_id = available[global_strip_position].second;

	pc->Changed.connect (_subview_connections, MISSING_INVALIDATOR, boost::bind (&DynamicsSubview::notify_change, this, boost::weak_ptr<AutomationControl>(pc), global_strip_position, false, true), ui_context());
	vpot->set_control (pc);

	if (!pot_id.empty()) {
		pending_display[0] = pot_id;
	} else {
		pending_display[0] = std::string();
	}

	notify_change (boost::weak_ptr<AutomationControl>(pc), global_strip_position, true, false);
}

void
DynamicsSubview::notify_change (boost::weak_ptr<ARDOUR::AutomationControl> pc, uint32_t global_strip_position, bool force, bool propagate_mode) 
{
	if (!_subview_stripable) {
		return;
	}
	
	Strip* strip = 0;
	Pot* vpot = 0;
	std::string* pending_display = 0;
	if (!retrieve_pointers(&strip, &vpot, &pending_display, global_strip_position))
	{
		return;
	}
	
	if (!strip || !vpot || !pending_display)
	{
		return;
	}

	boost::shared_ptr<AutomationControl> control= pc.lock ();
	bool reset_all = false;

	if (propagate_mode && reset_all) {
		// @TODO: this line can never be reached due to reset_all being set to false. What was intended here?
		strip->surface()->subview_mode_changed ();
	}

	if (control) {
		float val = control->get_value();
		if (control == _subview_stripable->comp_mode_controllable ()) {
			pending_display[1] = _subview_stripable->comp_mode_name (val);
		} else {
			//do_parameter_display (control->desc(), val, true);
			{
				bool screen_hold = true;
				pending_display[1] = Strip::format_paramater_for_display(
						control->desc(), 
						val, 
						strip->stripable(), 
						screen_hold
					);
		
				if (screen_hold) {
					/* we just queued up a parameter to be displayed.
						1 second from now, switch back to vpot mode display.
					*/
					strip->block_vpot_mode_display_for (1000);
				}
			}
		}
		/* update pot/encoder */
		strip->surface()->write (vpot->set (control->internal_to_interface (val), true, Pot::wrap));
	}
}



SendsSubview::SendsSubview(MackieControlProtocol& mcp, boost::shared_ptr<ARDOUR::Stripable> subview_stripable) 
	: Subview(mcp, subview_stripable)
{}

SendsSubview::~SendsSubview() 
{}

bool SendsSubview::subview_mode_would_be_ok (boost::shared_ptr<ARDOUR::Stripable> r, std::string& reason_why_not) 
{
	if (r && r->send_level_controllable (0)) {
		return true;
	}
	
	reason_why_not = "no sends for selected track/bus";
	return false;
}

void SendsSubview::update_global_buttons() 
{
	_mcp.update_global_button (Button::Send, on);
	_mcp.update_global_button (Button::Plugin, off);
	_mcp.update_global_button (Button::Eq, off);
	_mcp.update_global_button (Button::Dyn, off);
	_mcp.update_global_button (Button::Track, off);
	_mcp.update_global_button (Button::Pan, off);
}

void SendsSubview::setup_vpot(
		Strip* strip,
		Pot* vpot, 
		std::string pending_display[2])
{
	
}



TrackViewSubview::TrackViewSubview(MackieControlProtocol& mcp, boost::shared_ptr<ARDOUR::Stripable> subview_stripable) 
	: Subview(mcp, subview_stripable)
{}

TrackViewSubview::~TrackViewSubview() 
{}

bool TrackViewSubview::subview_mode_would_be_ok (boost::shared_ptr<ARDOUR::Stripable> r, std::string& reason_why_not) 
{
	if (r)  {
		return true;
	}
	
	reason_why_not = "no track view possible";
	return false;
}

void TrackViewSubview::update_global_buttons() 
{
	_mcp.update_global_button (Button::Send, off);
	_mcp.update_global_button (Button::Plugin, off);
	_mcp.update_global_button (Button::Eq, off);
	_mcp.update_global_button (Button::Dyn, off);
	_mcp.update_global_button (Button::Track, on);
	_mcp.update_global_button (Button::Pan, off);
}

void TrackViewSubview::setup_vpot(
		Strip* strip, 
		Pot* vpot, 
		std::string pending_display[2])
{
	const uint32_t global_strip_position = _mcp.global_index (*strip);
	store_pointers(strip, vpot, pending_display, global_strip_position);
	
	if (global_strip_position > 4) {
		/* nothing to control */
		vpot->set_control (boost::shared_ptr<AutomationControl>());
		pending_display[0] = std::string();
		pending_display[1] = std::string();
		return;
	}
	
	if (!_subview_stripable) {
		return;
	}

	boost::shared_ptr<AutomationControl> pc;
	boost::shared_ptr<Track> track = boost::dynamic_pointer_cast<Track> (_subview_stripable);

	switch (global_strip_position) {
	case 0:
		pc = _subview_stripable->trim_control ();
		if (pc) {
			pc->Changed.connect (_subview_connections, MISSING_INVALIDATOR, boost::bind (&TrackViewSubview::notify_change, this, TrimAutomation, global_strip_position, false), ui_context());
			pending_display[0] = "Trim";
			notify_change (TrimAutomation, global_strip_position, true);
		}
		break;
	case 1:
		if (track) {
			pc = track->monitoring_control();
			if (pc) {
				pc->Changed.connect (_subview_connections, MISSING_INVALIDATOR, boost::bind (&TrackViewSubview::notify_change, this, MonitoringAutomation, global_strip_position, false), ui_context());
				pending_display[0] = "Mon";
				notify_change (MonitoringAutomation, global_strip_position, true);
			}
		}
		break;
	case 2:
		pc = _subview_stripable->solo_isolate_control ();
		if (pc) {
			pc->Changed.connect (_subview_connections, MISSING_INVALIDATOR, boost::bind (&TrackViewSubview::notify_change, this, SoloIsolateAutomation, global_strip_position, false), ui_context());
			notify_change (SoloIsolateAutomation, global_strip_position, true);
			pending_display[0] = "S-Iso";
		}
		break;
	case 3:
		pc = _subview_stripable->solo_safe_control ();
		if (pc) {
			pc->Changed.connect (_subview_connections, MISSING_INVALIDATOR, boost::bind (&TrackViewSubview::notify_change, this, SoloSafeAutomation, global_strip_position, false), ui_context());
			notify_change (SoloSafeAutomation, global_strip_position, true);
			pending_display[0] = "S-Safe";
		}
		break;
	case 4:
		pc = _subview_stripable->phase_control();
		if (pc) {
			pc->Changed.connect (_subview_connections, MISSING_INVALIDATOR, boost::bind (&TrackViewSubview::notify_change, this, PhaseAutomation, global_strip_position, false), ui_context());
			notify_change (PhaseAutomation, global_strip_position, true);
			pending_display[0] = "Phase";
		}
		break;
	}
	
	if (!pc) {
		pending_display[0] = std::string();
		pending_display[1] = std::string();
		return;
	}

	vpot->set_control (pc);
}

void
TrackViewSubview::notify_change (AutomationType type, uint32_t global_strip_position, bool force_update)
{
	if (!_subview_stripable) {
		return;
	}
	
	DEBUG_TRACE (DEBUG::MackieControl, "1\n");
	
	Strip* strip = 0;
	Pot* vpot = 0;
	std::string* pending_display = 0;
	if (!retrieve_pointers(&strip, &vpot, &pending_display, global_strip_position))
	{
		return;
	}
	
	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("strip: %1, pot: %2, disp: %3\n", strip, vpot, pending_display));
	
	if (!strip || !vpot || !pending_display)
	{
		return;
	}
	
	DEBUG_TRACE (DEBUG::MackieControl, "3\n");

	boost::shared_ptr<AutomationControl> control;
	boost::shared_ptr<Track> track = boost::dynamic_pointer_cast<Track> (_subview_stripable);
	bool screen_hold = false;

	switch (type) {
		case TrimAutomation:
			control = _subview_stripable->trim_control();
			screen_hold = true;
			break;
		case SoloIsolateAutomation:
			control = _subview_stripable->solo_isolate_control ();
			break;
		case SoloSafeAutomation:
			control = _subview_stripable->solo_safe_control ();
			break;
		case MonitoringAutomation:
			if (track) {
				control = track->monitoring_control();
				screen_hold = true;
			}
			break;
		case PhaseAutomation:
			control = _subview_stripable->phase_control ();
			screen_hold = true;
			break;
		default:
			break;
	}

	if (control) {
		float val = control->get_value();

		/* Note: all of the displayed controllables require the display
		 * of their *actual* ("internal") value, not the version mapped
		 * into the normalized 0..1.0 ("interface") range.
		 */

		//do_parameter_display (control->desc(), val, screen_hold);
		{
			pending_display[1] = Strip::format_paramater_for_display(control->desc(), val, strip->stripable(), screen_hold);
			DEBUG_TRACE (DEBUG::MackieControl, pending_display[1]);
	
			if (screen_hold) {
				/* we just queued up a parameter to be displayed.
					1 second from now, switch back to vpot mode display.
				*/
				strip->block_vpot_mode_display_for (1000);
			}
		}
		
		/* update pot/encoder */
		strip->surface()->write (vpot->set (control->internal_to_interface (val), true, Pot::wrap));
	}
}



PluginSelectSubview::PluginSelectSubview(MackieControlProtocol& mcp, boost::shared_ptr<ARDOUR::Stripable> subview_stripable) 
	: Subview(mcp, subview_stripable)
{}

PluginSelectSubview::~PluginSelectSubview() 
{}

bool PluginSelectSubview::subview_mode_would_be_ok (boost::shared_ptr<ARDOUR::Stripable> r, std::string& reason_why_not) 
{
	if (r) {
		boost::shared_ptr<Route> route = boost::dynamic_pointer_cast<Route> (r);
		if (route && route->nth_plugin(0)) {
			return true;
		}
	}
	
	reason_why_not = "no plugins in selected track/bus";
	return false;
}

void PluginSelectSubview::update_global_buttons() 
{
	_mcp.update_global_button (Button::Send, off);
	_mcp.update_global_button (Button::Plugin, on);
	_mcp.update_global_button (Button::Eq, off);
	_mcp.update_global_button (Button::Dyn, off);
	_mcp.update_global_button (Button::Track, off);
	_mcp.update_global_button (Button::Pan, off);
}

void PluginSelectSubview::setup_vpot(
		Strip* strip,
		Pot* vpot, 
		std::string pending_display[2])
{
	
}


PluginEditSubview::PluginEditSubview(MackieControlProtocol& mcp, boost::shared_ptr<ARDOUR::Stripable> subview_stripable) 
	: Subview(mcp, subview_stripable)
{}

PluginEditSubview::~PluginEditSubview() 
{}

bool PluginEditSubview::subview_mode_would_be_ok (boost::shared_ptr<ARDOUR::Stripable> r, std::string& reason_why_not) 
{
	reason_why_not = "pluginedit subview not yet implemented";
	return false;
}

void PluginEditSubview::update_global_buttons() 
{
	_mcp.update_global_button (Button::Send, off);
	_mcp.update_global_button (Button::Plugin, on);
	_mcp.update_global_button (Button::Eq, off);
	_mcp.update_global_button (Button::Dyn, off);
	_mcp.update_global_button (Button::Track, off);
	_mcp.update_global_button (Button::Pan, off);
}

void PluginEditSubview::setup_vpot(
		Strip* strip,
		Pot* vpot, 
		std::string pending_display[2])
{
	
}

