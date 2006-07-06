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

#define MEMENTO_COMMAND(obj_T, mem_T, obj, meth) \
    (MementoCommand<(obj_T),(mem_T)>((obj),sigc::mem_fun((obj),&(meth)),#meth))

#define MEMENTO_COMMAND_1(obj_T, mem_T, obj, meth, arg1) \
    (MementoCommand<(obj_T),(mem_T)>((obj),\
                                     sigc::bind(sigc::mem_fun((obj),\
                                                              &(meth)),\
                                                arg1),#meth))

#define MEMENTO_COMMAND_2(obj_T, mem_T, obj, meth, arg1, arg2) \
    (MementoCommand<(obj_T),(mem_T)>((obj),\
                                     sigc::bind(sigc::mem_fun((obj),\
                                                              &(meth)),\
                                                arg1, arg2),#meth))

#define MEMENTO_COMMAND_3(obj_T, mem_T, obj, meth, arg1, arg2, arg3) \
    (MementoCommand<(obj_T),(mem_T)>((obj),\
                                     sigc::bind(sigc::mem_fun((obj),\
                                                              &(meth)),\
                                                arg1, arg2, arg3),#meth))


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
        MementoCommand(obj_T obj, 
                       sigc::slot<void> action,
                       std::string key
                       Serializable *arg1 = 0,
                       Serializable *arg2 = 0,
                       Serializable *arg3 = 0
                       ) 
            : obj(obj), action(action), key(key), memento(obj.get_memento())
        {
            if (arg1 == 0)
                return;
            args.push_back(arg1);

            if (arg2 == 0)
                return;
            args.push_back(arg2);

            if (arg3 == 0)
                return;
            args.push_back(arg3);
        }
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
