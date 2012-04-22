#ifndef mackie_jog_wheel
#define mackie_jog_wheel

#include "timer.h"

#include <stack>
#include <deque>
#include <queue>

class MackieControlProtocol;

namespace Mackie
{

class JogWheel
{
  public:
	enum Mode { scroll };
	
	JogWheel (MackieControlProtocol & mcp);

	/// As the wheel turns...
	void jog_event (float delta);
	void set_mode (Mode m);
	Mode mode() const { return _mode; }

private:
	MackieControlProtocol & _mcp;
	Mode _mode;
};

}

#endif
