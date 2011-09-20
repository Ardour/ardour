#ifndef __lxvst_plugin_ui_h__
#define __lxvst_plugin_ui_h__

#include <vector>
#include <map>
#include <list>

#include <sigc++/signal.h>
#include <gtkmm/widget.h>

#include <ardour_dialog.h>
#include <ardour/types.h>
#include "plugin_ui.h"

#ifdef LXVST_SUPPORT

namespace ARDOUR {
	class PluginInsert;
	class LXVSTPlugin;
}

class LXVSTPluginUI : public PlugUIBase, public Gtk::VBox
{
  public:
	LXVSTPluginUI (boost::shared_ptr<ARDOUR::PluginInsert>, boost::shared_ptr<ARDOUR::LXVSTPlugin>);
	~LXVSTPluginUI ();

	gint get_preferred_height ();
	gint get_preferred_width ();
	bool start_updating(GdkEventAny*);
	bool stop_updating(GdkEventAny*);

	int package (Gtk::Window&);
	void forward_key_event (GdkEventKey *);
	bool non_gtk_gui() const { return true; }

  private:
	boost::shared_ptr<ARDOUR::LXVSTPlugin>  lxvst;
	Gtk::Socket socket;
	Gtk::HBox   preset_box;
	Gtk::VBox   vpacker;
	
	sigc::connection _screen_update_connection;
	
	bool configure_handler (GdkEventConfigure*, Gtk::Socket*);
	void save_plugin_setting ();

	struct PresetModelColumns : public Gtk::TreeModel::ColumnRecord {
	    PresetModelColumns() { 
		    add (name);
		    add (number);
	    }
	    Gtk::TreeModelColumn<Glib::ustring> name;
	    Gtk::TreeModelColumn<int> number;
	};

	PresetModelColumns preset_columns;
	Glib::RefPtr<Gtk::ListStore> preset_model;
	Gtk::ComboBox lxvst_preset_combo;

	void create_preset_store ();
	void preset_chosen ();
	void preset_selected ();
	void resize_callback();
};

#endif //LXVST_SUPPORT

#endif
