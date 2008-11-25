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

#ifndef __gtkmm2ext_dndtreeview_h__
#define __gtkmm2ext_dndtreeview_h__

#include <stdint.h>
#include <string>
#include <gtkmm/treeview.h>
#include <gtkmm/treeselection.h>
#include <gtkmm/selectiondata.h>

namespace Gtkmm2ext {

template<class DataType>
struct SerializedObjectPointers {
    uint32_t size;
    uint32_t cnt;
    char     type[32];
    DataType data[0];
};

class DnDTreeViewBase : public Gtk::TreeView 
{
  private:
  public:
	DnDTreeViewBase ();
	~DnDTreeViewBase() {}

	void add_drop_targets (std::list<Gtk::TargetEntry>&);
	void add_object_drag (int column, std::string type_name);
	
	void on_drag_leave(const Glib::RefPtr<Gdk::DragContext>& context, guint time) {
		suggested_action = context->get_suggested_action();
		TreeView::on_drag_leave (context, time);
	}

	bool on_drag_motion(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, guint time) {
		suggested_action = context->get_suggested_action();
		return TreeView::on_drag_motion (context, x, y, time);
	}

	bool on_drag_drop(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, guint time);

  protected:
	std::list<Gtk::TargetEntry> draggable;
	Gdk::DragAction             suggested_action;
	int                         data_column;
	std::string                 object_type;

	struct DragData {
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
};

template<class DataType>
class DnDTreeView : public DnDTreeViewBase
{
  public:
	DnDTreeView() {} 
	~DnDTreeView() {}

	sigc::signal<void,const std::list<DataType>& > signal_drop;

	void on_drag_data_get(const Glib::RefPtr<Gdk::DragContext>& context, Gtk::SelectionData& selection_data, guint info, guint time) {
		if (selection_data.get_target() == "GTK_TREE_MODEL_ROW") {

			TreeView::on_drag_data_get (context, selection_data, info, time);

		} else if (selection_data.get_target() == object_type) {

			start_object_drag ();

			/* we don't care about the data passed around by DnD, but
			   we have to provide something otherwise it will stop.
			 */

			guchar c;
			selection_data.set (8, (guchar*)&c, 1);
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


		} else if (selection_data.get_target() == object_type) {
			
			end_object_drag ();

		} else {
			/* some kind of target type added by the app, which will be handled by a signal handler */
		}
	}

  private:

	void end_object_drag () {
		Glib::RefPtr<Gtk::TreeModel> model = drag_data.source->get_model();
		DataType v;
		std::list<DataType> l;
		Gtk::TreeSelection::ListHandle_Path selection = drag_data.source->get_selection()->get_selected_rows ();
		
		for (Gtk::TreeSelection::ListHandle_Path::iterator x = selection.begin(); x != selection.end(); ++x) {
			model->get_iter (*x)->get_value (drag_data.data_column, v);
			l.push_back (v);
		}

		signal_drop (l);
	}
};

} // namespace
 
#endif /* __gtkmm2ext_dndtreeview_h__ */
