#ifndef mackie_jog_wheel
#define mackie_jog_wheel

#include "timer.h"

#include <stack>
#include <deque>
#include <queue>

namespace ArdourSurface {

class US2400Protocol;

namespace US2400
{

class JogWheel
{
  public:
	enum Mode { scroll };

	JogWheel (US2400Protocol & mcp);

	/// As the wheel turns...
	void jog_event (float delta);
	void set_mode (Mode m);
	Mode mode() const { return _mode; }

private:
	US2400Protocol & _mcp;
	Mode _mode;
};

}
}

#endif
