#ifndef ardour_control_protocol_manager_h
#define ardour_control_protocol_manager_h

#include <string>
#include <list>

#include <pbd/lockmonitor.h>

namespace ARDOUR {

class ControlProtocol;
class ControlProtocolDescriptor;

struct ControlProtocolInfo {
    ControlProtocolDescriptor* descriptor;
    ControlProtocol* protocol;
    std::string name;
    std::string path;
};

class ControlProtocolManager
{
  public:
	ControlProtocolManager ();
	~ControlProtocolManager ();

	static ControlProtocolManager& instance() { return *_instance; }

	void discover_control_protocols (std::string search_path);
	void startup (Session&);

	ControlProtocol* instantiate (Session&, std::string protocol_name);
	int              teardown (std::string protocol_name);

  private:
	static ControlProtocolManager* _instance;

	PBD::Lock protocols_lock;
	std::list<ControlProtocolInfo*> control_protocol_info;
	std::list<ControlProtocol*>    control_protocols;

	int control_protocol_discover (std::string path);
	ControlProtocolDescriptor* get_descriptor (std::string path);
};

} // namespace

#endif // ardour_control_protocol_manager_h
