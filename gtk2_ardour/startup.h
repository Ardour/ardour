#ifndef __gtk2_ardour_startup_h__
#define __gtk2_ardour_startup_h__

#include <string>

#include <gdkmm/pixbuf.h>
#include <gtkmm/assistant.h>
#include <gtkmm/label.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/box.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/filechooserbutton.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treeview.h>
#include <gtkmm/treestore.h>
#include <gtkmm/checkbutton.h>

class ArdourStartup : public Gtk::Assistant {
  public:
	ArdourStartup ();
	~ArdourStartup ();

  private:
	bool applying;

	void on_apply ();
	void on_cancel ();
	void on_close ();
	void on_prepare (Gtk::Widget*);

	static ArdourStartup *the_startup;

	Glib::RefPtr<Gdk::Pixbuf> icon_pixbuf;

	void setup_new_user_page ();
	Glib::RefPtr<Gdk::Pixbuf> splash_pixbuf;
	Gtk::DrawingArea splash_area;
	bool splash_expose (GdkEventExpose* ev);

	void setup_first_time_config_page ();

	/* first page */
	void setup_first_page ();

	/* initial choice page */

	void setup_initial_choice_page ();
	Gtk::VBox ic_vbox;
	Gtk::RadioButton ic_new_session_button;
	Gtk::RadioButton ic_existing_session_button;

	/* monitoring choices */

	Gtk::VBox mon_vbox;
	Gtk::Label monitor_label;
	Gtk::RadioButton monitor_via_hardware_button;
	Gtk::RadioButton monitor_via_ardour_button;
	void setup_monitoring_choice_page ();

	/* session page (could be new or existing) */

	void setup_session_page ();
	Gtk::VBox session_vbox;
	Gtk::HBox session_hbox;
	
	/* recent sessions */
	
	void setup_existing_session_page ();
	
	struct RecentSessionsSorter {
	    bool operator() (std::pair<std::string,std::string> a, std::pair<std::string,std::string> b) const {
		    return cmp_nocase(a.first, b.first) == -1;
	    }
	};

	struct RecentSessionModelColumns : public Gtk::TreeModel::ColumnRecord {
	    RecentSessionModelColumns() {
		    add (visible_name);
		    add (fullpath);
	    }
	    Gtk::TreeModelColumn<Glib::ustring> visible_name;
	    Gtk::TreeModelColumn<Glib::ustring> fullpath;
	};

	RecentSessionModelColumns    recent_session_columns;
	Gtk::TreeView                recent_session_display;
	Glib::RefPtr<Gtk::TreeStore> recent_session_model;
	Gtk::ScrolledWindow          recent_scroller;
	void redisplay_recent_sessions ();
	void recent_session_row_selected ();

	/* new sessions */

	void setup_new_session_page ();
	Gtk::Entry new_name_entry;
	Gtk::FileChooserButton new_folder_chooser;
	Gtk::FileChooserButton session_template_chooser;
	Gtk::VBox session_new_vbox;
	Gtk::CheckButton more_new_session_options_button;

	void more_new_session_options_button_clicked();
	void new_name_changed ();

	/* more options for new sessions */

	Gtk::VBox more_options_vbox;
	Gtk::HBox more_options_hbox;
	void setup_more_options_page ();

	/* final page */

	void setup_final_page ();
	Gtk::Label final_page;

	/* always there */

	Glib::RefPtr<Pango::Layout> layout;



};

#endif /* __gtk2_ardour_startup_h__ */
