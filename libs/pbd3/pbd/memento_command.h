/* 
   Copyright (C) 2006 Hans Fugal & Paul Davis

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

    $Id: /local/undo/libs/pbd3/pbd/undo.h 132 2006-06-29T18:45:16.609763Z fugalh  $
*/

#ifndef __lib_pbd_memento_command_h__
#define __lib_pbd_memento_command_h__

#include <pbd/command.h>
#include <sigc++/slot.h>

// TODO: make a macro to make constructing this even easier
template <class obj_T, class mem_T>
class MementoCommand : public Command
{
    public:
        MementoCommand(obj_T obj, 
                       sigc::slot<void> action,
                       std::string key
                       std::list<Serializable *> args
                       ) 
            : obj(obj), action(action), key(key), args(args), memento(obj.get_memento()) {}
        void operator() () { action(); }
        void undo() { obj.set_memento(memento); }
        virtual XMLNode &serialize() 
        {
            // obj.id
            // key
            // args
        }
    protected:
        obj_T &obj;
        mem_T memento;
        sigc::slot<void> action;
        std::string key;
        std::list<Serializable*> args;
};

#endif // __lib_pbd_memento_h__
