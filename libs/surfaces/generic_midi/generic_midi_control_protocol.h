#ifndef ardour_generic_midi_control_protocol_h
#define ardour_generic_midi_control_protocol_h

#include <set>
#include <glibmm/thread.h>
#include <ardour/types.h>

#include <control_protocol/control_protocol.h>

namespace MIDI {
	class Port;
}

namespace PBD {
	class Controllable;
}	

namespace ARDOUR {
	class Session;
}

class MIDIControllable;

class GenericMidiControlProtocol : public ARDOUR::ControlProtocol {
  public:
	GenericMidiControlProtocol (ARDOUR::Session&);
	virtual ~GenericMidiControlProtocol();

	int set_active (bool yn);
	static bool probe() { return true; }

	MIDI::Port* port () const { return _port; }
	void set_feedback_interval (ARDOUR::microseconds_t);

	int set_feedback (bool yn);
	bool get_feedback () const;

	XMLNode& get_state ();
	int set_state (const XMLNode&);

  private:
	MIDI::Port* _port;
	ARDOUR::microseconds_t _feedback_interval;
	ARDOUR::microseconds_t last_feedback_time;

	bool  do_feedback;
	void _send_feedback ();
	void  send_feedback ();

	typedef std::set<MIDIControllable*> MIDIControllables;
	MIDIControllables controllables;
	MIDIControllables pending_controllables;
	Glib::Mutex controllables_lock;
	Glib::Mutex pending_lock;

	bool start_learning (PBD::Controllable*);
	void stop_learning (PBD::Controllable*);

	void learning_stopped (MIDIControllable*);
};

#endif /* ardour_generic_midi_control_protocol_h */
