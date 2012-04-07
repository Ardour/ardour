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
#include "controls.h"
#include "types.h"
#include "mackie_midi_builder.h"

#include <iostream>
#include <iomanip>
#include <sstream>

using namespace Mackie;
using namespace std;

uint32_t Control::button_cnt = 0;
uint32_t Control::pot_cnt = 0;
uint32_t Control::fader_cnt = 0;
uint32_t Control::led_cnt = 0;
uint32_t Control::jog_cnt = 0;

void Group::add( Control & control )
{
	_controls.push_back( &control );
}

Strip::Strip( const std::string & name, int index )
	: Group( name )
	, _solo( 0 )
	, _recenable( 0 )
	, _mute( 0 )
	, _select( 0 )
	, _vselect( 0 )
	, _fader_touch( 0 )
	, _vpot( 0 )
	, _gain( 0 )
	, _index( index )
{
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

Control::Control( int id, int ordinal, std::string name, Group & group )
: _id( id )
, _ordinal( ordinal )
, _name( name )
, _group( group )
, _in_use( false )
{
}

/**
 generated with

controls[1].each do |x|
  puts <<EOF
#{x.class.name} & Strip::#{x.name}()
{
	if ( _#{x.name} == 0 )
		throw MackieControlException( "#{x.name} is null" );
	return *_#{x.name};
}
EOF
end
*/
Fader & Strip::gain()
{
	if ( _gain == 0 )
		throw MackieControlException( "gain is null" );
	return *_gain;
}
Pot & Strip::vpot()
{
	if ( _vpot == 0 )
		throw MackieControlException( "vpot is null" );
	return *_vpot;
}
Button & Strip::recenable()
{
	if ( _recenable == 0 )
		throw MackieControlException( "recenable is null" );
	return *_recenable;
}
Button & Strip::solo()
{
	if ( _solo == 0 )
		throw MackieControlException( "solo is null" );
	return *_solo;
}
Button & Strip::mute()
{
	if ( _mute == 0 )
		throw MackieControlException( "mute is null" );
	return *_mute;
}
Button & Strip::select()
{
	if ( _select == 0 )
		throw MackieControlException( "select is null" );
	return *_select;
}
Button & Strip::vselect()
{
	if ( _vselect == 0 )
		throw MackieControlException( "vselect is null" );
	return *_vselect;
}
Button & Strip::fader_touch()
{
	if ( _fader_touch == 0 )
		throw MackieControlException( "fader_touch is null" );
	return *_fader_touch;
}

/** @return true if the control is in use, or false otherwise.
    Buttons are `in use' when they are held down.
    Faders with touch support are `in use' when they are being touched.
    Pots, or faders without touch support, are `in use' from the first move
    event until a timeout after the last move event.
*/
bool
Control::in_use () const
{
	return _in_use;
}

void
Control::set_in_use (bool in_use)
{
	_in_use = in_use;
}

ostream & Mackie::operator << ( ostream & os, const Mackie::Control & control )
{
	os << typeid( control ).name();
	os << " { ";
	os << "name: " << control.name();
	os << ", ";
	os << "id: " << "0x" << setw(4) << setfill('0') << hex << control.id() << setfill(' ');
	os << ", ";
	os << "type: " << "0x" << setw(2) << setfill('0') << hex << control.type() << setfill(' ');
	os << ", ";
	os << "raw_id: " << "0x" << setw(2) << setfill('0') << hex << control.raw_id() << setfill(' ');
	os << ", ";
	os << "ordinal: " << dec << control.ordinal();
	os << ", ";
	os << "group: " << control.group().name();
	os << " }";
	
	return os;
}

std::ostream & Mackie::operator << ( std::ostream & os, const Strip & strip )
{
	os << typeid( strip ).name();
	os << " { ";
	os << "has_solo: " << boolalpha << strip.has_solo();
	os << ", ";
	os << "has_recenable: " << boolalpha << strip.has_recenable();
	os << ", ";
	os << "has_mute: " << boolalpha << strip.has_mute();
	os << ", ";
	os << "has_select: " << boolalpha << strip.has_select();
	os << ", ";
	os << "has_vselect: " << boolalpha << strip.has_vselect();
	os << ", ";
	os << "has_fader_touch: " << boolalpha << strip.has_fader_touch();
	os << ", ";
	os << "has_vpot: " << boolalpha << strip.has_vpot();
	os << ", ";
	os << "has_gain: " << boolalpha << strip.has_gain();
	os << " }";
	
	return os;
}
