#ifndef ardour_control_protocols_h
#define ardour_control_protocols_h

#include <string>
#include <list>
#include <sigc++/sigc++.h>

namespace ARDOUR {

class Route;
class Session;

class ControlProtocol : sigc::trackable {
  public:
	ControlProtocol (Session&, std::string name);
	virtual ~ControlProtocol();

	virtual int init () { return 0; }
	virtual bool active() const = 0;

	enum SendWhat {
		SendRoute,
		SendGlobal
	};

	std::string name() const { return _name; }

	void set_send (SendWhat);

	bool send() const { return _send != 0; }
	bool send_route_feedback () const { return _send & SendRoute; }
	bool send_global_feedback () const { return _send & SendGlobal; }

	virtual void send_route_feedback (std::list<Route*>&) {}
	virtual void send_global_feedback () {}

  protected:
	ARDOUR::Session& session;
	SendWhat _send;
	std::string _name;
};

}

#endif // ardour_control_protocols_h
