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

#include <sigc++/slot.h>
#include <sigc++/bind.h>
#include <list>
#include <string>
#include <pbd/serializable.h>

using sigc::nil;
using sigc::slot;
using sigc::bind;
using sigc::mem_fun;
using std::list;
using std::string;


/* One of the joys of templates is that you have to do everything right here
 * in the header file; you can't split this to make undo_command.cc */

template <class T_obj, class T1=nil, class T2=nil, class T3=nil, class T4=nil>
class UndoCommand
{
    public:
	/* It only makes sense to use the constructor corresponding to the
	 * template given. e.g.
	 * 
	 *   UndoCommand<Foo> cmd(id, key, foo_instance);
	 */
	UndoCommand(T_obj &object, string key) 
	    : _obj(object), _key(key) 
	{
	    _slot = mem_fun( _obj, get_method(_key) );
	}
	UndoCommand(T_obj &object, string key, T1 &arg1)
	    : _obj(object), _key(key) 
	{ 
	    _slot = bind( mem_fun( _obj, get_method(_key) ), 
		    arg1);
	    _args.push_back(&arg1); 
	}
	UndoCommand(T_obj &object, string key, T1 &arg1, T2 &arg2)
	    : _obj(object), _key(key) 
	{ 
	    _slot = bind( mem_fun( _obj, get_method(_key) ), 
		    arg1, arg2);
	    _args.push_back(&arg1); 
	    _args.push_back(&arg2); 
	}
	UndoCommand(T_obj &object, string key, T1 &arg1, T2 &arg2, T3 &arg3)
	    : _obj(object), _key(key) 
	{ 
	    _slot = bind( mem_fun( _obj, get_method(_key) ), 
		    arg1, arg2, arg3);
	    _args.push_back(&arg1); 
	    _args.push_back(&arg2); 
	    _args.push_back(&arg3); 
	}
	UndoCommand(T_obj &object, string key, T1 &arg1, T2 &arg2, T3 &arg3, T4 &arg4)
	    : _obj(object), _key(key) 
	{ 
	    _slot = bind( mem_fun( _obj, get_method(_key) ), 
		    arg1, arg2, arg4);
	    _args.push_back(&arg1); 
	    _args.push_back(&arg2); 
	    _args.push_back(&arg3); 
	    _args.push_back(&arg4); 
	}

	void operator() () { return _slot(); }

	XMLNode &serialize();
    protected:
	template <class T_method> T_method &get_method(string);
	T_obj &_obj;
	string _key;
	slot<void> _slot;

	// Note that arguments must be instances of Serializable or this will
	// rightly cause a compiler error when compiling the constructor.
	list<Serializable*> _args;
};

#endif // __lib_pbd_undo_command_h__
