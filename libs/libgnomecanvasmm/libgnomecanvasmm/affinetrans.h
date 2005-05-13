#ifndef _LIBGNOMECANVASMM_AFFINETRANS_H
#define _LIBGNOMECANVASMM_AFFINETRANS_H

// -*- C++ -*-
/* $Id$ */

/* affinetrans.h
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

#include <glibmm/containers.h>
#include <libgnomecanvasmm/point.h>

namespace Gnome
{

namespace Art
{

//: Used by CanvasItem.
class AffineTrans
{
public:
  //: Initialize the affine as unit matrix, with a scaling factor
  AffineTrans(double scale = 1.0);

  //: aff[6]
  explicit AffineTrans(const double aff[6]);

  AffineTrans(const AffineTrans& src);
  AffineTrans& operator=(const AffineTrans& src);
  ~AffineTrans();
   
  double& operator[](unsigned int idx);
  const double& operator[](unsigned int idx) const;

  double* gobj();
  const double* gobj() const;

  //: Apply the affine to a given point
  //: e.g. Point dst = affine.apply(Point(x,y));
  //: is the same as:
  //: dst.x = x * affine[0] + y * affine[2] + affine[4];
  //: dst.y = x * affine[1] + y * affine[3] + affine[5];
  Point apply_to(const Point& p) const;
  
  //: Apply the affine to a given point
  Point operator*(const Point& p) const;
	
  //: Compose two affines
  AffineTrans operator*(const AffineTrans& aff2);
	
  //: Apply other affine to the affine
  AffineTrans const & operator*=(AffineTrans& other);
  
  bool operator==(const AffineTrans& other) const;
  bool operator!=(const AffineTrans& other) const;
                        
  //: Give the inverse of the affine
  void invert();
  
  //: Flip horizontally and/or vertically the affine
  void flip(bool horiz, bool vert);

  //: Determine whether the affine is rectilinear (rotates 0, 90, 180 or 270 degrees)
  bool rectilinear() const;

  //: Find the affine's "expansion factor", i.e. the scale amount
  double expansion() const;

  //: Set up the identity matrix
  static AffineTrans identity();

  //: Set up a scaling matrix
  static AffineTrans scaling(double s);

  //: Set up a scaling matrix
  static AffineTrans scaling(double sx, double sy);

  //: Set up a rotation matrix; theta is given in degrees
  static AffineTrans rotation(double theta);

  //: Set up a shearing matrix; theta given in degrees
  static AffineTrans shearing(double theta);

  //: Set up a translation matrix
  static AffineTrans translation(double dx, double dy);

  //: Set up a translation matrix
  static AffineTrans translation(const Point& p);

  Glib::ustring to_string() const;

protected:
  double trans_[6];
};

} //namespace Art

} /* namespace Gnome */

std::ostream& operator<<(std::ostream& out, const Gnome::Art::AffineTrans& aff);

#endif // _GNOMEMM_AFFINETRANS_H
