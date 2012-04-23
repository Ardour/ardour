/*
	powermate.cc
	Ben Loftis
	Created: 03/26/07 20:07:56
*/


#include <linux/input.h>
#include <cstring>
#include <cerrno>
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>

#include <glibmm.h>

#include "pbd/pthread_utils.h"
#include "pbd/xml++.h"
#include "pbd/error.h"

#include "ardour/debug.h"

#include "powermate.h"
#include "i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace sigc;
using namespace PBD;

#define NUM_VALID_PREFIXES 2

static const char *valid_prefix[NUM_VALID_PREFIXES] = {
  "Griffin PowerMate",
  "Griffin SoundKnob"
};

#define NUM_EVENT_DEVICES 16

int open_powermate (const char *dev, int mode)
{
	if (!Glib::file_test (dev, Glib::FILE_TEST_EXISTS)) {
		return -1;
	}
	
	int fd = open(dev, mode);
	int i;
	char name[255];
	
	if (fd < 0) {
		if (errno != EACCES) {
			error << string_compose ("Unable to open \"%1\": %2", dev, strerror(errno)) << endmsg;
		}
		return -1;
	}

	/* placate valgrind */
	name[0] = '\0';

	if (ioctl (fd, EVIOCGNAME (sizeof(name)), name) < 0) {
		error << string_compose ("\"%1\": EVIOCGNAME failed: %2", dev, strerror(errno)) << endmsg;
		close (fd);
		return -1;
	}

	// it's the correct device if the prefix matches what we expect it to be:
	for (i = 0; i < NUM_VALID_PREFIXES; ++i) {
		if (!strncasecmp (name, valid_prefix[i], strlen (valid_prefix[i]))) {
			return fd;
		}
	}
	
	close (fd);
	return -1;
}

int find_powermate(int mode)
{
	char devname[256];
	int i, r;
	
	for (i = 0; i < NUM_EVENT_DEVICES; i++) {
		sprintf (devname, "/dev/input/event%d", i);
		r = open_powermate (devname, mode);
		if (r >= 0) {
			return r;
		}
	}
	
	return -1;
}

PowermateControlProtocol::PowermateControlProtocol (Session& s)
	: ControlProtocol  (s, "powermate")
{
}

PowermateControlProtocol::~PowermateControlProtocol ()
{
	set_active (false);
}

bool
PowermateControlProtocol::probe ()
{
	int port = find_powermate( O_RDONLY ); 

	if (port < 0) {
		if (errno == ENOENT) {
			DEBUG_TRACE (DEBUG::ControlProtocols, "Powermate device not found; perhaps you have no powermate connected");
		} else {
			DEBUG_TRACE (DEBUG::ControlProtocols, string_compose ("powermate: Opening of powermate failed - %1\n", strerror(errno)));
		}
		return false;
	}

	close (port);
	return true;
}

int
PowermateControlProtocol::set_active (bool inActivate)
{
	if (inActivate != _active) {

		if (inActivate) {

			mPort = find_powermate(O_RDONLY);
			
			if ( mPort < 0 ) {
				return -1;
			}
			
			if (pthread_create_and_store ("Powermate", &mThread, SerialThreadEntry, this) == 0) {
				_active = true;
			} else {
				return -1;
			}

			printf("Powermate Control Protocol activated\n");

		} else {
			pthread_cancel (mThread);
			close (mPort);
			_active = false;
			printf("Powermate Control Protocol deactivated\n");
		} 
	}

	return 0;
}

XMLNode&
PowermateControlProtocol::get_state () 
{
	XMLNode* node = new XMLNode (X_("Protocol"));
	node->add_property (X_("name"), _name);
	return *node;
}

int
PowermateControlProtocol::set_state (const XMLNode& /*node*/, int /*version*/)
{
	return 0;
}


void*
PowermateControlProtocol::SerialThreadEntry (void* arg)
{
	static_cast<PowermateControlProtocol*>(arg)->register_thread ("Powermate");
	return static_cast<PowermateControlProtocol*>(arg)->SerialThread ();
}

#define BUFFER_SIZE 32

bool held = false;
bool skippingMarkers = false;

void
PowermateControlProtocol::ProcessEvent(struct input_event *ev)
{
#ifdef VERBOSE
  fprintf(stderr, "type=0x%04x, code=0x%04x, value=%d\n",
	  ev->type, ev->code, (int)ev->value);
#endif

  switch(ev->type){
  case EV_MSC:
    printf("The LED pulse settings were changed; code=0x%04x, value=0x%08x\n", ev->code, ev->value);
    break;
  case EV_REL:
    if(ev->code != REL_DIAL)
      fprintf(stderr, "Warning: unexpected rotation event; ev->code = 0x%04x\n", ev->code);
    else{
    	if (held) {
		//click and hold to skip forward and back by markers
    		skippingMarkers = true;;
		if (ev->value > 0)
    			next_marker();
		else
			prev_marker();
    	} else {
		//scale the range so that we can go from +/-8x within 180 degrees, with less precision at the higher speeds 
    		float speed = get_transport_speed();
    	 	speed += (float)ev->value * 0.05;
		if (speed > 1.5 || speed < -1.5 )
			speed += ev->value;
		set_transport_speed( speed );
    	}
    }
    break;
  case EV_KEY:
    if(ev->code != BTN_0)
      fprintf(stderr, "Warning: unexpected key event; ev->code = 0x%04x\n", ev->code);
    else
      if (ev->value)
		held = true;
      else {
		held = false;
		if (skippingMarkers) {
			skippingMarkers = false;
		} else {
			if (get_transport_speed() == 0.0) {
				set_transport_speed(1.0);
			} else {
				set_transport_speed(0.0);
			}
		}
	}
    break;
  }

  fflush(stdout);
}

void*
PowermateControlProtocol::SerialThread ()
{
  struct input_event ibuffer[BUFFER_SIZE];
  int r, events, i;

  while(1){
    r = read(mPort, ibuffer, sizeof(struct input_event) * BUFFER_SIZE);
    if( r > 0 ){
		events = r / sizeof(struct input_event);
      for(i=0; i<events; i++)
		ProcessEvent(&ibuffer[i]);
    }else{
      fprintf(stderr, "read() failed: %s\n", strerror(errno));
      return (void*) 0;
    }
  }

	return (void*) 0;
}


