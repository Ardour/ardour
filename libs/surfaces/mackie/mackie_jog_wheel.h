#ifndef mackie_jog_wheel
#define mackie_jog_wheel

#include "timer.h"

#include <stack>
#include <deque>
#include <queue>

class MackieControlProtocol;

namespace Mackie
{

class SurfacePort;
class Control;
class ControlState;

/**
	A jog wheel can be used to control many things. This
	handles all of the states and state transitions.
	
	Mainly it exists to avoid putting a bunch of messy
	stuff in MackieControlProtocol.
	
	But it doesn't really know who it is, with stacks, queues and various
	boolean state variables.
*/
class JogWheel
{
public:
	enum State { scroll, zoom, speed, scrub, shuttle, select };
	
	JogWheel (MackieControlProtocol & mcp);

	/// As the wheel turns...
	void jog_event (SurfacePort & port, Control & control, float delta);
	
	// These are for incoming button presses that change the internal state
	// but they're not actually used at the moment.
	void zoom_event (SurfacePort & port, Control & control, const ControlState & state);
	void scrub_event (SurfacePort & port, Control & control, const ControlState & state);
	void speed_event (SurfacePort & port, Control & control, const ControlState & state);
	void scroll_event (SurfacePort & port, Control & control, const ControlState & state);

	/// Return the current jog wheel mode, which defaults to Scroll
	State jog_wheel_state() const;
	
	/// The current transport speed for ffwd and rew. Can be
	/// set by wheel when they're pressed.
	float transport_speed() const { return _transport_speed; }
	
	/// one of -1,0,1
	int transport_direction() const { return _transport_direction; }
	void transport_direction (int rhs) { _transport_direction = rhs; }
	
	void push (State state);
	void pop();
	
	/// Turn zoom mode on and off
	void zoom_state_toggle();
	
	/**
		Cycle scrub -> shuttle -> previous
	*/
	State scrub_state_cycle();
	
	/// Check to see when the last scrub event was
	/// And stop scrubbing if it was too long ago.
	/// Intended to be called from a periodic timer of 
	/// some kind.
	void check_scrubbing();

protected:
	void add_scrub_interval (unsigned long elapsed);
	float average_scrub_interval();
	float std_dev_scrub_interval();

private:
	MackieControlProtocol & _mcp;

	/// transport speed for ffwd and rew, controller by jog
	float _transport_speed;
	int _transport_direction;

	/// Speed for shuttle
	float _shuttle_speed;
	
	/// a stack for keeping track of states
	std::stack<State> _jog_wheel_states;

	/// So we know how fast to set the transport speed while scrubbing
	Timer _scrub_timer;

	/// to keep track of what the current scrub rate is
	/// so we can calculate a moving average
	std::deque<unsigned long> _scrub_intervals;
};

}

#endif
