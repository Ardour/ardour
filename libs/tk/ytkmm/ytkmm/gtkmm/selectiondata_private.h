// -*- c++ -*-

/* sectiondata_private.h
 *
 * Copyright(C) 2002 The gtkmm Development Team
 *
 * This library is free software, ) you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, ) either
 * version 2.1 of the License, or(at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY, ) without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library, ) if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _GTKMM_SELECTIONDATA_PRIVATE_H
#define _GTKMM_SELECTIONDATA_PRIVATE_H

#include <gtkmm/selectiondata.h>


namespace Gtk
{

#ifndef DOXYGEN_SHOULD_SKIP_THIS

/** This class allows GtkSelectionData to be manipulated via a C++ API, but doesn't take a copy
 * or try to free the underlying instance in its destructor.
 * So far it's only used by gtkmm internally.
 */
class SelectionData_WithoutOwnership : public SelectionData
{
public:
  explicit SelectionData_WithoutOwnership(GtkSelectionData* gobject);
  ~SelectionData_WithoutOwnership();
};

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

} // namespace Gtk

#endif /* _GTKMM_SELECTIONDATA_PRIVATE_H */

