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

#ifndef __CAIROMM_PATTERN_H
#define __CAIROMM_PATTERN_H

#include <cairomm/surface.h>
#include <cairomm/enums.h>
#include <cairo.h>


namespace Cairo
{
struct ColorStop
{
  double offset;
  double red, green, blue, alpha;
};

/**
 * This is a reference-counted object that should be used via Cairo::RefPtr.
 */
class Pattern
{
protected:
  //Use derived constructors.

  //TODO?: Pattern(cairo_pattern_t *target);

public:

  /** Create a C++ wrapper for the C instance. This C++ instance should then be given to a RefPtr.
   * @param cobject The C instance.
   * @param has_reference Whether we already have a reference. Otherwise, the constructor will take an extra reference.
   */
  explicit Pattern(cairo_pattern_t* cobject, bool has_reference = false);

  virtual ~Pattern();

  void set_matrix(const cairo_matrix_t &matrix);
  void get_matrix(cairo_matrix_t &matrix) const;
  PatternType get_type() const;

  typedef cairo_pattern_t cobject;
  inline cobject* cobj() { return m_cobject; }
  inline const cobject* cobj() const { return m_cobject; }

  #ifndef DOXYGEN_IGNORE_THIS
  ///For use only by the cairomm implementation.
  inline ErrorStatus get_status() const
  { return cairo_pattern_status(const_cast<cairo_pattern_t*>(cobj())); }
  #endif //DOXYGEN_IGNORE_THIS

  void reference() const;
  void unreference() const;

protected:
  //Used by derived types only.
  Pattern();

  cobject* m_cobject;
};

class SolidPattern : public Pattern
{
protected:

public:

  /** Create a C++ wrapper for the C instance.
   * @param cobject The C instance.
   * @param has_reference Whether we already have a reference. Otherwise, the constructor will take an extra reference.
   */
  explicit SolidPattern(cairo_pattern_t* cobject, bool has_reference = false);

  /**
   * Gets the solid color for a solid color pattern.
   *
   * @param red return value for red component of color
   * @param green return value for green component of color
   * @param blue return value for blue component of color
   * @param alpha return value for alpha component of color
   *
   * @since 1.4
   **/
  void get_rgba (double& red, double& green,
                 double& blue, double& alpha) const;

  //TODO: Documentation
  static RefPtr<SolidPattern> create_rgb(double red, double green, double blue);

  //TODO: Documentation
  static RefPtr<SolidPattern> create_rgba(double red, double green,
                                          double blue, double alpha);

  //TODO?: SolidPattern(cairo_pattern_t *target);
  virtual ~SolidPattern();
};

class SurfacePattern : public Pattern
{
protected:

  explicit SurfacePattern(const RefPtr<Surface>& surface);

  //TODO?: SurfacePattern(cairo_pattern_t *target);

public:

  /** Create a C++ wrapper for the C instance. This C++ instance should then be given to a RefPtr.
   * @param cobject The C instance.
   * @param has_reference Whether we already have a reference. Otherwise, the constructor will take an extra reference.
   */
  explicit SurfacePattern(cairo_pattern_t* cobject, bool has_reference = false);

  /**
   * Gets the surface associated with this pattern
   *
   * @since 1.4
   **/
  RefPtr<const Surface> get_surface () const;

  /**
   * Gets the surface associated with this pattern
   *
   * @since 1.4
   **/
  RefPtr<Surface> get_surface ();

  virtual ~SurfacePattern();

  static RefPtr<SurfacePattern> create(const RefPtr<Surface>& surface);

  void set_extend(Extend extend);
  Extend get_extend() const;
  void set_filter(Filter filter);
  Filter get_filter() const;
};

class Gradient : public Pattern
{
protected:
  //Use derived constructors.

  //TODO?: Gradient(cairo_pattern_t *target);

public:

  /** Create a C++ wrapper for the C instance. This C++ instance should then be given to a RefPtr.
   * @param cobject The C instance.
   * @param has_reference Whether we already have a reference. Otherwise, the constructor will take an extra reference.
   */
  explicit Gradient(cairo_pattern_t* cobject, bool has_reference = false);

  virtual ~Gradient();

  /**
   * Adds an opaque color stop to a gradient pattern. The offset
   * specifies the location along the gradient's control vector. For
   * example, a linear gradient's control vector is from (x0,y0) to
   * (x1,y1) while a radial gradient's control vector is from any point
   * on the start circle to the corresponding point on the end circle.
   *
   * The color is specified in the same way as in Context::set_source_rgb().
   *
   * @param offset an offset in the range [0.0 .. 1.0]
   * @param red red component of color
   * @param green green component of color
   * @param blue blue component of color
   **/
  void add_color_stop_rgb(double offset, double red, double green, double blue);

  /**
   * Adds a translucent color stop to a gradient pattern. The offset
   * specifies the location along the gradient's control vector. For
   * example, a linear gradient's control vector is from (x0,y0) to
   * (x1,y1) while a radial gradient's control vector is from any point
   * on the start circle to the corresponding point on the end circle.
   *
   * The color is specified in the same way as in Context::set_source_rgba().
   *
   * @param offset an offset in the range [0.0 .. 1.0]
   * @param red red component of color
   * @param green green component of color
   * @param blue blue component of color
   * @param alpha alpha component of color
   */
  void add_color_stop_rgba(double offset, double red, double green, double blue, double alpha);

  /*
   * Gets the color stops and offsets for this Gradient
   *
   * @since 1.4
   */
  std::vector<ColorStop> get_color_stops() const;


protected:
  Gradient();
};

class LinearGradient : public Gradient
{
protected:

  LinearGradient(double x0, double y0, double x1, double y1);

public:

  /** Create a C++ wrapper for the C instance. This C++ instance should then be given to a RefPtr.
   * @param cobject The C instance.
   * @param has_reference Whether we already have a reference. Otherwise, the constructor will take an extra reference.
   */
  explicit LinearGradient(cairo_pattern_t* cobject, bool has_reference = false);

  /**
   * @param x0 return value for the x coordinate of the first point
   * @param y0 return value for the y coordinate of the first point
   * @param x1 return value for the x coordinate of the second point
   * @param y1 return value for the y coordinate of the second point
   *
   * Gets the gradient endpoints for a linear gradient.
   *
   * @since 1.4
   **/
  void get_linear_points(double &x0, double &y0,
                          double &x1, double &y1) const;

  //TODO?: LinearGradient(cairo_pattern_t *target);
  virtual ~LinearGradient();

  static RefPtr<LinearGradient> create(double x0, double y0, double x1, double y1);
};

class RadialGradient : public Gradient
{
protected:

  RadialGradient(double cx0, double cy0, double radius0, double cx1, double cy1, double radius1);

public:

  /** Create a C++ wrapper for the C instance. This C++ instance should then be given to a RefPtr.
   * @param cobject The C instance.
   * @param has_reference Whether we already have a reference. Otherwise, the constructor will take an extra reference.
   */
  explicit RadialGradient(cairo_pattern_t* cobject, bool has_reference = false);

  /**
   * @param x0 return value for the x coordinate of the center of the first (inner) circle
   * @param y0 return value for the y coordinate of the center of the first (inner) circle
   * @param r0 return value for the radius of the first (inner) circle
   * @param x1 return value for the x coordinate of the center of the second (outer) circle
   * @param y1 return value for the y coordinate of the center of the second (outer) circle
   * @param r1 return value for the radius of the second (outer) circle
   *
   * Gets the gradient endpoint circles for a radial gradient, each
   * specified as a center coordinate and a radius.
   *
   * @since 1.4
   **/
  void get_radial_circles(double& x0, double& y0, double& r0,
                           double& x1, double& y1, double& r1) const;

  //TODO?: RadialGradient(cairo_pattern_t *target);
  virtual ~RadialGradient();

  static RefPtr<RadialGradient> create(double cx0, double cy0, double radius0, double cx1, double cy1, double radius1);
};

} // namespace Cairo

#endif //__CAIROMM_PATTERN_H

// vim: ts=2 sw=2 et
