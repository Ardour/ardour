
/* 
    Copyright (C) 2006 Paul Davis
    Author: Hans Fugal

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

#ifndef __lib_pbd_command_h__
#define __lib_pbd_command_h__

#include <string>

#include "pbd/libpbd_visibility.h"
#include "pbd/signals.h"
#include "pbd/statefuldestructible.h"

class LIBPBD_API Command : public PBD::StatefulDestructible, public PBD::ScopedConnectionList
{
public:
	virtual ~Command() { /* NOTE: derived classes must call drop_references() */ }

	virtual void operator() () = 0;
	
	void set_name (const std::string& str) { _name = str; }
	const std::string& name() const { return _name; }

	virtual void undo() = 0;
	virtual void redo() { (*this)(); }
	
	virtual XMLNode &get_state();
	virtual int set_state(const XMLNode&, int /*version*/) { /* noop */ return 0; }

	virtual bool empty () const {
		return false;
	}

protected:
	Command() {}
	Command(const std::string& name) : _name(name) {}

	std::string _name;
};

#endif // __lib_pbd_command_h_
