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


#include "pbd/convert.h"
#include "pbd/failed_constructor.h"

#include "ardour/debug.h"
#include "ardour/monitor_control.h"
#include "ardour/phase_control.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/route.h"
#include "ardour/solo_isolate_control.h"
#include "ardour/stripable.h"
#include "ardour/track.h"
#include "ardour/well_known_enum.h"

#include "mackie_control_protocol.h"
#include "pot.h"
#include "strip.h"
#include "subview.h"
#include "surface.h"

using namespace ARDOUR;
using namespace ArdourSurface;
using namespace ArdourSurface::MACKIE_NAMESPACE;
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

std::shared_ptr<Subview> SubviewFactory::create_subview(
		Subview::Mode svm,
		MackieControlProtocol& mcp,
		std::shared_ptr<ARDOUR::Stripable> subview_stripable)
{
	switch (svm) {
		case Subview::EQ: {
			auto subview = std::shared_ptr<EQSubview>(new EQSubview (mcp, subview_stripable));
			subview->init_params();
			return subview;
		}
		case Subview::Dynamics: {
			auto subview = std::shared_ptr<DynamicsSubview>(new DynamicsSubview (mcp, subview_stripable));
			subview->init_params();
			return subview;
		}
		case Subview::Sends:
			return std::shared_ptr<SendsSubview>(new SendsSubview (mcp, subview_stripable));
		case Subview::TrackView:
			return std::shared_ptr<TrackViewSubview>(new TrackViewSubview (mcp, subview_stripable));
		case Subview::Plugin:
			return std::shared_ptr<PluginSubview>(new PluginSubview (mcp, subview_stripable));
		case Subview::None:
		default:
			return std::shared_ptr<NoneSubview>(new NoneSubview (mcp, subview_stripable));
	}
}


Subview::Subview(MackieControlProtocol& mcp, std::shared_ptr<ARDOUR::Stripable> subview_stripable)
	: _mcp(mcp)
	, _subview_stripable(subview_stripable)
{
	init_strip_vectors();
}

Subview::~Subview()
{
	reset_all_vpot_controls();
}

void
Subview::reset_all_vpot_controls()
{
	for (std::vector<Pot*>::iterator iter = _strip_vpots_over_all_surfaces.begin(); iter != _strip_vpots_over_all_surfaces.end(); ) {
		std::vector<Pot*>::iterator tmp;

		tmp = iter;
		++tmp;

		if (*iter != 0)
		{
			(*iter)->set_control (std::shared_ptr<AutomationControl>());
		}

		iter = tmp;
	}
}

void Subview::handle_vselect_event(uint32_t global_strip_position)
{
	Strip* strip = 0;
	Pot* vpot = 0;
	std::string* pending_display = 0;
	if (!retrieve_pointers(&strip, &vpot, &pending_display, global_strip_position))
	{
		return;
	}

	std::shared_ptr<AutomationControl> control = vpot->control ();
	if (!control) {
		return;
	}

	Controllable::GroupControlDisposition gcd;
	if (_mcp.main_modifier_state() & MackieControlProtocol::MODIFIER_SHIFT) {
		gcd = Controllable::InverseGroup;
	} else {
		gcd = Controllable::UseGroup;
	}

	if (control->toggled()) {
		if (control->toggled()) {
			control->set_value (!control->get_value(), gcd);
		}

	} else if (control->desc().enumeration || control->desc().integer_step) {

		double val = control->get_value ();
		if (val <= control->upper() - 1.0) {
			control->set_value (val + 1.0, gcd);
		} else {
			control->set_value (control->lower(), gcd);
		}
	}
}

bool
Subview::subview_mode_would_be_ok (Subview::Mode mode, std::shared_ptr<Stripable> r, std::string& reason_why_not)
{
	switch (mode) {
	case Subview::None:
		return NoneSubview::subview_mode_would_be_ok(r, reason_why_not);
	case Subview::Sends:
		return SendsSubview::subview_mode_would_be_ok(r, reason_why_not);
	case Subview::EQ:
		return EQSubview::subview_mode_would_be_ok(r, reason_why_not);
	case Subview::Dynamics:
		return DynamicsSubview::subview_mode_would_be_ok(r, reason_why_not);
	case Subview::TrackView:
		return TrackViewSubview::subview_mode_would_be_ok(r, reason_why_not);
	case Subview::Plugin:
		return PluginSubview::subview_mode_would_be_ok(r, reason_why_not);
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

	*strip = _strips_over_all_surfaces[global_strip_position];
	*vpot = _strip_vpots_over_all_surfaces[global_strip_position];
	*pending_display = _strip_pending_displays_over_all_surfaces[global_strip_position];

	if (!strip || !vpot || !pending_display)
	{
		return false;
	}

	return true;
}

void Subview::do_parameter_display(std::string& display, const ParameterDescriptor& pd, float param_val, Strip* strip, bool screen_hold)
{
	display = Strip::format_parameter_for_display(
			pd,
			param_val,
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



NoneSubview::NoneSubview(MackieControlProtocol& mcp, std::shared_ptr<ARDOUR::Stripable> subview_stripable)
	: Subview(mcp, subview_stripable)
{}

NoneSubview::~NoneSubview()
{}

bool NoneSubview::subview_mode_would_be_ok (std::shared_ptr<ARDOUR::Stripable> r, std::string& reason_why_not)
{
	// always possible
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
	// nothing to be done here. All pots are set in strip.cc
}



EQSubview::EQSubview(MackieControlProtocol& mcp, std::shared_ptr<ARDOUR::Stripable> subview_stripable)
	: Subview(mcp, subview_stripable)
	, _current_bank(0)
{}

EQSubview::~EQSubview()
{}

bool EQSubview::subview_mode_would_be_ok (std::shared_ptr<ARDOUR::Stripable> r, std::string& reason_why_not)
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

void EQSubview::init_params() {
	available.clear();

	std::shared_ptr<AutomationControl> elfc = _subview_stripable->mapped_control (EQ_BandFreq, 0);
	std::shared_ptr<AutomationControl> elgc = _subview_stripable->mapped_control (EQ_BandGain, 0);
	std::shared_ptr<AutomationControl> elmfc = _subview_stripable->mapped_control (EQ_BandFreq, 1);
	std::shared_ptr<AutomationControl> elmgc = _subview_stripable->mapped_control (EQ_BandGain, 1);
	std::shared_ptr<AutomationControl> elmqc = _subview_stripable->mapped_control (EQ_BandQ, 1);
	std::shared_ptr<AutomationControl> ehmfc = _subview_stripable->mapped_control (EQ_BandFreq, 2);
	std::shared_ptr<AutomationControl> ehmgc = _subview_stripable->mapped_control (EQ_BandGain, 2);
	std::shared_ptr<AutomationControl> ehmqc = _subview_stripable->mapped_control (EQ_BandQ, 2);
	std::shared_ptr<AutomationControl> ehfc = _subview_stripable->mapped_control (EQ_BandFreq, 3);
	std::shared_ptr<AutomationControl> ehgc = _subview_stripable->mapped_control (EQ_BandGain, 3);
	std::shared_ptr<AutomationControl> elsc = _subview_stripable->mapped_control (EQ_BandShape, 0);
	std::shared_ptr<AutomationControl> ehsc = _subview_stripable->mapped_control (EQ_BandShape, 3);
	std::shared_ptr<AutomationControl> emc = _subview_stripable->mapped_control (EQ_Mode);
	std::shared_ptr<AutomationControl> eec = _subview_stripable->mapped_control (EQ_Enable);

	std::shared_ptr<AutomationControl> flc = _subview_stripable->mapped_control (LPF_Freq);
	std::shared_ptr<AutomationControl> fhc = _subview_stripable->mapped_control (HPF_Freq);
	std::shared_ptr<AutomationControl> fec = _subview_stripable->mapped_control (HPF_Enable);

	/* we will control the global_strip_position-th available parameter, from the list in the
	 * order shown above.
	 */

	if (elfc) { available.push_back (std::make_pair (elfc, "loFreq")); }
	if (elgc) { available.push_back (std::make_pair (elgc, "loGain")); }
	if (elmfc) { available.push_back (std::make_pair (elmfc, "lmFreq")); }
	if (elmgc) { available.push_back (std::make_pair (elmgc, "lmGain")); }
	if (elmqc) { available.push_back (std::make_pair (elmqc, "lm Q")); }
	if (ehmfc) { available.push_back (std::make_pair (ehmfc, "hmFreq")); }
	if (ehmgc) { available.push_back (std::make_pair (ehmgc, "hmGain")); }
	if (ehmqc) { available.push_back (std::make_pair (ehmqc, "hm Q")); }
	if (ehfc) { available.push_back (std::make_pair (ehfc, "hiFreq")); }
	if (ehgc) { available.push_back (std::make_pair (ehgc, "hiGain")); }
	if (elsc) { available.push_back (std::make_pair (elsc, "lo Shp")); }
	if (ehsc) { available.push_back (std::make_pair (ehsc, "hi Shp")); }
	if (emc) { available.push_back (std::make_pair (emc, "EQMode")); }
	if (eec) { available.push_back (std::make_pair (eec, "EQ")); }

	if (flc) { available.push_back (std::make_pair (flc, "LPF")); }
	if (fhc) { available.push_back (std::make_pair (fhc, "HPF")); }
	if (fec) { available.push_back (std::make_pair (fec, "Filter")); }

	if (available.size() <= _current_bank + 1) {
		_current_bank = available.size() - 1;
	}
}

void EQSubview::setup_vpot(
		Strip* strip,
		Pot* vpot,
		std::string pending_display[2])
{
	const uint32_t global_strip_position = _mcp.global_index (*strip) + _current_bank;
	store_pointers(strip, vpot, pending_display, global_strip_position - _current_bank);

	if (!_subview_stripable) {
		return;
	}

	if (global_strip_position >= available.size()) {
		/* this knob is not needed to control the available parameters */
		vpot->set_control (std::shared_ptr<AutomationControl>());
		pending_display[0] = std::string();
		pending_display[1] = std::string();
		strip->surface()->write (vpot->set (0, false, Pot::wrap));
		return;
	}

	std::shared_ptr<AutomationControl> pc;

	pc = available[global_strip_position].first;
	std::string pot_id = available[global_strip_position].second;

	//If a controllable was found, connect it up, and put the labels in the display.
	if (pc) {
		pc->Changed.connect (_subview_connections, MISSING_INVALIDATOR, std::bind (&EQSubview::notify_change, this, std::weak_ptr<AutomationControl>(pc), global_strip_position, false), ui_context());
		vpot->set_control (pc);

		if (!pot_id.empty()) {
			pending_display[0] = pot_id;
		} else {
			pending_display[0] = std::string();
		}

	} else {  //no controllable was found;  just clear this knob
		vpot->set_control (std::shared_ptr<AutomationControl>());
		pending_display[0] = std::string();
		pending_display[1] = std::string();
		strip->surface()->write (vpot->set (0, false, Pot::wrap));
	}

	notify_change (std::weak_ptr<AutomationControl>(pc), global_strip_position, true);
}

void EQSubview::notify_change (std::weak_ptr<ARDOUR::AutomationControl> pc, uint32_t global_strip_position, bool force)
{
	if (!_subview_stripable) {
		return;
	}

	Strip* strip = 0;
	Pot* vpot = 0;
	std::string* pending_display = 0;
	if (!retrieve_pointers(&strip, &vpot, &pending_display, global_strip_position - _current_bank))
	{
		return;
	}

	std::shared_ptr<AutomationControl> control = pc.lock ();
	if (control) {
		float val = control->get_value();
		pending_display[1] = Strip::remove_units(control->get_user_string());
		/* update pot/encoder */
		strip->surface()->write (vpot->set (control->internal_to_interface (val), true, Pot::wrap));
	}
}

bool EQSubview::handle_cursor_left_press()
{
	if (_current_bank >= 1)
	{
		_current_bank -= 1;
		mcp().redisplay_subview_mode();
	}

	return true;
}

bool EQSubview::handle_cursor_right_press()
{
	if (available.size() > _current_bank + 1) {
		_current_bank += 1;
		mcp().redisplay_subview_mode();
	}

	return true;
}



DynamicsSubview::DynamicsSubview(MackieControlProtocol& mcp, std::shared_ptr<ARDOUR::Stripable> subview_stripable)
	: Subview(mcp, subview_stripable)
	, _current_bank(0)
{}

DynamicsSubview::~DynamicsSubview()
{}

bool DynamicsSubview::subview_mode_would_be_ok (std::shared_ptr<ARDOUR::Stripable> r, std::string& reason_why_not)
{
	if (r && r->mapped_control (Comp_Enable)) {
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

void DynamicsSubview::init_params() {
	available.clear();

	std::shared_ptr<AutomationControl> cec = _subview_stripable->mapped_control (Comp_Enable);
	std::shared_ptr<AutomationControl> cmc = _subview_stripable->mapped_control (Comp_Mode);
	std::shared_ptr<AutomationControl> ctc = _subview_stripable->mapped_control (Comp_Threshold);
	std::shared_ptr<AutomationControl> crc = _subview_stripable->mapped_control (Comp_Ratio);
	std::shared_ptr<AutomationControl> cac = _subview_stripable->mapped_control (Comp_Attack);
	std::shared_ptr<AutomationControl> cfac = _subview_stripable->mapped_control (Comp_FastAttack);
	std::shared_ptr<AutomationControl> csc = _subview_stripable->mapped_control (Comp_Release);
	std::shared_ptr<AutomationControl> ckc = _subview_stripable->mapped_control (Comp_Makeup);
	std::shared_ptr<AutomationControl> cfc = _subview_stripable->mapped_control (Comp_KeyFilterFreq);
	std::shared_ptr<AutomationControl> cpc = _subview_stripable->mapped_control (Comp_RMSPeak);
	std::shared_ptr<AutomationControl> cxc = _subview_stripable->mapped_control (Comp_Mix);

	std::shared_ptr<AutomationControl> gec = _subview_stripable->mapped_control (Gate_Enable);
	std::shared_ptr<AutomationControl> gmc = _subview_stripable->mapped_control (Gate_Mode);
	std::shared_ptr<AutomationControl> gtc = _subview_stripable->mapped_control (Gate_Threshold);
	std::shared_ptr<AutomationControl> gdc = _subview_stripable->mapped_control (Gate_Depth);
	std::shared_ptr<AutomationControl> gac = _subview_stripable->mapped_control (Gate_Attack);
	std::shared_ptr<AutomationControl> gfac = _subview_stripable->mapped_control (Gate_FastAttack);
	std::shared_ptr<AutomationControl> grc = _subview_stripable->mapped_control (Gate_Release);
	std::shared_ptr<AutomationControl> gyc = _subview_stripable->mapped_control (Gate_Hysteresis);
	std::shared_ptr<AutomationControl> ghc = _subview_stripable->mapped_control (Gate_Hold);
	std::shared_ptr<AutomationControl> gkc = _subview_stripable->mapped_control (Gate_Knee);
	std::shared_ptr<AutomationControl> grac = _subview_stripable->mapped_control (Gate_Ratio);
	std::shared_ptr<AutomationControl> gfc = _subview_stripable->mapped_control (Gate_KeyFilterEnable);
	std::shared_ptr<AutomationControl> gfrc = _subview_stripable->mapped_control (Gate_KeyFilterFreq);
	std::shared_ptr<AutomationControl> glc = _subview_stripable->mapped_control (Gate_KeyListen);

	/* we will control the global_strip_position-th available parameter, from the list in the
	 * order shown above.
	 */

	if (cec) { available.push_back (std::make_pair (cec, "Comp")); }
	if (cmc) { available.push_back (std::make_pair (cmc, "CMode")); }
	if (ctc) { available.push_back (std::make_pair (ctc, "CThrsh")); }
	if (crc) { available.push_back (std::make_pair (crc, "CRatio")); }
	if (cac) { available.push_back (std::make_pair (cac, "CAttk")); }
	if (cfac) { available.push_back (std::make_pair (cfac, "CFstAt")); }
	if (csc) { available.push_back (std::make_pair (csc, "CRels")); }
	if (ckc) { available.push_back (std::make_pair (ckc, "CMkup")); }
	if (cfc) { available.push_back (std::make_pair (cfc, "CEmph")); }
	if (cpc) { available.push_back (std::make_pair (cpc, "CPeak")); }
	if (cxc) { available.push_back (std::make_pair (cxc, "CMmix")); }

	if (gec) { available.push_back (std::make_pair (gec, "Gate")); }
	if (gmc) { available.push_back (std::make_pair (gmc, "GMode")); }
	if (gtc) { available.push_back (std::make_pair (gtc, "GThrsh")); }
	if (gdc) { available.push_back (std::make_pair (gdc, "GDepth")); }
	if (gac) { available.push_back (std::make_pair (gac, "GAttk")); }
	if (gfac) { available.push_back (std::make_pair (gfac, "GFstAt")); }
	if (grc) { available.push_back (std::make_pair (grc, "GRels")); }
	if (gyc) { available.push_back (std::make_pair (gyc, "GHyst")); }
	if (ghc) { available.push_back (std::make_pair (ghc, "GHold")); }
	if (gkc) { available.push_back (std::make_pair (gkc, "GKnee")); }
	if (grac) { available.push_back (std::make_pair (grac, "GRatio")); }
	if (gfc) { available.push_back (std::make_pair (gfc, "GSdChn")); }
	if (gfrc) { available.push_back (std::make_pair (gfrc, "GFreq")); }
	if (glc) { available.push_back (std::make_pair (glc, "GList")); }

	if (available.size() <= _current_bank + 1) {
		_current_bank = available.size() - 1;
	}
}

void DynamicsSubview::setup_vpot(
		Strip* strip,
		Pot* vpot,
		std::string pending_display[2])
{
	const uint32_t global_strip_position = _mcp.global_index (*strip) + _current_bank;
	store_pointers(strip, vpot, pending_display, global_strip_position - _current_bank);

	if (!_subview_stripable) {
		return;
	}

	if (global_strip_position >= available.size()) {
		/* this knob is not needed to control the available parameters */
		vpot->set_control (std::shared_ptr<AutomationControl>());
		pending_display[0] = std::string();
		pending_display[1] = std::string();
		strip->surface()->write (vpot->set (0, false, Pot::wrap));
		return;
	}

	std::shared_ptr<AutomationControl> pc;

	pc = available[global_strip_position].first;
	std::string pot_id = available[global_strip_position].second;

	pc->Changed.connect (_subview_connections, MISSING_INVALIDATOR, std::bind (&DynamicsSubview::notify_change, this, std::weak_ptr<AutomationControl>(pc), global_strip_position, false, true), ui_context());
	vpot->set_control (pc);

	if (!pot_id.empty()) {
		pending_display[0] = pot_id;
	} else {
		pending_display[0] = std::string();
	}

	notify_change (std::weak_ptr<AutomationControl>(pc), global_strip_position, true, false);
}

void
DynamicsSubview::notify_change (std::weak_ptr<ARDOUR::AutomationControl> pc, uint32_t global_strip_position, bool force, bool propagate_mode)
{
	if (!_subview_stripable)
	{
		return;
	}

	Strip* strip = 0;
	Pot* vpot = 0;
	std::string* pending_display = 0;
	if (!retrieve_pointers(&strip, &vpot, &pending_display, global_strip_position - _current_bank))
	{
		return;
	}

	std::shared_ptr<AutomationControl> control= pc.lock ();
	bool reset_all = false;

	if (propagate_mode && reset_all) {
		// @TODO: this line can never be reached due to reset_all being set to false. What was intended here?
		strip->surface()->subview_mode_changed ();
	}

	if (control) {
		float val = control->get_value();
		pending_display[1] = Strip::remove_units(control->get_user_string());
		/* update pot/encoder */
		strip->surface()->write (vpot->set (control->internal_to_interface (val), true, Pot::wrap));
	}
}

bool DynamicsSubview::handle_cursor_left_press()
{
	if (_current_bank >= 1)
	{
		_current_bank -= 1;
		mcp().redisplay_subview_mode();
	}

	return true;
}

bool DynamicsSubview::handle_cursor_right_press()
{
	if (available.size() > _current_bank + 1) {
		_current_bank += 1;
		mcp().redisplay_subview_mode();
	}

	return true;
}



SendsSubview::SendsSubview(MackieControlProtocol& mcp, std::shared_ptr<ARDOUR::Stripable> subview_stripable)
	: Subview(mcp, subview_stripable)
	, _current_bank(0)
{}

SendsSubview::~SendsSubview()
{}

bool SendsSubview::subview_mode_would_be_ok (std::shared_ptr<ARDOUR::Stripable> r, std::string& reason_why_not)
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
	const uint32_t global_strip_position = _mcp.global_index (*strip) + _current_bank;
	store_pointers(strip, vpot, pending_display, global_strip_position - _current_bank);

	if (!_subview_stripable) {
		return;
	}

	std::shared_ptr<AutomationControl> pc = _subview_stripable->send_level_controllable (global_strip_position);

	if (!pc) {
		/* nothing to control */
		vpot->set_control (std::shared_ptr<AutomationControl>());
		pending_display[0] = std::string();
		pending_display[1] = std::string();
		strip->surface()->write (vpot->set (0, false, Pot::wrap));
		return;
	}

	pc->Changed.connect (_subview_connections, MISSING_INVALIDATOR, std::bind (&SendsSubview::notify_send_level_change, this, global_strip_position, false), ui_context());
	vpot->set_control (pc);

	pending_display[0] = PBD::short_version (_subview_stripable->send_name (global_strip_position), 6);

	notify_send_level_change (global_strip_position, true);
}

void
SendsSubview::notify_send_level_change (uint32_t global_strip_position, bool force)
{
	if (!_subview_stripable) {
		return;
	}

	Strip* strip = 0;
	Pot* vpot = 0;
	std::string* pending_display = 0;
	if (!retrieve_pointers(&strip, &vpot, &pending_display, global_strip_position - _current_bank))
	{
		return;
	}

	std::shared_ptr<AutomationControl> control = _subview_stripable->send_level_controllable (global_strip_position);
	if (!control) {
		return;
	}

	if (control) {
		float val = control->get_value();
		do_parameter_display(pending_display[1], control->desc(), val, strip, false);

		if (vpot->control() == control) {
			/* update pot/encoder */
			strip->surface()->write (vpot->set (control->internal_to_interface (val), true, Pot::wrap));
		}
	}
}

void SendsSubview::handle_vselect_event(uint32_t global_strip_position)
{
	/* adjust global_strip_position to make sure we're accessing the
	 * correct controllable since we might be banked within the subview.
	 */
	global_strip_position += _current_bank;

	/* Send mode: press enables/disables the relevant
		* send, but the vpot is bound to the send-level so we
		* need to lookup the enable/disable control
		* explicitly.
		*/

	if (!_subview_stripable)
	{
		return;
	}

	Strip* strip = 0;
	Pot* vpot = 0;
	std::string* pending_display = 0;
	if (!retrieve_pointers(&strip, &vpot, &pending_display, global_strip_position - _current_bank))
	{
		return;
	}

	std::shared_ptr<AutomationControl> control = _subview_stripable->send_enable_controllable(global_strip_position);

	if (control) {
		bool currently_enabled = (bool) control->get_value();
		Controllable::GroupControlDisposition gcd;

		if (_mcp.main_modifier_state() & MackieControlProtocol::MODIFIER_SHIFT) {
			gcd = Controllable::InverseGroup;
		} else {
			gcd = Controllable::UseGroup;
		}

		control->set_value (!currently_enabled, gcd);

		if (currently_enabled) {
			/* we just turned it off */
			pending_display[1] = "off";
		} else {
			/* we just turned it on, show the level
			*/
			control = _subview_stripable->send_level_controllable (global_strip_position);
			do_parameter_display(pending_display[1], control->desc(), control->get_value(), strip, false);
		}
	}
}

bool SendsSubview::handle_cursor_left_press()
{
	if (_current_bank >= 1)
	{
		_current_bank -= 1;
	}
	mcp().redisplay_subview_mode();

	return true;
}

bool SendsSubview::handle_cursor_right_press()
{
	uint32_t num_sends = 0;
	if (_subview_stripable->send_name(0).size() > 0) {
		bool more_sends = true;
		while (more_sends) {
			if (_subview_stripable->send_name(num_sends).size() > 0) {
				num_sends++;
			} else {
				more_sends = false;
			}
		}
	}
	if (num_sends > _current_bank + 1) {
		_current_bank += 1;
		mcp().redisplay_subview_mode();
	}
	return true;
}


TrackViewSubview::TrackViewSubview(MackieControlProtocol& mcp, std::shared_ptr<ARDOUR::Stripable> subview_stripable)
	: Subview(mcp, subview_stripable)
{}

TrackViewSubview::~TrackViewSubview()
{}

bool TrackViewSubview::subview_mode_would_be_ok (std::shared_ptr<ARDOUR::Stripable> r, std::string& reason_why_not)
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
		vpot->set_control (std::shared_ptr<AutomationControl>());
		pending_display[0] = std::string();
		pending_display[1] = std::string();
		strip->surface()->write (vpot->set (0, false, Pot::wrap));
		return;
	}

	if (!_subview_stripable) {
		return;
	}

	std::shared_ptr<AutomationControl> pc;
	std::shared_ptr<Track> track = std::dynamic_pointer_cast<Track> (_subview_stripable);

	switch (global_strip_position) {
	case 0:
		pc = _subview_stripable->trim_control ();
		if (pc) {
			pc->Changed.connect (_subview_connections, MISSING_INVALIDATOR, std::bind (&TrackViewSubview::notify_change, this, TrimAutomation, global_strip_position, false), ui_context());
			pending_display[0] = "Trim";
			notify_change (TrimAutomation, global_strip_position, true);
		}
		break;
	case 1:
		if (track) {
			pc = track->monitoring_control();
			if (pc) {
				pc->Changed.connect (_subview_connections, MISSING_INVALIDATOR, std::bind (&TrackViewSubview::notify_change, this, MonitoringAutomation, global_strip_position, false), ui_context());
				pending_display[0] = "Mon";
				notify_change (MonitoringAutomation, global_strip_position, true);
			}
		}
		break;
	case 2:
		if (!_subview_stripable->is_master()) {
			pc = _subview_stripable->solo_isolate_control ();
			if (pc) {
				pc->Changed.connect (_subview_connections, MISSING_INVALIDATOR, std::bind (&TrackViewSubview::notify_change, this, SoloIsolateAutomation, global_strip_position, false), ui_context());
				notify_change (SoloIsolateAutomation, global_strip_position, true);
				pending_display[0] = "S-Iso";
			}
		}
		break;
	case 3:
		if (!_subview_stripable->is_master()) {
			pc = _subview_stripable->solo_safe_control ();
			if (pc) {
				pc->Changed.connect (_subview_connections, MISSING_INVALIDATOR, std::bind (&TrackViewSubview::notify_change, this, SoloSafeAutomation, global_strip_position, false), ui_context());
				notify_change (SoloSafeAutomation, global_strip_position, true);
				pending_display[0] = "S-Safe";
			}
		}
		break;
	case 4:
	if (!_subview_stripable->is_master()) {
			pc = _subview_stripable->phase_control();
			if (pc) {
				pc->Changed.connect (_subview_connections, MISSING_INVALIDATOR, std::bind (&TrackViewSubview::notify_change, this, PhaseAutomation, global_strip_position, false), ui_context());
				notify_change (PhaseAutomation, global_strip_position, true);
				pending_display[0] = "Phase";
			}
		}
		break;
	}

	if (!pc) {
		vpot->set_control (std::shared_ptr<AutomationControl>());
		pending_display[0] = std::string();
		pending_display[1] = std::string();
		strip->surface()->write (vpot->set (0, false, Pot::wrap));
		return;
	} else {
		vpot->set_control (pc);
	}
}

void
TrackViewSubview::notify_change (AutomationType type, uint32_t global_strip_position, bool force_update)
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

	std::shared_ptr<AutomationControl> control;
	std::shared_ptr<Track> track = std::dynamic_pointer_cast<Track> (_subview_stripable);
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
		do_parameter_display(pending_display[1], control->desc(), val, strip, screen_hold);

		/* update pot/encoder */
		strip->surface()->write (vpot->set (control->internal_to_interface (val), true, Pot::wrap));
	}
}



PluginSubview::PluginSubview(MackieControlProtocol& mcp, std::shared_ptr<ARDOUR::Stripable> subview_stripable)
	: Subview(mcp, subview_stripable)
{
	_plugin_subview_state = std::shared_ptr<PluginSelect>(new PluginSelect (*this));
	connect_processors_changed_signal();
}

PluginSubview::~PluginSubview()
{}

void PluginSubview::connect_processors_changed_signal()
{
	std::shared_ptr<Route> route = std::dynamic_pointer_cast<Route> (_subview_stripable);
	if (route)
	{
		route->processors_changed.connect(_subview_connections, MISSING_INVALIDATOR, std::bind (&PluginSubview::handle_processors_changed, this), ui_context());
	}
}

void PluginSubview::handle_processors_changed()
{
	_mcp.redisplay_subview_mode();
}

bool PluginSubview::subview_mode_would_be_ok (std::shared_ptr<ARDOUR::Stripable> r, std::string& reason_why_not)
{
	if (r) {
		std::shared_ptr<Route> route = std::dynamic_pointer_cast<Route> (r);
		if (route && route->nth_plugin(0)) {
			return true;
		}
	}

	reason_why_not = "no plugins in selected track/bus";
	return false;
}

void PluginSubview::update_global_buttons()
{
	_mcp.update_global_button (Button::Send, off);
	_mcp.update_global_button (Button::Plugin, on);
	_mcp.update_global_button (Button::Eq, off);
	_mcp.update_global_button (Button::Dyn, off);
	_mcp.update_global_button (Button::Track, off);
	_mcp.update_global_button (Button::Pan, off);
}

bool PluginSubview::permit_flipping_faders_and_pots()
{
	return _plugin_subview_state->permit_flipping_faders_and_pots();
}

void PluginSubview::setup_vpot(
		Strip* strip,
		Pot* vpot,
		std::string pending_display[2])
{
	const uint32_t global_strip_position = _mcp.global_index (*strip);
	store_pointers(strip, vpot, pending_display, global_strip_position);
	_plugin_subview_state->setup_vpot(strip, vpot, pending_display, global_strip_position, _subview_stripable);
}

void PluginSubview::handle_vselect_event(uint32_t global_strip_position)
{
	_plugin_subview_state->handle_vselect_event(global_strip_position, _subview_stripable);
}

bool PluginSubview::handle_cursor_right_press()
{
	return _plugin_subview_state->handle_cursor_right_press();
}

bool PluginSubview::handle_cursor_left_press()
{
	return _plugin_subview_state->handle_cursor_left_press();
}

void PluginSubview::set_state(std::shared_ptr<PluginSubviewState> new_state)
{
	_plugin_subview_state = new_state;

	const uint32_t num_strips = _strips_over_all_surfaces.size();
	for (uint32_t strip_index = 0; strip_index < num_strips; strip_index++)
	{
		Strip* strip = 0;
		Pot* vpot = 0;
		std::string* pending_display = 0;
		if (!retrieve_pointers(&strip, &vpot, &pending_display, strip_index))
		{
			return;
		}
		_plugin_subview_state->setup_vpot(strip, vpot, pending_display, strip_index, _subview_stripable);
	}
}




PluginSubviewState::PluginSubviewState(PluginSubview& context)
  : _context(context)
  , _bank_size(context.mcp().n_strips())
  , _current_bank(0)
{
}

PluginSubviewState::~PluginSubviewState()
{}

std::string
PluginSubviewState::shorten_display_text(const std::string& text, std::string::size_type target_length)
{
	if (text.length() <= target_length) {
		return text;
	}

	return PBD::short_version (text, target_length);
}

bool PluginSubviewState::handle_cursor_left_press()
{
	if (_current_bank >= 1)
	{
		_current_bank = _current_bank - 1;
	}
	bank_changed();
	return true;
}

uint32_t PluginSubviewState::calculate_virtual_strip_position(uint32_t strip_index) const
{
	return _current_bank * _bank_size + strip_index;
}



PluginSelect::PluginSelect(PluginSubview& context)
  : PluginSubviewState(context)
  , _bank_size(_context.mcp().n_strips())
{}

PluginSelect::~PluginSelect()
{}

void PluginSelect::setup_vpot(
	    Strip* strip,
		Pot* vpot,
		std::string pending_display[2],
		uint32_t global_strip_position,
		std::shared_ptr<ARDOUR::Stripable> subview_stripable)
{
	if (!subview_stripable) {
		return;
	}

	std::shared_ptr<Route> route = std::dynamic_pointer_cast<Route> (subview_stripable);
	if (!route) {
		return;
	}

	vpot->set_control (std::shared_ptr<AutomationControl>());

	uint32_t virtual_strip_position = calculate_virtual_strip_position(global_strip_position);

	std::shared_ptr<Processor> plugin = route->nth_plugin(virtual_strip_position);

	if (plugin) {
		DEBUG_TRACE (DEBUG::MackieControl, string_compose ("plugin of strip %1 is %2\n", global_strip_position, plugin->display_name()));
		pending_display[0] = string_compose("Ins%1Pl", virtual_strip_position + 1);
		pending_display[1] = PluginSubviewState::shorten_display_text(plugin->display_name(), 6);
	}
	else {
		pending_display[0] = "";
		pending_display[1] = "";
	}
	strip->surface()->write (vpot->set (0, false, Pot::wrap));
}

void PluginSelect::handle_vselect_event(uint32_t global_strip_position,
		std::shared_ptr<ARDOUR::Stripable> subview_stripable)
{
	/* adjust global_strip_position to make sure we're accessing the
	 * correct controllable since we might be banked within the subview.
	 */
	global_strip_position += _current_bank;

	/* PluginSelect mode: press selects the plugin shown on the strip's LCD */
	if (!subview_stripable) {
		return;
	}

	std::shared_ptr<Route> route = std::dynamic_pointer_cast<Route> (subview_stripable);
	if (!route) {
		return;
	}

	uint32_t virtual_strip_position = calculate_virtual_strip_position(global_strip_position);

	std::shared_ptr<Processor> processor = route->nth_plugin(virtual_strip_position);
	std::shared_ptr<PluginInsert> plugin = std::dynamic_pointer_cast<PluginInsert>(processor);
	if (plugin) {
		processor->ShowUI();
		_context.set_state (std::shared_ptr<PluginEdit> (new PluginEdit (_context, std::weak_ptr<PluginInsert>(plugin))));
	}
}

bool PluginSelect::handle_cursor_right_press()
{
	std::shared_ptr<Route> route = std::dynamic_pointer_cast<Route> (_context.subview_stripable());
	if (!route) {
		return true;
	}
	std::shared_ptr<Processor> plugin = route->nth_plugin(0);
	uint32_t num_plugins = 0;
	while (plugin) {
		plugin = route->nth_plugin(++num_plugins);
	}

	if (num_plugins > (_current_bank + 1) * _bank_size) {
		_current_bank = _current_bank + 1;
		bank_changed();
	}
	return true;
}

void PluginSelect::bank_changed()
{
	_context.mcp().redisplay_subview_mode();
}



PluginEdit::PluginEdit(PluginSubview& context, std::weak_ptr<PluginInsert> weak_subview_plugin_insert)
  : PluginSubviewState(context)
  , _weak_subview_plugin_insert(weak_subview_plugin_insert)
{
	try {
		init();
	}
	catch (...) {
		throw failed_constructor();
	}
}

PluginEdit::~PluginEdit()
{}

void PluginEdit::init()
{
	std::shared_ptr<PluginInsert> plugin_insert = _weak_subview_plugin_insert.lock();
	_weak_subview_plugin = std::weak_ptr<ARDOUR::Plugin>(plugin_insert->plugin());
	std::shared_ptr<ARDOUR::Plugin> subview_plugin = _weak_subview_plugin.lock();
	_plugin_input_parameter_indices.clear();

	if (!subview_plugin) {
		return;
	}

	bool ok = false;
	// put only input controls into a vector
	uint32_t nplug_params = subview_plugin->parameter_count();
	for (uint32_t ppi = 0; ppi < nplug_params; ++ppi) {
		uint32_t controlid = subview_plugin->nth_parameter(ppi, ok);
		if (!ok) {
			continue;
		}
		if (subview_plugin->parameter_is_input(controlid)) {
			_plugin_input_parameter_indices.push_back(ppi);
		}
	}
}

std::shared_ptr<AutomationControl> PluginEdit::parameter_control(uint32_t global_strip_position) const
{
	uint32_t virtual_strip_position = calculate_virtual_strip_position(global_strip_position);
	if (virtual_strip_position >= _plugin_input_parameter_indices.size()) {
		return std::shared_ptr<AutomationControl>();
	}

	std::shared_ptr<PluginInsert> plugin_insert = _weak_subview_plugin_insert.lock();
	std::shared_ptr<ARDOUR::Plugin> subview_plugin = _weak_subview_plugin.lock();
	if (!plugin_insert || !subview_plugin) {
		return std::shared_ptr<AutomationControl>();
	}

	uint32_t plugin_parameter_index = _plugin_input_parameter_indices[virtual_strip_position];
	bool ok = false;
	uint32_t controlid = subview_plugin->nth_parameter(plugin_parameter_index, ok);
	if (!ok) {
		return std::shared_ptr<AutomationControl>();
	}
	return plugin_insert->automation_control(Evoral::Parameter(PluginAutomation, 0, controlid));
}

bool PluginEdit::plugin_went_away() const
{
	// is shared_ptr reset?
	std::shared_ptr<PluginInsert> plugin_insert = _weak_subview_plugin_insert.lock();
	std::shared_ptr<ARDOUR::Plugin> subview_plugin = _weak_subview_plugin.lock();
	if (!plugin_insert || !subview_plugin) {
		return true;
	}

	// is plugin not registered with stripable any more?
	std::shared_ptr<Route> route = std::dynamic_pointer_cast<Route> (_context.subview_stripable());
	if (!route) {
		return true;
	}

	if (!route->processor_by_id(plugin_insert->id())) {
		// plugin_insert is not registered with route any more -> it was removed
		return true;
	}

	return false;
}

void PluginEdit::switch_to_plugin_select_state()
{
	_context.set_state (std::shared_ptr <PluginSelect>(new PluginSelect (_context)));
}

void PluginEdit::setup_vpot(
		Strip* strip,
		Pot* vpot,
		std::string pending_display[2],
		uint32_t global_strip_position,
		std::shared_ptr<ARDOUR::Stripable> subview_stripable)
{
	if (plugin_went_away()) {
		switch_to_plugin_select_state();
		return;
	}

	std::shared_ptr<AutomationControl> c = parameter_control(global_strip_position);

	if (!c) {
		vpot->set_control (std::shared_ptr<AutomationControl>());
		pending_display[0] = std::string();
		pending_display[1] = std::string();
		strip->surface()->write (vpot->set (0, false, Pot::wrap));
		return;
	}

	c->Changed.connect (_context.subview_connections(), MISSING_INVALIDATOR, std::bind (&PluginEdit::notify_parameter_change, this, strip, vpot, pending_display, global_strip_position), ui_context());
	vpot->set_control (c);
	pending_display[0] = PluginSubviewState::shorten_display_text(c->desc().label, 6);
	notify_parameter_change (strip, vpot, pending_display, global_strip_position);
}


void PluginEdit::notify_parameter_change(Strip* strip, Pot* vpot, std::string pending_display[2], uint32_t global_strip_position)
{
	std::shared_ptr<AutomationControl> control = parameter_control(global_strip_position);
	if (!control)
	{
		return;
	}

	float val = control->get_value();
	_context.do_parameter_display(pending_display[1], control->desc(), val, strip, false);

	if (vpot->control() == control) {
		/* update pot/encoder */
		strip->surface()->write(vpot->set (control->internal_to_interface (val), true, Pot::wrap));
	}
}

void PluginEdit::handle_vselect_event(uint32_t global_strip_position, std::shared_ptr<ARDOUR::Stripable> subview_stripable)
{
}

bool PluginEdit::handle_cursor_right_press()
{
	if (_plugin_input_parameter_indices.size() > (_current_bank + 1) * _bank_size) {
		_current_bank = _current_bank + 1;
		bank_changed();
	}
	return true;
}

void PluginEdit::bank_changed()
{
	_context.mcp().redisplay_subview_mode();
}
