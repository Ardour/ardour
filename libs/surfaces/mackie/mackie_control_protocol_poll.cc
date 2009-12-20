#include "mackie_control_protocol.h"

#include "midi_byte_array.h"
#include "surface_port.h"

#include "pbd/pthread_utils.h"
#include "pbd/error.h"

#include "midi++/types.h"
#include "midi++/port.h"
#include "midi++/manager.h"
#include "i18n.h"

#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <string.h>

#include <iostream>
#include <string>
#include <vector>

using namespace std;
using namespace Mackie;
using namespace PBD;

const char * MackieControlProtocol::default_port_name = "mcu";

bool MackieControlProtocol::probe()
{
	if ( MIDI::Manager::instance()->port(default_port_name)  == 0 ) {
		info << "Mackie: No MIDI port called " << default_port_name << endmsg;
		return false;
	} else {
		return true;
	}
}

void MackieControlProtocol::handle_port_inactive( SurfacePort * port )
{
	// port gone away. So stop polling it ASAP
	{
		// delete the port instance
		Glib::Mutex::Lock lock( update_mutex );
		MackiePorts::iterator it = find( _ports.begin(), _ports.end(), port );
		if ( it != _ports.end() )
		{
			delete *it;
			_ports.erase( it );
		}
	}

	// TODO all the rebuilding of surfaces and so on
}

void MackieControlProtocol::handle_port_active (SurfacePort *)
{
	// no need to re-add port because it was already added
	// during the init phase. So just update the local surface
	// representation and send the representation to 
	// all existing ports
	
	// TODO update bank size
	
	// TODO rebuild surface, to have new units
	
	// finally update session state to the surface
	// TODO but this is also done in set_active, and
	// in fact update_surface won't execute unless
#ifdef DEBUG
	cout << "update_surface in handle_port_active" << endl;
#endif
	// _active == true
	update_surface();
}

void MackieControlProtocol::handle_port_init (Mackie::SurfacePort *)
{
#ifdef DEBUG
	cout << "MackieControlProtocol::handle_port_init" << endl;
#endif
#ifdef DEBUG
	cout << "MackieControlProtocol::handle_port_init finish" << endl;
#endif
}
