/*
 * Copyright (C) 2010-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#include <iostream>

#include "gtkmm2ext/treeutils.h"

using namespace Glib;
using namespace Gtk;

void
Gtkmm2ext::treeview_select_one (RefPtr<TreeSelection> selection, RefPtr<TreeModel> /*model*/, TreeView& view,
                                TreeIter /*iter*/, TreePath path, TreeViewColumn* col)
{
        if (!view.row_expanded (path)) {
                // cerr << "!! selecting a row that isn't expanded! " << path.to_string() << endl;
        }

        selection->unselect_all ();
        view.set_cursor (path, *col, true);
}

void
Gtkmm2ext::treeview_select_previous (TreeView& view, RefPtr<TreeModel> model, TreeViewColumn* col)
{
	RefPtr<TreeSelection> selection = view.get_selection();
	TreeView::Selection::ListHandle_Path rows = selection->get_selected_rows ();

	if (selection->count_selected_rows() == 0 || !col || model->children().size() < 2) {
		return;
	}

	TreeView::Selection::ListHandle_Path::iterator i = rows.begin();
        TreeModel::Path start = *i;
        TreePath prev = start;
        TreeIter iter;

        iter = model->get_iter (prev);

        if (iter == model->children().begin()) {

                /* at the start, go to the end */

                TreeIter x = iter;
                while (iter != model->children().end()) {
                        x = iter;
                        iter++;
                }

                /* "x" is now an iterator for the last row */

                iter = x;
                prev = model->get_path (iter);

        } else {

                prev.prev();
        }

        if (prev == start) {
                /* can't go back, go up */

                if (!prev.empty()) {
                        prev.up ();
                }
        }

        iter = model->get_iter (prev);

        if (iter) {

                treeview_select_one (selection, model, view, iter, prev, col);

        } else {

                /* can't move to previous, so restart at selected and move up the tree */

                prev = start;
                prev.up ();

                if (!prev.empty()) {
                        iter = model->get_iter (prev);

                        if (!iter) {
                                /* can't move up the tree*/
                                return;
                        } else {
                                /* moved up from child to parent, now move to ??? */
                                prev.prev();
                        }

                        iter = model->get_iter (prev);
                }

                if (iter) {
                        treeview_select_one (selection, model, view, iter, prev, col);
                } else {

                        /* we could not forward, so wrap around to the first row */

                        /* grr: no nice way to get an iter for the
                           last row, because there is no operator--
                           for TreeIter
                        */

                        TreeIter x = model->children().begin();
                        TreeIter px = x;
                        while (x != model->children().end()) {
                                px = x;
                                x++;
                        }
                        prev = model->get_path (px);
                        treeview_select_one (selection, model, view, px, prev, col);
                }
        }
}

void
Gtkmm2ext::treeview_select_next (TreeView& view, RefPtr<TreeModel> model, TreeViewColumn* col)
{
	RefPtr<TreeSelection> selection = view.get_selection();
	TreeView::Selection::ListHandle_Path rows = selection->get_selected_rows ();

	if (selection->count_selected_rows() == 0 || !col || model->children().size() < 2) {
		return;
	}

        /* start with the last selected row, not the first */

	TreeView::Selection::ListHandle_Path::iterator i = rows.begin();
	TreeView::Selection::ListHandle_Path::iterator p = i;

        /* get the last selected row */

        while (i != rows.end()) {
                p = i;
                ++i;
        }

        TreeModel::Path start = *p;
        TreePath next = start;
        TreeIter iter;

        /* if the row we intend to start from has children but it is not expanded,
           do not try to go down.
        */

        iter = model->get_iter (start);

        TreeRow row = (*iter);
        bool down_allowed = false;

        if (!row.children().empty()) {
                TreePath tp = model->get_path (iter);

                if (!view.row_expanded (tp)) {
                        down_allowed = false;
                } else {
                        down_allowed = true;
                }
        }

        start = next;

        if (down_allowed) {
                next.down ();
                TreeIter iter = model->get_iter (next);
                if (!iter) {
                        /* can't go down, so move to next */
                        next = start;
                        next.next ();
                }
        } else {
                next.next ();
        }

        iter = model->get_iter (next);

        if (iter) {

                treeview_select_one (selection, model, view, iter, next, col);

        } else {

                /* can't move down/next, so restart at selected and move up the tree */

                next = start;
                next.up ();

                if (!next.empty()) {
                        iter = model->get_iter (next);

                        if (!iter) {
                                /* can't move up the tree*/
                                return;
                        } else {
                                /* moved up from child to parent, now move to next */
                                next.next();
                        }

                        iter = model->get_iter (next);
                }

                if (iter) {
                        treeview_select_one (selection, model, view, iter, next, col);
                } else {
                        /* we could not forward, so wrap around to the first row */
                        next = model->get_path (model->children().begin());
                        treeview_select_one (selection, model, view, model->children().begin(), next, col);
                }
        }
}
