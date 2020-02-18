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

#ifndef ardour_control_protocol_manager_h
#define ardour_control_protocol_manager_h

#include <string>
#include <list>


#include <glibmm/threads.h>

#include "pbd/stateful.h"

#include "control_protocol/types.h"

#include "ardour/session_handle.h"

namespace ARDOUR {

class ControlProtocol;
class ControlProtocolDescriptor;
class Session;

class LIBARDOUR_API ControlProtocolInfo {
	public:
		ControlProtocolDescriptor* descriptor;
		ControlProtocol* protocol;
		std::string name;
		std::string path;
		bool requested;
		bool mandatory;
		bool supports_feedback;
		XMLNode* state;

		ControlProtocolInfo() : descriptor (0), protocol (0), requested(false),
		mandatory(false), supports_feedback(false), state (0)
	{}
		~ControlProtocolInfo();

};

class LIBARDOUR_API ControlProtocolManager : public PBD::Stateful, public ARDOUR::SessionHandlePtr
{
  public:
	~ControlProtocolManager ();

	static ControlProtocolManager& instance();

	void set_session (Session*);
	void discover_control_protocols ();
	void foreach_known_protocol (boost::function<void(const ControlProtocolInfo*)>);
	void load_mandatory_protocols ();
	void midi_connectivity_established ();
	void drop_protocols ();
	void register_request_buffer_factories ();

	int activate (ControlProtocolInfo&);
        int deactivate (ControlProtocolInfo&);

	std::list<ControlProtocolInfo*> control_protocol_info;

	static const std::string state_node_name;

	int set_state (const XMLNode&, int version);
	XMLNode& get_state (void);

        PBD::Signal1<void,ControlProtocolInfo*> ProtocolStatusChange;

        void stripable_selection_changed (ARDOUR::StripableNotificationListPtr);
        static PBD::Signal1<void,ARDOUR::StripableNotificationListPtr> StripableSelectionChanged;

  private:
	ControlProtocolManager ();
	static ControlProtocolManager* _instance;

	Glib::Threads::RWLock protocols_lock;
	std::list<ControlProtocol*>    control_protocols;

	void session_going_away ();

	int control_protocol_discover (std::string path);
	ControlProtocolDescriptor* get_descriptor (std::string path);
	ControlProtocolInfo* cpi_by_name (std::string);
	ControlProtocol* instantiate (ControlProtocolInfo&);
	int teardown (ControlProtocolInfo&, bool lock_required);
};

} // namespace

#endif // ardour_control_protocol_manager_h
