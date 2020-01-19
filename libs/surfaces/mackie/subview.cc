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
	
#define ui_context() MackieControlProtocol::instance() /* a UICallback-derived object that specifies the event loop for signal handling */

SubviewFactory* SubviewFactory::_instance = 0;

SubviewFactory* SubviewFactory::instance() {
	if (!_instance) {
		_instance = new SubviewFactory();
	}
	return _instance;
}

SubviewFactory::SubviewFactory() {};

boost::shared_ptr<Subview> SubviewFactory::create_subview(SubViewMode svm, boost::shared_ptr<ARDOUR::Stripable> subview_stripable) {
	switch (svm) {
		case SubViewMode::EQ:
			return boost::make_shared<EQSubview>(subview_stripable);
		case SubViewMode::Dynamics:
			return boost::make_shared<DynamicsSubview>(subview_stripable);
		case SubViewMode::Sends:
			return boost::make_shared<SendsSubview>(subview_stripable);
		case SubViewMode::TrackView:
			return boost::make_shared<TrackViewSubview>(subview_stripable);
		case SubViewMode::PluginSelect:
			return boost::make_shared<PluginSelectSubview>(subview_stripable);
		case SubViewMode::PluginEdit:
			return boost::make_shared<PluginEditSubview>(subview_stripable);
		case SubViewMode::None:
		default:
			return boost::make_shared<NoneSubview>(subview_stripable);
	}
}


Subview::Subview(boost::shared_ptr<ARDOUR::Stripable> subview_stripable)
	: _subview_stripable(subview_stripable)
{}

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




NoneSubview::NoneSubview(boost::shared_ptr<ARDOUR::Stripable> subview_stripable) 
	: Subview(subview_stripable)
{}

NoneSubview::~NoneSubview() 
{}

bool NoneSubview::subview_mode_would_be_ok (boost::shared_ptr<ARDOUR::Stripable> r, std::string& reason_why_not) 
{
	return true;
}

void NoneSubview::update_global_buttons(MackieControlProtocol* mcp) 
{
	if (!mcp) {
		return;
	}
	
	mcp->update_global_button (Button::Send, off);
	mcp->update_global_button (Button::Plugin, off);
	mcp->update_global_button (Button::Eq, off);
	mcp->update_global_button (Button::Dyn, off);
	mcp->update_global_button (Button::Track, off);
	mcp->update_global_button (Button::Pan, on);
}

void NoneSubview::setup_vpot(Surface* surface, 
		Strip* strip,
		Pot* vpot, 
		std::string pending_display[2])
{
	
}



EQSubview::EQSubview(boost::shared_ptr<ARDOUR::Stripable> subview_stripable) 
	: Subview(subview_stripable)
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

void EQSubview::update_global_buttons(MackieControlProtocol* mcp) 
{
	if (!mcp) {
		return;
	}
	
	mcp->update_global_button (Button::Send, off);
	mcp->update_global_button (Button::Plugin, off);
	mcp->update_global_button (Button::Eq, on);
	mcp->update_global_button (Button::Dyn, off);
	mcp->update_global_button (Button::Track, off);
	mcp->update_global_button (Button::Pan, off);
}

void EQSubview::setup_vpot(Surface* surface, 
		Strip* strip,
		Pot* vpot, 
		std::string pending_display[2])
{
	
}



DynamicsSubview::DynamicsSubview(boost::shared_ptr<ARDOUR::Stripable> subview_stripable) 
	: Subview(subview_stripable)
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

void DynamicsSubview::update_global_buttons(MackieControlProtocol* mcp) 
{
	if (!mcp) {
		return;
	}
	
	mcp->update_global_button (Button::Send, off);
	mcp->update_global_button (Button::Plugin, off);
	mcp->update_global_button (Button::Eq, off);
	mcp->update_global_button (Button::Dyn, on);
	mcp->update_global_button (Button::Track, off);
	mcp->update_global_button (Button::Pan, off);
}

void DynamicsSubview::setup_vpot(Surface* surface, 
		Strip* strip,
		Pot* vpot, 
		std::string pending_display[2])
{
	
}



SendsSubview::SendsSubview(boost::shared_ptr<ARDOUR::Stripable> subview_stripable) 
	: Subview(subview_stripable)
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

void SendsSubview::update_global_buttons(MackieControlProtocol* mcp) 
{
	if (!mcp) {
		return;
	}
	
	mcp->update_global_button (Button::Send, on);
	mcp->update_global_button (Button::Plugin, off);
	mcp->update_global_button (Button::Eq, off);
	mcp->update_global_button (Button::Dyn, off);
	mcp->update_global_button (Button::Track, off);
	mcp->update_global_button (Button::Pan, off);
}

void SendsSubview::setup_vpot(Surface* surface, 
		Strip* strip,
		Pot* vpot, 
		std::string pending_display[2])
{
	
}



TrackViewSubview::TrackViewSubview(boost::shared_ptr<ARDOUR::Stripable> subview_stripable) 
	: Subview(subview_stripable)
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

void TrackViewSubview::update_global_buttons(MackieControlProtocol* mcp) 
{
	if (!mcp) {
		return;
	}
	
	mcp->update_global_button (Button::Send, off);
	mcp->update_global_button (Button::Plugin, off);
	mcp->update_global_button (Button::Eq, off);
	mcp->update_global_button (Button::Dyn, off);
	mcp->update_global_button (Button::Track, on);
	mcp->update_global_button (Button::Pan, off);
}

void TrackViewSubview::setup_vpot(
		Surface* surface, 
		Strip* strip, 
		Pot* vpot, 
		std::string pending_display[2])
{
	const uint32_t strip_position_on_surface = surface->mcp().global_index (*strip);
	
	if (strip_position_on_surface >= 8) {
		/* nothing to control */
		vpot->set_control (boost::shared_ptr<AutomationControl>());
		pending_display[0] = std::string();
		pending_display[1] = std::string();
		return;
	}
	
	// local pointer to strip members
	if (_strips_surface != surface) {
		_strips_surface = surface;
	}
	_strips[strip_position_on_surface] = strip;
	_strip_vpots[strip_position_on_surface] = vpot;
	_strips_pending_displays[strip_position_on_surface] = pending_display;
	
	if (!_subview_stripable) {
		return;
	}

	boost::shared_ptr<AutomationControl> pc;
	boost::shared_ptr<Track> track = boost::dynamic_pointer_cast<Track> (_subview_stripable);

	switch (strip_position_on_surface) {
	case 0:
		pc = _subview_stripable->trim_control ();
		if (pc) {
			pc->Changed.connect (subview_connections, MISSING_INVALIDATOR, boost::bind (&TrackViewSubview::notify_change, this, TrimAutomation, strip_position_on_surface, false), ui_context());
			pending_display[0] = "Trim";
			notify_change (TrimAutomation, strip_position_on_surface, true);
		}
		break;
	case 1:
		if (track) {
			pc = track->monitoring_control();
			if (pc) {
				pc->Changed.connect (subview_connections, MISSING_INVALIDATOR, boost::bind (&TrackViewSubview::notify_change, this, MonitoringAutomation, strip_position_on_surface, false), ui_context());
				pending_display[0] = "Mon";
				notify_change (MonitoringAutomation, strip_position_on_surface, true);
			}
		}
		break;
	case 2:
		pc = _subview_stripable->solo_isolate_control ();
		if (pc) {
			pc->Changed.connect (subview_connections, MISSING_INVALIDATOR, boost::bind (&TrackViewSubview::notify_change, this, SoloIsolateAutomation, strip_position_on_surface, false), ui_context());
			notify_change (SoloIsolateAutomation, strip_position_on_surface, true);
			pending_display[0] = "S-Iso";
		}
		break;
	case 3:
		pc = _subview_stripable->solo_safe_control ();
		if (pc) {
			pc->Changed.connect (subview_connections, MISSING_INVALIDATOR, boost::bind (&TrackViewSubview::notify_change, this, SoloSafeAutomation, strip_position_on_surface, false), ui_context());
			notify_change (SoloSafeAutomation, strip_position_on_surface, true);
			pending_display[0] = "S-Safe";
		}
		break;
	case 4:
		pc = _subview_stripable->phase_control();
		if (pc) {
			pc->Changed.connect (subview_connections, MISSING_INVALIDATOR, boost::bind (&TrackViewSubview::notify_change, this, PhaseAutomation, strip_position_on_surface, false), ui_context());
			notify_change (PhaseAutomation, strip_position_on_surface, true);
			pending_display[0] = "Phase";
		}
		break;
	case 5:
		// pc = _subview_stripable->trim_control ();
		break;
	case 6:
		// pc = _subview_stripable->trim_control ();
		break;
	case 7:
		// pc = _subview_stripable->trim_control ();
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
TrackViewSubview::notify_change (AutomationType type, uint32_t strip_position_on_surface, bool force_update)
{
	if (!_subview_stripable) {
		return;
	}

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
			_strips_pending_displays[strip_position_on_surface][1] = Strip::format_paramater_for_display(control->desc(), val, _strips[strip_position_on_surface]->stripable(), screen_hold);
	
			if (screen_hold) {
				/* we just queued up a parameter to be displayed.
					1 second from now, switch back to vpot mode display.
				*/
				_strips[strip_position_on_surface]->block_vpot_mode_display_for (1000);
			}
		}
		
		/* update pot/encoder */
		_strips_surface->write (_strip_vpots[strip_position_on_surface]->set (control->internal_to_interface (val), true, Pot::wrap));
	}
}



PluginSelectSubview::PluginSelectSubview(boost::shared_ptr<ARDOUR::Stripable> subview_stripable) 
	: Subview(subview_stripable)
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

void PluginSelectSubview::update_global_buttons(MackieControlProtocol* mcp) 
{
	if (!mcp) {
		return;
	}
	
	mcp->update_global_button (Button::Send, off);
	mcp->update_global_button (Button::Plugin, on);
	mcp->update_global_button (Button::Eq, off);
	mcp->update_global_button (Button::Dyn, off);
	mcp->update_global_button (Button::Track, off);
	mcp->update_global_button (Button::Pan, off);
}

void PluginSelectSubview::setup_vpot(Surface* surface, 
		Strip* strip,
		Pot* vpot, 
		std::string pending_display[2])
{
	
}


PluginEditSubview::PluginEditSubview(boost::shared_ptr<ARDOUR::Stripable> subview_stripable) 
	: Subview(subview_stripable)
{}

PluginEditSubview::~PluginEditSubview() 
{}

bool PluginEditSubview::subview_mode_would_be_ok (boost::shared_ptr<ARDOUR::Stripable> r, std::string& reason_why_not) 
{
	reason_why_not = "pluginedit subview not yet implemented";
	return false;
}

void PluginEditSubview::update_global_buttons(MackieControlProtocol* mcp) 
{
	if (!mcp) {
		return;
	}
	
	mcp->update_global_button (Button::Send, off);
	mcp->update_global_button (Button::Plugin, on);
	mcp->update_global_button (Button::Eq, off);
	mcp->update_global_button (Button::Dyn, off);
	mcp->update_global_button (Button::Track, off);
	mcp->update_global_button (Button::Pan, off);
}

void PluginEditSubview::setup_vpot(Surface* surface, 
		Strip* strip,
		Pot* vpot, 
		std::string pending_display[2])
{
	
}

