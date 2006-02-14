#ifndef __ardour_gtk_color_manager_h__
#define __ardour_gtk_color_manager_h__

#include <gtkmm/treeview.h>
#include <gtkmm/liststore.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/colorselection.h>
#include "ardour_dialog.h"
#include "color.h"

class ColorManager : public ArdourDialog
{
  public:
	ColorManager();
	~ColorManager();

	int load (std::string path);
	int save (std::string path);

  private:
	struct ColorDisplayModelColumns : public Gtk::TreeModel::ColumnRecord {
	    ColorDisplayModelColumns() { 
		    add (name);
		    add (color);
		    add (gdkcolor);
		    add (id);
		    add (rgba);
	    }
	    
	    Gtk::TreeModelColumn<Glib::ustring>  name;
	    Gtk::TreeModelColumn<Glib::ustring>  color;
	    Gtk::TreeModelColumn<Gdk::Color>     gdkcolor;
	    Gtk::TreeModelColumn<ColorID> id;
	    Gtk::TreeModelColumn<uint32_t>       rgba;
	};

	ColorDisplayModelColumns columns;
	Gtk::TreeView color_display;
	Glib::RefPtr<Gtk::ListStore> color_list;
	Gtk::ColorSelectionDialog color_dialog;
	Gtk::ScrolledWindow scroller;

	bool button_press_event (GdkEventButton*);
};


#endif /* __ardour_gtk_color_manager_h__ */

