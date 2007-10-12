#ifndef __ardour_gtk_key_editor_h__
#define __ardour_gtk_key_editor_h__

#include <string>

#include <gtkmm/treeview.h>
#include <gtkmm/treestore.h>
#include <gtkmm/scrolledwindow.h>
#include <glibmm/ustring.h>

#include "ardour_dialog.h"

class KeyEditor : public ArdourDialog
{
  public:
	KeyEditor ();
	
  protected:
	void on_show ();
	void on_unmap ();
	bool on_key_release_event (GdkEventKey*);

  private:
	struct KeyEditorColumns : public Gtk::TreeModel::ColumnRecord {
	    KeyEditorColumns () {
		    add (action);
		    add (binding);
		    add (path);
	    }
	    Gtk::TreeModelColumn<Glib::ustring> action;
	    Gtk::TreeModelColumn<std::string> binding;
	    Gtk::TreeModelColumn<std::string> path;
	};

	Gtk::ScrolledWindow scroller;
	Gtk::TreeView view;
	Glib::RefPtr<Gtk::TreeStore> model;
	KeyEditorColumns columns;

	void action_selected ();
	void populate ();
};

#endif /* __ardour_gtk_key_editor_h__ */
