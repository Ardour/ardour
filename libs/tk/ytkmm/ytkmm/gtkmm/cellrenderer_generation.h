#ifndef _GTKMM_CELLRENDERER_GENERATION_H
#define _GTKMM_CELLRENDERER_GENERATION_H
/* $Id$ */

/* cellrenderer_generation.h
 *
 * Copyright(C) 2003 The gtkmm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or(at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <gtkmm/cellrenderertext.h>
#include <gtkmm/cellrendererpixbuf.h>
#include <gtkmm/cellrenderertoggle.h>
#include <gtkmm/cellrendereraccel.h>

namespace Gtk
{

#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace CellRenderer_Generation
{

template<class T_ModelColumnType> //e.g. bool or Glib::ustring.
CellRenderer* generate_cellrenderer(bool editable = false)
{
  CellRendererText* pCellRenderer = new CellRendererText(); //the default - template specializations will use other renderers.
  //CellRendererText can render both strings and numerical values.

  pCellRenderer->property_editable() = editable;

  return pCellRenderer;
}

#if !defined(__GNUC__) || __GNUC__ > 2
// gcc 2.95.x fails in TreeView::append_column_editable if the
// following specializations are declared in the header.
template<>
CellRenderer* generate_cellrenderer<bool>(bool editable);

template<>
CellRenderer* generate_cellrenderer< Glib::RefPtr<Gdk::Pixbuf> >(bool editable);

template<>
CellRenderer* generate_cellrenderer<AccelKey>(bool editable);

#endif

} //CellRenderer_Generation
#endif //DOXYGEN_SHOULD_SKIP_THIS

} // namespace Gtk


#endif /* _GTKMM_CELLRENDERER_GENERATION_H */
