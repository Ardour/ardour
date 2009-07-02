// -*- c++ -*-
#ifndef _GLIBMM_PATTERN_H
#define _GLIBMM_PATTERN_H

/* $Id: pattern.h 779 2009-01-19 17:58:50Z murrayc $ */

/* pattern.h
 *
 * Copyright (C) 2002 The gtkmm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

extern "C" { typedef struct _GPatternSpec GPatternSpec; }

#include <glibmm/ustring.h>


namespace Glib
{

/** @defgroup PatternMatching Glob-style Pattern Matching
 * Match strings against patterns containing '*' (wildcard) and '?' (joker).
 * @{
 */

class PatternSpec
{
public:
  explicit PatternSpec(const Glib::ustring& pattern);
  explicit PatternSpec(GPatternSpec* gobject);
  ~PatternSpec();

  bool match(const Glib::ustring& str) const;
  bool match(const Glib::ustring& str, const Glib::ustring& str_reversed) const;

  bool operator==(const PatternSpec& rhs) const;
  bool operator!=(const PatternSpec& rhs) const;

  GPatternSpec*       gobj()       { return gobject_; }
  const GPatternSpec* gobj() const { return gobject_; }

private:
  GPatternSpec* gobject_;

  // noncopyable
  PatternSpec(const PatternSpec&);
  PatternSpec& operator=(const PatternSpec&);
};

/** @} group PatternMatching */

} // namespace Glib


#endif /* _GLIBMM_PATTERN_H */

