/* Copyright (C) 2005 The cairomm Development Team
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <cairomm/pattern.h>
#include <cairomm/private.h>

namespace Cairo
{

Pattern::Pattern()
: m_cobject(0)
{
}

Pattern::Pattern(cairo_pattern_t* cobject, bool has_reference)
: m_cobject(0)
{
  if(has_reference)
    m_cobject = cobject;
  else
    m_cobject = cairo_pattern_reference(cobject);
}

Pattern::~Pattern()
{
  if(m_cobject)
    cairo_pattern_destroy(m_cobject);
}

void Pattern::reference() const
{
 cairo_pattern_reference(m_cobject);
}

void Pattern::unreference() const
{
  cairo_pattern_destroy(m_cobject);
}

void Pattern::set_matrix(const cairo_matrix_t &matrix)
{
  cairo_pattern_set_matrix(m_cobject, &matrix);
  check_object_status_and_throw_exception(*this);
}

void Pattern::get_matrix(cairo_matrix_t &matrix) const
{
  cairo_pattern_get_matrix(m_cobject, &matrix);
  check_object_status_and_throw_exception(*this);
}

PatternType Pattern::get_type() const
{
  cairo_pattern_type_t pattern_type = cairo_pattern_get_type(m_cobject);
  check_object_status_and_throw_exception(*this);
  return static_cast<PatternType>(pattern_type);
}



SolidPattern::SolidPattern(cairo_pattern_t* cobject, bool has_reference)
: Pattern(cobject, has_reference)
{
}

SolidPattern::~SolidPattern()
{
}

RefPtr<SolidPattern> SolidPattern::create_rgb(double red, double green, double blue)
{
  cairo_pattern_t* cobject = cairo_pattern_create_rgb(red, green, blue);
  check_status_and_throw_exception(cairo_pattern_status(cobject)); 
  return RefPtr<SolidPattern>(new SolidPattern(cobject, true /* has reference */));
}

RefPtr<SolidPattern> SolidPattern::create_rgba(double red, double green, double blue, double alpha)
{
  cairo_pattern_t* cobject  = cairo_pattern_create_rgba(red, green, blue, alpha);
  check_status_and_throw_exception(cairo_pattern_status(cobject));
  return RefPtr<SolidPattern>(new SolidPattern(cobject, true /* has reference */));
}


SurfacePattern::SurfacePattern(const RefPtr<Surface>& surface)
{
  m_cobject = cairo_pattern_create_for_surface(surface->cobj());
  check_object_status_and_throw_exception(*this); 
}

RefPtr<SurfacePattern> SurfacePattern::create(const RefPtr<Surface>& surface)
{
  return RefPtr<SurfacePattern>(new SurfacePattern(surface));
}

SurfacePattern::SurfacePattern(cairo_pattern_t* cobject, bool has_reference)
: Pattern(cobject, has_reference)
{
}

SurfacePattern::~SurfacePattern()
{
}

void SurfacePattern::set_extend(Extend extend)
{
  cairo_pattern_set_extend(m_cobject, (cairo_extend_t)extend);
  check_object_status_and_throw_exception(*this);
}

Extend SurfacePattern::get_extend() const
{
  const Extend result = static_cast<Extend>(cairo_pattern_get_extend(m_cobject));
  check_object_status_and_throw_exception(*this);
  return result;
}

void SurfacePattern::set_filter(Filter filter)
{
  cairo_pattern_set_filter(m_cobject, (cairo_filter_t)filter);
  check_object_status_and_throw_exception(*this);
}

Filter SurfacePattern::get_filter() const
{
  Filter result = static_cast<Filter>(cairo_pattern_get_filter(m_cobject));
  check_object_status_and_throw_exception(*this);
  return result;
}



Gradient::Gradient()
{
}

Gradient::Gradient(cairo_pattern_t* cobject, bool has_reference)
: Pattern(cobject, has_reference)
{
}

Gradient::~Gradient()
{
}

void Gradient::add_color_stop_rgb(double offset, double red, double green, double blue)
{
  cairo_pattern_add_color_stop_rgb(m_cobject, offset, red, green, blue);
  check_object_status_and_throw_exception(*this);
}

void Gradient::add_color_stop_rgba(double offset, double red, double green, double blue, double alpha)
{
  cairo_pattern_add_color_stop_rgba(m_cobject, offset, red, green, blue, alpha);
  check_object_status_and_throw_exception(*this);
}



LinearGradient::LinearGradient(double x0, double y0, double x1, double y1)
{
  m_cobject = cairo_pattern_create_linear(x0, y0, x1, y1);
  check_object_status_and_throw_exception(*this); 
}

RefPtr<LinearGradient> LinearGradient::create(double x0, double y0, double x1, double y1)
{
  return RefPtr<LinearGradient>(new LinearGradient(x0, y0, x1, y1));
}

LinearGradient::LinearGradient(cairo_pattern_t* cobject, bool has_reference)
: Gradient(cobject, has_reference)
{
}

LinearGradient::~LinearGradient()
{
}


RadialGradient::RadialGradient(double cx0, double cy0, double radius0, double cx1, double cy1, double radius1)
{
  m_cobject = cairo_pattern_create_radial(cx0, cy0, radius0, cx1, cy1, radius1);
  check_object_status_and_throw_exception(*this); 
}

RefPtr<RadialGradient> RadialGradient::create(double cx0, double cy0, double radius0, double cx1, double cy1, double radius1)
{
  return RefPtr<RadialGradient>(new RadialGradient(cx0, cy0, radius0, cx1, cy1, radius1));
}

RadialGradient::RadialGradient(cairo_pattern_t* cobject, bool has_reference)
: Gradient(cobject, has_reference)
{
}

RadialGradient::~RadialGradient()
{
}


} //namespace Cairo

// vim: ts=2 sw=2 et
