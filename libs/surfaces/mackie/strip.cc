/*
	Copyright (C) 2006,2007 John Anderson
	Copyright (C) 2012 Paul Davis

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

#include <sstream>
#include <stdint.h>
#include "strip.h"

#include "button.h"
#include "led.h"
#include "ledring.h"
#include "pot.h"
#include "fader.h"
#include "jog.h"
#include "meter.h"

using namespace Mackie;
using namespace std;

Strip::Strip (const std::string& name, int index)
	: Group (name)
	, _solo (0)
	, _recenable (0)
	, _mute (0)
	, _select (0)
	, _vselect (0)
	, _fader_touch (0)
	, _vpot (0)
	, _gain (0)
	, _index (index)
{
	/* master strip only */
}

Strip::Strip (Surface& surface, const std::string& name, int index, int unit_index, StripControlDefinition* ctls)
	: Group (name)
	, _solo (0)
	, _recenable (0)
	, _mute (0)
	, _select (0)
	, _vselect (0)
	, _fader_touch (0)
	, _vpot (0)
	, _gain (0)
	, _index (index)
{
	/* build the controls for this track, which will automatically add them
	   to the Group 
	*/

	for (uint32_t i = 0; ctls[i].name[0]; ++i) {
		ctls[i].factory (surface, ctls[i].base_id + unit_index, unit_index+1, ctls[i].name, *this);
	}
}	

/**
	TODO could optimise this to use enum, but it's only
	called during the protocol class instantiation.
*/
void Strip::add (Control & control)
{
	Group::add (control);

	if  (control.name() == "gain") {
		_gain = reinterpret_cast<Fader*>(&control);
	} else if  (control.name() == "vpot") {
		_vpot = reinterpret_cast<Pot*>(&control);
	} else if  (control.name() == "recenable") {
		_recenable = reinterpret_cast<Button*>(&control);
	} else if  (control.name() == "solo") {
		_solo = reinterpret_cast<Button*>(&control);
	} else if  (control.name() == "mute") {
		_mute = reinterpret_cast<Button*>(&control);
	} else if  (control.name() == "select") {
		_select = reinterpret_cast<Button*>(&control);
	} else if  (control.name() == "vselect") {
		_vselect = reinterpret_cast<Button*>(&control);
	} else if  (control.name() == "fader_touch") {
		_fader_touch = reinterpret_cast<Button*>(&control);
	} else if  (control.name() == "meter") {
		_meter = reinterpret_cast<Meter*>(&control);
	} else if  (control.type() == Control::type_led || control.type() == Control::type_led_ring) {
		// relax
	} else {
		ostringstream os;
		os << "Strip::add: unknown control type " << control;
		throw MackieControlException (os.str());
	}
}

Fader& 
Strip::gain()
{
	if  (_gain == 0) {
		throw MackieControlException ("gain is null");
	}
	return *_gain;
}

Pot& 
Strip::vpot()
{
	if  (_vpot == 0) {
		throw MackieControlException ("vpot is null");
	}
	return *_vpot;
}

Button& 
Strip::recenable()
{
	if  (_recenable == 0) {
		throw MackieControlException ("recenable is null");
	}
	return *_recenable;
}

Button& 
Strip::solo()
{
	if  (_solo == 0) {
		throw MackieControlException ("solo is null");
	}
	return *_solo;
}
Button& 
Strip::mute()
{
	if  (_mute == 0) {
		throw MackieControlException ("mute is null");
	}
	return *_mute;
}

Button& 
Strip::select()
{
	if  (_select == 0) {
		throw MackieControlException ("select is null");
	}
	return *_select;
}

Button& 
Strip::vselect()
{
	if  (_vselect == 0) {
		throw MackieControlException ("vselect is null");
	}
	return *_vselect;
}

Button& 
Strip::fader_touch()
{
	if  (_fader_touch == 0) {
		throw MackieControlException ("fader_touch is null");
	}
	return *_fader_touch;
}

Meter& 
Strip::meter()
{
	if  (_meter == 0) {
		throw MackieControlException ("meter is null");
	}
	return *_meter;
}

std::ostream & Mackie::operator <<  (std::ostream & os, const Strip & strip)
{
	os << typeid (strip).name();
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
