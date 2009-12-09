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

#ifndef __pbd__crossthread_h__
#define __pbd__crossthread_h__

#include <glibmm/main.h>

class CrossThreadChannel { 
  public:
	CrossThreadChannel();
	~CrossThreadChannel();
	
	void wakeup();
	int selectable() const { return fds[0]; }

	void drain ();
	static void drain (int fd);

	Glib::RefPtr<Glib::IOSource> ios();
	bool ok() const { return fds[0] >= 0 && fds[1] >= 0; }

  private:
	Glib::RefPtr<Glib::IOSource> _ios; // lazily constructed
	int fds[2];
};

#endif /* __pbd__crossthread_h__ */
