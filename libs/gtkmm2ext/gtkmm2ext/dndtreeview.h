#ifndef __gtkmm2ext_dndtreeview_h__
#define __gtkmm2ext_dndtreeview_h__

#include <stdint.h>
#include <string>
#include <gtkmm/treeview.h>
#include <gtkmm/treeselection.h>
#include <gtkmm/selectiondata.h>

namespace Gtkmm2ext {

class DnDTreeView : public Gtk::TreeView 
{

  private:
  public:
	DnDTreeView ();
	~DnDTreeView() {}

	/* this is the structure pointed to if add_object_drag() is called
	   and a drop happens on a destination which has declared itself
	   willing to accept a target of the type named in the call
	   to add_object_drag().
	*/
	
	struct SerializedObjectPointers {
	    uint32_t size;
	    uint32_t cnt;
	    char     type[32];
	    void*    ptr[0];
	};
	
	void add_drop_targets (std::list<Gtk::TargetEntry>&);
	void add_object_drag (int column, std::string type_name);
	sigc::signal<void,std::string,uint32_t,void**> signal_object_drop;
	
	void on_drag_begin(const Glib::RefPtr<Gdk::DragContext>& context) {
		TreeView::on_drag_begin (context);
	}
	void on_drag_end(const Glib::RefPtr<Gdk::DragContext>& context) {
		TreeView::on_drag_end (context);
	}
	void on_drag_data_delete(const Glib::RefPtr<Gdk::DragContext>& context) {
		TreeView::on_drag_data_delete (context);
	}
	void on_drag_leave(const Glib::RefPtr<Gdk::DragContext>& context, guint time) {
	    suggested_action = context->get_suggested_action();
	    TreeView::on_drag_leave (context, time);
	}
	bool on_drag_motion(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, guint time) {
		suggested_action = context->get_suggested_action();
		return TreeView::on_drag_motion (context, x, y, time);
	}
	bool on_drag_drop(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, guint time);
	void on_drag_data_get(const Glib::RefPtr<Gdk::DragContext>& context, Gtk::SelectionData& selection_data, guint info, guint time);
	void on_drag_data_received(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, const Gtk::SelectionData& selection_data, guint info, guint time);

  private:
	std::list<Gtk::TargetEntry> draggable;
	Gdk::DragAction             suggested_action;
	int                         data_column;
	
	SerializedObjectPointers* serialize_pointers (Glib::RefPtr<Gtk::TreeModel> m, 
						      Gtk::TreeSelection::ListHandle_Path*,
						      Glib::ustring type);
};

} // namespace
 
#endif /* __gtkmm2ext_dndtreeview_h__ */
