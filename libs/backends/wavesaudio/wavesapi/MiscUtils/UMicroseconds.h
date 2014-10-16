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

#ifndef __UMicroseconds_h__
	#define __UMicroseconds_h__
	
/* Copy to include
#include "UMicroseconds.h"
*/



#include "BasicTypes/WUDefines.h"
#include "BasicTypes/WUTypes.h"

namespace wvNS { 
// a wraper for Microseconds function from Timer.h
class DllExport UMicroseconds
{
public:

#ifdef PLATFORM_WINDOWS
 	typedef int64_t TimeKeeper;
#endif
#ifdef __APPLE__
 	typedef uint64_t TimeKeeper;
#endif
#ifdef __linux__
 	typedef uint64_t TimeKeeper;
#endif

private:
	TimeKeeper theTime;

public:

	UMicroseconds()
	{
		ReadTime();
	}

	UMicroseconds(const TimeKeeper in_initVal) : theTime(in_initVal) {}

	UMicroseconds(const UMicroseconds& inUM) : theTime(inUM.theTime) {}
	UMicroseconds& operator=(const UMicroseconds& inUM) {theTime = inUM.theTime;  return *this;}
	UMicroseconds& operator+=(const TimeKeeper in_timeToAdd)  {theTime += in_timeToAdd;  return *this;}

	UMicroseconds& ReadTime();
  
	TimeKeeper GetNativeTime() const {return theTime;}
	operator uint64_t () {return static_cast<uint64_t>(theTime);}
	operator double () const {return static_cast<const double>(theTime);}

	double Seconds() const {return static_cast<double>(theTime) / double(1000000);}
	double MilliSeconds() const {return static_cast<double>(theTime) / double(1000);}
	double MicroSeconds() const {return static_cast<double>(theTime);}

#ifdef __APPLE__
	uint32_t hi();
	uint32_t lo();
#endif
};

inline UMicroseconds operator-(const UMicroseconds& in_one, const UMicroseconds& in_two)
{
	UMicroseconds retVal(in_one.GetNativeTime() - in_two.GetNativeTime());
	return retVal;
}

class UMicrosecondsAccumulator
{
public:
	UMicrosecondsAccumulator() : m_start_time(0), m_accumulator(0) {}
	
	void Start();
	void Stop();
	void Clear();
	
	UMicroseconds GetAccumulatedTime() const;
	
	UMicrosecondsAccumulator& operator+=(const UMicrosecondsAccumulator&);
	
protected:
	UMicroseconds m_start_time;
	UMicroseconds m_accumulator;
};

inline UMicroseconds operator-(const UMicrosecondsAccumulator& in_one, const UMicrosecondsAccumulator& in_two)
{
	UMicroseconds retVal(in_one.GetAccumulatedTime() - in_two.GetAccumulatedTime());
	return retVal;
}

//=========================================================================================//
inline void MicrosecondDelay(double amt)
//=========================================================================================//
{
	UMicroseconds than;
	UMicroseconds now;

	do
	{
		now.ReadTime();
	}	while ((now.MicroSeconds() - than.MicroSeconds()) < amt);
}
	
} // namespace wvNS { 
#endif //#ifndef __UMicroseconds_h__
