/*
    Copyright (C) 1998-99 Paul Barton-Davis

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

#ifndef __selectable_h__
#define __selectable_h__

#include <list>
#include <string>
#include <stdio.h>

#include <sigc++/sigc++.h>

#include <sys/types.h>

#include "pbd/libpbd_visibility.h"

namespace Select {
    enum LIBPBD_API Condition {
		Readable = 0x1,
		Writable = 0x2,
		Exception = 0x4
    };

class LIBPBD_API Selectable : public sigc::trackable

{
  public:
	Selectable (int fd);
	Selectable (const std::string &, int flags, int mode = 0);
	Selectable (FILE *);
	~Selectable ();

	sigc::signal<void,Selectable *,Select::Condition> readable;
	sigc::signal<void,Selectable *,Select::Condition> writable;
	sigc::signal<void,Selectable *,Select::Condition> exceptioned;

	int  fd() { return _fd; }
	bool ok() { return _ok; }

  protected:
	void selected (unsigned int condition);
	int condition;
	int _fd;

	friend class Selector;

  private:
	enum {
		fromFD,
		fromPath,
		fromFILE
	};
		
	bool _ok;
	int _type;
	std::string path;
};

class LIBPBD_API Selector {
  private:
	int post_select (fd_set *, fd_set *, fd_set *);
	int _max_fd;

	typedef std::list<Selectable *> Selectables;
	Selectables selectables;
	pthread_mutex_t list_lock;

	static bool use_list_lock;

  public:
	Selector ();

	void multithreaded (bool yn) {
		use_list_lock = yn;
	}
	
	void add (int condition, Selectable *s);
	void remove (Selectable *);
	int select (unsigned long usecs);
};



} /* namespace */


#endif // __selectable_h__
