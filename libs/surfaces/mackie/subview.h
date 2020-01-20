/*
 * Copyright (C) 2006-2007 John Anderson
 * Copyright (C) 2012-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_mackie_control_protocol_subview_h__
#define __ardour_mackie_control_protocol_subview_h__

#include <boost/smart_ptr.hpp>

#include "ardour/types.h"

#include "subview_modes.h"

namespace ArdourSurface {

class MackieControlProtocol;

namespace Mackie {

class Pot;
class Strip;
class Subview;
class Surface;

class SubviewFactory {
  public:
	static SubviewFactory* instance();
	
	boost::shared_ptr<Subview> create_subview(SubViewMode svm, 
		MackieControlProtocol& mcp, boost::shared_ptr<ARDOUR::Stripable> subview_stripable);
  protected:
	SubviewFactory();
  private:
	static SubviewFactory* _instance;
};


/**
	This implements the subviews of the Mackie control in a Strategy pattern
*/
class Subview {
  public:
	Subview(MackieControlProtocol& mcp, boost::shared_ptr<ARDOUR::Stripable> subview_stripable);
	virtual ~Subview();
	
	virtual SubViewMode subview_mode () const = 0;
	virtual void update_global_buttons() = 0;
	virtual void setup_vpot(
		Strip* strip, 
		Pot* vpot, 
		std::string pending_display[2]) = 0;
	
	static bool subview_mode_would_be_ok (SubViewMode, boost::shared_ptr<ARDOUR::Stripable>, std::string& reason_why_not);
	boost::shared_ptr<ARDOUR::Stripable> subview_stripable() const { return _subview_stripable; }
	
	void notify_subview_stripable_deleted ();
	
	PBD::ScopedConnectionList& subview_stripable_connections() { return _subview_stripable_connections; }
	
  protected:
	void init_strip_vectors();
	void store_pointers(Strip* strip, Pot* vpot, std::string* pending_display, uint32_t global_strip_position);
	bool retrieve_pointers(Strip** strip, Pot** vpot, std::string** pending_display, uint32_t global_strip_position);
  
	MackieControlProtocol& _mcp;
	boost::shared_ptr<ARDOUR::Stripable> _subview_stripable;
	PBD::ScopedConnectionList _subview_stripable_connections;
	
	std::vector<Strip*> _strips_over_all_surfaces;
	std::vector<Pot*> _strip_vpots_over_all_surfaces;
	std::vector<std::string*> _strip_pending_displays_over_all_surfaces;
	PBD::ScopedConnectionList _subview_connections;
};

class NoneSubview : public Subview {
  public:
	NoneSubview(MackieControlProtocol& mcp, boost::shared_ptr<ARDOUR::Stripable> subview_stripable);
	virtual ~NoneSubview();
	
	virtual SubViewMode subview_mode () const { return SubViewMode::None; }
	static bool subview_mode_would_be_ok (boost::shared_ptr<ARDOUR::Stripable> r, std::string& reason_why_not);
	
	virtual void update_global_buttons();
	virtual void setup_vpot( 
		Strip* strip,
		Pot* vpot, 
		std::string pending_display[2]);
};

class EQSubview : public Subview {
  public:
	EQSubview(MackieControlProtocol& mcp, boost::shared_ptr<ARDOUR::Stripable> subview_stripable);
	virtual ~EQSubview();
	
	virtual SubViewMode subview_mode () const { return SubViewMode::EQ; }
	static bool subview_mode_would_be_ok (boost::shared_ptr<ARDOUR::Stripable> r, std::string& reason_why_not);
	virtual void update_global_buttons();
	virtual void setup_vpot(
		Strip* strip,
		Pot* vpot, 
		std::string pending_display[2]);
	void notify_change (boost::weak_ptr<ARDOUR::AutomationControl>, uint32_t global_strip_position, bool force);
};

class DynamicsSubview : public Subview {
  public:
	DynamicsSubview(MackieControlProtocol& mcp, boost::shared_ptr<ARDOUR::Stripable> subview_stripable);
	virtual ~DynamicsSubview();
	
	virtual SubViewMode subview_mode () const { return SubViewMode::Dynamics; }
	static bool subview_mode_would_be_ok (boost::shared_ptr<ARDOUR::Stripable> r, std::string& reason_why_not);
	virtual void update_global_buttons();
	virtual void setup_vpot(
		Strip* strip,
		Pot* vpot, 
		std::string pending_display[2]);
	void notify_change (boost::weak_ptr<ARDOUR::AutomationControl>, uint32_t global_strip_position, bool force, bool propagate_mode_change);
};

class SendsSubview : public Subview {
  public:
	SendsSubview(MackieControlProtocol& mcp, boost::shared_ptr<ARDOUR::Stripable> subview_stripable);
	virtual ~SendsSubview();
	
	virtual SubViewMode subview_mode () const { return SubViewMode::Sends; }
	static bool subview_mode_would_be_ok (boost::shared_ptr<ARDOUR::Stripable> r, std::string& reason_why_not);
	virtual void update_global_buttons();
	virtual void setup_vpot(
		Strip* strip,
		Pot* vpot, 
		std::string pending_display[2]);
};

class TrackViewSubview : public Subview {
  public:
	TrackViewSubview(MackieControlProtocol& mcp, boost::shared_ptr<ARDOUR::Stripable> subview_stripable);
	virtual ~TrackViewSubview();
	
	virtual SubViewMode subview_mode () const { return SubViewMode::TrackView; }
	static bool subview_mode_would_be_ok (boost::shared_ptr<ARDOUR::Stripable> r, std::string& reason_why_not);
	virtual void update_global_buttons();
	virtual void setup_vpot(
		Strip* strip,
		Pot* vpot, 
		std::string pending_display[2]);
	void notify_change (ARDOUR::AutomationType, uint32_t global_strip_position, bool force);
};

class PluginSelectSubview : public Subview {
  public:
	PluginSelectSubview(MackieControlProtocol& mcp, boost::shared_ptr<ARDOUR::Stripable> subview_stripable);
	virtual ~PluginSelectSubview();
	
	virtual SubViewMode subview_mode () const { return SubViewMode::PluginSelect; }
	static bool subview_mode_would_be_ok (boost::shared_ptr<ARDOUR::Stripable> r, std::string& reason_why_not);
	virtual void update_global_buttons();
	virtual void setup_vpot(
		Strip* strip,
		Pot* vpot, 
		std::string pending_display[2]);
};

class PluginEditSubview : public Subview {
  public:
	PluginEditSubview(MackieControlProtocol& mcp, boost::shared_ptr<ARDOUR::Stripable> subview_stripable);
	virtual ~PluginEditSubview();
	
	virtual SubViewMode subview_mode () const { return SubViewMode::PluginEdit; }
	static bool subview_mode_would_be_ok (boost::shared_ptr<ARDOUR::Stripable> r, std::string& reason_why_not);
	virtual void update_global_buttons();
	virtual void setup_vpot(
		Strip* strip,
		Pot* vpot, 
		std::string pending_display[2]);
};

}
}

#endif /* __ardour_mackie_control_protocol_subview_h__ */
