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
 
#include "ardour/route.h"
#include "ardour/stripable.h"

#include "subview.h"
#include "mackie_control_protocol.h"
 
using namespace ARDOUR;

namespace ArdourSurface {

namespace Mackie {

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

}
}
