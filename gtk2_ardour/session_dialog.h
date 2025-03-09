/*
 * Copyright (C) 2013-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2017 Ben Loftis <ben@harrisonconsoles.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __gtk2_ardour_session_dialog_h__
#define __gtk2_ardour_session_dialog_h__

#include <string>

#include <ydkmm/pixbuf.h>
#include <ytkmm/label.h>
#include <ytkmm/drawingarea.h>
#include <ytkmm/expander.h>
#include <ytkmm/box.h>
#include <ytkmm/radiobutton.h>
#include <ytkmm/filechooserbutton.h>
#include <ytkmm/filechooserwidget.h>
#include <ytkmm/scrolledwindow.h>
#include <ytkmm/textview.h>
#include <ytkmm/treeview.h>
#include <ytkmm/treestore.h>
#include <ytkmm/checkbutton.h>
#include <ytkmm/table.h>
#include <ytkmm/frame.h>
#include <ytkmm/spinbutton.h>
#include <ytkmm/liststore.h>
#include <ytkmm/combobox.h>
#include <ytkmm/comboboxtext.h>

#include "temporal/domain_provider.h"

#include "ardour/utils.h"

#include "ardour_dialog.h"
#include "option_editor.h"

class EngineControl;

class SessionDialog : public ArdourDialog
{
public:
	enum DialogTab
	{
		New = 0,
		Recent,
		Open,
		Prefs
	};

	SessionDialog (DialogTab initial_tab, const std::string& session_name, const std::string& session_path,
	               const std::string& template_name, bool cancel_not_quit);
	~SessionDialog ();

	std::string session_name (bool& should_be_new);
	std::string session_folder ();

	Temporal::TimeDomain session_domain () const;

	bool use_session_template() const;
	std::string session_template_name();

	uint32_t master_channel_count();
	void on_show ();

	void set_provided_session (std::string const & name, std::string const & path);
	void clear_name ();
	bool was_new_name_edited() const { return new_name_was_edited; }

	void delete_selected_template();
	void show_template_context_menu (int button, int time);
	bool template_button_press (GdkEventButton*);

private:
	bool on_delete_event (GdkEventAny*);

	Gtk::Button* cancel_button;
	Gtk::Button* open_button;
	Gtk::Button* quit_button;

	ArdourWidgets::ArdourButton new_button;
	ArdourWidgets::ArdourButton recent_button;
	ArdourWidgets::ArdourButton existing_button;
	ArdourWidgets::ArdourButton prefs_button;

	Gtk::ComboBoxText  timebase_chooser;

	bool new_button_pressed (GdkEventButton*);
	bool recent_button_pressed (GdkEventButton*);
	bool existing_button_pressed (GdkEventButton*);
	bool prefs_button_pressed (GdkEventButton*);

	bool open_button_pressed (GdkEventButton*);

	Gtk::HBox _info_box;

	Gtk::Table _open_table;

	/* initial choice page */

	void setup_existing_box ();
	void setup_recent_sessions ();
	Gtk::VBox recent_vbox;

	DialogTab _initial_tab;

#ifdef MIXBUS
	Gtk::Button _license_button;
	Gtk::Label  _license_label;
	void license_button_clicked ();
#endif

	/* recent sessions */

	void setup_existing_session_page ();

	struct RecentSessionsSorter
	{
		bool operator() (std::pair<std::string,std::string> a, std::pair<std::string,std::string> b) const {
			return ARDOUR::cmp_nocase(a.first, b.first) == -1;
		}
	};

	struct RecentSessionModelColumns : public Gtk::TreeModel::ColumnRecord {
		RecentSessionModelColumns()
		{
			add (visible_name);
			add (tip);
			add (fullpath);
			add (sample_rate);
			add (disk_format);
			add (modified_with);
			add (time_modified);
			add (time_formatted);
		}
		Gtk::TreeModelColumn<std::string> visible_name;
		Gtk::TreeModelColumn<std::string> tip;
		Gtk::TreeModelColumn<std::string> fullpath;
		Gtk::TreeModelColumn<std::string> sample_rate;
		Gtk::TreeModelColumn<std::string> disk_format;
		Gtk::TreeModelColumn<std::string> modified_with;
		Gtk::TreeModelColumn<int64_t>     time_modified;
		Gtk::TreeModelColumn<std::string> time_formatted;
	};

	RecentSessionModelColumns    recent_session_columns;
	Gtk::TreeView                recent_session_display;
	Glib::RefPtr<Gtk::TreeStore> recent_session_model;
	Gtk::ScrolledWindow          recent_scroller;
	Gtk::Label                   recent_label;
	Gtk::FileChooserWidget       existing_session_chooser;
	int redisplay_recent_sessions ();
	void recent_session_row_selected ();
	void recent_session_sort_changed ();
	void recent_row_activated (const Gtk::TreePath& path, Gtk::TreeViewColumn* col);
	bool recent_button_press (GdkEventButton*);
	void recent_context_mennu (GdkEventButton*);
	void recent_remove_selected ();

	void session_selected ();

	void existing_file_selected();
	void existing_file_activated ();

	/* new sessions */

	void setup_new_session_page ();
	Gtk::Entry new_name_entry;
	bool new_name_was_edited;
	bool new_name_edited (GdkEventKey*);

	void setup_untitled_session ();

	Gtk::FileChooserButton new_folder_chooser;

	struct SessionTemplateColumns : public Gtk::TreeModel::ColumnRecord {
		SessionTemplateColumns () {
			add (name);
			add (path);
			add (description);
			add (modified_with_short);
			add (modified_with_long);
			add (removable);
		}

		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<std::string> path;
		Gtk::TreeModelColumn<std::string> description;
		Gtk::TreeModelColumn<std::string> modified_with_short;
		Gtk::TreeModelColumn<std::string> modified_with_long;
		Gtk::TreeModelColumn<bool> removable;
	};

	SessionTemplateColumns session_template_columns;

	Glib::RefPtr<Gtk::TreeStore>  template_model;
	Gtk::TreeView                 template_chooser;
	Gtk::ScrolledWindow           template_scroller;

	void template_row_selected ();

	Gtk::TextView template_desc;
	Gtk::Frame    template_desc_frame;

	Gtk::VBox session_new_vbox;
	std::string load_template_override;

	void new_name_changed ();
	void new_name_activated ();
	void populate_session_templates ();

	void tab_page_switched(GtkNotebookPage*, guint page_number);

	/* --disable plugins UI */
	Gtk::CheckButton _disable_plugins;
	void disable_plugins_clicked ();

	/* meta-template */
	static uint32_t meta_master_bus_profile (std::string script);

	/* always there */

	Glib::RefPtr<Pango::Layout> layout;

	Gtk::Label info_scroller_label;
	std::string::size_type info_scroller_count;
	bool info_scroller_update();
	sigc::connection info_scroller_connection;
	void updates_button_clicked ();

	Gtk::Notebook _tabs;
};

#endif /* __gtk2_ardour_session_dialog_h__ */
