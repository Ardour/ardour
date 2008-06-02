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

#include <libgnomecanvasmm/affinetrans.h>
#include <libgnomecanvas/gnome-canvas.h>

namespace Gnome
{

namespace Art
{

AffineTrans::AffineTrans(double scale)
{
  trans_[0] = scale;
  trans_[1] = 0.0;
  trans_[2] = 0.0;
  trans_[3] = scale;
  trans_[4] = 0.0;
  trans_[5] = 0.0;
}

AffineTrans::AffineTrans(const double aff[6])
{
	trans_[0] = aff[0];
	trans_[1] = aff[1];
	trans_[2] = aff[2];
	trans_[3] = aff[3];
	trans_[4] = aff[4];
	trans_[5] = aff[5];
}

AffineTrans::AffineTrans(const AffineTrans& src)
{
  operator=(src);
}

AffineTrans::~AffineTrans()
{
}

AffineTrans& AffineTrans::operator=(const AffineTrans& src)
{
  for(unsigned int i = 0; i < 6; i++)
  {
    trans_[i] = src.trans_[i];
  }

  return *this;
}

double&
AffineTrans::operator[](unsigned int idx)
{
  if(idx > 5)
  {
    g_warning("AffineTrans::operator[] called with idx > 5");
    return trans_[5]; //0; //Can't convert 0 to double& - throw exception?
  }

  return trans_[idx];
}

const double&
AffineTrans::operator[](unsigned int idx) const
{
  if(idx > 5)
  {
    g_warning("AffineTrans::operator[] const called with idx > 5");
    return trans_[5]; //0; //Can't convert 0 to double& - throw exception?
  }

  return trans_[idx];
}

Point AffineTrans::apply_to(const Point& p) const
{
  Point result;
  art_affine_point(result.gobj(), p.gobj(), trans_);
  return result;
}
  
Point AffineTrans::operator*(const Point& p) const
{
  return apply_to(p);
}

AffineTrans
AffineTrans::operator*(const AffineTrans& aff2)
{
  AffineTrans result;
  art_affine_multiply(result.gobj(), gobj(), aff2.gobj());
  return result;
}

bool AffineTrans::operator==(const AffineTrans& other) const
{
  return (bool)art_affine_equal(const_cast<double*>(trans_),
                                const_cast<double*>(other.gobj()));
}
 
bool AffineTrans::operator!=(const AffineTrans& other) const
{
  return !(bool)art_affine_equal(const_cast<double*>(trans_),
                                 const_cast<double*>(other.gobj()));
}
                        
void AffineTrans::invert()
{
  art_affine_invert(trans_, trans_);
}
  
void AffineTrans::flip(bool horiz, bool vert)
{
  art_affine_flip(trans_, trans_, horiz, vert);
}

bool AffineTrans::rectilinear() const
{
  return art_affine_rectilinear(trans_);
}

double AffineTrans::expansion() const
{
  return art_affine_expansion(trans_);
}

AffineTrans const &
AffineTrans::operator*=(AffineTrans& other)
{
  art_affine_multiply(gobj(), gobj(), other.gobj());
  return *this;
}


AffineTrans AffineTrans::identity()
{
  AffineTrans tmp;
	art_affine_identity(tmp.gobj());
	return tmp;
}

AffineTrans
AffineTrans::scaling(double s)
{
  return scaling(s, s);
}

AffineTrans
AffineTrans::scaling(double sx, double sy)
{
	AffineTrans tmp;
	art_affine_scale(tmp.gobj(), sx, sy);
	return tmp;
}

AffineTrans
AffineTrans::rotation(double theta)
{
	AffineTrans tmp;
	art_affine_rotate(tmp.gobj(), theta);
	return tmp;
}

AffineTrans
AffineTrans::translation(double dx, double dy)
{
	AffineTrans tmp;
	art_affine_translate(tmp.gobj(), dx, dy);
	return tmp;
}

AffineTrans
AffineTrans::translation(const Point& p)
{
	AffineTrans tmp;
	art_affine_translate(tmp.gobj(), p.get_x(), p.get_y());
	return tmp;
}


AffineTrans
AffineTrans::shearing(double theta)
{
	AffineTrans tmp;
	art_affine_shear(tmp.gobj(), theta);
	return tmp;
}

double* AffineTrans::gobj()
{
  return trans_;
} 

const double* AffineTrans::gobj() const
{
  return trans_;
}

Glib::ustring AffineTrans::to_string() const
{
  char pchStr[128];
  pchStr[127] = 0; //Just in case art_affine_to_string doesn't work properly.
  art_affine_to_string(pchStr, gobj());
  return Glib::ustring(pchStr);
}

} //namespace Art

} //namespace Gnome

std::ostream& operator<<(std::ostream& out, const Gnome::Art::AffineTrans& aff)
{
  return out << aff.to_string();
}
