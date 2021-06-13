/*
 * Copyright (C) 2020 Luciano Iam <oss@lucianoiam.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <boost/lexical_cast.hpp>
#include <cmath>
#include <limits>
#include <string>

#include "typed_value.h"

using namespace ArdourSurface;

#define DBL_TOLERANCE 0.001

TypedValue::TypedValue ()
    : _type (Empty)
    , _b (false)
    , _i (0)
    , _d (0)
{
}

TypedValue::TypedValue (bool value)
    : _type (Bool)
    , _b (value)
    , _i (0)
    , _d (0)
{
}

TypedValue::TypedValue (int value)
    : _type (Int)
    , _b (false)
    , _i (value)
    , _d (0)
{
}

TypedValue::TypedValue (double value)
    : _type (Double)
    , _b (false)
    , _i (0)
    , _d (value)
{
}

TypedValue::TypedValue (std::string value)
    : _type (String)
    , _b (false)
    , _i (0)
    , _d (0)
    , _s (value)
{
}

TypedValue::operator bool () const
{
	switch (_type) {
		case Bool:
			return _b;
		case Int:
			return _i != 0;
		case Double:
			return _d != 0;
		case String:
			return _s == "true";
		default:
			return false;
	}
}

TypedValue::operator int () const
{
	switch (_type) {
		case Int:
			return _i;
		case Bool:
			return _b ? 1 : 0;
		case Double:
			return static_cast<int> (_d);
		case String:
			try {
				return boost::lexical_cast<int> (_s);
			} catch (const boost::bad_lexical_cast&) {
				return 0;
			}
		default:
			return 0;
	}
}

TypedValue::operator double () const
{
	switch (_type) {
		case Double:
			return _d;
		case Bool:
			return _b ? 1.f : 0;
		case Int:
			return static_cast<double> (_i);
		case String:
			try {
				return boost::lexical_cast<double> (_s);
			} catch (const boost::bad_lexical_cast&) {
				return 0;
			}
		default:
			return 0;
	}
}

TypedValue::operator std::string () const
{
	switch (_type) {
		case String:
			return _s;
		case Bool:
			return _b ? "true" : "false";
		case Int:
			return boost::lexical_cast<std::string> (_i);
		case Double:
			return boost::lexical_cast<std::string> (_d);
		default:
			return "";
	}
}

bool
TypedValue::operator== (const TypedValue& other) const
{
	if (_type != other._type) {
		/* make an exception when comparing doubles and ints
		 * for example browser json implementations will send
		 * 1 instead of 1.0 removing any type hint
		 */
		if ((_type == Int) && (other._type == Double)) {
			return fabs (static_cast<double> (_i) - other._d) < DBL_TOLERANCE;
		} else if ((_type == Double) && (other._type == Int)) {
			return fabs (_d - static_cast<double> (other._i)) < DBL_TOLERANCE;
		}

		return false;
	}

	switch (_type) {
		case Bool:
			return _b == other._b;
		case Int:
			return _i == other._i;
		case Double: {
			double inf = std::numeric_limits<double>::infinity ();
			return ((_d == inf) && (other._d == inf)) || ((_d == -inf) && (other._d == -inf)) || (fabs (_d - other._d) < DBL_TOLERANCE);
		}
		case String:
			return _s == other._s;
		default:
			return false;
	}
}

bool
TypedValue::operator!= (const TypedValue& other) const
{
	return !(*this == other);
}

std::string
TypedValue::debug_str () const
{
	char s[256];

	sprintf (s, "type = %d; b = %d; i = %d; d = %f; s = \"%s\"",
	         _type, _b, _i, _d, _s.c_str ());

	return s;
}
