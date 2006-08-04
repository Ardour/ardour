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

    $Id: /local/undo/libs/pbd3/pbd/undo.h 80 2006-06-22T22:37:01.079855Z fugalh  $
*/

#ifndef __lib_pbd_command_h__
#define __lib_pbd_command_h__

#include <pbd/serializable.h>

class Command : public Serializable
{
    public:
	virtual ~Command() {}
	virtual void operator() () = 0;
        virtual void undo() = 0;
        virtual void redo() { (*this)(); }
        virtual XMLNode &serialize();
};

#endif // __lib_pbd_command_h_
