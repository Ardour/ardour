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
*/

#include <iostream>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>
#include <iomanip>

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
#include <pbd/memento_command.h>
#include <pbd/convert.h>

#include <ardour/route.h>
#include <ardour/session.h>
#include <ardour/location.h>
#include <ardour/dB.h>
#include <ardour/panner.h>
#include <ardour/tempo.h>
#include <ardour/types.h>

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

MackieControlProtocol::MackieControlProtocol (Session& session)
	: ControlProtocol  (session, X_("Mackie"))
	, _current_initial_bank( 0 )
	, connections_back( _connections )
	, _surface( 0 )
	, _ports_changed( false )
	, _polling( true )
	, pfd( 0 )
	, nfds( 0 )
	, _jog_wheel( *this )
	, _timecode_type( ARDOUR::AnyTime::BBT )
{
#ifdef DEBUG
	cout << "MackieControlProtocol::MackieControlProtocol" << endl;
#endif
	// will start reading from ports, as soon as there are some
	pthread_create_and_store (X_("mackie monitor"), &thread, 0, _monitor_work, this);
}

MackieControlProtocol::~MackieControlProtocol()
{
#ifdef DEBUG
	cout << "~MackieControlProtocol::MackieControlProtocol" << endl;
#endif
	try
	{
		close();
	}
	catch ( exception & e )
	{
		cout << "~MackieControlProtocol caught " << e.what() << endl;
	}
	catch ( ... )
	{
		cout << "~MackieControlProtocol caught unknown" << endl;
	}
#ifdef DEBUG
	cout << "finished ~MackieControlProtocol::MackieControlProtocol" << endl;
#endif
}

Mackie::Surface & MackieControlProtocol::surface()
{
	if ( _surface == 0 )
	{
		throw MackieControlException( "_surface is 0 in MackieControlProtocol::surface" );
	}
	return *_surface;
}

const Mackie::SurfacePort & MackieControlProtocol::mcu_port() const
{
	if ( _ports.size() < 1 )
	{
		return _dummy_port;
	}
	else
	{
		return dynamic_cast<const MackiePort &>( *_ports[0] );
	}
}

Mackie::SurfacePort & MackieControlProtocol::mcu_port()
{
	if ( _ports.size() < 1 )
	{
		return _dummy_port;
	}
	else
	{
		return dynamic_cast<MackiePort &>( *_ports[0] );
	}
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
#ifdef DEBUG
		cout << "not switching to " << initial << endl;
#endif
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
#ifdef DEBUG
		cout << "switch to " << _current_initial_bank << ", " << end_pos << endl;
#endif
		
		// link routes to strips
		uint32_t i = 0;
		for ( ; it != end && it != sorted.end(); ++it, ++i )
		{
			boost::shared_ptr<Route> route = *it;
			Strip & strip = *surface().strips[i];
#ifdef DEBUG
			cout << "remote id " << route->remote_control_id() << " connecting " << route->name() << " to " << strip.name() << " with port " << port_for_id(i) << endl;
#endif
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
			MackiePort & port = port_for_id(i);
			port.write( builder.zero_strip( port, strip ) );
		}
	}
	
	// display the current start bank.
	surface().display_bank_start( mcu_port(), builder, _current_initial_bank );
}

void MackieControlProtocol::zero_all()
{
	// TODO turn off SMPTE displays
	
	// zero all strips
	for ( Surface::Strips::iterator it = surface().strips.begin(); it != surface().strips.end(); ++it )
	{
		MackiePort & port = port_for_id( (*it)->index() );
		port.write( builder.zero_strip( port, **it ) );
	}
	
	// and the master strip
	mcu_port().write( builder.zero_strip( dynamic_cast<MackiePort&>( mcu_port() ), master_strip() ) );
	
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

	// any hardware-specific stuff
	surface().zero_all( mcu_port(), builder );
}

int MackieControlProtocol::set_active( bool yn )
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
				
				// create MackiePorts
				{
					Glib::Mutex::Lock lock( update_mutex );
					create_ports();
				}
				
				// make sure the ports are being listened to
				update_ports();
				
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
				// TODO a more sophisticated approach would
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
				close();
				_active = false;
			}
		}
		catch( exception & e )
		{
#ifdef DEBUG
			cout << "set_active to false because exception caught: " << e.what() << endl;
#endif
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
		control.strip().gain().in_use( state );
	}
	
	return state;
}

void MackieControlProtocol::update_led( Mackie::Button & button, Mackie::LedState ls )
{
	if ( ls != none )
	{
		SurfacePort * port = 0;
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
		port->write( builder.build_led( button, ls ) );
	}
}

void MackieControlProtocol::update_smpte_beats_led()
{
	switch ( _timecode_type )
	{
		case ARDOUR::AnyTime::BBT:
			update_global_led( "beats", on );
			update_global_led( "smpte", off );
			break;
		case ARDOUR::AnyTime::SMPTE:
			update_global_led( "smpte", on );
			update_global_led( "beats", off );
			break;
		default:
			ostringstream os;
			os << "Unknown Anytime::Type " << _timecode_type;
			throw runtime_error( os.str() );
	}
}

void MackieControlProtocol::update_global_button( const string & name, LedState ls )
{
	if ( surface().controls_by_name.find( name ) != surface().controls_by_name.end() )
	{
		Button * button = dynamic_cast<Button*>( surface().controls_by_name[name] );
		mcu_port().write( builder.build_led( button->led(), ls ) );
	}
	else
	{
#ifdef DEBUG
		cout << "Button " << name << " not found" << endl;
#endif
	}
}

void MackieControlProtocol::update_global_led( const string & name, LedState ls )
{
	if ( surface().controls_by_name.find( name ) != surface().controls_by_name.end() )
	{
		Led * led = dynamic_cast<Led*>( surface().controls_by_name[name] );
		mcu_port().write( builder.build_led( *led, ls ) );
	}
	else
	{
#ifdef DEBUG
		cout << "Led " << name << " not found" << endl;
#endif
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
		
		// sometimes the jog wheel is a pot
		surface().blank_jog_ring( mcu_port(), builder );
		
		// update global buttons and displays
		notify_record_state_changed();
		notify_transport_state_changed();
		update_smpte_beats_led();
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
		connections_back = (*it)->RemoteControlIDChanged.connect( ( mem_fun (*this, &MackieControlProtocol::notify_remote_id_changed) ) );
	}
}

void MackieControlProtocol::add_port( MIDI::Port & midi_port, int number )
{
#ifdef DEBUG
	cout << "add port " << midi_port.name() << ", " << midi_port.device() << ", " << midi_port.type() << endl;
	cout << "MIDI::Port::ALSA_Sequencer " << MIDI::Port::ALSA_Sequencer << endl;
	cout << "MIDI::Port::Unknown " << MIDI::Port::Unknown << endl;
#endif
	if ( string( midi_port.device() ) == string( "ardour" ) )
	{
		throw MackieControlException( "The Mackie MCU driver will not use a port with device=ardour" );
	}
	else if ( midi_port.type() == MIDI::Port::ALSA_Sequencer )
	{
		throw MackieControlException( "alsa/sequencer ports don't work with the Mackie MCU driver right now" );
	}
	else
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
				mem_fun (*this, &MackieControlProtocol::handle_port_active)
				, sport
			)
		);

		connections_back = sport->inactive_event.connect(
			sigc::bind (
				mem_fun (*this, &MackieControlProtocol::handle_port_inactive)
				, sport
			)
		);
		
		_ports_changed = true;
	}
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
	
	// TODO same as code in mackie_port.cc
	string emulation = ARDOUR::Config->get_mackie_emulation();
	if ( emulation == "bcf" )
	{
		_surface = new BcfSurface( strips );
	}
	else if ( emulation == "mcu" )
	{
		_surface = new MackieSurface( strips );
	}
	else
	{
		ostringstream os;
		os << "no Surface class found for emulation: " << emulation;
		throw MackieControlException( os.str() );
	}

	_surface->init();
	
	// Connect events. Must be after route table otherwise there will be trouble
	for( MackiePorts::iterator it = _ports.begin(); it != _ports.end(); ++it )
	{
		connections_back = (*it)->control_event.connect( ( mem_fun (*this, &MackieControlProtocol::handle_control_event) ) );
	}
}

void MackieControlProtocol::close()
{
	// stop polling, and wait for it...
	// must be before other shutdown otherwise polling loop
	// calls methods on objects that are deleted
	_polling = false;
	pthread_join( thread, 0 );
	
	// TODO disconnect port active/inactive signals
	// Or at least put a lock here
	
	// disconnect global signals from Session
	// TODO Since *this is a sigc::trackable, this shouldn't be necessary
	// but it is for some reason
#if 0
	for( vector<sigc::connection>::iterator it = _connections.begin(); it != _connections.end(); ++it )
	{
		it->disconnect();
	}
#endif
	
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
#ifdef DEBUG
			cout << "MackieControlProtocol::close caught exception: " << e.what() << endl;
#endif
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
#ifdef DEBUG
				cout << "MackieControlProtocol::close caught exception: " << e.what() << endl;
#endif
			}
		}
		
		// disconnect routes from strips
		clear_route_signals();
		
		delete _surface;
		_surface = 0;
	}
	
	// shut down MackiePorts
	for( MackiePorts::iterator it = _ports.begin(); it != _ports.end(); ++it )
	{
		delete *it;
	}
	_ports.clear();
	
	// this is done already in monitor_work. But it's here so we know.
	delete[] pfd;
	pfd = 0;
	nfds = 0;
}

void* MackieControlProtocol::_monitor_work (void* arg)
{
	return static_cast<MackieControlProtocol*>(arg)->monitor_work ();
}

XMLNode & MackieControlProtocol::get_state()
{
#ifdef DEBUG
	cout << "MackieControlProtocol::get_state" << endl;
#endif
	
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
#ifdef DEBUG
	cout << "MackieControlProtocol::set_state: active " << _active << endl;
#endif
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
#ifdef DEBUG
			cout << "exception in MackieControlProtocol::set_state: " << e.what() << endl;
#endif
			return -1;
		}
	}
	
	return retval;
}

void MackieControlProtocol::handle_control_event( SurfacePort & port, Control & control, const ControlState & state )
{
	// find the route for the control, if there is one
	boost::shared_ptr<Route> route;
	if ( control.group().is_strip() )
	{
		if ( control.group().is_master() )
		{
			route = master_route();
		}
		else
		{
			uint32_t index = control.ordinal() - 1 + ( port.number() * port.strips() );
			if ( index < route_table.size() )
				route = route_table[index];
			else
				cerr << "Warning: index is " << index << " which is not in the route table, size: " << route_table.size() << endl;
		}
	}
	
	// This handles control element events from the surface
	// the state of the controls on the surface is usually updated
	// from UI events.
	switch ( control.type() )
	{
		case Control::type_fader:
			// find the route in the route table for the id
			// if the route isn't available, skip it
			// at which point the fader should just reset itself
			if ( route != 0 )
			{
				route->gain_control().set_value( state.pos );
				
				// must echo bytes back to slider now, because
				// the notifier only works if the fader is not being
				// touched. Which it is if we're getting input.
				port.write( builder.build_fader( (Fader&)control, state.pos ) );
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
				if ( route != 0 )
				{
					handle_strip_button( control, state.button_state, route );
				}
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
					// pan for mono input routes, or stereo linked panners
					if ( route->panner().size() == 1 || ( route->panner().size() == 2 && route->panner().linked() ) )
					{
						// assume pan for now
						float xpos;
						route->panner()[0]->get_effective_position (xpos);
						
						// calculate new value, and trim
						xpos += state.delta * state.sign;
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
				if ( control.is_jog() )
				{
					_jog_wheel.jog_event( port, control, state );
				}
				else
				{
					cout << "external controller" << state.ticks * state.sign << endl;
				}
			}
			break;
			
		default:
			cout << "Control::type not handled: " << control.type() << endl;
	}
}

/////////////////////////////////////////////////
// handlers for Route signals
// TODO should these be part of RouteSignal?
// They started off as sigc handlers for signals
// from Route, but they're also used in polling for automation
/////////////////////////////////////////////////

void MackieControlProtocol::notify_solo_changed( RouteSignal * route_signal )
{
	try
	{
		Button & button = route_signal->strip().solo();
		route_signal->port().write( builder.build_led( button, route_signal->route().soloed() ) );
	}
	catch( exception & e )
	{
		cout << e.what() << endl;
	}
}

void MackieControlProtocol::notify_mute_changed( RouteSignal * route_signal )
{
	try
	{
		Button & button = route_signal->strip().mute();
		route_signal->port().write( builder.build_led( button, route_signal->route().muted() ) );
	}
	catch( exception & e )
	{
		cout << e.what() << endl;
	}
}

void MackieControlProtocol::notify_record_enable_changed( RouteSignal * route_signal )
{
	try
	{
		Button & button = route_signal->strip().recenable();
		route_signal->port().write( builder.build_led( button, route_signal->route().record_enabled() ) );
	}
	catch( exception & e )
	{
		cout << e.what() << endl;
	}
}

void MackieControlProtocol::notify_active_changed( RouteSignal * route_signal )
{
	try
	{
#ifdef DEBUG
		cout << "MackieControlProtocol::notify_active_changed" << endl;
#endif
		refresh_current_bank();
	}
	catch( exception & e )
	{
		cout << e.what() << endl;
	}
}
	
void MackieControlProtocol::notify_gain_changed( RouteSignal * route_signal, bool force_update )
{
	try
	{
		Fader & fader = route_signal->strip().gain();
		if ( !fader.in_use() )
		{
			float gain_value = route_signal->route().gain_control().get_value();
			// check that something has actually changed
			if ( force_update || gain_value != route_signal->last_gain_written() )
			{
				route_signal->port().write( builder.build_fader( fader, gain_value ) );
				route_signal->last_gain_written( gain_value );
			}
		}
	}
	catch( exception & e )
	{
		cout << e.what() << endl;
	}
}

void MackieControlProtocol::notify_name_changed( void *, RouteSignal * route_signal )
{
	try
	{
		Strip & strip = route_signal->strip();
		if ( !strip.is_master() )
		{
			string line1;
			string fullname = route_signal->route().name();
			
			if ( fullname.length() <= 6 )
			{
				line1 = fullname;
			}
			else
			{
				line1 = PBD::short_version( fullname, 6 );
			}
			
			SurfacePort & port = route_signal->port();
			port.write( builder.strip_display( port, strip, 0, line1 ) );
			port.write( builder.strip_display_blank( port, strip, 1 ) );
		}
	}
	catch( exception & e )
	{
		cout << e.what() << endl;
	}
}

void MackieControlProtocol::notify_panner_changed( RouteSignal * route_signal, bool force_update )
{
	try
	{
		Pot & pot = route_signal->strip().vpot();
		const Panner & panner = route_signal->route().panner();
		if ( panner.size() == 1 || ( panner.size() == 2 && panner.linked() ) )
		{
			float pos;
			route_signal->route().panner()[0]->get_effective_position( pos );
			
			// cache the MidiByteArray here, because the mackie led control is much lower
			// resolution than the panner control. So we save lots of byte
			// sends in spite of more work on the comparison
			MidiByteArray bytes = builder.build_led_ring( pot, ControlState( on, pos ), MackieMidiBuilder::midi_pot_mode_dot );
			// check that something has actually changed
			if ( force_update || bytes != route_signal->last_pan_written() )
			{
				route_signal->port().write( bytes );
				route_signal->last_pan_written( bytes );
			}
		}
		else
		{
			route_signal->port().write( builder.zero_control( pot ) );
		}
	}
	catch( exception & e )
	{
		cout << e.what() << endl;
	}
}

// TODO handle plugin automation polling
void MackieControlProtocol::update_automation( RouteSignal & rs )
{
	ARDOUR::AutoState gain_state = rs.route().gain_automation_state();
	if ( gain_state == Touch || gain_state == Play )
	{
		notify_gain_changed( &rs, false );
	}
	
	ARDOUR::AutoState panner_state = rs.route().panner().automation_state();
	if ( panner_state == Touch || panner_state == Play )
	{
		notify_panner_changed( &rs, false );
	}
	_automation_last.start();
}

string MackieControlProtocol::format_bbt_timecode( nframes_t now_frame )
{
	BBT_Time bbt_time;
	session->bbt_time( now_frame, bbt_time );
	
	// According to the Logic docs
	// digits: 888/88/88/888
	// BBT mode: Bars/Beats/Subdivisions/Ticks
	ostringstream os;
	os << setw(3) << setfill('0') << bbt_time.bars;
	os << setw(2) << setfill('0') << bbt_time.beats;
	
	// figure out subdivisions per beat
	const Meter & meter = session->tempo_map().meter_at( now_frame );
	int subdiv = 2;
	if ( meter.note_divisor() == 8 && (meter.beats_per_bar() == 12.0 || meter.beats_per_bar() == 9.0 || meter.beats_per_bar() == 6.0) )
	{
		subdiv = 3;
	}
	
	uint32_t subdivisions = bbt_time.ticks / uint32_t( Meter::ticks_per_beat / subdiv );
	uint32_t ticks = bbt_time.ticks % uint32_t( Meter::ticks_per_beat / subdiv );
	
	os << setw(2) << setfill('0') << subdivisions + 1;
	os << setw(3) << setfill('0') << ticks;
	
	return os.str();
}

string MackieControlProtocol::format_smpte_timecode( nframes_t now_frame )
{
	SMPTE::Time smpte;
	session->smpte_time( now_frame, smpte );

	// According to the Logic docs
	// digits: 888/88/88/888
	// SMPTE mode: Hours/Minutes/Seconds/Frames
	ostringstream os;
	os << setw(3) << setfill('0') << smpte.hours;
	os << setw(2) << setfill('0') << smpte.minutes;
	os << setw(2) << setfill('0') << smpte.seconds;
	os << setw(3) << setfill('0') << smpte.frames;
	
	return os.str();
}

void MackieControlProtocol::update_timecode_display()
{
	if ( surface().has_timecode_display() )
	{
		// do assignment here so current_frame is fixed
		nframes_t current_frame = session->transport_frame();
		string timecode;
		
		switch ( _timecode_type )
		{
			case ARDOUR::AnyTime::BBT:
				timecode = format_bbt_timecode( current_frame );
				break;
			case ARDOUR::AnyTime::SMPTE:
				timecode = format_smpte_timecode( current_frame );
				break;
			default:
				ostringstream os;
				os << "Unknown timecode: " << _timecode_type;
				throw runtime_error( os.str() );
		}	
		
		// only write the timecode string to the MCU if it's changed
		// since last time. This is to reduce midi bandwidth used.
		if ( timecode != _timecode_last )
		{
			surface().display_timecode( mcu_port(), builder, timecode, _timecode_last );
			_timecode_last = timecode;
		}
	}
}

void MackieControlProtocol::poll_session_data()
{
	if ( _active && _automation_last.elapsed() >= 20 )
	{
		// do all currently mapped routes
		for( RouteSignals::iterator it = route_signals.begin(); it != route_signals.end(); ++it )
		{
			update_automation( **it );
		}
		
		// and the master strip
		if ( master_route_signal != 0 )
		{
			update_automation( *master_route_signal );
		}
		
		update_timecode_display();
		
		_automation_last.start();
	}
}

/////////////////////////////////////
// Transport Buttons
/////////////////////////////////////

LedState MackieControlProtocol::frm_left_press( Button & button )
{
	// can use first_mark_before/after as well
	unsigned long elapsed = _frm_left_last.restart();
	
	Location * loc = session->locations()->first_location_before (
		session->transport_frame()
	);
	
	// allow a quick double to go past a previous mark 
	if ( session->transport_rolling() && elapsed < 500 && loc != 0 )
	{
		Location * loc_two_back = session->locations()->first_location_before ( loc->start() );
		if ( loc_two_back != 0 )
		{
			loc = loc_two_back;
		}
	}
	
	// move to the location, if it's valid
	if ( loc != 0 )
	{
		session->request_locate( loc->start(), session->transport_rolling() );
	}
	
	return on;
}

LedState MackieControlProtocol::frm_left_release( Button & button )
{
	return off;
}

LedState MackieControlProtocol::frm_right_press( Button & button )
{
	// can use first_mark_before/after as well
	Location * loc = session->locations()->first_location_after (
		session->transport_frame()
	);
	if ( loc != 0 ) session->request_locate( loc->start(), session->transport_rolling() );
	return on;
}

LedState MackieControlProtocol::frm_right_release( Button & button )
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

LedState MackieControlProtocol::rewind_press( Button & button )
{
	_jog_wheel.push( JogWheel::speed );
	_jog_wheel.transport_direction( -1 );
	session->request_transport_speed( -_jog_wheel.transport_speed() );
	return on;
}

LedState MackieControlProtocol::rewind_release( Button & button )
{
	_jog_wheel.pop();
	_jog_wheel.transport_direction( 0 );
	if ( _transport_previously_rolling )
		session->request_transport_speed( 1.0 );
	else
		session->request_stop();
	return off;
}

LedState MackieControlProtocol::ffwd_press( Button & button )
{
	_jog_wheel.push( JogWheel::speed );
	_jog_wheel.transport_direction( 1 );
	session->request_transport_speed( _jog_wheel.transport_speed() );
	return on;
}

LedState MackieControlProtocol::ffwd_release( Button & button )
{
	_jog_wheel.pop();
	_jog_wheel.transport_direction( 0 );
	if ( _transport_previously_rolling )
		session->request_transport_speed( 1.0 );
	else
		session->request_stop();
	return off;
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

LedState MackieControlProtocol::clicking_press( Button & button )
{
	bool state = !Config->get_clicking();
	Config->set_clicking( state );
	return state;
}

LedState MackieControlProtocol::clicking_release( Button & button )
{
	return Config->get_clicking();
}

LedState MackieControlProtocol::global_solo_press( Button & button )
{
	bool state = !session->soloing();
	session->set_all_solo ( state );
	return state;
}

LedState MackieControlProtocol::global_solo_release( Button & button )
{
	return session->soloing();
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
	else if ( name == "clicking" )
	{
		update_global_button( "clicking", Config->get_clicking() );
	}
	else
	{
#ifdef DEBUG
		cout << "parameter changed: " << name << endl;
#endif
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
		connections_back = (*it)->RemoteControlIDChanged.connect( ( mem_fun (*this, &MackieControlProtocol::notify_remote_id_changed) ) );
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
		switch_banks( max((Sorted::size_type) 0, sorted.size() - route_signals.size() ) );
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
	
	_transport_previously_rolling = session->transport_rolling();
	
	// rec is special because it's tristate
	Button * rec = reinterpret_cast<Button*>( surface().controls_by_name["record"] );
	mcu_port().write( builder.build_led( *rec, record_release( *rec ) ) );
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

/////////////////////////////////////
// Functions
/////////////////////////////////////
LedState MackieControlProtocol::marker_press( Button & button )
{
	// cut'n'paste from LocationUI::add_new_location()
	string markername;
	nframes_t where = session->audible_frame();
	session->locations()->next_available_name(markername,"mcu");
	Location *location = new Location (where, where, markername, Location::IsMark);
	session->begin_reversible_command (_("add marker"));
	XMLNode &before = session->locations()->get_state();
	session->locations()->add (location, true);
	XMLNode &after = session->locations()->get_state();
	session->add_command (new MementoCommand<Locations>(*(session->locations()), &before, &after));
	session->commit_reversible_command ();
	return on;
}

LedState MackieControlProtocol::marker_release( Button & button )
{
	return off;
}

void jog_wheel_state_display( JogWheel::State state, SurfacePort & port )
{
	switch( state )
	{
		case JogWheel::zoom: port.write( builder.two_char_display( "Zm" ) ); break;
		case JogWheel::scroll: port.write( builder.two_char_display( "Sc" ) ); break;
		case JogWheel::scrub: port.write( builder.two_char_display( "Sb" ) ); break;
		case JogWheel::shuttle: port.write( builder.two_char_display( "Sh" ) ); break;
		case JogWheel::speed: port.write( builder.two_char_display( "Sp" ) ); break;
		case JogWheel::select: port.write( builder.two_char_display( "Se" ) ); break;
	}
}

Mackie::LedState MackieControlProtocol::zoom_press( Mackie::Button & )
{
	_jog_wheel.zoom_state_toggle();
	update_global_button( "scrub", _jog_wheel.jog_wheel_state() == JogWheel::scrub );
	jog_wheel_state_display( _jog_wheel.jog_wheel_state(), mcu_port() );
	return _jog_wheel.jog_wheel_state() == JogWheel::zoom;
}

Mackie::LedState MackieControlProtocol::zoom_release( Mackie::Button & )
{
	return _jog_wheel.jog_wheel_state() == JogWheel::zoom;
}

Mackie::LedState MackieControlProtocol::scrub_press( Mackie::Button & )
{
	_jog_wheel.scrub_state_cycle();
	update_global_button( "zoom", _jog_wheel.jog_wheel_state() == JogWheel::zoom );
	jog_wheel_state_display( _jog_wheel.jog_wheel_state(), mcu_port() );
	return
		_jog_wheel.jog_wheel_state() == JogWheel::scrub
		||
		_jog_wheel.jog_wheel_state() == JogWheel::shuttle
	;
}

Mackie::LedState MackieControlProtocol::scrub_release( Mackie::Button & )
{
	return
		_jog_wheel.jog_wheel_state() == JogWheel::scrub
		||
		_jog_wheel.jog_wheel_state() == JogWheel::shuttle
	;
}

LedState MackieControlProtocol::drop_press( Button & button )
{
	session->remove_last_capture();
	return on;
}

LedState MackieControlProtocol::drop_release( Button & button )
{
	return off;
}

LedState MackieControlProtocol::save_press( Button & button )
{
	session->save_state( "" );
	return on;
}

LedState MackieControlProtocol::save_release( Button & button )
{
	return off;
}

LedState MackieControlProtocol::smpte_beats_press( Button & )
{
	switch ( _timecode_type )
	{
		case ARDOUR::AnyTime::BBT:
			_timecode_type = ARDOUR::AnyTime::SMPTE;
			break;
		case ARDOUR::AnyTime::SMPTE:
			_timecode_type = ARDOUR::AnyTime::BBT;
			break;
		default:
			ostringstream os;
			os << "Unknown Anytime::Type " << _timecode_type;
			throw runtime_error( os.str() );
	}
	update_smpte_beats_led();
	return on;
}

LedState MackieControlProtocol::smpte_beats_release( Button & )
{
	return off;
}
