divert(-1)

dnl $Id: value_basictypes.cc.m4 348 2006-11-22 11:14:43Z murrayc $

dnl  Glib::Value specializations for fundamental types
dnl
dnl  Copyright 2002 The gtkmm Development Team
dnl
dnl  This library is free software; you can redistribute it and/or
dnl  modify it under the terms of the GNU Library General Public
dnl  License as published by the Free Software Foundation; either
dnl  version 2 of the License, or (at your option) any later version.
dnl
dnl  This library is distributed in the hope that it will be useful,
dnl  but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
dnl  Library General Public License for more details.
dnl
dnl  You should have received a copy of the GNU Library General Public
dnl  License along with this library; if not, write to the Free
dnl  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

include(template.macros.m4)

dnl
dnl GLIB_VALUE_BASIC(bool, boolean)
dnl
define([GLIB_VALUE_BASIC],[dnl
LINE(]__line__[)dnl

dnl Please ignore the format stuff.  I was just tired and played a little.
/**** Glib::Value<$1> translit(format([%]eval(57-len([$1]))[s],[****/]),[ ],[*])

// static
GType Value<$1>::value_type()
{
  return G_TYPE_[]UPPER($2);
}

void Value<$1>::set($1 data)
{
  g_value_set_$2(&gobject_, data);
}

$1 Value<$1>::get() const
{
  return g_value_get_$2(&gobject_);
}

GParamSpec* Value<$1>::create_param_spec(const Glib::ustring& name) const
{
  return g_param_spec_$2(
      name.c_str(), 0, 0,ifelse($2,pointer,,[
      ifelse($3,,,[$3, $4, ])[]g_value_get_$2(&gobject_),])
      GParamFlags(G_PARAM_READABLE | G_PARAM_WRITABLE));
}
])

divert[]dnl
// -*- c++ -*-
// This is a generated file, do not edit.  Generated from __file__

#include <glibmm/value.h>

namespace Glib
{

G_GNUC_EXTENSION typedef long long long_long;
G_GNUC_EXTENSION typedef unsigned long long unsigned_long_long;

GLIB_VALUE_BASIC(bool, boolean)
GLIB_VALUE_BASIC(char, char, -128, 127)
GLIB_VALUE_BASIC(unsigned char, uchar, 0, 255)
GLIB_VALUE_BASIC(int, int, G_MININT, G_MAXINT)
GLIB_VALUE_BASIC(unsigned int, uint, 0, G_MAXUINT)
GLIB_VALUE_BASIC(long, long, G_MINLONG, G_MAXLONG)
GLIB_VALUE_BASIC(unsigned long, ulong, 0, G_MAXULONG)
GLIB_VALUE_BASIC(long_long, int64, G_GINT64_CONSTANT[](0x8000000000000000), G_GINT64_CONSTANT[](0x7fffffffffffffff))
GLIB_VALUE_BASIC(unsigned_long_long, uint64, G_GINT64_CONSTANT[](0U), G_GINT64_CONSTANT[](0xffffffffffffffffU))
GLIB_VALUE_BASIC(float, float, -G_MAXFLOAT, G_MAXFLOAT)
GLIB_VALUE_BASIC(double, double, -G_MAXDOUBLE, G_MAXDOUBLE)
GLIB_VALUE_BASIC(void*, pointer)
} // namespace Glib

