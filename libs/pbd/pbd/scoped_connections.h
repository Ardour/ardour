/*
    Copyright (C) 2009 Paul Davis 

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

#ifndef __pbd_scoped_connections_h__
#define __pbd_scoped_connections_h__

#include <list>
#include <glibmm/thread.h>
#include <boost/signals2.hpp>

#include "pbd/destructible.h"

namespace PBD {

class ScopedConnectionList 
{
  public:
	ScopedConnectionList();
	~ScopedConnectionList ();
	
	void add_connection (const boost::signals2::connection& c);
	void drop_connections ();

	template<typename S> void scoped_connect (S& sig, const typename S::slot_function_type& sf) {
		add_connection (sig.connect (sf));
	}

  private:
	/* this class is not copyable */
	ScopedConnectionList(const ScopedConnectionList&) {}

	/* this lock is shared by all instances of a ScopedConnectionList.
	   We do not want one mutex per list, and since we only need the lock
	   when adding or dropping connections, which are generally occuring
	   in object creation and UI operations, the contention on this 
	   lock is low and not of significant consequence. Even though
	   boost::signals2 is thread-safe, this additional list of
	   scoped connections needs to be protected in 2 cases:

	   (1) (unlikely) we make a connection involving a callback on the
	       same object from 2 threads. (wouldn't that just be appalling 
	       programming style?)
	     
	   (2) where we are dropping connections in one thread and adding
	       one from another.
	 */

	static Glib::StaticMutex _lock;

	typedef std::list<boost::signals2::scoped_connection*> ConnectionList;
	ConnectionList _list;
};

} /* namespace */

#endif /* __pbd_scoped_connections_h__ */
