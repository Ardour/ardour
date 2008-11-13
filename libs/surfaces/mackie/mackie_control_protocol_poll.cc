#include "mackie_control_protocol.h"

#include "midi_byte_array.h"
#include "surface_port.h"

#include <pbd/pthread_utils.h>
#include <pbd/error.h>

#include <midi++/types.h>
#include <midi++/port.h>
#include <midi++/manager.h>
#include "i18n.h"

#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>

#include <iostream>
#include <string>
#include <vector>

using namespace std;
using namespace Mackie;
using namespace PBD;

const char * MackieControlProtocol::default_port_name = "mcu";

bool MackieControlProtocol::probe()
{
	if ( MIDI::Manager::instance()->port( default_port_name ) == 0 )
	{
		error << "No port called mcu. Add it to ardour.rc." << endmsg;
		return false;
	}
	else
	{
		return true;
	}
}

void * MackieControlProtocol::monitor_work()
{
	PBD::notify_gui_about_thread_creation (pthread_self(), X_("Mackie"));

	pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, 0);
	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, 0);

	// read from midi ports
	while ( _polling )
	{
		try
		{
			if ( poll_ports() )
			{
				try { read_ports(); }
				catch ( exception & e ) {
					cout << "MackieControlProtocol::poll_ports caught exception: " << e.what() << endl;
					_ports_changed = true;
					update_ports();
				}
			}
			// poll for session data that needs to go to the unit
			poll_session_data();
		}
		catch ( exception & e )
		{
			cout << "caught exception in MackieControlProtocol::monitor_work " << e.what() << endl;
		}
	}

	// TODO ports and pfd and nfds should be in a separate class
	delete[] pfd;
	pfd = 0;
	nfds = 0;

	return (void*) 0;
}

void MackieControlProtocol::update_ports()
{
#ifdef DEBUG
	cout << "MackieControlProtocol::update_ports" << endl;
#endif
	if ( _ports_changed )
	{
		Glib::Mutex::Lock ul( update_mutex );
		// yes, this is a double-test locking paradigm, or whatever it's called
		// because we don't *always* need to acquire the lock for the first test
#ifdef DEBUG
		cout << "MackieControlProtocol::update_ports lock acquired" << endl;
#endif
		if ( _ports_changed )
		{
			// create new pollfd structures
			if ( pfd != 0 )
			{
				delete[] pfd;
				pfd = 0;
			}
			pfd = new pollfd[_ports.size()];
#ifdef DEBUG
			cout << "pfd: " << pfd << endl;
#endif
			nfds = 0;
			for( MackiePorts::iterator it = _ports.begin(); it != _ports.end(); ++it )
			{
				// add the port any handler
				(*it)->connect_any();
#ifdef DEBUG
				cout << "adding pollfd for port " << (*it)->port().name() << " to pollfd " << nfds << endl;
#endif
				pfd[nfds].fd = (*it)->port().selectable();
				pfd[nfds].events = POLLIN|POLLHUP|POLLERR;
				++nfds;
			}
			_ports_changed = false;
		}
#ifdef DEBUG
		cout << "MackieControlProtocol::update_ports signal" << endl;
#endif
		update_cond.signal();
	}
#ifdef DEBUG
	cout << "MackieControlProtocol::update_ports finish" << endl;
#endif
}

void MackieControlProtocol::read_ports()
{
	/* now read any data on the ports */
	Glib::Mutex::Lock lock( update_mutex );
	for ( int p = 0; p < nfds; ++p )
	{
		// this will cause handle_midi_any in the MackiePort to be triggered
		// for alsa/raw ports
		// alsa/sequencer ports trigger the midi parser off poll
		if ( (pfd[p].revents & POLLIN) > 0 )
		{
			// avoid deadlocking?
			// doesn't seem to make a difference
			//lock.release();
			_ports[p]->read();
			//lock.acquire();
		}
	}
}

bool MackieControlProtocol::poll_ports()
{
	int timeout = 10; // milliseconds
	int no_ports_sleep = 1000; // milliseconds

	Glib::Mutex::Lock lock( update_mutex );
	// if there are no ports
	if ( nfds < 1 )
	{
		lock.release();
#ifdef DEBUG
		cout << "poll_ports no ports" << endl;
#endif
		usleep( no_ports_sleep * 1000 );
		return false;
	}

	int retval = ::poll( pfd, nfds, timeout );
	if ( retval < 0 )
	{
		// gdb at work, perhaps
		if ( errno != EINTR )
		{
			error << string_compose(_("Mackie MIDI thread poll failed (%1)"), strerror( errno ) ) << endmsg;
		}
		return false;
	}
	
	return retval > 0;
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
	_ports_changed = true;
	update_ports();
	
	// TODO all the rebuilding of surfaces and so on
}

void MackieControlProtocol::handle_port_active( SurfacePort * port )
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

void MackieControlProtocol::handle_port_init( Mackie::SurfacePort * sport )
{
#ifdef DEBUG
	cout << "MackieControlProtocol::handle_port_init" << endl;
#endif
	_ports_changed = true;
	update_ports();
#ifdef DEBUG
	cout << "MackieControlProtocol::handle_port_init finish" << endl;
#endif
}
