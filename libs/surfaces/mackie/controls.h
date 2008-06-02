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
#ifndef mackie_controls_h
#define mackie_controls_h

#include <map>
#include <vector>
#include <string>

#include "mackie_control_exception.h"

namespace Mackie
{

class Control;

/**
	This is a loose group of controls, eg cursor buttons,
	transport buttons, functions buttons etc.
*/
class Group
{
public:
	Group( const std::string & name )
	: _name( name )
	{
	}
	
	virtual ~Group() {}
	
	virtual bool is_strip() const
	{
		return false;
	}
	
	virtual bool is_master() const
	{
		return false;
	}
	
	virtual void add( Control & control );
	
	const std::string & name() const
	{
		return _name;
	}
	
	// This is for Surface only
	void name( const std::string & rhs ) { _name = rhs; }
	
	typedef std::vector<Control*> Controls;
	const Controls & controls() const { return _controls; }
	
protected:
	Controls _controls;
	
private:
	std::string _name;
};

class Button;
class Pot;
class Fader;

/**
	This is the set of controls that make up a strip.
*/
class Strip : public Group
{
public:
	Strip( const std::string & name, int index );
	
	virtual bool is_strip() const
	{
		return true;
	}
	
	virtual void add( Control & control );
	
	/// This is the index of the strip
	int index() const { return _index; }
	
	/// This is for Surface only
	void index( int rhs ) { _index = rhs; }
	
	Button & solo();
	Button & recenable();
	Button & mute();
	Button & select();
	Button & vselect();
	Button & fader_touch();
	Pot & vpot();
	Fader & gain();
	
	bool has_solo() { return _solo != 0; }
	bool has_recenable() { return _recenable != 0; }
	bool has_mute() { return _mute != 0; }
	bool has_select() { return _select != 0; }
	bool has_vselect() { return _vselect != 0; }
	bool has_fader_touch() { return _fader_touch != 0; }
	bool has_vpot() { return _vpot != 0; }
	bool has_gain() { return _gain != 0; }
	
private:
	Button * _solo;
	Button * _recenable;
	Button * _mute;
	Button * _select;
	Button * _vselect;
	Button * _fader_touch;
	Pot * _vpot;
	Fader * _gain;
	int _index;
};

class MasterStrip : public Strip
{
public:
	MasterStrip( const std::string & name, int index )
	: Strip( name, index )
	{
	}
	
	virtual bool is_master() const
	{
		return true;
	}
};

class Led;

/**
	The base class for controls on the surface. They deliberately
	don't know the midi protocol for updating them.
*/
class Control
{
public:
	enum type_t { type_fader, type_button, type_pot, type_led, type_led_ring };
	
	Control( int id, int ordinal, std::string name, Group & group )
	: _id( id ), _ordinal( ordinal ), _name( name ), _group( group )
	{
	}
	
	virtual ~Control() {}
	
	virtual const Led & led() const
	{
		throw MackieControlException( "no led available" );
	}

	/// The midi id of the control
	int id() const
	{
		return _id;
	}
	
	/// The 1-based number of the control
	int ordinal() const
	{
		return _ordinal;
	}
	
	const std::string & name() const
	{
		return _name;
	}
	
	const Group & group() const
	{
		return _group;
	}
	
	const Strip & strip() const
	{
		return dynamic_cast<const Strip&>( _group );
	}
	
	Strip & strip()
	{
		return dynamic_cast<Strip&>( _group );
	}
	
	virtual bool accepts_feedback() const
	{
		return true;
	}
	
	virtual type_t type() const = 0;
	
private:
	int _id;
	int _ordinal;
	std::string _name;
	Group & _group;
};

std::ostream & operator << ( std::ostream & os, const Control & control );

class Fader : public Control
{
public:
	Fader( int id, int ordinal, std::string name, Group & group )
	: Control( id, ordinal, name, group )
	, _touch( false )
	{
	}
	
	bool touch() const { return _touch; }
	
	void touch( bool yn ) { _touch = yn; }

	virtual type_t type() const { return type_fader; }

private:
	bool _touch;
};

class Led : public Control
{
public:
	Led( int id, int ordinal, std::string name, Group & group )
	: Control( id, ordinal, name, group )
	{
	}
	
	virtual const Led & led() const { return *this; }

	virtual type_t type() const { return type_led; }
};

class Button : public Control
{
public:
	Button( int id, int ordinal, std::string name, Group & group )
	: Control( id, ordinal, name, group )
	, _led( id, ordinal, name + "_led", group )
	{
	}
	
	virtual const Led & led() const
	{
		return _led;
	}
	
	virtual type_t type() const { return type_button; };

private:
	Led _led;
};

class LedRing : public Led
{
public:
	LedRing( int id, int ordinal, std::string name, Group & group )
	: Led( id, ordinal, name, group )
	{
	}

	virtual type_t type() const { return type_led_ring; }
};

class Pot : public Control
{
public:
	Pot( int id, int ordinal, std::string name, Group & group )
	: Control( id, ordinal, name, group )
	, _led_ring( id, ordinal, name + "_ring", group )
	{
	}

	virtual type_t type() const { return type_pot; }

	virtual const LedRing & led_ring() const
	{
		return _led_ring;
	}

private:
	LedRing _led_ring;
};

}

#endif
