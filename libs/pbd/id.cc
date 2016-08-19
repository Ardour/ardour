/*
    Copyright (C) 2000-2007 Paul Davis

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

#include <ostream>
#include <stdio.h>

#include "pbd/id.h"
#include "pbd/string_convert.h"

#include <string>

using namespace std;
using namespace PBD;

Glib::Threads::Mutex* ID::counter_lock = 0;
uint64_t ID::_counter = 0;

void
ID::init ()
{
	if (!counter_lock)
		counter_lock = new Glib::Threads::Mutex;
}

ID::ID ()
{
	reset ();
}

ID::ID (const ID& other)
{
	_id = other._id;
}

ID::ID (string str)
{
	string_assign (str);
}

void
ID::reset ()
{
	Glib::Threads::Mutex::Lock lm (*counter_lock);
	_id = _counter++;
}

bool
ID::string_assign (string str)
{
	return string_to_uint64 (str, _id);
}

std::string
ID::to_s () const
{
	return to_string (_id);
}

bool
ID::operator== (const string& str) const
{
	return to_string (_id) == str;
}

ID&
ID::operator= (string str)
{
	string_assign (str);
	return *this;
}

ID&
ID::operator= (const ID& other)
{
	if (&other != this) {
		_id = other._id;
	}
	return *this;
}

ostream&
operator<< (ostream& ostr, const ID& id)
{
	ostr << id.to_s();
	return ostr;
}

