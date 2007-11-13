// -*- c++ -*-
#ifndef _GLIBMM_RANDOM_H
#define _GLIBMM_RANDOM_H

/* $Id: random.h,v 1.1.1.1 2003/01/07 16:58:52 murrayc Exp $ */

/* random.h
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

#include <glib/gtypes.h>

extern "C" { typedef struct _GRand GRand; }


namespace Glib
{

/** @defgroup Random Random Numbers
 * Pseudo random number generator.
 * @{
 */

class Rand
{
public:
  Rand();
  explicit Rand(guint32 seed);
  ~Rand();

  void set_seed(guint32 seed);

  bool get_bool();

  guint32 get_int();
  gint32  get_int_range(gint32 begin, gint32 end);

  double get_double();
  double get_double_range(double begin, double end);

  GRand*       gobj()       { return gobject_; }
  const GRand* gobj() const { return gobject_; }

private:
  GRand* gobject_;

  // noncopyable
  Rand(const Rand&);
  Rand& operator=(const Rand&);
};

/** @} group Random */

} // namespace Glib


#endif /* _GLIBMM_RANDOM_H */

