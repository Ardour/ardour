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

#ifndef __CAIROMM_PATH_H
#define __CAIROMM_PATH_H

#include <cairomm/enums.h>
#include <string>
#include <cairo.h>


namespace Cairo
{

/** A data structure for holding a path.
 * Use Context::copy_path() or Context::copy_path_flat() to instantiate a new
 * Path.  The application is responsible for freeing the Path object when it is
 * no longer needed.
 *
 * @todo There's currently no way to access the path data without reverting to
 * the C object (see cobj())
 */
class Path
{
public:
  //Path();
  explicit Path(cairo_path_t* cobject, bool take_ownership = false);
  //Path(const Path& src);

  virtual ~Path();

  //Path& operator=(const Path& src);

  //bool operator ==(const Path& src) const;
  //bool operator !=(const Path& src) const;

  typedef cairo_path_t cobject;
  inline cobject* cobj() { return m_cobject; }
  inline const cobject* cobj() const { return m_cobject; }

  #ifndef DOXYGEN_IGNORE_THIS
  ///For use only by the cairomm implementation.
  //There is no *_status() function for this object:
  //inline ErrorStatus get_status() const
  //{ return cairo_path_status(const_cast<cairo_path_t*>(cobj())); }
  #endif //DOXYGEN_IGNORE_THIS

protected:

  cobject* m_cobject;
};

} // namespace Cairo

#endif //__CAIROMM_PATH_H

// vim: ts=2 sw=2 et
