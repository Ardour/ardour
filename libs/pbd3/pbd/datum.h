/*
    Copyright (C) 1998-99 Paul Barton-Davis 
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

#ifndef __qm_datum_h__
#define __qm_datum_h__

/* A basic data type used whenever we want to represent
   something that might be a string or a number.
*/

struct Datum {
	enum Type {
		String,
		Numeric,
	};
	Type type;
	union {
		const char *str;
		float n;
	};

	Datum &operator=(float val) {
		type = Numeric;
		n = val;
		return *this;
	}

	Datum &operator=(int val) {
		type = Numeric;
		n=(float) val;
		return *this;
	}

	Datum &operator=(const char *val) {
		type = String;
		str = val;
		return *this;
	}
};

#endif // __qm_datum_h__
