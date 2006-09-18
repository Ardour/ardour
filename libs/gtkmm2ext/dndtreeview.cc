#include <cstdio>
#include <iostream>

#include <gtkmm2ext/dndtreeview.h>

using namespace std;
using namespace sigc;
using namespace Gdk;
using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;

DnDTreeView::DnDTreeView ()
	: TreeView ()
{
	draggable.push_back (TargetEntry ("GTK_TREE_MODEL_ROW", TARGET_SAME_WIDGET));
	data_column = -1;

	enable_model_drag_source (draggable);
	enable_model_drag_dest (draggable);

 	suggested_action = Gdk::DragAction (0);
}

void
DnDTreeView::add_drop_targets (list<TargetEntry>& targets)
{
	for (list<TargetEntry>::iterator i = targets.begin(); i != targets.end(); ++i) {
		draggable.push_back (*i);
	}
	enable_model_drag_source (draggable);
	enable_model_drag_dest (draggable);
}	

void
DnDTreeView::add_object_drag (int column, string type_name)
{
	draggable.push_back (TargetEntry (type_name, TargetFlags(0)));
	data_column = column;

	enable_model_drag_source (draggable);
	enable_model_drag_dest (draggable);
}

DnDTreeView::SerializedObjectPointers* 
DnDTreeView::serialize_pointers (RefPtr<TreeModel> model, TreeSelection::ListHandle_Path* selection, ustring type)
{
	uint32_t cnt = selection->size();
	uint32_t sz = (sizeof (void*) * cnt) + sizeof (SerializedObjectPointers);

	cerr << "lets plan to serialize " << cnt << " from selection\n";

	char* buf = new char[sz];
	SerializedObjectPointers* sr = new (buf) SerializedObjectPointers;
	
	sr->cnt = cnt;
	sr->size = sz;

	snprintf (sr->type, sizeof (sr->type), "%s", type.c_str());

	cnt = 0;

	for (TreeSelection::ListHandle_Path::iterator x = selection->begin(); x != selection->end(); ++x, ++cnt) {
		cerr << "getting next item\n";
		TreeModel::Row row = *(model->get_iter (*x));
		row.get_value (data_column, sr->ptr[cnt]);
	}

	cerr << "returning an SR with size = " << sr->size << endl;
	return sr;
}

void
DnDTreeView::on_drag_data_get(const RefPtr<DragContext>& context, SelectionData& selection_data, guint info, guint time)
{
	if (selection_data.get_target() == "GTK_TREE_MODEL_ROW") {

		TreeView::on_drag_data_get (context, selection_data, info, time);
		
	} else if (data_column >= 0) {
		
		Gtk::TreeSelection::ListHandle_Path selection = get_selection()->get_selected_rows ();
		SerializedObjectPointers* sr = serialize_pointers (get_model(), &selection, selection_data.get_target());
		selection_data.set (8, (guchar*)sr, sr->size);
		
		cerr << "selection data set to contain " << sr->size << endl;
	}
}

void 
DnDTreeView::on_drag_data_received(const RefPtr<DragContext>& context, int x, int y, const SelectionData& selection_data, guint info, guint time)
{
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
		
		const SerializedObjectPointers* sr = reinterpret_cast<const SerializedObjectPointers *>(selection_data.get_data());
		
		if (sr) {
			signal_object_drop (sr->type, sr->cnt, const_cast<void**>(sr->ptr));
		}

	} else {
		/* some kind of target type added by the app, which will be handled by a signal handler */
	}
}

bool 
DnDTreeView::on_drag_drop(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, guint time)
{
	suggested_action = Gdk::DragAction (0);
	return TreeView::on_drag_drop (context, x, y, time);
}

