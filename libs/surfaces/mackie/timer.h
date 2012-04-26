/*
  Copyright (C) 1998, 1999, 2000, 2007 John Anderson
  
  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU Library General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef timer_h
#define timer_h

#ifdef _WIN32
#include "windows.h"
#else
#include <sys/time.h>
#endif

namespace Mackie
{

/**
	millisecond timer class.
*/
class Timer
{
public:
	
	/**
		start the timer running if true, or just create the
		object if false.
	*/
	Timer( bool shouldStart = true )
	{
		if ( shouldStart )
			start();
	}
	
	/**
		Start the timer running. Return the current timestamp, in milliseconds
	*/
	unsigned long start()
	{
#ifdef _WIN32
		_start = (unsigned long)::GetTickCount();
#else
		gettimeofday ( &_start, 0 );
#endif
		running = true;
#ifdef _WIN32
		return _start;
#else
		return ( _start.tv_sec * 1000000 + _start.tv_usec ) / 1000;
#endif
	}

	/**
		returns the number of milliseconds since start
		also stops the timer running
	*/
	unsigned long stop()
	{
#ifdef _WIN32
		_stop = (unsigned long)::GetTickCount();
#else
		gettimeofday ( &_stop, 0 );
#endif
		running = false;
		return elapsed();
	}

	/**
		returns the number of milliseconds since start
	*/
	unsigned long elapsed() const
	{
		if ( running )
		{
#ifdef _WIN32
			DWORD current = ::GetTickCount();
			return current - _start;
#else
			struct timeval current;
			gettimeofday ( &current, 0 );
			return (
				( current.tv_sec * 1000000 + current.tv_usec ) - ( _start.tv_sec * 1000000 + _start.tv_usec )
			) / 1000
			;
#endif
		}
		else
		{
#ifdef _WIN32
			return _stop - _start;
#else
			return (
				( _stop.tv_sec * 1000000 + _stop.tv_usec ) - ( _start.tv_sec * 1000000 + _start.tv_usec )
			) / 1000
			;
#endif
		}
	}
	
	/**
		Call stop and then start. Return the value from stop.
	*/
	unsigned long restart()
	{
		unsigned long retval = stop();
		start();
		return retval;
	}

private:
#ifdef _WIN32
	unsigned long _start;
	unsigned long _stop;
#else
	struct timeval _start;
	struct timeval _stop;
#endif
	bool running;
};

}

#endif
