/* point.cc
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

#include <libgnomecanvasmm/point.h>

namespace Gnome
{

namespace Art
{

Point::Point(const ArtPoint& artpoint)
{
  m_ArtPoint = artpoint;
}
  
Point::Point(gdouble x /* = 0.0 */, gdouble y /* = 0.0 */)
{
  m_ArtPoint.x = x;
  m_ArtPoint.y = y;
}

Point::Point(const Point& src)
{
  operator=(src);
}

Point& Point::operator=(const Point& src)
{
  m_ArtPoint = src.m_ArtPoint;
  return *this;
}

Point::~Point()
{
}

gdouble Point::get_x() const
{
  return m_ArtPoint.x; 
}

void Point::set_x(gdouble x)
{
  m_ArtPoint.x = x; 
}

gdouble Point::get_y() const
{
  return m_ArtPoint.y; 
}

void Point::set_y(gdouble y)
{
  m_ArtPoint.y = y;
}
  
Point Point::operator+(const Point& p2)
{
  return Point(get_x() + p2.get_x(), get_y() + p2.get_y());
}

Point Point::operator-(const Point& p2)
{
  return Point(get_x() - p2.get_x(), get_y() - p2.get_y());
}

Point const & Point::operator+=(const Point& other)
{
  set_x(get_x() + other.get_x());
  set_y(get_y() + other.get_y());
  return *this;
}

Point const & Point::operator-=(const Point& other)
{
  set_x(get_x() - other.get_x());
  set_y(get_y() - other.get_y());
  return *this;
}

ArtPoint* Point::gobj()
{
  return &m_ArtPoint;
}

const ArtPoint* Point::gobj() const
{
  return &m_ArtPoint;
}


} //namespace Art

} //namespace Gnome


std::ostream& operator<<(std::ostream& out, const Gnome::Art::Point& p)
{
  return out << '(' << p.get_x() << ", " << p.get_y() << ')';
}
