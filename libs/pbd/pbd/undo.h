/* 
   Copyright (C) 2002 Brett Viren & Paul Davis

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

    $Id$
*/

#ifndef __lib_pbd_undo_h__
#define __lib_pbd_undo_h__

#include <string>
#include <list>
#include <map>
#include <sigc++/slot.h>
#include <sigc++/bind.h>
#include <sys/time.h>
#include <pbd/command.h>

using std::string;
using std::list;

typedef sigc::slot<void> UndoAction;

class UndoTransaction : public Command
{
  public:
	UndoTransaction ();
	UndoTransaction (const UndoTransaction&);
	UndoTransaction& operator= (const UndoTransaction&);

	void clear ();

	void add_command (Command *const);

        void operator() ();
	void undo();
	void redo();

	XMLNode &get_state();
	
	void set_name (const string& str) {
		_name = str;
	}
	const string& name() const { return _name; }

	void set_timestamp (struct timeval &t) {
		_timestamp = t;
	}

	const struct timeval& timestamp() const {
		return _timestamp;
	}

  private:
	list<Command*>	 actions;
	struct timeval   _timestamp;
	string           _name;
};

class UndoHistory
{
  public:
	UndoHistory() {}
	~UndoHistory() {}
	
	void add (UndoTransaction ut);
	void undo (unsigned int n);
	void redo (unsigned int n);
	
	unsigned long undo_depth() const { return UndoList.size(); }
	unsigned long redo_depth() const { return RedoList.size(); }
	
	string next_undo() const { return (UndoList.empty() ? string("") : UndoList.back().name()); }
	string next_redo() const { return (RedoList.empty() ? string("") : RedoList.back().name()); }

	void clear ();
	void clear_undo ();
	void clear_redo ();

  private:
	list<UndoTransaction> UndoList;
	list<UndoTransaction> RedoList;
};


#endif /* __lib_pbd_undo_h__ */
