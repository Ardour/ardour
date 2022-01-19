/*
 * Copyright (C) 2005-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
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

#ifndef __gtkmm2ext_dndtreeview_h__
#define __gtkmm2ext_dndtreeview_h__

#include <stdint.h>
#include <string>
#include <gtkmm/treeview.h>
#include <gtkmm/treeselection.h>
#include <gtkmm/selectiondata.h>

#include "gtkmm2ext/visibility.h"

namespace Gtkmm2ext {

template<class DataType>
struct /*LIBGTKMM2EXT_API*/ SerializedObjectPointers {
    uint32_t size;
    uint32_t cnt;
    char     type[32];
    DataType data[0];
};

class LIBGTKMM2EXT_API DnDTreeViewBase : public Gtk::TreeView
{
  private:
  public:
	DnDTreeViewBase ();
	~DnDTreeViewBase() {}

	struct BoolAccumulator {
		typedef bool result_type;
		template <class U>
			result_type operator () (U first, U last) {
				while (first != last) {
					if (!*first) {
						/* break on first slot that returns false */
						return false;
					}
					++first;
				}
				/* no connected slots -> return true */
				return true;
			}
	};


	sigc::signal4<bool, const Glib::RefPtr<Gdk::DragContext>&, int, int, guint, BoolAccumulator> signal_motion;

	void add_drop_targets (std::list<Gtk::TargetEntry>&);
	void add_object_drag (int column, std::string type_name, Gtk::TargetFlags flags = Gtk::TargetFlags (0));

	void on_drag_begin (Glib::RefPtr<Gdk::DragContext> const & context);
	void on_drag_end (Glib::RefPtr<Gdk::DragContext> const & context);

	bool on_button_press_event (GdkEventButton *ev) {
		press_start_x = ev->x;
		press_start_y = ev->y;
		return TreeView::on_button_press_event (ev);
	}

	void on_drag_leave(const Glib::RefPtr<Gdk::DragContext>& context, guint time) {
		TreeView::on_drag_leave (context, time);
		suggested_action = context->get_suggested_action();
	}

	bool on_drag_motion(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, guint time);
	bool on_drag_drop(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, guint time);

	void set_drag_column (int c) {
		_drag_column = c;
	}

  protected:
	std::list<Gtk::TargetEntry> draggable;
	Gdk::DragAction             suggested_action;
	int                         data_column;
	std::string                 object_type;

	double press_start_x;
	double press_start_y;
	int _drag_column;

	struct DragData {
	    DragData () : source (0) {}

	    Gtk::TreeView* source;
	    int            data_column;
	    std::string    object_type;
	};

	static DragData drag_data;

	void start_object_drag () {
		drag_data.source = this;
		drag_data.data_column = data_column;
		drag_data.object_type = object_type;
	}

	void end_object_drag () {
		drag_data.source = 0;
		drag_data.data_column = -1;
		drag_data.object_type = "";
	}
};

template<class DataType>
class /*LIBGTKMM2EXT_API*/ DnDTreeView : public DnDTreeViewBase
{
  public:
	DnDTreeView() {}
	~DnDTreeView() {}

	sigc::signal<void, const Glib::RefPtr<Gdk::DragContext>&, const Gtk::SelectionData&> signal_drop;

	void on_drag_data_get(const Glib::RefPtr<Gdk::DragContext>& context, Gtk::SelectionData& selection_data, guint info, guint time) {
		if (selection_data.get_target() == "GTK_TREE_MODEL_ROW") {

			TreeView::on_drag_data_get (context, selection_data, info, time);

		} else if (selection_data.get_target() == object_type && drag_data.data_column >= 0) {

			/* return a pointer to this object, which allows
			 * the receiver to call on_drag_data_received()
			 */
			void *c = this;
			selection_data.set (8, (guchar*)&c, sizeof(void*));
		} else {
			TreeView::on_drag_data_get (context, selection_data, info, time);
		}
	}

	void on_drag_data_received(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, const Gtk::SelectionData& selection_data, guint info, guint time) {
		if (suggested_action) {
			/* this is a drag motion callback. just update the status to
			   say that we are still dragging, and that's it.
			*/
			suggested_action = Gdk::DragAction (0);
			TreeView::on_drag_data_received (context, x, y, selection_data, info, time);
			return;
		}

		if (selection_data.get_target() == "GTK_TREE_MODEL_ROW") {
			TreeView::on_drag_data_received (context, x, y, selection_data, info, time);
		} else {
			/* some kind of target type, usually 'object_type' added by the app,
			 * which will be handled by a signal handler */
			for (std::list<Gtk::TargetEntry>::const_iterator i = draggable.begin(); i != draggable.end (); ++i) {
				if (selection_data.get_target() == (*i).get_target()) {
					signal_drop (context, selection_data);
					context->drag_finish (true, false, time);
					break;
				}
			}
		}
	}

	/**
	 * This can be called by the Treeview itself or by some other
	 * object that wants to get the list of dragged items.
	 */

	void get_object_drag_data (std::list<DataType>& l, Gtk::TreeView** source) const {

		if (drag_data.source == 0 || drag_data.data_column < 0) {
			return;
		}

		Glib::RefPtr<Gtk::TreeModel> model = drag_data.source->get_model();
		DataType v;
		Gtk::TreeSelection::ListHandle_Path selection = drag_data.source->get_selection()->get_selected_rows ();

		for (Gtk::TreeSelection::ListHandle_Path::iterator x = selection.begin(); x != selection.end(); ++x) {
			model->get_iter (*x)->get_value (drag_data.data_column, v);
			l.push_back (v);
		}

		*source = drag_data.source;
	}
};

} // namespace

#endif /* __gtkmm2ext_dndtreeview_h__ */
