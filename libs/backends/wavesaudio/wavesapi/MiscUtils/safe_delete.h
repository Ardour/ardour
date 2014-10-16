/*
    Copyright (C) 2014 Waves Audio Ltd.

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

#ifndef __safe_delete_h__
	#define __safe_delete_h__
	

/* Copy to include:
#include "safe_delete.h"
*/

#define	safe_delete(__pObject__) {if((__pObject__) != 0) {delete (__pObject__); (__pObject__) = 0;}}

#define	safe_delete_array(__pArray__) {if((__pArray__) != 0) {delete [] (__pArray__); (__pArray__) = 0;}}

template <class T> void safe_delete_from_iterator(T* pToDelete)
{
	safe_delete(pToDelete);
}

#endif // __safe_delete_h__
