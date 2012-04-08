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

#ifndef __mackie_controls_h__
#define __mackie_controls_h__

#include <map>
#include <vector>
#include <string>
#include <stdint.h>

#include "pbd/signals.h"

#include "mackie_control_exception.h"

namespace Mackie
{

class Control;
class Surface;

/**
	This is a loose group of controls, eg cursor buttons,
	transport buttons, functions buttons etc.
*/
class Group
{
public:
	Group (const std::string & name)
		: _name (name) {}

	virtual ~Group() {}
	
	virtual bool is_strip() const { return false; }
	virtual bool is_master() const { return false; }
	
	virtual void add (Control & control);
	
	const std::string & name() const { return _name; }
	void set_name (const std::string & rhs) { _name = rhs; }
	
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

struct StripControlDefinition {
    const char* name;
    uint32_t base_id;
    Control* (*factory)(Surface&, int index, int ordinal, const char* name, Group&);
};

struct GlobalControlDefinition {
    const char* name;
    uint32_t id;
    Control* (*factory)(Surface&, int index, int ordinal, const char* name, Group&);
    const char* group_name;
};

/**
	This is the set of controls that make up a strip.
*/
class Strip : public Group
{
public:
	Strip (const std::string& name, int index); /* master strip only */
	Strip (Surface&, const std::string & name, int index, int unit_index, StripControlDefinition* ctls);

	virtual bool is_strip() const { return true; }
	virtual void add (Control & control);
	int index() const { return _index; } // zero based
	
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
	Button* _solo;
	Button* _recenable;
	Button* _mute;
	Button* _select;
	Button* _vselect;
	Button* _fader_touch;
	Pot*    _vpot;
	Fader*  _gain;
	int     _index;
};

std::ostream & operator <<  (std::ostream &, const Strip &);

class MasterStrip : public Strip
{
public:
	MasterStrip (const std::string & name, int index)
		: Strip (name, index) {}
	
	virtual bool is_master() const  { return true; }
};

class Led;

/**
	The base class for controls on the surface. They deliberately
	don't know the midi protocol for updating them.
*/
class Control
{
public:
	enum type_t { 
		type_led, 
		type_led_ring, 
		type_fader = 0xe0, 
		type_button = 0x90, 
		type_pot = 0xb0 
	};

	enum base_id_t {
		fader_base_id = 0x0,
		pot_base_id = 0x10,
		jog_base_id = 0x3c,
		fader_touch_button_base_id = 0x68,
		vselect_button_base_id = 0x20,
		select_button_base_id = 0x18,
		mute_button_base_id = 0x10,
		solo_button_base_id = 0x08,
		recenable_button_base_id = 0x0,
	};
	
	Control (int id, int ordinal, std::string name, Group& group);
	virtual ~Control() {}
	
	virtual const Led & led() const { throw MackieControlException ("no led available"); }

	/// type() << 8 + midi id of the control. This
	/// provides a unique id for any control on the surface.
	int id() const { return (type() << 8) + _id; }
	
	/// the value of the second bytes of the message. It's
	/// the id of the control, but only guaranteed to be
	/// unique within the control type.
	int raw_id() const { return _id; }
	
	/// The 1-based number of the control
	int ordinal() const { return _ordinal; }
	
	const std::string & name() const  { return _name; }
	const Group & group() const { return _group; }
	const Strip & strip() const { return dynamic_cast<const Strip&> (_group); }
	Strip & strip() { return dynamic_cast<Strip&> (_group); }
	virtual bool accepts_feedback() const  { return true; }
	
	virtual type_t type() const = 0;
	
	/// Return true if this control is the one and only Jog Wheel
	virtual bool is_jog() const { return false; }

	bool in_use () const;
	void set_in_use (bool);
	
	/// Keep track of the timeout so it can be updated with more incoming events
	sigc::connection in_use_connection;

	/** If we are doing an in_use timeout for a fader without touch, this
	 *  is its touch button control; otherwise 0.
	 */
	Control* in_use_touch_control;

private:
	int _id;
	int _ordinal;
	std::string _name;
	Group& _group;
	bool _in_use;
};

std::ostream & operator <<  (std::ostream & os, const Control & control);

class Fader : public Control
{
public:
	Fader (int id, int ordinal, std::string name, Group & group)
		: Control (id, ordinal, name, group)
	{
	}
	
	virtual type_t type() const { return type_fader; }

	static Control* factory (Surface&, int id, int ordinal, const char*, Group&);
};

class Led : public Control
{
public:
	Led (int id, int ordinal, std::string name, Group & group)
		: Control (id, ordinal, name, group)
	{
	}
	
	virtual const Led & led() const { return *this; }

	virtual type_t type() const { return type_led; }

	static Control* factory (Surface&, int id, int ordinal, const char*, Group&);
};

class Button : public Control
{
public:
	Button (int id, int ordinal, std::string name, Group & group)
		: Control (id,  ordinal, name, group)
		, _led  (id, ordinal, name + "_led", group) {}
	
	virtual const Led & led() const  { return _led; }
	
	virtual type_t type() const { return type_button; };

	static Control* factory (Surface&, int id, int ordinal, const char*, Group&);
	
private:
	Led _led;
};

class LedRing : public Led
{
public:
	LedRing (int id, int ordinal, std::string name, Group & group)
		: Led (id, ordinal, name, group)
	{
	}

	virtual type_t type() const { return type_led_ring; }
};

class Pot : public Control
{
public:
	Pot (int id, int ordinal, std::string name, Group & group)
		: Control (id, ordinal, name, group)
		, _led_ring (id, ordinal, name + "_ring", group) {}

	virtual type_t type() const { return type_pot; }

	virtual const LedRing & led_ring() const {return _led_ring; }

	static Control* factory (Surface&, int id, int ordinal, const char*, Group&);

private:
	LedRing _led_ring;
};

class Jog : public Pot
{
public:
	Jog (int id, int ordinal, std::string name, Group & group)
		: Pot  (id, ordinal, name, group)
	{
	}

	virtual bool is_jog() const { return true; }

	static Control* factory (Surface&, int id, int ordinal, const char*, Group&);
};

}

#endif /* __mackie_controls_h__ */
