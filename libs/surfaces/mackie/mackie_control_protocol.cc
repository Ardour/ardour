/*
	Copyright (C) 2006,2007 John Anderson

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	$Id: mackie_control_protocol.cc 1059 2006-11-02 12:27:49Z paul $
*/

#include <iostream>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <float.h>
#include <sys/time.h>
#include <errno.h>
#include <poll.h>

#include <boost/shared_array.hpp>

#include <midi++/types.h>
#include <midi++/port.h>
#include <midi++/manager.h>
#include <pbd/pthread_utils.h>
#include <pbd/error.h>

#include <ardour/route.h>
#include <ardour/session.h>
#include <ardour/location.h>
#include <ardour/dB.h>
#include <ardour/panner.h>

#include "mackie_control_protocol.h"

#include "midi_byte_array.h"
#include "mackie_control_exception.h"
#include "route_signal.h"
#include "mackie_midi_builder.h"
#include "surface_port.h"
#include "surface.h"
#include "bcf_surface.h"
#include "mackie_surface.h"

using namespace ARDOUR;
using namespace std;
using namespace sigc;
using namespace Mackie;
using namespace PBD;

using boost::shared_ptr;

#include "i18n.h"

MackieMidiBuilder builder;

// Copied from tranzport_control_protocol.cc
static inline double 
gain_to_slider_position (ARDOUR::gain_t g)
{
	if (g == 0) return 0;
	return pow((6.0*log(g)/log(2.0)+192.0)/198.0, 8.0);
}

/*
	Copied from tranzport_control_protocol.cc
	TODO this seems to return slightly wrong values, namely
	with the UI slider at max, we get a 0.99something value.
*/
static inline ARDOUR::gain_t 
slider_position_to_gain (double pos)
{
	/* XXX Marcus writes: this doesn't seem right to me. but i don't have a better answer ... */
	if (pos == 0.0) return 0;
	return pow (2.0,(sqrt(sqrt(sqrt(pos)))*198.0-192.0)/6.0);
}

MackieControlProtocol::MackieControlProtocol (Session& session)
	: ControlProtocol  (session, X_("Mackie"))
	, _current_initial_bank( 0 )
	, connections_back( _connections )
	, _surface( 0 )
	, _ports_changed( false )
	, pfd( 0 )
	, nfds( 0 )
{
	cout << "MackieControlProtocol::MackieControlProtocol" << endl;
	// will start reading from ports, as soon as there are some
	pthread_create_and_store (X_("mackie monitor"), &thread, 0, _monitor_work, this);
}

MackieControlProtocol::~MackieControlProtocol()
{
	cout << "~MackieControlProtocol::MackieControlProtocol" << endl;
	close();
}

Mackie::Surface & MackieControlProtocol::surface()
{
	if ( _surface == 0 )
	{
		throw MackieControlException( "_surface is 0 in MackieControlProtocol::surface" );
	}
	return *_surface;
}

const Mackie::MackiePort & MackieControlProtocol::mcu_port() const
{
	return dynamic_cast<const MackiePort &>( *_ports[0] );
}

Mackie::MackiePort & MackieControlProtocol::mcu_port()
{
	return dynamic_cast<const MackiePort &>( *_ports[0] );
}

// go to the previous track.
// Assume that get_sorted_routes().size() > route_table.size()
void MackieControlProtocol::prev_track()
{
	if ( _current_initial_bank >= 1 )
	{
		session->set_dirty();
		switch_banks( _current_initial_bank - 1 );
	}
}

// go to the next track.
// Assume that get_sorted_routes().size() > route_table.size()
void MackieControlProtocol::next_track()
{
	Sorted sorted = get_sorted_routes();
	if ( _current_initial_bank + route_table.size() < sorted.size() )
	{
		session->set_dirty();
		switch_banks( _current_initial_bank + 1 );
	}
}

void MackieControlProtocol::clear_route_signals()
{
	for( RouteSignals::iterator it = route_signals.begin(); it != route_signals.end(); ++it )
	{
		delete *it;
	}
	route_signals.clear();
}

// return the port for a given id - 0 based
// throws an exception if no port found
MackiePort & MackieControlProtocol::port_for_id( uint32_t index )
{
	uint32_t current_max = 0;
	for( MackiePorts::iterator it = _ports.begin(); it != _ports.end(); ++it )
	{
		current_max += (*it)->strips();
		if ( index < current_max ) return **it;
	}
	
	// oops - no matching port
	ostringstream os;
	os << "No port for index " << index;
	throw MackieControlException( os.str() );
}

// predicate for sort call in get_sorted_routes
struct RouteByRemoteId
{
	bool operator () ( const shared_ptr<Route> & a, const shared_ptr<Route> & b ) const
	{
		return a->remote_control_id() < b->remote_control_id();
	}

	bool operator () ( const Route & a, const Route & b ) const
	{
		return a.remote_control_id() < b.remote_control_id();
	}

	bool operator () ( const Route * a, const Route * b ) const
	{
		return a->remote_control_id() < b->remote_control_id();
	}
};

MackieControlProtocol::Sorted MackieControlProtocol::get_sorted_routes()
{
	Sorted sorted;
	
	// fetch all routes
	boost::shared_ptr<Session::RouteList> routes = session->get_routes();
	set<uint32_t> remote_ids;
	
	// routes with remote_id 0 should never be added
	// TODO verify this with ardour devs
	// remote_ids.insert( 0 );
	
	// sort in remote_id order, and exclude master, control and hidden routes
	// and any routes that are already set.
	for ( Session::RouteList::iterator it = routes->begin(); it != routes->end(); ++it )
	{
		Route & route = **it;
		if (
				route.active()
				&& !route.master()
				&& !route.hidden()
				&& !route.control()
				&& remote_ids.find( route.remote_control_id() ) == remote_ids.end()
		)
		{
			sorted.push_back( *it );
			remote_ids.insert( route.remote_control_id() );
		}
	}
	sort( sorted.begin(), sorted.end(), RouteByRemoteId() );
	return sorted;
}

void MackieControlProtocol::refresh_current_bank()
{
	switch_banks( _current_initial_bank );
}

void MackieControlProtocol::switch_banks( int initial )
{
	// DON'T prevent bank switch if initial == _current_initial_bank
	// because then this method can't be used as a refresh
	
	// sanity checking
	Sorted sorted = get_sorted_routes();
	int delta = sorted.size() - route_table.size();
	if ( initial < 0 || ( delta > 0 && initial > delta ) )
	{
		cout << "not switching to " << initial << endl;
		return;
	}
	_current_initial_bank = initial;
	
	// first clear the signals from old routes
	// taken care of by the RouteSignal destructors
	clear_route_signals();
	
	// now set the signals for new routes
	if ( _current_initial_bank <= sorted.size() )
	{
		// fetch the bank start and end to switch to
		uint32_t end_pos = min( route_table.size(), sorted.size() );
		Sorted::iterator it = sorted.begin() + _current_initial_bank;
		Sorted::iterator end = sorted.begin() + _current_initial_bank + end_pos;
		//cout << "switch to " << _current_initial_bank << ", " << end_pos << endl;
		
		// link routes to strips
		uint32_t i = 0;
		for ( ; it != end && it != sorted.end(); ++it, ++i )
		{
			boost::shared_ptr<Route> route = *it;
			Strip & strip = *surface().strips[i];
			//cout << "remote id " << route->remote_control_id() << " connecting " << route->name() << " to " << strip.name() << " with port " << port_for_id(i) << endl;
			route_table[i] = route;
			RouteSignal * rs = new RouteSignal( *route, *this, strip, port_for_id(i) );
			route_signals.push_back( rs );
			// update strip from route
			rs->notify_all();
		}
		
		// create dead strips if there aren't enough routes to
		// fill a bank
		for ( ; i < route_table.size(); ++i )
		{
			Strip & strip = *surface().strips[i];
			// send zero for this strip
			port_for_id(i).write( builder.zero_strip( strip ) );
		}
	}
	
	// display the current start bank.
	if ( mcu_port().emulation() == MackiePort::bcf2000 )
	{
		if ( _current_initial_bank == 0 )
		{
			// send Ar. to 2-char display on the master
			mcu_port().write( builder.two_char_display( "Ar", ".." ) );
		}
		else
		{
			// write the current first remote_id to the 2-char display
			mcu_port().write( builder.two_char_display( _current_initial_bank ) );
		}
	}
}

void MackieControlProtocol::zero_all()
{
	// TODO turn off 55-char and SMPTE displays
	
	if ( mcu_port().emulation() == MackiePort::bcf2000 )
	{
		// clear 2-char display
		mcu_port().write( builder.two_char_display( "LC" ) );
	}
	
	// zero all strips
	for ( Surface::Strips::iterator it = surface().strips.begin(); it != surface().strips.end(); ++it )
	{
		port_for_id( (*it)->index() ).write( builder.zero_strip( **it ) );
	}
	
	// and the master strip
	mcu_port().write( builder.zero_strip( master_strip() ) );
	
	// and the led ring for the master strip, in bcf mode
	if ( mcu_port().emulation() == MackiePort::bcf2000 )
	{
		Control & control = *surface().controls_by_name["jog"];
		mcu_port().write( builder.build_led_ring( dynamic_cast<Pot &>( control ), off ) );
	}
	
	// turn off global buttons and leds
	// global buttons are only ever on mcu_port, so we don't have
	// to figure out which port.
	for ( Surface::Controls::iterator it = surface().controls.begin(); it != surface().controls.end(); ++it )
	{
		Control & control = **it;
		if ( !control.group().is_strip() && control.accepts_feedback() )
		{
			mcu_port().write( builder.zero_control( control ) );
		}
	}
}

int MackieControlProtocol::set_active (bool yn)
{
	if ( yn != _active )
	{
		try
		{
			// the reason for the locking and unlocking is that
			// glibmm can't do a condition wait on a RecMutex
			if ( yn )
			{
				// TODO what happens if this fails half way?
				cout << "set_active true" << endl;
				
				// create MackiePorts
				{
					Glib::Mutex::Lock lock( update_mutex );
					create_ports();
				}
				
				// wait until poll thread is running, with ports to poll
				// the mutex is only there because conditions require a mutex
				{
					Glib::Mutex::Lock lock( update_mutex );
					while ( nfds == 0 ) update_cond.wait( update_mutex );
				}
				
				// now initialise MackiePorts - ie exchange sysex messages
				for( MackiePorts::iterator it = _ports.begin(); it != _ports.end(); ++it )
				{
					(*it)->open();
				}
				
				// wait until all ports are active
				// TODO a more sophisticate approach would
				// allow things to start up with only an MCU, even if
				// extenders were specified but not responding.
				for( MackiePorts::iterator it = _ports.begin(); it != _ports.end(); ++it )
				{
					(*it)->wait_for_init();
				}
				
				// create surface object. This depends on the ports being
				// correctly initialised
				initialize_surface();
				connect_session_signals();
				
				// yeehah!
				_active = true;
				
				// send current control positions to surface
				// must come after _active = true otherwise it won't run
				update_surface();
			}
			else
			{
				cout << "set_active false" << endl;
				close();
				_active = false;
			}
		}
		catch( exception & e )
		{
			cout << "set_active to false because " << e.what() << endl;
			_active = false;
			throw;
		}
	}

	return 0;
}

bool MackieControlProtocol::handle_strip_button( Control & control, ButtonState bs, boost::shared_ptr<Route> route )
{
	bool state = false;

	if ( bs == press )
	{
		if ( control.name() == "recenable" )
		{
			state = !route->record_enabled();
			route->set_record_enable( state, this );
		}
		else if ( control.name() == "mute" )
		{
			state = !route->muted();
			route->set_mute( state, this );
		}
		else if ( control.name() == "solo" )
		{
			state = !route->soloed();
			route->set_solo( state, this );
		}
		else if ( control.name() == "select" )
		{
			// TODO make the track selected. Whatever that means.
			//state = default_button_press( dynamic_cast<Button&>( control ) );
		}
		else if ( control.name() == "vselect" )
		{
			// TODO could be used to select different things to apply the pot to?
			//state = default_button_press( dynamic_cast<Button&>( control ) );
		}
	}
	
	if ( control.name() == "fader_touch" )
	{
		state = bs == press;
		control.strip().gain().touch( state );
	}
	
	return state;
}

void MackieControlProtocol::update_led( Mackie::Button & button, Mackie::LedState ls )
{
	MackiePort * port = 0;
	if ( button.group().is_strip() )
	{
		if ( button.group().is_master() )
		{
			port = &mcu_port();
		}
		else
		{
			port = &port_for_id( dynamic_cast<const Strip&>( button.group() ).index() );
		}
	}
	else
	{
		port = &mcu_port();
	}
	if ( ls != none ) port->write( builder.build_led( button, ls ) );
}

void MackieControlProtocol::update_global_button( const string & name, LedState ls )
{
	if ( surface().controls_by_name.find( name ) !=surface().controls_by_name.end() )
	{
		Button * button = dynamic_cast<Button*>( surface().controls_by_name[name] );
		mcu_port().write( builder.build_led( button->led(), ls ) );
	}
	else
	{
		cout << "Button " << name << " not found" << endl;
	}
}

// send messages to surface to set controls to correct values
void MackieControlProtocol::update_surface()
{
	if ( _active )
	{
		// do the initial bank switch to connect signals
		// _current_initial_bank is initialised by set_state
		switch_banks( _current_initial_bank );
		
		// create a RouteSignal for the master route
		// but only the first time around
		master_route_signal = shared_ptr<RouteSignal>( new RouteSignal( *master_route(), *this, master_strip(), mcu_port() ) );
		// update strip from route
		master_route_signal->notify_all();
		
		// update global buttons and displays
		notify_record_state_changed();
		notify_transport_state_changed();
	}
}

void MackieControlProtocol::connect_session_signals()
{
	// receive routes added
	connections_back = session->RouteAdded.connect( ( mem_fun (*this, &MackieControlProtocol::notify_route_added) ) );
	// receive record state toggled
	connections_back = session->RecordStateChanged.connect( ( mem_fun (*this, &MackieControlProtocol::notify_record_state_changed) ) );
	// receive transport state changed
	connections_back = session->TransportStateChange.connect( ( mem_fun (*this, &MackieControlProtocol::notify_transport_state_changed) ) );
	// receive punch-in and punch-out
	connections_back = Config->ParameterChanged.connect( ( mem_fun (*this, &MackieControlProtocol::notify_parameter_changed) ) );
	// receive rude solo changed
	connections_back = session->SoloActive.connect( ( mem_fun (*this, &MackieControlProtocol::notify_solo_active_changed) ) );
	
	// make sure remote id changed signals reach here
	// see also notify_route_added
	Sorted sorted = get_sorted_routes();
	for ( Sorted::iterator it = sorted.begin(); it != sorted.end(); ++it )
	{
		(*it)->RemoteControlIDChanged.connect( ( mem_fun (*this, &MackieControlProtocol::notify_remote_id_changed) ) );
	}
}

void MackieControlProtocol::add_port( MIDI::Port & midi_port, int number )
{
	MackiePort * sport = new MackiePort( *this, midi_port, number );
	_ports.push_back( sport );

	connections_back = sport->init_event.connect(
		sigc::bind (
			mem_fun (*this, &MackieControlProtocol::handle_port_init)
			, sport
		)
	);

	connections_back = sport->active_event.connect(
		sigc::bind (
			mem_fun (*this, &MackieControlProtocol::handle_port_changed)
			, sport
			, true
		)
	);

	connections_back = sport->inactive_event.connect(
		sigc::bind (
			mem_fun (*this, &MackieControlProtocol::handle_port_changed)
			, sport
			, false
		)
	);
	
	_ports_changed = true;
}

void MackieControlProtocol::create_ports()
{
	MIDI::Manager * mm = MIDI::Manager::instance();

	// open main port
	{
		MIDI::Port * midi_port = mm->port( default_port_name );

		if ( midi_port == 0 ) {
			ostringstream os;
			os << string_compose( _("no MIDI port named \"%1\" exists - Mackie control disabled"), default_port_name );
			error << os.str() << endmsg;
			throw MackieControlException( os.str() );
		}
		add_port( *midi_port, 0 );
	}
	
	// open extender ports. Up to 9. Should be enough.
	// could also use mm->get_midi_ports()
	string ext_port_base = "mcu_xt_";
	for ( int index = 1; index <= 9; ++index )
	{
		ostringstream os;
		os << ext_port_base << index;
		MIDI::Port * midi_port = mm->port( os.str() );
		if ( midi_port != 0 ) add_port( *midi_port, index );
	}
}

shared_ptr<Route> MackieControlProtocol::master_route()
{
	shared_ptr<Route> retval;
	retval = session->route_by_name( "master" );
	if ( retval == 0 )
	{
		// TODO search through all routes for one with the master attribute set
	}
	return retval;
}

Strip & MackieControlProtocol::master_strip()
{
	return dynamic_cast<Strip&>( *surface().groups["master"] );
}

void MackieControlProtocol::initialize_surface()
{
	// set up the route table
	int strips = 0;
	for( MackiePorts::iterator it = _ports.begin(); it != _ports.end(); ++it )
	{
		strips += (*it)->strips();
	}
	
	set_route_table_size( strips );
	
	switch ( mcu_port().emulation() )
	{
		case MackiePort::bcf2000: _surface = new BcfSurface( strips ); break;
		case MackiePort::mackie: _surface = new MackieSurface( strips ); break;
		default:
			ostringstream os;
			os << "no Surface class found for emulation: " << mcu_port().emulation();
			throw MackieControlException( os.str() );
	}
	_surface->init();
	
	// Connect events. Must be after route table otherwise there will be trouble
	for( MackiePorts::iterator it = _ports.begin(); it != _ports.end(); ++it )
	{
		(*it)->control_event.connect( ( mem_fun (*this, &MackieControlProtocol::handle_control_event) ) );
	}
}

void MackieControlProtocol::close()
{
	// TODO disconnect port active/inactive signals
	// Or at least put a lock here
	
	if ( _surface != 0 )
	{
		// These will fail if the port has gone away.
		// So catch the exception and do the rest of the
		// close afterwards
		// because the bcf doesn't respond to the next 3 sysex messages
		try
		{
			zero_all();
		}
		catch ( exception & e )
		{
			cout << "MackieControlProtocol::close caught exception: " << e.what() << endl;
		}
		
		for( MackiePorts::iterator it = _ports.begin(); it != _ports.end(); ++it )
		{
			try
			{
				MackiePort & port = **it;
				// faders to minimum
				port.write_sysex( 0x61 );
				// All LEDs off
				port.write_sysex( 0x62 );
				// Reset (reboot into offline mode)
				port.write_sysex( 0x63 );
			}
			catch ( exception & e )
			{
				cout << "MackieControlProtocol::close caught exception: " << e.what() << endl;
			}
		}
		
		// disconnect global signals from Session
		// TODO Since *this is a sigc::trackable, this shouldn't be necessary
		for( vector<sigc::connection>::iterator it = _connections.begin(); it != _connections.end(); ++it )
		{
			it->disconnect();
		}
		
		// disconnect routes from strips
		clear_route_signals();
	}
	
	// stop polling, and wait for it...
	pthread_cancel_one( thread );
	pthread_join( thread, 0 );
	
	// shut down MackiePorts
	for( MackiePorts::iterator it = _ports.begin(); it != _ports.end(); ++it )
	{
		delete *it;
	}
	_ports.clear();
	
	delete[] pfd;
}

void* MackieControlProtocol::_monitor_work (void* arg)
{
	return static_cast<MackieControlProtocol*>(arg)->monitor_work ();
}

XMLNode & MackieControlProtocol::get_state()
{
	cout << "MackieControlProtocol::get_state" << endl;
	
	// add name of protocol
	XMLNode* node = new XMLNode( X_("Protocol") );
	node->add_property( X_("name"), _name );
	
	// add current bank
	ostringstream os;
	os << _current_initial_bank;
	node->add_property( X_("bank"), os.str() );
	
	return *node;
}

int MackieControlProtocol::set_state( const XMLNode & node )
{
	cout << "MackieControlProtocol::set_state: active " << _active << endl;
	int retval = 0;
	
	// fetch current bank
	if ( node.property( X_("bank") ) != 0 )
	{
		string bank = node.property( X_("bank") )->value();
		try
		{
			set_active( true );
			uint32_t new_bank = atoi( bank.c_str() );
			if ( _current_initial_bank != new_bank ) switch_banks( new_bank );
		}
		catch ( exception & e )
		{
			cout << "exception in MackieControlProtocol::set_state: " << e.what() << endl;
			return -1;
		}
	}
	
	return retval;
}

void MackieControlProtocol::handle_control_event( SurfacePort & port, Control & control, const ControlState & state )
{
	uint32_t index = control.ordinal() - 1 + ( port.number() * port.strips() );
	boost::shared_ptr<Route> route;
	if ( control.group().is_strip() )
	{
		if ( control.group().is_master() )
		{
			route = master_route();
		}
		else if ( index < route_table.size() )
			route = route_table[index];
		else
			cerr << "Warning: index is " << index << " which is not in the route table, size: " << route_table.size() << endl;
	}
	
	// This handles control element events from the surface
	// the state of the controls on the surface is usually updated
	// from UI events.
	switch ( control.type() )
	{
		case Control::type_fader:
			if ( control.group().is_strip() )
			{
				// find the route in the route table for the id
				// if the route isn't available, skip it
				// at which point the fader should just reset itself
				if ( route != 0 )
				{
					route->set_gain( slider_position_to_gain( state.pos ), this );
					
					// must echo bytes back to slider now, because
					// the notifier only works if the fader is not being
					// touched. Which it is if we're getting input.
					port.write( builder.build_fader( (Fader&)control, state.pos ) );
				}
			}
			else
			{
				// master fader
				boost::shared_ptr<Route> route = master_route();
				if ( route )
				{
					route->set_gain( slider_position_to_gain( state.pos ), this );
					port.write( builder.build_fader( (Fader&)control, state.pos ) );
				}
			}
			break;
			
		case Control::type_button:
			if ( control.group().is_strip() )
			{
				// strips
				if ( route != 0 )
				{
					handle_strip_button( control, state.button_state, route );
				}
				else
				{
					// no route so always switch the light off
					// because no signals will be emitted by a non-route
					port.write( builder.build_led( control.led(), off ) );
				}
			}
			else if ( control.group().is_master() )
			{
				// master fader touch
				boost::shared_ptr<Route> route = master_route();
				if ( route )
					handle_strip_button( control, state.button_state, route );
			}
			else
			{
				// handle all non-strip buttons
				surface().handle_button( *this, state.button_state, dynamic_cast<Button&>( control ) );
			}
			break;
			
		// pot (jog wheel, external control)
		case Control::type_pot:
			if ( control.group().is_strip() )
			{
				if ( route != 0 )
				{
					if ( route->panner().size() == 1 )
					{
						// assume pan for now
						float xpos;
						route->panner()[0]->get_effective_position (xpos);
						
						// calculate new value, and trim
						xpos += state.delta;
						if ( xpos > 1.0 )
							xpos = 1.0;
						else if ( xpos < 0.0 )
							xpos = 0.0;
						
						route->panner()[0]->set_position( xpos );
					}
				}
				else
				{
					// it's a pot for an umnapped route, so turn all the lights off
					port.write( builder.build_led_ring( dynamic_cast<Pot &>( control ), off ) );
				}
			}
			else
			{
				if ( control.name() == "jog" )
				{
					// TODO use current snap-to setting?
					long delta = state.ticks * 1000;
					nframes_t next = session->transport_frame() + delta;
					if ( delta < 0 && session->transport_frame() < (nframes_t) abs( delta )  )
					{
						next = session->current_start_frame();
					}
					else if ( next > session->current_end_frame() )
					{
						next = session->current_end_frame();
					}
					
					// moves jerkily and doesn't really work. eventually core-dumps
					session->request_locate( next, session->transport_rolling() );
					// ditto
					// ScrollTimeline( state.ticks );
					// mtaht says maybe snap-to code has some ideas
					
					// turn off the led ring, for bcf emulation mode
					port.write( builder.build_led_ring( dynamic_cast<Pot &>( control ), off ) );
				}
				else
				{
					cout << "external controller" << state.ticks << endl;
				}
			}
			break;
			
		default:
			cout << "Control::type not handled: " << control.type() << endl;
	}
}

/////////////////////////////////////////////////////////
// Notifications from UI
/////////////////////////////////////////////////////////
struct RouteSignalByRoute
{
	RouteSignalByRoute( Route & route ): _route( route ) {}
		
	bool operator () ( const RouteSignal & rs ) const
	{
		return rs.route().id() == _route.id();
	}
	
	bool operator () ( const RouteSignal * rs ) const
	{
		return rs->route().id() == _route.id();
	}

	Route & _route;
};

Strip & MackieControlProtocol::strip_from_route( Route * route )
{
	if ( route == 0 )
	{
		throw MackieControlException( "route is null in MackieControlProtocol::strip_from_route" );
	}
	
	if ( route->master() )
	{
		return master_strip();
	}
	
	RouteSignals::iterator it = find_if(
		route_signals.begin()
		, route_signals.end()
		, RouteSignalByRoute( *route )
	);
	
	if ( it == route_signals.end() )
	{
		ostringstream os;
		os << "No strip for route " << route->name();
		throw MackieControlException( os.str() );
	}
	
	return (*it)->strip();
}

/////////////////////////////////////////////////
// handlers for Route signals
// TODO should these be part of RouteSignal?
/////////////////////////////////////////////////

void MackieControlProtocol::notify_solo_changed( ARDOUR::Route * route, MackiePort * port )
{
	try
	{
		Button & button = strip_from_route( route ).solo();
		port->write( builder.build_led( button, route->soloed() ) );
	}
	catch( exception & e )
	{
		cout << e.what() << endl;
	}
}

void MackieControlProtocol::notify_mute_changed( ARDOUR::Route * route, MackiePort * port )
{
	try
	{
		Button & button = strip_from_route( route ).mute();
		port->write( builder.build_led( button, route->muted() ) );
	}
	catch( exception & e )
	{
		cout << e.what() << endl;
	}
}

void MackieControlProtocol::notify_record_enable_changed( ARDOUR::Route * route, MackiePort * port )
{
	try
	{
		Button & button = strip_from_route( route ).recenable();
		port->write( builder.build_led( button, route->record_enabled() ) );
	}
	catch( exception & e )
	{
		cout << e.what() << endl;
	}
}

void MackieControlProtocol::notify_gain_changed(  ARDOUR::Route * route, MackiePort * port )
{
	try
	{
		Fader & fader = strip_from_route( route ).gain();
		if ( !fader.touch() )
		{
			port->write( builder.build_fader( fader, gain_to_slider_position( route->gain() ) ) );
		}
	}
	catch( exception & e )
	{
		cout << e.what() << endl;
	}
}

void MackieControlProtocol::notify_name_changed( void *, ARDOUR::Route * route, MackiePort * port )
{
	try
	{
		// TODO implement MackieControlProtocol::notify_name_changed
	}
	catch( exception & e )
	{
		cout << e.what() << endl;
	}
}

// TODO deal with > 1 channel being panned
void MackieControlProtocol::notify_panner_changed( ARDOUR::Route * route, MackiePort * port )
{
	try
	{
		Pot & pot = strip_from_route( route ).vpot();
		if ( route->panner().size() == 1 )
		{
			float pos;
			route->panner()[0]->get_effective_position( pos);
			port->write( builder.build_led_ring( pot, ControlState( on, pos ) ) );
		}
		else
		{
			port->write( builder.zero_control( pot ) );
		}
	}
	catch( exception & e )
	{
		cout << e.what() << endl;
	}
}

/////////////////////////////////////
// Transport Buttons
/////////////////////////////////////

LedState MackieControlProtocol::rewind_press( Button & button )
{
	// can use first_mark_before/after as well
	Location * loc = session->locations()->first_location_before (
		session->transport_frame()
	);
	if ( loc != 0 ) session->request_locate( loc->start(), session->transport_rolling() );
	return on;
}

LedState MackieControlProtocol::rewind_release( Button & button )
{
	return off;
}

LedState MackieControlProtocol::ffwd_press( Button & button )
{
	// can use first_mark_before/after as well
	Location * loc = session->locations()->first_location_after (
		session->transport_frame()
	);
	if ( loc != 0 ) session->request_locate( loc->start(), session->transport_rolling() );
	return on;
}

LedState MackieControlProtocol::ffwd_release( Button & button )
{
	return off;
}

LedState MackieControlProtocol::stop_press( Button & button )
{
	session->request_stop();
	return on;
}

LedState MackieControlProtocol::stop_release( Button & button )
{
	return session->transport_stopped();
}

LedState MackieControlProtocol::play_press( Button & button )
{
	session->request_transport_speed( 1.0 );
	return on;
}

LedState MackieControlProtocol::play_release( Button & button )
{
	return session->transport_rolling();
}

LedState MackieControlProtocol::record_press( Button & button )
{
	if ( session->get_record_enabled() )
		session->disable_record( false );
	else
		session->maybe_enable_record();
	return on;
}

LedState MackieControlProtocol::record_release( Button & button )
{
	if ( session->get_record_enabled() )
	{
		if ( session->transport_rolling() )
			return on;
		else
			return flashing;
	}
	else
		return off;
}

///////////////////////////////////////////
// Session signals
///////////////////////////////////////////

void MackieControlProtocol::notify_parameter_changed( const char * name_str )
{
	string name( name_str );
	if ( name == "punch-in" )
	{
		update_global_button( "punch_in", Config->get_punch_in() );
	}
	else if ( name == "punch-out" )
	{
		update_global_button( "punch_out", Config->get_punch_out() );
	}
}

// RouteList is the set of routes that have just been added
void MackieControlProtocol::notify_route_added( ARDOUR::Session::RouteList & rl )
{
	// currently assigned banks are less than the full set of
	// strips, so activate the new strip now.
	if ( route_signals.size() < route_table.size() )
	{
		refresh_current_bank();
	}
	// otherwise route added, but current bank needs no updating
	
	// make sure remote id changes in the new route are handled
	typedef ARDOUR::Session::RouteList ARS;
	for ( ARS::iterator it = rl.begin(); it != rl.end(); ++it )
	{
		(*it)->RemoteControlIDChanged.connect( ( mem_fun (*this, &MackieControlProtocol::notify_remote_id_changed) ) );
	}
}

void MackieControlProtocol::notify_solo_active_changed( bool active )
{
	Button * rude_solo = reinterpret_cast<Button*>( surface().controls_by_name["solo"] );
	mcu_port().write( builder.build_led( *rude_solo, active ? flashing : off ) );
}

void MackieControlProtocol::notify_remote_id_changed()
{
	Sorted sorted = get_sorted_routes();
	
	// if a remote id has been moved off the end, we need to shift
	// the current bank backwards.
	if ( sorted.size() - _current_initial_bank < route_signals.size() )
	{
		// but don't shift backwards past the zeroth channel
		switch_banks( max( (unsigned int)0, sorted.size() - route_signals.size() ) );
	}
	// Otherwise just refresh the current bank
	else
	{
		refresh_current_bank();
	}
}

///////////////////////////////////////////
// Transport signals
///////////////////////////////////////////

void MackieControlProtocol::notify_record_state_changed()
{
	// switch rec button on / off / flashing
	Button * rec = reinterpret_cast<Button*>( surface().controls_by_name["record"] );
	mcu_port().write( builder.build_led( *rec, record_release( *rec ) ) );
}

void MackieControlProtocol::notify_transport_state_changed()
{
	// switch various play and stop buttons on / off
	update_global_button( "play", session->transport_rolling() );
	update_global_button( "stop", !session->transport_rolling() );
	update_global_button( "loop", session->get_play_loop() );
	
	// rec is special because it's tristate
	Button * rec = reinterpret_cast<Button*>( surface().controls_by_name["record"] );
	mcu_port().write( builder.build_led( *rec, record_release( *rec ) ) );
}

LedState MackieControlProtocol::loop_press( Button & button )
{
	session->request_play_loop( !session->get_play_loop() );
	return on;
}

LedState MackieControlProtocol::loop_release( Button & button )
{
	return session->get_play_loop();
}

LedState MackieControlProtocol::punch_in_press( Button & button )
{
	bool state = !Config->get_punch_in();
	Config->set_punch_in( state );
	return state;
}

LedState MackieControlProtocol::punch_in_release( Button & button )
{
	return Config->get_punch_in();
}

LedState MackieControlProtocol::punch_out_press( Button & button )
{
	bool state = !Config->get_punch_out();
	Config->set_punch_out( state );
	return state;
}

LedState MackieControlProtocol::punch_out_release( Button & button )
{
	return Config->get_punch_out();
}

LedState MackieControlProtocol::home_press( Button & button )
{
	session->goto_start();
	return on;
}

LedState MackieControlProtocol::home_release( Button & button )
{
	return off;
}

LedState MackieControlProtocol::end_press( Button & button )
{
	session->goto_end();
	return on;
}

LedState MackieControlProtocol::end_release( Button & button )
{
	return off;
}

/////////////////////////////////////
// Bank Switching
/////////////////////////////////////
LedState MackieControlProtocol::left_press( Button & button )
{
	Sorted sorted = get_sorted_routes();
	if ( sorted.size() > route_table.size() )
	{
		int new_initial = _current_initial_bank - route_table.size();
		if ( new_initial < 0 ) new_initial = 0;
		if ( new_initial != int( _current_initial_bank ) )
		{
			session->set_dirty();
			switch_banks( new_initial );
		}
		
		return on;
	}
	else
	{
		return flashing;
	}
}

LedState MackieControlProtocol::left_release( Button & button )
{
	return off;
}

LedState MackieControlProtocol::right_press( Button & button )
{
	Sorted sorted = get_sorted_routes();
	if ( sorted.size() > route_table.size() )
	{
		uint32_t delta = sorted.size() - ( route_table.size() + _current_initial_bank );
		if ( delta > route_table.size() ) delta = route_table.size();
		if ( delta > 0 )
		{
			session->set_dirty();
			switch_banks( _current_initial_bank + delta );
		}
		
		return on;
	}
	else
	{
		return flashing;
	}
}

LedState MackieControlProtocol::right_release( Button & button )
{
	return off;
}

LedState MackieControlProtocol::channel_left_press( Button & button )
{
	Sorted sorted = get_sorted_routes();
	if ( sorted.size() > route_table.size() )
	{
		prev_track();
		return on;
	}
	else
	{
		return flashing;
	}
}

LedState MackieControlProtocol::channel_left_release( Button & button )
{
	return off;
}

LedState MackieControlProtocol::channel_right_press( Button & button )
{
	Sorted sorted = get_sorted_routes();
	if ( sorted.size() > route_table.size() )
	{
		next_track();
		return on;
	}
	else
	{
		return flashing;
	}
}

LedState MackieControlProtocol::channel_right_release( Button & button )
{
	return off;
}
