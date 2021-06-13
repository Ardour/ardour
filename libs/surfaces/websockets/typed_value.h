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

#ifndef _ardour_surface_websockets_typed_value_h_
#define _ardour_surface_websockets_typed_value_h_

#include <string>

namespace ArdourSurface {

class TypedValue
{
public:
	enum Type {
		Empty,
		Bool,
		Int,
		Double,
		String
	};

	TypedValue ();
	TypedValue (bool);
	TypedValue (int);
	TypedValue (double);
	TypedValue (std::string);

	bool empty () const
	{
		return _type == Empty;
	};
	Type type () const
	{
		return _type;
	};

	operator bool () const;
	operator int () const;
	operator double () const;
	operator std::string () const;

	bool operator== (const TypedValue& other) const;
	bool operator!= (const TypedValue& other) const;

	std::string debug_str () const;

private:
	Type        _type;
	bool        _b;
	int         _i;
	double      _d;
	std::string _s;
};

} // namespace ArdourSurface

#endif // _ardour_surface_websockets_typed_value_h_
