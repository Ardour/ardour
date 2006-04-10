#ifndef ardour_control_protocol_manager_h
#define ardour_control_protocol_manager_h

#include <string>
#include <list>

#include <sigc++/sigc++.h>

#include <pbd/lockmonitor.h>

namespace ARDOUR {

class ControlProtocol;
class ControlProtocolDescriptor;
class Session;

struct ControlProtocolInfo {
    ControlProtocolDescriptor* descriptor;
    ControlProtocol* protocol;
    std::string name;
    std::string path;
};

 class ControlProtocolManager : public sigc::trackable
{
  public:
	ControlProtocolManager ();
	~ControlProtocolManager ();

	static ControlProtocolManager& instance() { return *_instance; }

	void set_session (Session&);
	void discover_control_protocols (std::string search_path);
	void foreach_known_protocol (sigc::slot<void,const ControlProtocolInfo*>);

	ControlProtocol* instantiate (ControlProtocolInfo&);
	int              teardown (ControlProtocolInfo&);

	std::list<ControlProtocolInfo*> control_protocol_info;

  private:
	static ControlProtocolManager* _instance;

	Session* _session;
	PBD::Lock protocols_lock;
	std::list<ControlProtocol*>    control_protocols;

	void drop_session ();

	int control_protocol_discover (std::string path);
	ControlProtocolDescriptor* get_descriptor (std::string path);
};

} // namespace

#endif // ardour_control_protocol_manager_h
