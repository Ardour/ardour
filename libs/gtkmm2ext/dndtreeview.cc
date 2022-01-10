/*
 * Copyright (C) 2005-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2016 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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

DnDTreeViewBase::DragData DnDTreeViewBase::drag_data;

DnDTreeViewBase::DnDTreeViewBase ()
	: TreeView ()
	, _drag_column (-1)
{
	draggable.push_back (TargetEntry ("GTK_TREE_MODEL_ROW", TARGET_SAME_WIDGET));
	data_column = -1;

	enable_model_drag_source (draggable);
	enable_model_drag_dest (draggable);

 	suggested_action = Gdk::DragAction (0);
}

void
DnDTreeViewBase::on_drag_begin (Glib::RefPtr<Gdk::DragContext> const & context) {
	if (_drag_column >= 0) {
		/* this code is a customized drop-in replacement for
		 * Gtk::TreeView::on_drag_begin().
		 * We can use it's cleanup function for the generated Pixmap
		 */

		TreeModel::Path path;
		TreeViewColumn* column;
		int cell_x;
		int cell_y;

		if (!get_path_at_pos ((int)press_start_x, (int)press_start_y, path, column, cell_x, cell_y)) {
			return;
		}

		TreeIter iter = get_model()->get_iter (path);
		int x_offset, y_offset, width, height;

		Gdk::Rectangle unused;
		TreeViewColumn* clm = get_column(_drag_column);

		clm->cell_set_cell_data (get_model(), iter, false, false);
		clm->cell_get_size (unused, x_offset, y_offset, width, height);

		Glib::RefPtr<Gdk::Pixmap> pixmap = Gdk::Pixmap::create (get_root_window(), width, height);

		CellRenderer* cell_renderer = clm->get_first_cell ();
		Gdk::Rectangle cell_background (0, 0, width, height);
		Gdk::Rectangle cell_size (x_offset, y_offset, width, height);

		// the cell-renderer only clears the background if
		// cell->cell_background_set and priv->cell_background
		Gdk::Color clr = get_style()->get_bg(STATE_NORMAL);
		// code dup from gtk_cell_renderer_render() to clear the background:
		cairo_t *cr = gdk_cairo_create (Glib::unwrap(pixmap));
		gdk_cairo_rectangle (cr, (cell_background).gobj());
		gdk_cairo_set_source_color (cr, clr.gobj());
		cairo_fill (cr);
		cairo_destroy (cr);

		// gtkmm wants a "window", gtk itself is happy with a "drawable",
		// cell_renderer->render (pixmap, *this, cell_area, cell_area, cell_area, 0);
		// We ain't got no window, so use gtk directly:
		gtk_cell_renderer_render (cell_renderer->gobj(),
				Glib::unwrap(pixmap), ((Gtk::Widget*)this)->gobj(),
				(cell_background).gobj(),
				(cell_size).gobj(),
				(cell_size).gobj(),
				((GtkCellRendererState)(0)));

		context->set_icon (pixmap->get_colormap(),
				pixmap, Glib::RefPtr<Gdk::Bitmap>(NULL),
				width / 2 + 1, cell_y + 1);

	} else {
		Gtk::TreeView::on_drag_begin (context);
	}
	start_object_drag ();
}

void
DnDTreeViewBase::on_drag_end (Glib::RefPtr<Gdk::DragContext> const & context) {
	Gtk::TreeView::on_drag_end (context);
	end_object_drag ();
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
DnDTreeViewBase::add_object_drag (int column, string type_name, TargetFlags flags)
{
	draggable.push_back (TargetEntry (type_name, flags));
	data_column = column;
	object_type = type_name;

	enable_model_drag_source (draggable);
	enable_model_drag_dest (draggable);
}

bool
DnDTreeViewBase::on_drag_drop(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, guint time)
{
	suggested_action = Gdk::DragAction (0);
	drag_data.source = 0;
	return TreeView::on_drag_drop (context, x, y, time);
}

bool
DnDTreeViewBase::on_drag_motion(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, guint time)
{
	bool rv = TreeView::on_drag_motion (context, x, y, time);
	if (rv) {
		rv = signal_motion (context, x, y, time);
	}
	suggested_action = context->get_suggested_action();
	return rv;
}
