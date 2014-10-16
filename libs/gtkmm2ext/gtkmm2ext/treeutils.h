/*
    Copyright (C) 2010 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __libgtkmm2ext_treeutils_h__
#define __libgtkmm2ext_treeutils_h__

#include <gtkmm/treeview.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treeselection.h>
#include <gtkmm/treepath.h>
#include <gtkmm/treeiter.h>

#include "gtkmm2ext/visibility.h"

namespace Gtkmm2ext {

        LIBGTKMM2EXT_API void treeview_select_one (Glib::RefPtr<Gtk::TreeSelection> selection, Glib::RefPtr<Gtk::TreeModel> model, Gtk::TreeView& view,
						   Gtk::TreeIter iter, Gtk::TreePath path, Gtk::TreeViewColumn* col);
        LIBGTKMM2EXT_API void treeview_select_previous (Gtk::TreeView& view, Glib::RefPtr<Gtk::TreeModel> model, Gtk::TreeViewColumn* col);
        LIBGTKMM2EXT_API void treeview_select_next (Gtk::TreeView& view, Glib::RefPtr<Gtk::TreeModel> model, Gtk::TreeViewColumn* col);
}

#endif /* __libgtkmm2ext_treeutils_h__ */

