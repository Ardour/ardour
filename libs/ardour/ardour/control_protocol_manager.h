/*
    Copyright (C) 2000-2007 Paul Davis 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef ardour_control_protocol_manager_h
#define ardour_control_protocol_manager_h

#include <string>
#include <list>

#include <sigc++/sigc++.h>

#include <glibmm/thread.h>

#include <pbd/stateful.h> 

namespace ARDOUR {

class ControlProtocol;
class ControlProtocolDescriptor;
class Session;

struct ControlProtocolInfo {
    ControlProtocolDescriptor* descriptor;
    ControlProtocol* protocol;
    std::string name;
    std::string path;
    bool requested;
    bool mandatory;
    bool supports_feedback;
    XMLNode* state;

    ControlProtocolInfo() : descriptor (0), protocol (0), state (0) {}
    ~ControlProtocolInfo() { if (state) { delete state; } }
};

 class ControlProtocolManager : public sigc::trackable, public Stateful
{
  public:
	ControlProtocolManager ();
	~ControlProtocolManager ();

	static ControlProtocolManager& instance() { return *_instance; }

	void set_session (Session&);
	void discover_control_protocols (std::string search_path);
	void foreach_known_protocol (sigc::slot<void,const ControlProtocolInfo*>);
	void load_mandatory_protocols ();

	ControlProtocol* instantiate (ControlProtocolInfo&);
	int              teardown (ControlProtocolInfo&);

	std::list<ControlProtocolInfo*> control_protocol_info;

	static const std::string state_node_name;

	int set_state (const XMLNode&);
	XMLNode& get_state (void);

	sigc::signal<void,ControlProtocolInfo*> ProtocolStatusChange;

  private:
	static ControlProtocolManager* _instance;

	Session* _session;
	Glib::Mutex protocols_lock;
	std::list<ControlProtocol*>    control_protocols;

	void drop_session ();

	int control_protocol_discover (std::string path);
	ControlProtocolDescriptor* get_descriptor (std::string path);
	ControlProtocolInfo* cpi_by_name (std::string);
};

} // namespace

#endif // ardour_control_protocol_manager_h
