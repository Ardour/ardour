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
#include <gtkmm/table.h>
#include <gtkmm/frame.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/liststore.h>
#include <gtkmm/combobox.h>

class EngineControl;

class ArdourStartup : public Gtk::Assistant {
  public:
	ArdourStartup ();
	~ArdourStartup ();

	void set_new_only (bool);

	Glib::ustring session_name (bool& should_be_new);
	Glib::ustring session_folder ();

	bool use_session_template();
	Glib::ustring session_template_name();

	EngineControl* engine_control() { return engine_dialog; }

	// advanced session options

	bool create_master_bus() const;
	int master_channel_count() const;

	bool create_control_bus() const;
	int control_channel_count() const;

	bool connect_inputs() const;
	bool limit_inputs_used_for_connection() const;
	int input_limit_count() const;

	bool connect_outputs() const;
	bool limit_outputs_used_for_connection() const;
	int output_limit_count() const;

	bool connect_outs_to_master() const;
	bool connect_outs_to_physical() const ;

  private:
	bool applying;
	bool config_modified;
	bool new_user;
	bool new_only;

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
	Gtk::FileChooserButton* default_dir_chooser;
	void default_dir_changed();
	void setup_first_page ();

	/* initial choice page */

	void setup_initial_choice_page ();
	Gtk::VBox ic_vbox;
	Gtk::RadioButton ic_new_session_button;
	Gtk::RadioButton ic_existing_session_button;
	bool initial_choice_activated(GdkEventButton *);

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
	void recent_row_activated (const Gtk::TreePath& path, Gtk::TreeViewColumn* col);

	/* audio setup page */

	void setup_audio_page ();
	EngineControl* engine_dialog;

	/* new sessions */

	void setup_new_session_page ();
	Gtk::Entry new_name_entry;
	Gtk::FileChooserButton new_folder_chooser;
	Gtk::FileChooserButton session_template_chooser;

	struct SessionTemplateColumns : public Gtk::TreeModel::ColumnRecord {
		SessionTemplateColumns () {
			add (name);
			add (path);
		}

		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<std::string> path;
	};

	SessionTemplateColumns session_template_columns;
	Glib::RefPtr<Gtk::ListStore>  template_model;
	Gtk::ComboBox template_chooser;

	Gtk::VBox session_new_vbox;
	Gtk::CheckButton more_new_session_options_button;
	Gtk::RadioButtonGroup session_template_group;
	Gtk::RadioButton use_session_as_template_button;
	Gtk::RadioButton use_template_button;

	void more_new_session_options_button_clicked();
	void new_name_changed ();
	void populate_session_templates ();

	/* more options for new sessions */

	Gtk::VBox more_options_vbox;

	Gtk::Label chan_count_label_1;
	Gtk::Label chan_count_label_2;
	Gtk::Label chan_count_label_3;
	Gtk::Label chan_count_label_4;
	Gtk::Table advanced_table;
	Gtk::HBox input_port_limit_hbox;
	Gtk::VBox input_port_vbox;
	Gtk::Table input_table;
	Gtk::HBox input_hbox;

	Gtk::Label bus_label;
	Gtk::Frame bus_frame;
	Gtk::Table bus_table;
	Gtk::HBox bus_hbox;

	Gtk::Label input_label;
	Gtk::Frame input_frame;
	Gtk::HBox output_port_limit_hbox;
	Gtk::VBox output_port_vbox;
	Gtk::VBox output_conn_vbox;
	Gtk::VBox output_vbox;
	Gtk::HBox output_hbox;

	Gtk::Label output_label;
	Gtk::Frame output_frame;
	Gtk::VBox advanced_vbox;
	Gtk::Label advanced_label;

	Gtk::CheckButton _create_master_bus;
	Gtk::SpinButton _master_bus_channel_count;

	Gtk::CheckButton _create_control_bus;
	Gtk::SpinButton _control_bus_channel_count;

	Gtk::CheckButton _connect_inputs;
	Gtk::CheckButton _limit_input_ports;
	Gtk::SpinButton _input_limit_count;

	Gtk::CheckButton _connect_outputs;
	Gtk::CheckButton _limit_output_ports;
	Gtk::SpinButton _output_limit_count;

	Gtk::RadioButtonGroup connect_outputs_group;
	Gtk::RadioButton _connect_outputs_to_master;
	Gtk::RadioButton _connect_outputs_to_physical;

	Gtk::Adjustment _output_limit_count_adj;
	Gtk::Adjustment _input_limit_count_adj;
	Gtk::Adjustment _control_bus_channel_count_adj;
	Gtk::Adjustment _master_bus_channel_count_adj;

	void connect_inputs_clicked ();
	void connect_outputs_clicked ();
	void limit_inputs_clicked ();
	void limit_outputs_clicked ();
	void master_bus_button_clicked ();
	void monitor_bus_button_clicked ();
	void setup_more_options_page ();

	/* final page */

	void setup_final_page ();
	Gtk::Label final_page;

	/* always there */

	Glib::RefPtr<Pango::Layout> layout;

	/* page indices */

	gint audio_page_index;
	gint new_user_page_index;
	gint default_folder_page_index;
	gint monitoring_page_index;
	gint session_page_index;
	gint initial_choice_index;
	gint final_page_index;
	gint session_options_page_index;

	void move_along_now ();

};

#endif /* __gtk2_ardour_startup_h__ */
