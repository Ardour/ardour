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

#include <cairomm/scaledfont.h>
#include <cairomm/private.h>  // for check_status_and_throw_exception

namespace Cairo
{

ScaledFont::ScaledFont(cobject* cobj, bool has_reference)
{
  if(has_reference)
    m_cobject = cobj;
  else
    m_cobject = cairo_scaled_font_reference(cobj);
}

RefPtr<ScaledFont> ScaledFont::create(FontFace& font_face, const Matrix& font_matrix,
    const Matrix& ctm, const FontOptions& options)
{
  cairo_scaled_font_t* cobj = cairo_scaled_font_create(font_face.cobj(), &font_matrix, &ctm, options.cobj());
  check_status_and_throw_exception(cairo_scaled_font_status(cobj));
  return RefPtr<ScaledFont>(new ScaledFont(cobj, false));
}

void ScaledFont::extents(FontExtents& extents) const
{
  cairo_scaled_font_extents(m_cobject, static_cast<cairo_font_extents_t*>(&extents));
  check_object_status_and_throw_exception(*this);
}

void ScaledFont::text_extents(const std::string& utf8, TextExtents& extents) const
{
  cairo_scaled_font_text_extents(m_cobject, utf8.c_str(), static_cast<cairo_text_extents_t*>(&extents));
  check_object_status_and_throw_exception(*this);
}

void ScaledFont::glyph_extents(const std::vector<Glyph>& glyphs, TextExtents& extents)
{
  // copy the data from the vector to a standard C array.  I don't believe
  // this will be a frequently used function so I think the performance hit is
  // more than offset by the increased flexibility of the STL interface.
  
  // Use new to allocate memory as MSCV complains about non-const array size with
  // Glyph glyph_array[glyphs.size()]
  Glyph* glyph_array= new Glyph[glyphs.size()];
  std::copy(glyphs.begin(), glyphs.end(), glyph_array);

  cairo_scaled_font_glyph_extents(m_cobject, glyph_array, glyphs.size(),
      static_cast<cairo_text_extents_t*>(&extents));
  check_object_status_and_throw_exception(*this);
  delete[] glyph_array;
}

RefPtr<FontFace> ScaledFont::get_font_face() const
{
  cairo_font_face_t* face = cairo_scaled_font_get_font_face(m_cobject);
  check_object_status_and_throw_exception(*this);
  return RefPtr<FontFace>(new FontFace(face, true));
}

void ScaledFont::get_font_options(FontOptions& options) const
{
  cairo_scaled_font_get_font_options(m_cobject, options.cobj());
  check_object_status_and_throw_exception(*this);
}

void ScaledFont::get_font_matrix(Matrix& font_matrix) const
{
  cairo_scaled_font_get_font_matrix(m_cobject,
      static_cast<cairo_matrix_t*>(&font_matrix));
  check_object_status_and_throw_exception(*this);
}

void ScaledFont::get_ctm(Matrix& ctm) const
{
  cairo_scaled_font_get_ctm(m_cobject, static_cast<cairo_matrix_t*>(&ctm));
  check_object_status_and_throw_exception(*this);
}

FontType ScaledFont::get_type() const
{
  cairo_font_type_t font_type = cairo_scaled_font_get_type(m_cobject);
  check_object_status_and_throw_exception(*this);
  return static_cast<FontType>(font_type);
}

}   // namespace Cairo
// vim: ts=2 sw=2 et
