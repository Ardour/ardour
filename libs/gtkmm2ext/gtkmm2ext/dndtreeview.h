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
};

template<class DataType>
class DnDTreeView : public DnDTreeViewBase
{
  public:
	DnDTreeView() {} 
	~DnDTreeView() {}

	sigc::signal<void,std::string,uint32_t,const DataType*> signal_object_drop;

	void on_drag_data_get(const Glib::RefPtr<Gdk::DragContext>& context, Gtk::SelectionData& selection_data, guint info, guint time) {
		std::cerr << "DRAG DATA Get, context = " << context->gobj() << " src = " << context->gobj()->is_source << std::endl;
		if (selection_data.get_target() == "GTK_TREE_MODEL_ROW") {
			
			TreeView::on_drag_data_get (context, selection_data, info, time);
			
		} else if (data_column >= 0) {
			
			Gtk::TreeSelection::ListHandle_Path selection = get_selection()->get_selected_rows ();
			SerializedObjectPointers<DataType>* sr = serialize_pointers (get_model(), &selection, selection_data.get_target());
			selection_data.set (8, (guchar*)sr, sr->size);
		}
	}
	
	void on_drag_data_received(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, const Gtk::SelectionData& selection_data, guint info, guint time) {
		std::cerr << "DRAG DATA Receive, context = " << context->gobj() << " src = " << context->gobj()->is_source << std::endl;
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

		} else if (data_column >= 0) {
			
			/* object D-n-D */
			
			const void* data = selection_data.get_data();
			const SerializedObjectPointers<DataType>* sr = reinterpret_cast<const SerializedObjectPointers<DataType> *>(data);
			
			if (sr) {
				signal_object_drop (sr->type, sr->cnt, sr->data);
			}
			
		} else {
			/* some kind of target type added by the app, which will be handled by a signal handler */
		}
	}

  private:

	SerializedObjectPointers<DataType>* serialize_pointers (Glib::RefPtr<Gtk::TreeModel> model, 
								Gtk::TreeSelection::ListHandle_Path* selection,
								Glib::ustring type) {

		/* this nasty chunk of code is here because X's DnD protocol (probably other graphics UI's too) 
		   requires that we package up the entire data collection for DnD in a single contiguous region
		   (so that it can be trivially copied between address spaces). We don't know the type of DataType so
		   we have to mix-and-match C and C++ programming techniques here to get the right result.

		   The C trick is to use the "someType foo[0];" declaration trick to create a zero-sized array at the
		   end of a SerializedObjectPointers<DataType object. Then we allocate a raw memory buffer that extends
		   past that array and thus provides space for however many DataType items we actually want to pass
		   around.

		   The C++ trick is to use the placement operator new() syntax to initialize that extra
		   memory properly.
		*/
		
		uint32_t cnt = selection->size();
		uint32_t sz = (sizeof (DataType) * cnt) + sizeof (SerializedObjectPointers<DataType>);

		char* buf = new char[sz];
		SerializedObjectPointers<DataType>* sr = (SerializedObjectPointers<DataType>*) buf;

		for (uint32_t i = 0; i < cnt; ++i) {
			new ((void *) &sr->data[i]) DataType ();
		}
		
		sr->cnt = cnt;
		sr->size = sz;
		snprintf (sr->type, sizeof (sr->type), "%s", type.c_str());
		
		cnt = 0;
		
		for (Gtk::TreeSelection::ListHandle_Path::iterator x = selection->begin(); x != selection->end(); ++x, ++cnt) {
			model->get_iter (*x)->get_value (data_column, sr->data[cnt]);
		}

		return sr;
	}
};

} // namespace
 
#endif /* __gtkmm2ext_dndtreeview_h__ */
