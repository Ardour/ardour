/*
    Copyright (C) 2000-2007 Paul Davis 

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

#include <cstdio>
#include <iostream>

#include <gtkmm2ext/dndtreeview.h>

using namespace std;
using namespace sigc;
using namespace Gdk;
using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;

DnDTreeViewBase::DnDTreeViewBase ()
	: TreeView ()
{
	draggable.push_back (TargetEntry ("GTK_TREE_MODEL_ROW", TARGET_SAME_WIDGET));
	data_column = -1;

	enable_model_drag_source (draggable);
	enable_model_drag_dest (draggable);

 	suggested_action = Gdk::DragAction (0);
}

void
DnDTreeViewBase::add_drop_targets (list<TargetEntry>& targets)
{
	for (list<TargetEntry>::iterator i = targets.begin(); i != targets.end(); ++i) {
		draggable.push_back (*i);
	}
	
	enable_model_drag_source (draggable);
	enable_model_drag_dest (draggable);
}	

void
DnDTreeViewBase::add_object_drag (int column, string type_name)
{
	draggable.push_back (TargetEntry (type_name, TargetFlags(0)));
	data_column = column;

	enable_model_drag_source (draggable);
	enable_model_drag_dest (draggable);
}

bool 
DnDTreeViewBase::on_drag_drop(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, guint time)
{
	suggested_action = Gdk::DragAction (0);
	return TreeView::on_drag_drop (context, x, y, time);
}


