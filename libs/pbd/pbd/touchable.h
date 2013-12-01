/*
    Copyright (C) 1999 Paul Barton-Davis 
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

#ifndef __pbd_touchable_h__
#define __pbd_touchable_h__

#include "pbd/libpbd_visibility.h"

class /*LIBPBD_API*/ Touchable
{
  public:
	Touchable() : _delete_after_touch (false) {}
	virtual ~Touchable() {}

	void set_delete_after_touch (bool yn) { _delete_after_touch = yn; }
	bool delete_after_touch() const { return _delete_after_touch; }

	virtual void touch () = 0;

  protected:
	bool _delete_after_touch;
};

template<class T>
class /*LIBPBD_API*/ DynamicTouchable : public Touchable
{
  public:
	DynamicTouchable (T& t, void (T::*m)(void)) 
		: object (t), method (m) { set_delete_after_touch (true); }

	void touch () {
		(object.*method)();
	}
	
  protected:
	T& object;
	void (T::*method)(void);
};

template<class T1, class T2>
class /*LIBPBD_API*/ DynamicTouchable1 : public Touchable
{
  public:
	DynamicTouchable1 (T1& t, void (T1::*m)(T2), T2 a) 
		: object (t), method (m), arg (a) { set_delete_after_touch (true); }

	void touch () {
		(object.*method)(arg);
	}
	
  protected:
	T1& object;
	void (T1::*method)(T2);
	T2 arg;
};

template<class T1, class T2, class T3>
class /*LIBPBD_API*/ DynamicTouchable2 : public Touchable
{
  public:
	DynamicTouchable2 (T1& t, void (T1::*m)(T2, T3), T2 a1, T3 a2) 
		: object (t), method (m), arg1 (a1), arg2 (a2) { set_delete_after_touch (true); }

	void touch () {
		(object.*method)(arg1, arg2);
	}
	
  protected:
	T1& object;
	void (T1::*method)(T2,T3);
	T2 arg1;
	T3 arg2;
};
	
#endif // __pbd_touchable_h__
