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

#ifndef __lib_pbd_serializable_h__
#define __lib_pbd_serializable_h__

#include <pbd/xml++.h>

class Serializable 
{
public:
    XMLNode &serialize();
};

#endif // __lib_pbd_serializable_h__
