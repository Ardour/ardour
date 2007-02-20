#include "mackie_control_protocol.h"

#include "midi_byte_array.h"
#include "surface_port.h"

#include <pbd/pthread_utils.h>
#include <pbd/error.h>

#include <midi++/types.h>
#include <midi++/port.h>
#include <midi++/manager.h>
#include <midi++/port_request.h>
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
	return MIDI::Manager::instance()->port( default_port_name ) != 0;
}

void * MackieControlProtocol::monitor_work()
{
	cout << "MackieControlProtocol::monitor_work" << endl;
	// What does ThreadCreatedWithRequestSize do?
	PBD::ThreadCreated (pthread_self(), X_("Mackie"));

#if 0
	// it seems to do the "block" on poll less often
	// with this code disabled
	struct sched_param rtparam;
	memset (&rtparam, 0, sizeof (rtparam));
	rtparam.sched_priority = 9; /* XXX should be relative to audio (JACK) thread */
	
	int err;
	if ((err = pthread_setschedparam (pthread_self(), SCHED_FIFO, &rtparam)) != 0) {
		// do we care? not particularly.
		PBD::info << string_compose (_("%1: thread not running with realtime scheduling (%2)"), name(), strerror( errno )) << endmsg;
	} 
#endif
	
	pthread_setcancelstate (PTHREAD_CANCEL_ENABLE, 0);
	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, 0);

	// read from midi ports
	cout << "start poll cycle" << endl;
	while ( true )
	{
		update_ports();
		if ( poll_ports() )
		{
			try { read_ports(); }
			catch ( exception & e ) {
				cout << "MackieControlProtocol::poll_ports caught exception: " << e.what() << endl;
				_ports_changed = true;
				update_ports();
			}
		}
		// provide a cancellation point
		pthread_testcancel();
	}

	// these never get called
	cout << "MackieControlProtocol::poll_ports exiting" << endl;
	
	delete[] pfd;

	return (void*) 0;
}

void MackieControlProtocol::update_ports()
{
	// create pollfd structures if necessary
	if ( _ports_changed )
	{
		Glib::Mutex::Lock ul( update_mutex );
		// yes, this is a double-test locking paradigm, or whatever it's called
		// because we don't *always* need to acquire the lock for the first test
		if ( _ports_changed )
		{
			cout << "MackieControlProtocol::update_ports updating" << endl;
			if ( pfd != 0 ) delete[] pfd;
			// TODO This might be a memory leak. How does thread cancellation cleanup work?
			pfd = new pollfd[_ports.size()];
			nfds = 0;

			for( MackiePorts::iterator it = _ports.begin(); it != _ports.end(); ++it )
			{
				cout << "adding port " << (*it)->port().name() << " to pollfd" << endl;
				pfd[nfds].fd = (*it)->port().selectable();
				pfd[nfds].events = POLLIN|POLLHUP|POLLERR;
				++nfds;
			}
			_ports_changed = false;
		}
		cout << "MackieControlProtocol::update_ports signalling" << endl;
		update_cond.signal();
		cout << "MackieControlProtocol::update_ports finished" << endl;
	}
}

void MackieControlProtocol::read_ports()
{
	/* now read any data on the ports */
	Glib::Mutex::Lock lock( update_mutex );
	for ( int p = 0; p < nfds; ++p )
	{
		// this will cause handle_midi_any in the MackiePort to be triggered
		if ( pfd[p].revents & POLLIN > 0 )
		{
			lock.release();
			_ports[p]->read();
			lock.acquire();
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
		cout << "poll_ports no ports" << endl;
		usleep( no_ports_sleep * 1000 );
		return false;
	}

	int retval = poll( pfd, nfds, timeout );
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

void MackieControlProtocol::handle_port_changed( SurfacePort * port, bool active )
{
	cout << "MackieControlProtocol::handle_port_changed port: " << *port << " active: " << active << endl;
	if ( active == false )
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
	}
	else
	{
		_ports_changed = true;
		// port added
		update_ports();
		update_surface();
		
		// TODO update bank size
		
		// rebuild surface
	}
}

void MackieControlProtocol::handle_port_init( Mackie::SurfacePort * sport )
{
	cout << "MackieControlProtocol::handle_port_init" << endl;
	_ports_changed = true;
	update_ports();
	cout << "MackieControlProtocol::handle_port_init finished" << endl;
}
