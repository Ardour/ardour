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

*/

#ifndef __lib_pbd_undo_h__
#define __lib_pbd_undo_h__

#include <string>
#include <list>
#include <map>
#include <sigc++/slot.h>
#include <sigc++/bind.h>
#include <sys/time.h>

#include "pbd/libpbd_visibility.h"
#include "pbd/command.h"

typedef sigc::slot<void> UndoAction;

class LIBPBD_API UndoTransaction : public Command
{
  public:
	UndoTransaction ();
	UndoTransaction (const UndoTransaction&);
	UndoTransaction& operator= (const UndoTransaction&);
	~UndoTransaction ();

	void clear ();
	bool empty() const;
	bool clearing () const { return _clearing; }

	void add_command (Command* const);
	void remove_command (Command* const);

	void operator() ();
	void undo();
	void redo();

	XMLNode &get_state();

	void set_timestamp (struct timeval &t) {
		_timestamp = t;
	}

	const struct timeval& timestamp() const {
		return _timestamp;
	}

  private:
	std::list<Command*>    actions;
	struct timeval        _timestamp;
	bool                  _clearing;

	friend void command_death (UndoTransaction*, Command *);
	
	void about_to_explicitly_delete ();
};

class LIBPBD_API UndoHistory : public PBD::ScopedConnectionList
{
  public:
	UndoHistory();
	~UndoHistory() {}
	
	void add (UndoTransaction* ut);
	void undo (unsigned int n);
	void redo (unsigned int n);
	
	unsigned long undo_depth() const { return UndoList.size(); }
	unsigned long redo_depth() const { return RedoList.size(); }
	
	std::string next_undo() const { return (UndoList.empty() ? std::string() : UndoList.back()->name()); }
	std::string next_redo() const { return (RedoList.empty() ? std::string() : RedoList.back()->name()); }

	void clear ();
	void clear_undo ();
	void clear_redo ();

	/* returns all or part of the history.
	   If depth==0 it returns just the top
	   node. If depth<0, it returns everything.
	   If depth>0, it returns state for that
	   many elements of the history, or 
	   the full history, whichever is smaller.
	*/

        XMLNode &get_state(int32_t depth = 0);
        void save_state();

	void set_depth (uint32_t);

	PBD::Signal0<void> Changed;
	PBD::Signal0<void> BeginUndoRedo;
	PBD::Signal0<void> EndUndoRedo;
	
  private:
	bool _clearing;
	uint32_t _depth;
	std::list<UndoTransaction*> UndoList;
	std::list<UndoTransaction*> RedoList;

	void remove (UndoTransaction*);
};


#endif /* __lib_pbd_undo_h__ */
