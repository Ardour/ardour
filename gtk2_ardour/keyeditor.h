#ifndef __ardour_gtk_key_editor_h__
#define __ardour_gtk_key_editor_h__

#include <string>

#include <gtkmm/buttonbox.h>
#include <gtkmm/treeview.h>
#include <gtkmm/treestore.h>
#include <gtkmm/scrolledwindow.h>

#include "ardour_window.h"

class KeyEditor : public ArdourWindow
{
  public:
	KeyEditor ();

  protected:
	void on_show ();
	void on_unmap ();
	bool on_key_press_event (GdkEventKey*);
	bool on_key_release_event (GdkEventKey*);

  private:
	struct KeyEditorColumns : public Gtk::TreeModel::ColumnRecord {
	    KeyEditorColumns () {
		    add (action);
		    add (binding);
		    add (path);
		    add (bindable);
	    }
	    Gtk::TreeModelColumn<std::string> action;
	    Gtk::TreeModelColumn<std::string> binding;
	    Gtk::TreeModelColumn<std::string> path;
	    Gtk::TreeModelColumn<bool> bindable;
	};

        Gtk::VBox vpacker;
	Gtk::ScrolledWindow scroller;
	Gtk::TreeView view;
	Glib::RefPtr<Gtk::TreeStore> model;
	KeyEditorColumns columns;
	Gtk::Button unbind_button;
	Gtk::HButtonBox unbind_box;

	void unbind ();

	bool can_bind;
	guint last_state;

	void action_selected ();
	void populate ();
};

#endif /* __ardour_gtk_key_editor_h__ */
