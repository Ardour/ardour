/* 
   Copyright (C) 2006 Paul Davis

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

    $Id: /local/undo/libs/pbd3/pbd/undo.h 59 2006-06-15T18:16:20.960977Z fugalh  $
*/

#ifndef __lib_pbd_undo_command_h__
#define __lib_pbd_undo_command_h__

#include <pbd/serializable.h>

using sigc::nil;
using sigc::slot;
using std::list;
using std::string;

template <class T1=nil, class T2=nil, class T3=nil, class T4=nil>
class UndoCommand
{
    public:
	/* It only makes sense to use the constructor corresponding to the
	 * template given. e.g.
	 * 
	 *   UndoCommand<Foo> cmd(id, key, foo_instance);
	 */
	UndoCommand(id_t object_id, string key) 
	    : _obj_id(object_id), _key(key) {}
	UndoCommand(id_t object_id, string key, T1 arg1)
	    : _obj_id(object_id), _key(key) 
	{ 
	    _args.push_back(arg1); 
	}
	UndoCommand(id_t object_id, string key, T1 arg1, T2 arg2)
	    : _obj_id(object_id), _key(key) 
	{ 
	    _args.push_back(arg1); 
	    _args.push_back(arg2); 
	}
	UndoCommand(id_t object_id, string key, T1 arg1, T2 arg2, T3 arg3)
	    : _obj_id(object_id), _key(key) 
	{ 
	    _args.push_back(arg1); 
	    _args.push_back(arg2); 
	    _args.push_back(arg3); 
	}
	UndoCommand(id_t object_id, string key, T1 arg1, T2 arg2, T3 arg3, T4 arg4)
	    : _obj_id(object_id), _key(key) 
	{ 
	    _args.push_back(arg1); 
	    _args.push_back(arg2); 
	    _args.push_back(arg3); 
	    _args.push_back(arg4); 
	}

	void operator() () { return _slot(); }
	XMLNode &serialize();
    protected:
	id_t _obj_id;
	string _key;
	slot<void> _slot;
	// Note that arguments must be instances of Serializable or this will
	// rightly cause a compiler error when compiling the constructor.
	list<Serializable*> _args;
};

#endif // __lib_pbd_undo_command_h__
