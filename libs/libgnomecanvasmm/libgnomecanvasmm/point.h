#ifndef _LIBGNOMECANVASMM_POINT_H
#define _LIBGNOMECANVASMM_POINT_H

// -*- C++ -*-
/* $Id$ */

/* point.h
 * 
 * Copyright (C) 1999 The gnomemm Development Team
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

#include <libgnomecanvas/gnome-canvas.h>
//#include <libgnomecanvasmm/types.h>
#include <iostream>

namespace Gnome
{

namespace Art
{

/** Wrapper for ArtPoint struct.
 * Used by AffineTrans and CanvasPoints.
 */
class Point
{
public:
  Point(gdouble x = 0.0, gdouble y = 0.0);
  Point(const ArtPoint& artpoint);
  Point(const Point& src);
  Point& operator=(const Point& src);
  ~Point();

  gdouble get_x() const;
  void set_x(gdouble x);
  gdouble get_y() const;
  void set_y(gdouble y);
  
  Point operator+(const Point& p2);
  Point operator-(const Point& p2);
  Point const & operator+=(const Point& other);
  Point const & operator-=(const Point& other);

  ArtPoint* gobj();
  const ArtPoint* gobj() const;

  protected:
    //GnomeCanvasPoints uses arrays of double, 2 at a time, which is the same as a set of ArtPoints
    //because an ArtPoint struct only has 2 double members.
    ArtPoint m_ArtPoint;
};

} //namespace Art

} //namespace Gnome

std::ostream& operator<<(std::ostream& out, const Gnome::Art::Point& p);

#endif /* _LIBGNOMECANVASMM_POINT_H */

