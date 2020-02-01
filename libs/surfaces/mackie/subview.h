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
	virtual bool permit_flipping_faders_and_pots() { return false; }
	virtual void setup_vpot(
		Strip* strip, 
		Pot* vpot, 
		std::string pending_display[2]) = 0;
	virtual void handle_vselect_event(uint32_t global_strip_position);
	// returns true if press was handled in the subview, default is false
	virtual bool handle_cursor_right_press() { return false; }
	// returns true if press was handled in the subview, default is false
	virtual bool handle_cursor_left_press() { return false; }
	
	static bool subview_mode_would_be_ok (SubViewMode, boost::shared_ptr<ARDOUR::Stripable>, std::string& reason_why_not);
	boost::shared_ptr<ARDOUR::Stripable> subview_stripable() const { return _subview_stripable; }
	
	void notify_subview_stripable_deleted ();
	MackieControlProtocol& mcp() { return _mcp; }
	
	PBD::ScopedConnectionList& subview_stripable_connections() { return _subview_stripable_connections; }
	PBD::ScopedConnectionList& subview_connections() { return _subview_connections; }
	
	void do_parameter_display(std::string& display, const ARDOUR::ParameterDescriptor& pd, float param_val, Strip* strip, bool screen_hold);
	
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
	virtual bool permit_flipping_faders_and_pots() { return true; }
	virtual void setup_vpot(
		Strip* strip,
		Pot* vpot, 
		std::string pending_display[2]);
	void notify_send_level_change (uint32_t global_strip_position, bool force);

	virtual void handle_vselect_event(uint32_t global_strip_position);
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

class PluginSubviewState;

class PluginSubview : public Subview {
  public:
    PluginSubview(MackieControlProtocol& mcp, boost::shared_ptr<ARDOUR::Stripable> subview_stripable);
	virtual ~PluginSubview();

	virtual SubViewMode subview_mode () const { return SubViewMode::Plugin; }
	static bool subview_mode_would_be_ok (boost::shared_ptr<ARDOUR::Stripable> r, std::string& reason_why_not);
	virtual void update_global_buttons();
	virtual bool permit_flipping_faders_and_pots();
	virtual void setup_vpot(
		Strip* strip,
		Pot* vpot, 
		std::string pending_display[2]);
	virtual void handle_vselect_event(uint32_t global_strip_position);
	virtual bool handle_cursor_right_press();
	virtual bool handle_cursor_left_press();

	void set_state(boost::shared_ptr<PluginSubviewState> new_state);

  protected:
    boost::shared_ptr<PluginSubviewState> _plugin_subview_state;
};

class PluginSubviewState {
  public:
    PluginSubviewState(PluginSubview& context);
	virtual ~PluginSubviewState();
    
	virtual bool permit_flipping_faders_and_pots() { return false; }
	virtual void setup_vpot(
		Strip* strip,
		Pot* vpot, 
		std::string pending_display[2],
		uint32_t global_strip_position,
		boost::shared_ptr<ARDOUR::Stripable> subview_stripable) = 0;
	virtual void handle_vselect_event(uint32_t global_strip_position, boost::shared_ptr<ARDOUR::Stripable> subview_stripable) = 0;
	static std::string shorten_display_text(const std::string& text, std::string::size_type target_length);
	virtual bool handle_cursor_right_press();
	virtual bool handle_cursor_left_press();
	virtual void bank_changed() = 0;

  protected:
	uint32_t calculate_virtual_strip_position(uint32_t strip_index) const;

    PluginSubview& _context;
	const uint32_t _bank_size;
	uint32_t _current_bank;
};

class PluginSelect : public PluginSubviewState {
  public:
	PluginSelect(PluginSubview& context);
	virtual ~PluginSelect();
	
	virtual void setup_vpot(
		Strip* strip,
		Pot* vpot, 
		std::string pending_display[2],
		uint32_t global_strip_position,
		boost::shared_ptr<ARDOUR::Stripable> subview_stripable);
	virtual void handle_vselect_event(uint32_t global_strip_position, boost::shared_ptr<ARDOUR::Stripable> subview_stripable);
	virtual void bank_changed();
};

class PluginEdit : public PluginSubviewState {
  public:
	PluginEdit(PluginSubview& context, boost::shared_ptr<ARDOUR::PluginInsert> subview_plugin);
	virtual ~PluginEdit();
	
	virtual bool permit_flipping_faders_and_pots() { return true; }
	virtual void setup_vpot(
		Strip* strip,
		Pot* vpot, 
		std::string pending_display[2],
		uint32_t global_strip_position,
		boost::shared_ptr<ARDOUR::Stripable> subview_stripable);
	virtual void handle_vselect_event(uint32_t global_strip_position, boost::shared_ptr<ARDOUR::Stripable> subview_stripable);
	virtual void bank_changed();
	
	void notify_parameter_change(Strip* strip, Pot* vpot, std::string pending_display[2], uint32_t global_strip_position);
	void init(boost::shared_ptr<ARDOUR::PluginInsert> plugin_insert);
	
	boost::shared_ptr<ARDOUR::AutomationControl> parameter_control(uint32_t global_strip_position) const;

	boost::shared_ptr<ARDOUR::PluginInsert> _subview_plugin_insert;
	boost::shared_ptr<ARDOUR::Plugin> _subview_plugin;
	std::vector<uint32_t> _plugin_input_parameter_indices;
};

}
}

#endif /* __ardour_mackie_control_protocol_subview_h__ */
