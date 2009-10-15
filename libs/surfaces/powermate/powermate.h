#ifndef ardour_powermate_h
#define ardour_powermate_h

#include <sys/time.h>
#include <pthread.h>

#include "control_protocol/control_protocol.h"

class PowermateControlProtocol : public ARDOUR::ControlProtocol
{
  public:
	PowermateControlProtocol (ARDOUR::Session&);
	virtual ~PowermateControlProtocol();

	int set_active (bool yn);
	static bool probe ();

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

  private:
	
	static void* SerialThreadEntry (void* arg);
	void* SerialThread ();
	
	void	ProcessEvent(struct input_event *ev);
	
	int			mPort;
	pthread_t		mThread;

};


#endif
