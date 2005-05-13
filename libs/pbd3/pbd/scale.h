/*
    Copyright (C) 2000 Paul Barton-Davis 

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

#ifndef __pbd_scale_h__
#define __pbd_scale_h__

#include <cmath>

inline float
scale (float value, float lower, float upper)
{
	return fabs (lower + value) / (upper-lower);
}	

inline float
scale_with_range (float value, float lower, float range)
{
	return fabs (lower + value) / range;
}	


inline float 
scale_to (float value, float lower, float upper, float to)
{
	return (fabs (lower + value) / (upper-lower)) * to;
}	

inline float
scale_to_with_range (float value, float lower, float range, float to)
{
	return (fabs (lower + value) / range) * to;
}	

#endif /* __pbd_scale_h__ */


