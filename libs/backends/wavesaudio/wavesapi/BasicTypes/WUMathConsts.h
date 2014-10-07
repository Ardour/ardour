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

#ifndef __WUMathConsts_h__
	#define __WUMathConsts_h__
	
/* Copy to include:
#include "BasicTypes/WUMathConsts.h"
*/

const float kfPI =  3.1415926535898f; // PI, single precision
const double kdPI = 3.1415926535897932384626433832795; // PI, double precision

const float kf2PI =  6.2831853071796f; // 2*PI
const double kd2PI = 6.283185307179586476925286766559; // 2*PI

const float kfhalfPI =  1.5707963267949f; // 0.5*PI
const double kdhalfPI = 1.57079632679489661923; // 0.5*PI

const double kdLn2 = 0.69314718055994530942;	// natural log(2.0)
const double kdOneOverLn2 = 1.4426950408889634073599246810019;	// natural (1.0/log(2.0)) - for multiply log() to get it as with base 2

const double kdLog2 = 0.301029995663981;	// log10(2.0)
const double kdOneOverLog2 = 3.321928094887363;	// (1.0/log10(2.0)) - for multiply log() to get it as with base 2

const double kdExponent = 2.718281828459045235360287471352; // e

const double kdSqrt2 = 1.41421356237309504880; // sqrt(2)



#endif //__WUMathConsts_h__
