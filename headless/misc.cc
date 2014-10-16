#include <iostream>
#include <cstdlib>

#include "misc.h"

void
TestReceiver::receive (Transmitter::Channel chn, const char * str)
{
	const char *prefix = "";
	
	switch (chn) {
	case Transmitter::Error:
		prefix = ": [ERROR]: ";
		break;
	case Transmitter::Info:
		/* ignore */
		return;
	case Transmitter::Warning:
		prefix = ": [WARNING]: ";
		break;
	case Transmitter::Fatal:
		prefix = ": [FATAL]: ";
		break;
	case Transmitter::Throw:
		/* this isn't supposed to happen */
		abort ();
	}
	
	/* note: iostreams are already thread-safe: no external
	   lock required.
	*/
	
	std::cout << prefix << str << std::endl;
	
	if (chn == Transmitter::Fatal) {
		::exit (9);
	}
}

/* temporarily required due to some code design confusion (Feb 2014) */

#include "ardour/vst_types.h"

int vstfx_init (void*) { return 0; }
void vstfx_exit () {}
void vstfx_destroy_editor (VSTState*) {}
