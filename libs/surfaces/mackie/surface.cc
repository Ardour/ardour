#include "surface.h"

#include <sstream>
#include <iomanip>
#include <iostream>

using namespace std;
using namespace Mackie;

Surface::Surface( uint32_t max_strips, uint32_t unit_strips )
: _max_strips( max_strips ), _unit_strips( unit_strips )
{
}

void Surface::init()
{
	init_controls();
	init_strips( _max_strips, _unit_strips );
}

Surface::~Surface()
{
	// delete groups
	for( Groups::iterator it = groups.begin(); it != groups.end(); ++it )
	{
		delete it->second;
	}
	
	// delete controls
	for( Controls::iterator it = controls.begin(); it != controls.end(); ++it )
	{
		delete *it;
	}
}

// Mackie-specific, because of multiple devices on separate ports
// add the strips from 9..max_strips
// unit_strips is the number of strips for additional units.
void Surface::init_strips( uint32_t max_strips, uint32_t unit_strips )
{
	if ( strips.size() < max_strips )
	{
		strips.resize( max_strips );
		for ( uint32_t i = strips.size(); i < max_strips; ++i )
		{
			// because I can't find itoa
			ostringstream os;
			os << "strip_" << i + 1;
			string name = os.str();
			
			// shallow copy existing strip
			// which works because the controls
			// have the same ids across units
			Strip * strip = new Strip( *strips[i % unit_strips] );
			
			// update the relevant values
			strip->index( i );
			strip->name( name );
			
			// add to data structures
			groups[name] = strip;
			strips[i] = strip;
		}
	}
}

/**
	TODO could optimise this to use enum, but it's only
	called during the protocol class instantiation.

	generated using

	irb -r controls.rb
	sf=Surface.new
	sf.parse
	controls = sf.groups.find{|x| x[0] =~ /strip/}.each{|x| puts x[1]}
	controls[1].each {|x| puts "\telse if ( control.name() == \"#{x.name}\" )\n\t{\n\t\t_#{x.name} = reinterpret_cast<#{x.class.name}*>(&control);\n\t}\n"}
*/
void Strip::add( Control & control )
{
	Group::add( control );
	if ( control.name() == "gain" )
	{
		_gain = reinterpret_cast<Fader*>(&control);
	}
	else if ( control.name() == "vpot" )
	{
		_vpot = reinterpret_cast<Pot*>(&control);
	}
	else if ( control.name() == "recenable" )
	{
		_recenable = reinterpret_cast<Button*>(&control);
	}
	else if ( control.name() == "solo" )
	{
		_solo = reinterpret_cast<Button*>(&control);
	}
	else if ( control.name() == "mute" )
	{
		_mute = reinterpret_cast<Button*>(&control);
	}
	else if ( control.name() == "select" )
	{
		_select = reinterpret_cast<Button*>(&control);
	}
	else if ( control.name() == "vselect" )
	{
		_vselect = reinterpret_cast<Button*>(&control);
	}
	else if ( control.name() == "fader_touch" )
	{
		_fader_touch = reinterpret_cast<Button*>(&control);
	}
	else if ( control.type() == Control::type_led || control.type() == Control::type_led_ring )
	{
		// do nothing
		cout << "Strip::add not adding " << control << endl;
	}
	else
	{
		ostringstream os;
		os << "Strip::add: unknown control type " << control;
		throw MackieControlException( os.str() );
	}
}

