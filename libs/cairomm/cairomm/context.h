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

#ifndef __CAIROMM_CONTEXT_H
#define __CAIROMM_CONTEXT_H

#include <cairomm/surface.h>
#include <cairomm/fontface.h>
#include <cairomm/pattern.h>
#include <cairomm/path.h>
#include <valarray>
#include <vector>
#include <cairo.h>


namespace Cairo
{

typedef cairo_glyph_t Glyph; //A simple struct.
typedef cairo_font_extents_t FontExtents; //A simple struct.
typedef cairo_text_extents_t TextExtents; //A simple struct.
typedef cairo_matrix_t Matrix; //A simple struct. //TODO: Derive and add operator[] and operator. matrix multiplication?

/** Context is the main class used to draw in cairomm. 
 * In the simplest case, create a Context with its target Surface, set its
 * drawing options (line width, color, etc), create shapes with methods like
 * move_to() and line_to(), and then draw the shapes to the Surface using
 * methods such as stroke() or fill().
 *
 * Context is a reference-counted object that should be used via Cairo::RefPtr.
 */
class Context
{
protected:
  explicit Context(const RefPtr<Surface>& target);

public:

  /** Create a C++ wrapper for the C instance. This C++ instance should then be
   * given to a RefPtr.
   *
   * @param cobject The C instance.
   * @param has_reference Whether we already have a reference. Otherwise, the
   * constructor will take an extra reference.
   */
  explicit Context(cairo_t* cobject, bool has_reference = false);

  static RefPtr<Context> create(const RefPtr<Surface>& target);

  virtual ~Context();

  /** Makes a copy of the current state of the Context and saves it on an
   * internal stack of saved states. When restore() is called, it will be
   * restored to the saved state. Multiple calls to save() and restore() can be
   * nested; each call to restore() restores the state from the matching paired
   * save().
   *
   * It isn't necessary to clear all saved states before a cairo_t is freed. 
   * Any saved states will be freed when the Context is destroyed.
   *
   * @sa restore()
   */
  void save();

  /** Restores cr to the state saved by a preceding call to save() and removes
   * that state from the stack of saved states.
   *
   * @sa save()
   */
  void restore();

  /** Sets the compositing operator to be used for all drawing operations. See
   * Operator for details on the semantics of each available compositing
   * operator.
   *
   * @param op	a compositing operator, specified as a Operator
   */
  void set_operator(Operator op);

  /** Sets the source pattern within the Context to source. This Pattern will
   * then be used for any subsequent drawing operation until a new source
   * pattern is set.
   *
   * Note: The Pattern's transformation matrix will be locked to the user space
   * in effect at the time of set_source(). This means that further
   * modifications of the current transformation matrix will not affect the
   * source pattern. 
   *
   * @param source	a Pattern to be used as the source for subsequent drawing
   * operations.
   *
   * @sa Pattern::set_matrix()
   * @sa set_source_rgb()
   * @sa set_source_rgba()
   * @sa set_source(const RefPtr<Surface>& surface, double x, double y)
   */
  void set_source(const RefPtr<const Pattern>& source);

  /** Sets the source pattern within the Context to an opaque color. This
   * opaque color will then be used for any subsequent drawing operation until
   * a new source pattern is set.
   *
   * The color components are floating point numbers in the range 0 to 1. If
   * the values passed in are outside that range, they will be clamped.
   *
   * @param red	red component of color
   * @param green	green component of color
   * @param blue	blue component of color
   *
   * @sa set_source_rgba()
   * @sa set_source()
   */
  void set_source_rgb(double red, double green, double blue);

  /** Sets the source pattern within the Context to a translucent color. This
   * color will then be used for any subsequent drawing operation until a new
   * source pattern is set.
   *
   * The color and alpha components are floating point numbers in the range 0
   * to 1. If the values passed in are outside that range, they will be
   * clamped.
   *
   * @param red	red component of color
   * @param green	green component of color
   * @param blue	blue component of color
   * @param alpha	alpha component of color
   *
   * @sa set_source_rgb()
   * @sa set_source()
   */
  void set_source_rgba(double red, double green, double blue, double alpha);

  /** This is a convenience function for creating a pattern from a Surface and
   * setting it as the source
   *
   * The x and y parameters give the user-space coordinate at which the Surface
   * origin should appear. (The Surface origin is its upper-left corner before
   * any transformation has been applied.) The x and y patterns are negated and
   * then set as translation values in the pattern matrix.
   *
   * Other than the initial translation pattern matrix, as described above, all
   * other pattern attributes, (such as its extend mode), are set to the
   * default values as in Context::create(const RefPtr<Surface>& target). The
   * resulting pattern can be queried with get_source() so that these
   * attributes can be modified if desired, (eg. to create a repeating pattern
   * with Pattern::set_extend()).
   *
   * @param surface : 	a Surface to be used to set the source pattern
   * @param x : 	User-space X coordinate for surface origin
   * @param y : 	User-space Y coordinate for surface origin 
   */
  void set_source(const RefPtr<Surface>& surface, double x, double y);

  /** Sets the tolerance used when converting paths into trapezoids. Curved
   * segments of the path will be subdivided until the maximum deviation
   * between the original path and the polygonal approximation is less than
   * tolerance. The default value is 0.1. A larger value will give better
   * performance, a smaller value, better appearance. (Reducing the value from
   * the default value of 0.1 is unlikely to improve appearance significantly.)
   *
   * @param tolerance	the tolerance, in device units (typically pixels)
   */
  void set_tolerance(double tolerance);

  /** Set the antialiasing mode of the rasterizer used for drawing shapes. This
   * value is a hint, and a particular backend may or may not support a
   * particular value. At the current time, no backend supports
   * CAIRO_ANTIALIAS_SUBPIXEL when drawing shapes.
   *
   * Note that this option does not affect text rendering, instead see
   * FontOptions::set_antialias().
   *
   * @param antialias	the new antialiasing mode
   */
  void set_antialias(Antialias antialias);

  /** Set the current fill rule within the cairo Context. The fill rule is used
   * to determine which regions are inside or outside a complex (potentially
   * self-intersecting) path. The current fill rule affects both fill() and
   * clip(). See FillRule for details on the semantics of each available fill
   * rule.
   *
   * @param fill_rule	a fill rule, specified as a FillRule
   */
  void set_fill_rule(FillRule fill_rule);

  /** Sets the current line width within the cairo Context. The line width
   * specifies the diameter of a pen that is circular in user-space.
   *
   * As with the other stroke parameters, the current line cap style is
   * examined by stroke(), stroke_extents(), and stroke_to_path(), but does not
   * have any effect during path construction.
   *
   * @param width	a line width, as a user-space value
   */
  void set_line_width(double width);
  
  /** Sets the current line cap style within the cairo Context. See
   * LineCap for details about how the available line cap styles are drawn.
   *
   * As with the other stroke parameters, the current line cap style is
   * examined by stroke(), stroke_extents(), and stroke_to_path(), but does not
   * have any effect during path construction.
   *
   * @param line_cap	a line cap style, as a LineCap
   */
  void set_line_cap(LineCap line_cap);

  /** Sets the current line join style within the cairo Context. See LineJoin
   * for details about how the available line join styles are drawn.
   *
   * As with the other stroke parameters, the current line join style is
   * examined by stroke(), stroke_extents(), and stroke_to_path(), but does not
   * have any effect during path construction.
   *
   * @param line_join	a line joint style, as a LineJoin
   */
  void set_line_join(LineJoin line_join);

  /** Sets the dash pattern to be used by stroke(). A dash pattern is specified
   * by dashes, an array of positive values. Each value provides the user-space
   * length of altenate "on" and "off" portions of the stroke. The offset
   * specifies an offset into the pattern at which the stroke begins.
   *
   * If dashes is empty dashing is disabled.  If the size of dashes is 1, a
   * symmetric pattern is assumed with alternating on and off portions of the
   * size specified by the single value in dashes.
   *
   * It is invalid for any value in dashes to be negative, or for all values to
   * be 0.  If this is the case, an exception will be thrown
   *
   * @param dashes	an array specifying alternate lengths of on and off portions
   * @param offset	an offset into the dash pattern at which the stroke should start
   *
   * @exception
   */
  void set_dash(std::valarray<double>& dashes, double offset);

  /** This function disables a dash pattern that was set with set_dash()
   */
  void unset_dash();
  void set_miter_limit(double limit);

  /** Modifies the current transformation matrix (CTM) by translating the
   * user-space origin by (tx, ty). This offset is interpreted as a user-space
   * coordinate according to the CTM in place before the new call to
   * cairo_translate. In other words, the translation of the user-space origin
   * takes place after any existing transformation.
   *
   * @param tx	amount to translate in the X direction
   * @param ty	amount to translate in the Y direction 
   */
  void translate(double tx, double ty);

  /** Modifies the current transformation matrix (CTM) by scaling the X and Y
   * user-space axes by sx and sy respectively. The scaling of the axes takes
   * place after any existing transformation of user space.
   *
   * @param sx	scale factor for the X dimension
   * @param sy	scale factor for the Y dimension
   */
  void scale(double sx, double sy);

  /** Modifies the current transformation matrix (CTM) by rotating the
   * user-space axes by angle radians. The rotation of the axes takes places
   * after any existing transformation of user space. The rotation direction
   * for positive angles is from the positive X axis toward the positive Y
   * axis.
   *
   * @param angle	angle (in radians) by which the user-space axes will be
   * rotated
   */
  void rotate(double angle_radians);

  /** A convenience wrapper around rotate() that accepts angles in degrees
   *
   * @param angle_degrees angle (in degrees) by which the user-space axes
   * should be rotated
   */
  void rotate_degrees(double angle_degres);

  /** Modifies the current transformation matrix (CTM) by applying matrix as an
   * additional transformation. The new transformation of user space takes
   * place after any existing transformation.
   *
   * @param matrix	a transformation to be applied to the user-space axes
   */
  void transform(const Matrix& matrix);

  /** Modifies the current transformation matrix (CTM) by setting it equal to
   * matrix.
   *
   * @param matrix	a transformation matrix from user space to device space
   */
  void set_matrix(const Matrix& matrix);

  /** Resets the current transformation matrix (CTM) by setting it equal to the
   * identity matrix. That is, the user-space and device-space axes will be
   * aligned and one user-space unit will transform to one device-space unit.
   */
  void set_identity_matrix();

  /** Transform a coordinate from user space to device space by multiplying the
   * given point by the current transformation matrix (CTM).
   *
   * @param x	X value of coordinate (in/out parameter)
   * @param y	Y value of coordinate (in/out parameter)
   */
  void user_to_device(double& x, double& y);

  /** Transform a distance vector from user space to device space. This
   * function is similar to user_to_device() except that the translation
   * components of the CTM will be ignored when transforming (dx,dy).
   *
   * @param dx	X component of a distance vector (in/out parameter)
   * @param dy	Y component of a distance vector (in/out parameter)
   */
  void user_to_device_distance(double& dx, double& dy);

  /** Transform a coordinate from device space to user space by multiplying the
   * given point by the inverse of the current transformation matrix (CTM).
   *
   * @param x	X value of coordinate (in/out parameter)
   * @param y	Y value of coordinate (in/out parameter)
   */
  void device_to_user(double& x, double& y);

  /** Transform a distance vector from device space to user space. This
   * function is similar to device_to_user() except that the translation
   * components of the inverse CTM will be ignored when transforming (dx,dy).
   *
   * @param dx	X component of a distance vector (in/out parameter)
   * @param dy	Y component of a distance vector (in/out parameter)
   */
  void device_to_user_distance(double& dx, double& dy);

  /** Clears the current path. After this call there will be no current point.
   */
  void begin_new_path();

  /** Begin a new subpath. Note that the existing path is not affected. After
   * this call there will be no current point.
   *
   * In many cases, this call is not needed since new subpaths are frequently
   * started with move_to().
   *
   * A call to begin_new_sub_path() is particularly useful when beginning a new
   * subpath with one of the arc() calls. This makes things easier as it is no
   * longer necessary to manually compute the arc's initial coordinates for a
   * call to move_to().
   */
  void begin_new_sub_path();

  /** If the current subpath is not empty, begin a new subpath. After this call
   * the current point will be (x, y).
   *
   * @param x	the X coordinate of the new position
   * @param y	the Y coordinate of the new position
   */
  void move_to(double x, double y);

  /** Adds a line to the path from the current point to position (x, y) in
   * user-space coordinates. After this call the current point will be (x, y).
   *
   * @param x	the X coordinate of the end of the new line
   * @param y	the Y coordinate of the end of the new line
   */
  void line_to(double x, double y);

  /** Adds a cubic Bezier spline to the path from the current point to position
   * (x3, y3) in user-space coordinates, using (x1, y1) and (x2, y2) as the
   * control points. After this call the current point will be (x3, y3).
   *
   * @param x1	the X coordinate of the first control point
   * @param y1	the Y coordinate of the first control point
   * @param x2	the X coordinate of the second control point
   * @param y2	the Y coordinate of the second control point
   * @param x3	the X coordinate of the end of the curve
   * @param y3	the Y coordinate of the end of the curve
   */
  void curve_to(double x1, double y1, double x2, double y2, double x3, double y3);

  /** Adds a circular arc of the given radius to the current path. The arc is
   * centered at (xc, yc), begins at angle1 and proceeds in the direction of
   * increasing angles to end at angle2. If angle2 is less than angle1 it will
   * be progressively increased by 2*M_PI until it is greater than angle1.
   *
   * If there is a current point, an initial line segment will be added to the
   * path to connect the current point to the beginning of the arc.
   *
   * Angles are measured in radians. An angle of 0 is in the direction of the
   * positive X axis (in user-space). An angle of M_PI radians (90 degrees) is
   * in the direction of the positive Y axis (in user-space). Angles increase
   * in the direction from the positive X axis toward the positive Y axis. So
   * with the default transformation matrix, angles increase in a clockwise
   * direction.
   *
   * (To convert from degrees to radians, use degrees * (M_PI / 180.).)
   *
   * This function gives the arc in the direction of increasing angles; see
   * arc_negative() to get the arc in the direction of decreasing angles.
   *
   * The arc is circular in user-space. To achieve an elliptical arc, you can
   * scale the current transformation matrix by different amounts in the X and
   * Y directions. For example, to draw an ellipse in the box given by x, y,
   * width, height:
   *
   * @code
   * context->save();
   * context->translate(x, y);
   * context->scale(width / 2.0, height / 2.0);
   * context->arc(0.0, 0.0, 1.0, 0.0, 2 * M_PI);
   * context->restore();
   * @endcode
   *
   * @param xc	X position of the center of the arc
   * @param yc	Y position of the center of the arc
   * @param radius	the radius of the arc
   * @param angle1	the start angle, in radians
   * @param angle2	the end angle, in radians
   */
  void arc(double xc, double yc, double radius, double angle1, double angle2);

  /** Adds a circular arc of the given radius to the current path. The arc is
   * centered at (xc, yc), begins at angle1 and proceeds in the direction of
   * decreasing angles to end at angle2. If angle2 is greater than angle1 it
   * will be progressively decreased by 2*M_PI until it is greater than angle1.
   *
   * See arc() for more details. This function differs only in the direction of
   * the arc between the two angles.
   *
   * @param xc	X position of the center of the arc
   * @param yc	Y position of the center of the arc
   * @param radius	the radius of the arc
   * @param angle1	the start angle, in radians
   * @param angle2	the end angle, in radians
   */
  void arc_negative(double xc, double yc, double radius, double angle1, double angle2);

  /** If the current subpath is not empty, begin a new subpath. After this call
   * the current point will offset by (x, y).
   *
   * Given a current point of (x, y),
   * @code
   * rel_move_to(dx, dy)
   * @endcode
   * is logically equivalent to
   * @code
   * move_to(x + dx, y + dy)
   * @endcode
   *
   * @param dx	the X offset
   * @param dy	the Y offset
   */
  void rel_move_to(double dx, double dy);

  /** Relative-coordinate version of line_to(). Adds a line to the path from
   * the current point to a point that is offset from the current point by (dx,
   * dy) in user space. After this call the current point will be offset by
   * (dx, dy).
   *
   * Given a current point of (x, y),
   * @code
   * rel_line_to(dx, dy)
   * @endcode
   * is logically equivalent to
   * @code
   * line_to(x + dx, y + dy).
   * @endcode
   *
   * @param dx	the X offset to the end of the new line
   * @param dy	the Y offset to the end of the new line
   */
  void rel_line_to(double dx, double dy);

  /** Relative-coordinate version of curve_to(). All offsets are relative to
   * the current point. Adds a cubic Bezier spline to the path from the current
   * point to a point offset from the current point by (dx3, dy3), using points
   * offset by (dx1, dy1) and (dx2, dy2) as the control points.  After this
   * call the current point will be offset by (dx3, dy3).
   *
   * Given a current point of (x, y),
   * @code
   * rel_curve_to(dx1, dy1, dx2, dy2, dx3, dy3)
   * @endcode
   * is logically equivalent to
   * @code
   * curve_to(x + dx1, y + dy1, x + dx2, y + dy2, x + dx3, y + dy3).
   * @endcode
   *
   * @param dx1	the X offset to the first control point
   * @param dy1	the Y offset to the first control point
   * @param dx2	the X offset to the second control point
   * @param dy2	the Y offset to the second control point
   * @param dx3	the X offset to the end of the curve
   * @param dy3	the Y offset to the end of the curve
   */
  void rel_curve_to(double dx1, double dy1, double dx2, double dy2, double dx3, double dy3);

  /** Adds a closed-subpath rectangle of the given size to the current path at
   * position (x, y) in user-space coordinates.
   *
   * This function is logically equivalent to:
   *
   * @code
   * context->move_to(x, y);
   * context->rel_line_to(width, 0);
   * context->rel_line_to(0, height);
   * context->rel_line_to(-width, 0);
   * context->close_path();
   * @endcode
   *
   * @param x	the X coordinate of the top left corner of the rectangle
   * @param y	the Y coordinate to the top left corner of the rectangle
   * @param width	the width of the rectangle
   * @param height	the height of the rectangle
   */
  void rectangle(double x, double y, double width, double height);

  /** Adds a line segment to the path from the current point to the beginning
   * of the current subpath, (the most recent point passed to move_to()), and
   * closes this subpath.
   *
   * The behavior of close_path() is distinct from simply calling line_to()
   * with the equivalent coordinate in the case of stroking.  When a closed
   * subpath is stroked, there are no caps on the ends of the subpath. Instead,
   * there is a line join connecting the final and initial segments of the
   * subpath.
   */
  void close_path();

  /** A drawing operator that paints the current source everywhere within the
   * current clip region.
   */
  void paint();

  /** A drawing operator that paints the current source everywhere within the
   * current clip region using a mask of constant alpha value alpha. The effect
   * is similar to paint(), but the drawing is faded out using the alpha
   * value.
   *
   * @param alpha	an alpha value, between 0 (transparent) and 1 (opaque)
   */
  void paint_with_alpha(double alpha);

  /** A drawing operator that paints the current source using the alpha channel
   * of pattern as a mask. (Opaque areas of mask are painted with the source,
   * transparent areas are not painted.)
   *
   * @param pattern a Pattern
   */
  void mask(const RefPtr<const Pattern>& pattern);

  /** A drawing operator that paints the current source using the alpha channel
   * of surface as a mask. (Opaque areas of surface are painted with the
   * source, transparent areas are not painted.)
   *
   * @param surface	a Surface
   * @param surface_x	X coordinate at which to place the origin of surface
   * @param surface_y	Y coordinate at which to place the origin of surface
   */
  void mask(const RefPtr<const Surface>& surface, double surface_x, double surface_y);

  /** A drawing operator that strokes the current Path according to the current
   * line width, line join, line cap, and dash settings. After stroke(),
   * the current Path will be cleared from the cairo Context.
   *
   * @sa set_line_width()
   * @sa set_line_join()
   * @sa set_line_cap()
   * @sa set_dash()
   * @sa stroke_preserve().
   */
  void stroke();

  /** A drawing operator that strokes the current Path according to the current
   * line width, line join, line cap, and dash settings. Unlike stroke(),
   * stroke_preserve() preserves the Path within the cairo Context.
   *
   * @sa set_line_width()
   * @sa set_line_join()
   * @sa set_line_cap()
   * @sa set_dash()
   * @sa stroke_preserve().
   */
  void stroke_preserve();

  /** A drawing operator that fills the current path according to the current
   * fill rule, (each sub-path is implicitly closed before being filled). After
   * fill(), the current path will be cleared from the cairo context. 
   *
   * @sa set_fill_rule() 
   * @sa fill_preserve()
   */
  void fill();

  /** A drawing operator that fills the current path according to the current
   * fill rule, (each sub-path is implicitly closed before being filled).
   * Unlike fill(), fill_preserve() preserves the path within the
   * cairo Context.
   *
   * @sa set_fill_rule() 
   * @sa fill().
   */
  void fill_preserve();
  void copy_page();
  void show_page();
  bool in_stroke(double x, double y) const;
  bool in_fill(double x, double y) const;
  void get_stroke_extents(double& x1, double& y1, double& x2, double& y2) const;
  void get_fill_extents(double& x1, double& y1, double& x2, double& y2) const;

  /** Reset the current clip region to its original, unrestricted state. That
   * is, set the clip region to an infinitely large shape containing the target
   * surface. Equivalently, if infinity is too hard to grasp, one can imagine
   * the clip region being reset to the exact bounds of the target surface.
   *
   * Note that code meant to be reusable should not call reset_clip() as it
   * will cause results unexpected by higher-level code which calls clip().
   * Consider using save() and restore() around clip() as a more robust means
   * of temporarily restricting the clip region.
   */
  void reset_clip();

  /** Establishes a new clip region by intersecting the current clip region
   * with the current Path as it would be filled by fill() and according to the
   * current fill rule.
   *
   * After clip(), the current path will be cleared from the cairo Context.
   *
   * The current clip region affects all drawing operations by effectively
   * masking out any changes to the surface that are outside the current clip
   * region.
   *
   * Calling clip() can only make the clip region smaller, never larger.  But
   * the current clip is part of the graphics state, so a temporary restriction
   * of the clip region can be achieved by calling cairo_clip() within a
   * save()/restore() pair. The only other means of increasing the size of the
   * clip region is reset_clip().
   *
   * @sa set_fill_rule()
   */
  void clip();

  /** Establishes a new clip region by intersecting the current clip region
   * with the current path as it would be filled by fill() and according to the
   * current fill rule.
   *
   * Unlike clip(), cairo_clip_preserve preserves the path within the cairo
   * Context. 
   *
   * @sa clip()
   * @sa set_fill_rule()
   */
  void clip_preserve();
  void select_font_face(const std::string& family, FontSlant slant, FontWeight weight);
  void set_font_size(double size);
  void set_font_matrix(const Matrix& matrix);
  void get_font_matrix(Matrix& matrix) const;
  void set_font_options(const FontOptions& options);
  void show_text(const std::string& utf8);
  void show_glyphs(const std::vector<Glyph>& glyphs);
  RefPtr<FontFace> get_font_face();
  RefPtr<const FontFace> get_font_face() const;
  void get_font_extents(FontExtents& extents) const;
  void set_font_face(const RefPtr<const FontFace>& font_face);
  void get_text_extents(const std::string& utf8, TextExtents& extents) const;
  void get_glyph_extents(const std::vector<Glyph>& glyphs, TextExtents& extents) const;
  void text_path(const std::string& utf8);
  void glyph_path(const std::vector<Glyph>& glyphs);

  /** Gets the current compositing operator for a cairo Context
   */
  Operator get_operator() const;

  /** Gets the current source pattern for the Context
   */
  RefPtr<Pattern> get_source();
  RefPtr<const Pattern> get_source() const;

  /** Gets the current tolerance value, as set by set_tolerance()
   */
  double get_tolerance() const;

  /** Gets the current shape antialiasing mode, as set by set_antialias()
   */
  Antialias get_antialias() const;

  /** Gets the current point of the current path, which is conceptually the
   * final point reached by the path so far.
   *
   * The current point is returned in the user-space coordinate system. If
   * there is no defined current point then x and y will both be set to 0.0.
   * 
   * Most path construction functions alter the current point. See the
   * following for details on how they affect the current point: clear_path(),
   * move_to(), line_to(), curve_to(), arc(), rel_move_to(), rel_line_to(),
   * rel_curve_to(), arc(), and text_path()
   *
   * @param x	return value for X coordinate of the current point
   * @param y	return value for Y coordinate of the current point
   */
  void get_current_point (double& x, double& y) const;

  /** Gets the current fill rule, as set by set_fill_rule().
   */
  FillRule get_fill_rule() const;

  /** Gets the current line width, as set by set_line_width()
   */
  double get_line_width() const;

  /** Gets the current line cap style, as set by set_line_cap()
   */
  LineCap get_line_cap() const;

  /** Gets the current line join style, as set by set_line_join()
   */
  LineJoin get_line_join() const;

  /** Gets the current miter limit, as set by set_miter_limit()
   */
  double get_miter_limit() const;

  /** Stores the current transformation matrix (CTM) into matrix.
   *
   * @param matrix	return value for the matrix
   */
  void get_matrix(Matrix& matrix);

  /** Gets the target surface associated with this Context.
   *
   * @exception
   */
  RefPtr<Surface> get_target();

  /** Gets the target surface associated with this Context.
   *
   * @exception
   */
  RefPtr<const Surface> get_target() const;
  
  //TODO: Copy or reference-count a Path somethow instead of asking the caller to delete it?
  /** Creates a copy of the current path and returns it to the user.
   *
   * @todo See cairo_path_data_t for hints on how to iterate over the returned
   * data structure.
   *
   * @note The caller owns the Path object returned from this function.  The
   * Path object must be freed when you are finished with it.
   */
  Path* copy_path() const;

  /** Gets a flattened copy of the current path and returns it to the user
   *
   * @todo See cairo_path_data_t for hints on how to iterate over the returned
   * data structure.
   *
   * This function is like copy_path() except that any curves in the path will
   * be approximated with piecewise-linear approximations, (accurate to within
   * the current tolerance value). That is, the result is guaranteed to not have
   * any elements of type CAIRO_PATH_CURVE_TO which will instead be
   * replaced by a series of CAIRO_PATH_LINE_TO elements. 
   *
   * @note The caller owns the Path object returned from this function.  The
   * Path object must be freed when you are finished with it.
   */
  Path* copy_path_flat() const;

  /** Append the path onto the current path. The path may be either the return
   * value from one of copy_path() or copy_path_flat() or it may be constructed
   * manually. 
   *
   * @param path	path to be appended
   */
  void append_path(const Path& path);

  /** Temporarily redirects drawing to an intermediate surface known as a group.
   * The redirection lasts until the group is completed by a call to pop_group()
   * or pop_group_to_source(). These calls provide the result of any drawing to
   * the group as a pattern, (either as an explicit object, or set as the source
   * pattern).
   *
   * This group functionality can be convenient for performing intermediate
   * compositing. One common use of a group is to render objects as opaque
   * within the group, (so that they occlude each other), and then blend the
   * result with translucence onto the destination.
   *
   * Groups can be nested arbitrarily deep by making balanced calls to
   * push_group()/pop_group(). Each call pushes/pops the new target group
   * onto/from a stack.
   *
   * The push_group() function calls save() so that any changes to the graphics
   * state will not be visible outside the group, (the pop_group functions call
   * restore()).
   *
   * By default the intermediate group will have a content type of
   * CONTENT_COLOR_ALPHA. Other content types can be chosen for the group by
   * using push_group_with_content() instead.
   *
   * As an example, here is how one might fill and stroke a path with
   * translucence, but without any portion of the fill being visible under the
   * stroke:
   *
   * @code
   * cr->push_group();
   * cr->set_source(fill_pattern);
   * cr->fill_preserve();
   * cr->set_source(stroke_pattern);
   * cr->stroke();
   * cr->pop_group_to_source();
   * cr->paint_with_alpha(alpha);
   * @endcode
   */
  void push_group();

  /**
   * Temporarily redirects drawing to an intermediate surface known as a
   * group. The redirection lasts until the group is completed by a call
   * to pop_group() or pop_group_to_source(). These calls provide the result of
   * any drawing to the group as a pattern, (either as an explicit object, or set
   * as the source pattern).
   *
   * The group will have a content type of @content. The ability to control this
   * content type is the only distinction between this function and push_group()
   * which you should see for a more detailed description of group rendering.
   *
   * @param content: indicates the type of group that will be created
   */
  void push_group_with_content(Content content);

  /**
   * Terminates the redirection begun by a call to push_group() or
   * push_group_with_content() and returns a new pattern containing the results
   * of all drawing operations performed to the group.
   *
   * The pop_group() function calls restore(), (balancing a call to save() by
   * the push_group function), so that any changes to the graphics state will
   * not be visible outside the group.
   *
   * @return a (surface) pattern containing the results of all drawing
   * operations performed to the group.
   **/
  RefPtr<Pattern> pop_group();

  /**
   * Terminates the redirection begun by a call to push_group() or
   * push_group_with_content() and installs the resulting pattern as the source
   * pattern in the given cairo Context.
   *
   * The behavior of this function is equivalent to the sequence of operations:
   *
   * @code
   * RefPtr<Pattern> group = cr->pop_group();
   * cr->set_source(group);
   * @endcode
   *
   * but is more convenient as their is no need for a variable to store
   * the short-lived pointer to the pattern.
   *
   * The pop_group() function calls restore(), (balancing a call to save() by
   * the push_group function), so that any changes to the graphics state will
   * not be visible outside the group.
   **/
  void pop_group_to_source();

  /**
   * Gets the target surface for the current group as started by the most recent
   * call to push_group() or push_group_with_content().
   *
   * This function will return NULL if called "outside" of any group rendering
   * blocks, (that is, after the last balancing call to pop_group() or
   * pop_group_to_source()).
   *
   * @exception
   *
   **/
  RefPtr<Surface> get_group_target();

  /**
   * Same as the non-const version but returns a reference to a const Surface
   */
  RefPtr<const Surface> get_group_target() const;

  /** The base cairo C type that is wrapped by Cairo::Context
   */
  typedef cairo_t cobject;

  /** Gets a pointer to the base C type that is wrapped by the Context
   */
  inline cobject* cobj() { return m_cobject; }

  /** Gets a pointer to the base C type that is wrapped by the Context
   */
  inline const cobject* cobj() const { return m_cobject; }
 
  #ifndef DOXYGEN_IGNORE_THIS
  ///For use only by the cairomm implementation.
  inline ErrorStatus get_status() const
  { return cairo_status(const_cast<cairo_t*>(cobj())); }

  void reference() const;
  void unreference() const;
  #endif //DOXYGEN_IGNORE_THIS

protected:

 
  cobject* m_cobject;
};

} // namespace Cairo

#endif //__CAIROMM_CONTEXT_H

// vim: ts=2 sw=2 et
