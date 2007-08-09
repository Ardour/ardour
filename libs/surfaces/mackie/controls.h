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

#include <sigc++/sigc++.h>

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
	/**
		\param is the index of the strip. 0-based.
	*/
	Strip( const std::string & name, int index );
	
	virtual bool is_strip() const
	{
		return true;
	}
	
	virtual void add( Control & control );
	
	/// This is the index of the strip. zero-based.
	int index() const { return _index; }
	
	/// This is for Surface only
	/// index is zero-based
	void index( int rhs ) { _index = rhs; }
	
	Button & solo();
	Button & recenable();
	Button & mute();
	Button & select();
	Button & vselect();
	Button & fader_touch();
	Pot & vpot();
	Fader & gain();
	
	bool has_solo() const { return _solo != 0; }
	bool has_recenable() const { return _recenable != 0; }
	bool has_mute() const { return _mute != 0; }
	bool has_select() const { return _select != 0; }
	bool has_vselect() const { return _vselect != 0; }
	bool has_fader_touch() const { return _fader_touch != 0; }
	bool has_vpot() const { return _vpot != 0; }
	bool has_gain() const { return _gain != 0; }
	
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

std::ostream & operator << ( std::ostream &, const Strip & );

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
	enum type_t { type_led, type_led_ring, type_fader = 0xe0, type_button = 0x90, type_pot = 0xb0 };
	
	Control( int id, int ordinal, std::string name, Group & group );
	virtual ~Control() {}
	
	virtual const Led & led() const
	{
		throw MackieControlException( "no led available" );
	}

	/// type() << 8 + midi id of the control. This
	/// provides a unique id for any control on the surface.
	int id() const
	{
		return ( type() << 8 ) + _id;
	}
	
	/// the value of the second bytes of the message. It's
	/// the id of the control, but only guaranteed to be
	/// unique within the control type.
	int raw_id() const { return _id; }
	
	/// The 1-based number of the control
	int ordinal() const { return _ordinal; }
	
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
	
	/// Return true if this control is the one and only
	/// Jog Wheel
	virtual bool is_jog() const { return false; }

	/**
		Return true if the controlis in use, or false otherwise. For buttons
		this returns true if the button is currently being held down. For
		faders, the touch button has not been released. For pots, this returns
		true from the first move event until a timeout after the last move event.
	*/
	virtual bool in_use() const;
	virtual Control & in_use( bool );
	
	/// The timeout value for this control. Normally defaulted to 250ms, but
	/// certain controls (ie jog wheel) may want to override it.
	virtual unsigned int in_use_timeout() { return _in_use_timeout; }

	/// Keep track of the timeout so it can be updated with more incoming events
	sigc::connection in_use_connection;
	
private:
	int _id;
	int _ordinal;
	std::string _name;
	Group & _group;
	bool _in_use;
	unsigned int _in_use_timeout;
};

std::ostream & operator << ( std::ostream & os, const Control & control );

class Fader : public Control
{
public:
	Fader( int id, int ordinal, std::string name, Group & group )
	: Control( id, ordinal, name, group )
	{
	}
	
	virtual type_t type() const { return type_fader; }
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

class Jog : public Pot
{
public:
	Jog( int id, int ordinal, std::string name, Group & group )
	: Pot( id, ordinal, name, group )
	{
	}

	virtual bool is_jog() const { return true; }
};

}

#endif
