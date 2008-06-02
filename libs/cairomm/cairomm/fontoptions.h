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

#ifndef __CAIROMM_FONTOPTIONS_H
#define __CAIROMM_FONTOPTIONS_H

#include <cairomm/enums.h>
#include <string>
#include <cairo.h>


namespace Cairo
{

/** How a font should be rendered.
 */
class FontOptions
{
public:
  FontOptions();
  explicit FontOptions(cairo_font_options_t* cobject, bool take_ownership = false);
  FontOptions(const FontOptions& src);

  virtual ~FontOptions();

  FontOptions& operator=(const FontOptions& src);

  bool operator ==(const FontOptions& src) const;
  //bool operator !=(const FontOptions& src) const;

  void merge(const FontOptions& other);

  unsigned long hash() const;

  void set_antialias(Antialias antialias);
  Antialias get_antialias() const;

  void set_subpixel_order(SubpixelOrder subpixel_order);
  SubpixelOrder get_subpixel_order() const;

  void set_hint_style(HintStyle hint_style);
  HintStyle get_hint_style() const;

  void set_hint_metrics(HintMetrics hint_metrics);
  HintMetrics get_hint_metrics() const;

  typedef cairo_font_options_t cobject;
  inline cobject* cobj() { return m_cobject; }
  inline const cobject* cobj() const { return m_cobject; }

  #ifndef DOXYGEN_IGNORE_THIS
  ///For use only by the cairomm implementation.
  inline ErrorStatus get_status() const
  { return cairo_font_options_status(const_cast<cairo_font_options_t*>(cobj())); }
  #endif //DOXYGEN_IGNORE_THIS

protected:

  cobject* m_cobject;
};

} // namespace Cairo

#endif //__CAIROMM_FONTOPTIONS_H

// vim: ts=2 sw=2 et
