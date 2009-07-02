// -*- c++ -*-
/* $Id: pattern.cc 749 2008-12-10 14:23:33Z jjongsma $ */

/* pattern.cc
 *
 * Copyright (C) 2002 The gtkmm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <glib.h>
#include <glibmm/pattern.h>


namespace Glib
{

PatternSpec::PatternSpec(const Glib::ustring& pattern)
:
  gobject_ (g_pattern_spec_new(pattern.c_str()))
{}

PatternSpec::PatternSpec(GPatternSpec* gobject)
:
  gobject_ (gobject)
{}

PatternSpec::~PatternSpec()
{
  g_pattern_spec_free(gobject_);
}

bool PatternSpec::match(const Glib::ustring& str) const
{
  return g_pattern_match(gobject_, str.bytes(), str.c_str(), 0);
}

bool PatternSpec::match(const Glib::ustring& str, const Glib::ustring& str_reversed) const
{
  return g_pattern_match(gobject_, str.bytes(), str.c_str(), str_reversed.c_str());
}

bool PatternSpec::operator==(const PatternSpec& rhs) const
{
  return g_pattern_spec_equal(gobject_, rhs.gobject_);
}

bool PatternSpec::operator!=(const PatternSpec& rhs) const
{
  return !g_pattern_spec_equal(gobject_, rhs.gobject_);
}

} // namespace Glib

