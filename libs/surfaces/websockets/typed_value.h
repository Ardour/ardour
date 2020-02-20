/*
 * Copyright (C) 2020 Luciano Iam <lucianito@gmail.com>
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

#include <string>

#ifndef typed_value_h
#define typed_value_h

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

    TypedValue (): _type (Empty) { }
    TypedValue (bool value): _type { Bool }, _b (value) { }
    TypedValue (int value): _type { Int }, _i (value) { }
    TypedValue (double value): _type { Double }, _d (value) { }
    TypedValue (std::string value): _type { String }, _s (value) { }

    bool empty () const { return _type == Empty; };
    Type type () const { return _type; };

    operator bool () const;
    operator int () const;
    operator double () const;
    operator std::string () const;

    bool operator== (const TypedValue& other) const;
    bool operator!= (const TypedValue& other) const;

    std::string debug_str () const;

  private:

    Type _type;
    bool _b = false;
    int _i = 0;
    double _d = 0.0;
    std::string _s;

};

#endif // typed_value_h
