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

#ifndef __pbd_id_h__
#define __pbd_id_h__

#include <stdint.h>
#include <string>

#include <glibmm/threads.h>

#include "pbd/libpbd_visibility.h"

namespace PBD {

class LIBPBD_API ID {
  public:
	ID ();
	ID (std::string);
	ID (const ID&);

	void reset ();

	bool operator== (const ID& other) const {
		return _id == other._id; 
	}

	bool operator!= (const ID& other) const {
		return _id != other._id;
	}

	bool operator== (const std::string&) const;

	ID& operator= (std::string); 
	ID& operator= (const ID&); 

	bool operator< (const ID& other) const {
		return _id < other._id;
	}

	void print (char* buf, uint32_t bufsize) const;
        std::string to_s() const;
	
	static uint64_t counter() { return _counter; }
	static void init_counter (uint64_t val) { _counter = val; }
	static void init ();

  private:
	uint64_t _id;
	int string_assign (std::string);

	static Glib::Threads::Mutex* counter_lock;
	static uint64_t _counter;
};

}

LIBPBD_API std::ostream& operator<< (std::ostream& ostr, const PBD::ID&);

#endif /* __pbd_id_h__ */
