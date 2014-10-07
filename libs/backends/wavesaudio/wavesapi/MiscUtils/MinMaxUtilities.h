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

#ifndef __MinMaxUtilities_h__
#define __MinMaxUtilities_h__

/* copy to include
#include "MiscUtils/MinMaxUtilities.h"
*/

#include "BasicTypes/WUDefines.h"
#include "BasicTypes/WUMathConsts.h"
#include "WavesPublicAPI/wstdint.h"

#ifdef __GNUC__
#undef round
#endif

// New accelerated templates
#if defined ( __cplusplus ) && !defined (__WUMinMax)
#define __WUMinMax   // Also defined in Nativepr.h


template<class T> inline T WUMin(const T &a, const T &b) {return (a < b) ? a : b;} // requires only < to be defined for T
template<class T> inline T WUMax(const T &a,const T &b) {return (a < b) ? b : a;} // requires only < to be defined for T
template<class T> inline T WUMinMax(const T &Smallest, const T &Biggest, const T &Val)  // requires only < to be defined for T
{	
	return ((Val < Smallest) ? Smallest : ((Biggest < Val) ? Biggest : Val));
}
/*	
// Min and Max
	template<class T> inline T WUMin(T a,T b) {return (a < b) ? a : b;} // requires only < to be defined for T
	template<class T> inline T WUMax(T a,T b) {return (a < b) ? b : a;} // requires only < to be defined for T
	template<class T> inline T WUMinMax(T SMALLEST, T BIGGEST, T X)  // requires only < to be defined for T
	{
		return ((X < SMALLEST) ? SMALLEST : ((BIGGEST < X) ? BIGGEST : X));
	}
 */	
// Absolute value

#ifdef PLATFORM_WINDOWS
	#include <math.h>

#ifndef __GNUC__
#define __abs(x)	abs(x) 
#define __labs(x)	labs(x)
#define __fabs(x)	fabs(x)
#endif

#endif

#ifdef __GNUC__
	#include <iostream> // why don't know makes it work need to check
	#include <cstdlib>
	#include <cmath>

#define __abs(x)	std::abs(x)
#define __labs(x)	std::labs(x)
#define __fabs(x)	std::fabs(x)
#endif
	#ifdef __APPLE__
        #ifdef __GNUC__
            #include <iostream> // why don't know makes it work need to check
            #include <cmath>
#define __abs(x)	std::abs(x)
#define __labs(x)	std::labs(x)
#define __fabs(x)	std::fabs(x)
        #endif
	#endif

// log2: on Windows there's no proper definition for log2, whereas on other platform there is.
	#ifndef WUlog2
    #if defined(PLATFORM_WINDOWS)
        #define WUlog2(x)  (kdOneOverLog2 * log10((x))) 
    #else    
        #define WUlog2(x) log2(x)
    #endif
    #endif

template <class T> inline T WUAbs(const T &xA)
{
	return (xA > T(0))? xA: -xA;
}

template <> inline int WUAbs(const int &xA)
{
	return __abs(xA);
}

//template <> inline int32_t WUAbs(const int32_t &xA)// 64BitConversion
//{
//	return __labs(xA);
//}

template <> inline float WUAbs(const float &xA)
{
	return (float) __fabs(xA);
}

template <> inline double WUAbs(const double &xA)
{
	return __fabs(xA);
}

#endif

int32_t DllExport WURand(intptr_t in_Seed);
int32_t DllExport WURand();
int32_t DllExport rand_gen_formula(int32_t rndSeed);

template <class T> inline bool WUIsEqualWithTolerance(const T &xA, const T &xB, const T &xTolerance)
{
	return (WUAbs(xA - xB) < xTolerance) ? true : false;
}


#endif
