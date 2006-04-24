#ifndef ardour_control_protocols_h
#define ardour_control_protocols_h

#include <string>
#include <list>
#include <sigc++/sigc++.h>

#include <ardour/basic_ui.h>

namespace ARDOUR {

class Route;
class Session;

class ControlProtocol : public sigc::trackable, public BasicUI {
  public:
	ControlProtocol (Session&, std::string name);
	virtual ~ControlProtocol();

	std::string name() const { return _name; }

	virtual int set_active (bool yn) = 0;
	bool get_active() const { return _active; }

	sigc::signal<void> ActiveChanged;


	/* signals that a control protocol can emit and other (presumably graphical)
	   user interfaces can respond to
	*/

	static sigc::signal<void> ZoomToSession;
	static sigc::signal<void> ZoomIn;
	static sigc::signal<void> ZoomOut;
	static sigc::signal<void> Enter;
	static sigc::signal<void,float> ScrollTimeline;

  protected:
	std::string _name;
	bool _active;
};

extern "C" {
	struct ControlProtocolDescriptor {
	    const char* name;   /* descriptive */
	    const char* id;     /* unique and version-specific */
	    void*       ptr;    /* protocol can store a value here */
	    void*       module; /* not for public access */
	    ControlProtocol* (*initialize)(ControlProtocolDescriptor*,Session*);
	    void             (*destroy)(ControlProtocolDescriptor*,ControlProtocol*);
	    
	};
}

}

#endif // ardour_control_protocols_h
