/* Copyright (C) 2006 The cairomm Development Team
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

#ifndef __CAIROMM_SCALEDFONT_H
#define __CAIROMM_SCALEDFONT_H

#include <cairomm/context.h>
#include <cairomm/fontoptions.h>

namespace Cairo
{

typedef enum
{
} ScaledFontType;

/** A ScaledFont is a font scaled to a particular size and device resolution. It
 * is most useful for low-level font usage where a library or application wants
 * to cache a reference to a scaled font to speed up the computation of metrics.
 */
class ScaledFont
{

public: 
  /** The underlying C cairo object type */
  typedef cairo_scaled_font_t cobject;

  /** Provides acces to the underlying C cairo object */
  inline cobject* cobj() { return m_cobject; }

  /** Provides acces to the underlying C cairo object */
  inline const cobject* cobj() const { return m_cobject; }

#ifndef DOXYGEN_IGNORE_THIS
  // For use only by the cairomm implementation.
  inline ErrorStatus get_status() const
  { return cairo_scaled_font_status(const_cast<cairo_scaled_font_t*>(cobj())); }

  // for RefPtr
  void reference() const { cairo_scaled_font_reference(m_cobject); }
  void unreference() const { cairo_scaled_font_destroy(m_cobject); }
#endif //DOXYGEN_IGNORE_THIS

  /** Createa C++ wrapper object from the C instance.  This C++ object should
   * then be given to a RefPtr.
   */
  explicit ScaledFont(cobject* cobj, bool has_reference = false);

  /** Creates a ScaledFont object from a font face and matrices that describe
   * the size of the font and the environment in which it will be used.
   *
   * @param font_face A font face.
   * @param font_matrix font space to user space transformation matrix for the
   * font. In the simplest case of a N point font, this matrix is just a scale
   * by N, but it can also be used to shear the font or stretch it unequally
   * along the two axes. See Context::set_font_matrix().
   * @param ctm user to device transformation matrix with which the font will be
   * used.
   * @param options: options to use when getting metrics for the font and
   * rendering with it.
   */
  static RefPtr<ScaledFont> create(FontFace& font_face, const Matrix& font_matrix,
      const Matrix& ctm, const FontOptions& options);

  //TODO: This should really be get_extents().
  /** Gets the metrics for a ScaledFont */
  void extents(FontExtents& extents) const;

  //TODO: This should really be get_text_extents().
  /** Gets the extents for a string of text. The extents describe a user-space
   * rectangle that encloses the "inked" portion of the text drawn at the origin
   * (0,0) (as it would be drawn by Context::show_text() if the cairo graphics
   * state were set to the same font_face, font_matrix, ctm, and font_options as
   * the ScaledFont object).  Additionally, the x_advance and y_advance values
   * indicate the amount by which the current point would be advanced by
   * Context::show_text().
   *
   * Note that whitespace characters do not directly contribute to the size of
   * the rectangle (extents.width and extents.height). They do contribute
   * indirectly by changing the position of non-whitespace characters. In
   * particular, trailing whitespace characters are likely to not affect the
   * size of the rectangle, though they will affect the x_advance and y_advance
   * values.
   *
   * @param utf8  a string of text, encoded in UTF-8
   * @param extents Returns the extents of the given string
   *
   * @since 1.2
   */
  void text_extents(const std::string& utf8, TextExtents& extents) const;

  //TODO: This should really be get_glyph_extents().
  /** Gets the extents for an array of glyphs. The extents describe a user-space
   * rectangle that encloses the "inked" portion of the glyphs, (as they would
   * be drawn by Context::show_glyphs() if the cairo graphics state were set to the
   * same font_face, font_matrix, ctm, and font_options as the ScaledFont
   * object).  Additionally, the x_advance and y_advance values indicate the
   * amount by which the current point would be advanced by Context::show_glyphs().
   *
   * Note that whitespace glyphs do not contribute to the size of the rectangle
   * (extents.width and extents.height).
   *
   * @param glyphs A vector of glyphs to calculate the extents of
   * @param extents Returns the extents for the array of glyphs
   **/
  void glyph_extents(const std::vector<Glyph>& glyphs, TextExtents& extents);

  /** The FontFace with which this ScaledFont was created.
   * @since 1.2
   */
  RefPtr<FontFace> get_font_face() const;

  /** Gets the FontOptions with which the ScaledFont was created.
   * @since 1.2
   */
  void get_font_options(FontOptions& options) const;

  /** Gets the font matrix with which the ScaledFont was created.
   * @since 1.2
   */
  void get_font_matrix(Matrix& font_matrix) const;

  /** Gets the CTM with which the ScaledFont was created.
   * @since 1.2
   */
  void get_ctm(Matrix& ctm) const;

  /** Gets the type of scaled Font
   * @since 1.2
   */
  FontType get_type() const;

  protected:
  /** The underlying C cairo object that is wrapped by this ScaledFont */
  cobject* m_cobject;
};

}

#endif // __CAIROMM_SCALEDFONT_H
// vim: ts=2 sw=2 et
