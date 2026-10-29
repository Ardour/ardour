/*
 * Copyright (C) 2006-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015-2016 Robin Gareus <robin@gareus.org>
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

#pragma once

#include <list>
#include <string>

#include "pbd/rwlock.h"
#include "pbd/stateful.h"

#include "control_protocol/types.h"

#include "ardour/session_handle.h"

namespace ARDOUR
{

class ControlProtocol;
class ControlProtocolDescriptor;
class ControlProtocolManager;
class Session;

class LIBARDOUR_API ControlProtocolInfo : public PBD::Stateful
{
  public:
	std::string                   name;
	ControlProtocolDescriptor*    descriptor;
	mutable PBD::RWLock           instance_lock;
	std::vector<ControlProtocol*> instances;

	ControlProtocolInfo (std::string const & path);
	~ControlProtocolInfo ();

	bool is_active (CPDeviceIdentifier const &);

	bool active () const;
	std::vector<CPDeviceIdentifier> active_devices () const;

	ControlProtocolDescriptor*    get_descriptor();
	ControlProtocol* get_instance (CPDeviceIdentifier const &);

	ControlProtocol* add_instance (Session&, CPDeviceIdentifier const &);
	bool remove_instance (CPDeviceIdentifier const &);
	void remove_all_instances ();

	int activate (CPDeviceIdentifier const &);
	int deactivate (CPDeviceIdentifier const &);

	int      set_state (const XMLNode&, int version);
	XMLNode& get_state () const;

	void set_session (Session&);

	void midi_connectivity_established (bool);
	void stripable_selection_changed (ARDOUR::StripableNotificationListPtr);

  private:
	friend class ControlProtocolManager;

	Session*                      session;
	std::string                   path;

	static ControlProtocolDescriptor* get_descriptor (std::string path);
};

class LIBARDOUR_API ControlProtocolManager : public PBD::Stateful, public ARDOUR::SessionHandlePtr
{
  public:
	~ControlProtocolManager ();

	static ControlProtocolManager& instance ();

	void establish (Session&);
	void disestablish ();

	void set_session (Session*);
	void discover_control_protocols ();
	void midi_connectivity_established (bool);
	void drop_protocols ();
	void probe_midi_control_protocols ();
	void probe_usb_control_protocols (bool, uint16_t, uint16_t);

	const std::list<ControlProtocolInfo*> control_protocol_infos () const {
		PBD::RWLock::ReaderLock lm (_protocols_lock);
		/* return a (shallow) copy */
		return _control_protocol_info;
	}

	int user_activate (CPDeviceIdentifier const & cpdi) {
		return activate (false, cpdi);
	}
	int user_deactivate (CPDeviceIdentifier const & cpdi) {
		return deactivate (false, cpdi);
	}

	int auto_activate (CPDeviceIdentifier const & cpdi) {
		return activate (true, cpdi);
	}
	int auto_deactivate (CPDeviceIdentifier const & cpdi) {
		return deactivate (true, cpdi);
	}

	static const std::string state_node_name;

	int      set_state (const XMLNode&, int version);
	XMLNode& get_state () const;
	int      use_state ();

	PBD::Signal<void (CPDeviceIdentifier,bool)> ProtocolStatusChange;

	void                                                            stripable_selection_changed (ARDOUR::StripableNotificationListPtr);
	static PBD::Signal<void (ARDOUR::StripableNotificationListPtr)> StripableSelectionChanged;

	typedef std::list<CPDeviceIdentifier> SupportedDevices;
	SupportedDevices const & devices() const { return _devices; }

	ControlProtocolInfo*       cpi_by_name (std::string);

private:
	ControlProtocolManager ();
	static ControlProtocolManager* _instance;

	mutable PBD::RWLock             _protocols_lock;
	std::list<ControlProtocolInfo*> _control_protocol_info;
	XMLNode*                        pending_state;

	std::list<CPDeviceIdentifier>  _devices;

	void session_going_away ();

	int                        control_protocol_discover (std::string path);
	void                       remove_all_instances (ControlProtocolInfo&);

	int activate (bool detected, CPDeviceIdentifier const &);
	int deactivate (bool detected, CPDeviceIdentifier const&);
};

} // namespace ARDOUR
