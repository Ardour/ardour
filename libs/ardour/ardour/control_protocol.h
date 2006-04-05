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

	sigc::signal<void> ActiveChanged;

	enum SendWhat {
		SendRoute,
		SendGlobal
	};

	std::string name() const { return _name; }

	void set_send (SendWhat);
	void set_active (bool yn);
	bool get_active() const { return active_thread > 0; }


	bool send() const { return _send != 0; }
	bool send_route_feedback () const { return _send & SendRoute; }
	bool send_global_feedback () const { return _send & SendGlobal; }

	virtual void send_route_feedback (std::list<Route*>&) {}
	virtual void send_global_feedback () {}

  protected:

	ARDOUR::Session& session;
	SendWhat _send;
	std::string _name;
	int active_thread;
	int thread_request_pipe[2];
	pthread_t _thread;

	static void* _thread_work (void *);
	void* thread_work ();

	struct ThreadRequest {
	    enum Type {
		    Start,
		    Stop,
		    Quit
	    };
	};

	int init_thread();
	int start_thread ();
	int stop_thread ();
	void terminate_thread ();
	int  poke_thread (ThreadRequest::Type);
};

extern "C" {
	struct ControlProtocolDescriptor {
	    const char* name;
	    void*       ptr;
	    void*       module;
	    ControlProtocol* (*initialize)(ControlProtocolDescriptor*,Session*);
	    void             (*destroy)(ControlProtocolDescriptor*,ControlProtocol*);
	    
	};
}

}

#endif // ardour_control_protocols_h
