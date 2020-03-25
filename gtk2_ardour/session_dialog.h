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

#include <gdkmm/pixbuf.h>
#include <gtkmm/label.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/expander.h>
#include <gtkmm/box.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/filechooserbutton.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/textview.h>
#include <gtkmm/treeview.h>
#include <gtkmm/treestore.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/table.h>
#include <gtkmm/frame.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/liststore.h>
#include <gtkmm/combobox.h>

#include "ardour/utils.h"

#include "ardour_dialog.h"

class EngineControl;

class SessionDialog : public ArdourDialog
{
public:
	SessionDialog (bool require_new, const std::string& session_name, const std::string& session_path,
	               const std::string& template_name, bool cancel_not_quit);
	SessionDialog ();
	~SessionDialog ();

	std::string session_name (bool& should_be_new);
	std::string session_folder ();

	bool use_session_template() const;
	std::string session_template_name();

	uint32_t master_channel_count();

	void set_provided_session (std::string const & name, std::string const & path);
	void clear_name ();
	bool was_new_name_edited() const { return new_name_was_edited; }

private:
	bool new_only;

	bool on_delete_event (GdkEventAny*);

	Gtk::Button* cancel_button;
	Gtk::Button* open_button;
	Gtk::Button* back_button;
	Gtk::Button* quit_button;

	bool back_button_pressed (GdkEventButton*);
	bool open_button_pressed (GdkEventButton*);

	Gtk::Frame info_frame;

	/* initial choice page */

	void setup_initial_choice_box ();
	void setup_recent_sessions ();
	Gtk::VBox ic_vbox;
	Gtk::Button ic_new_session_button;
	void new_session_button_clicked ();

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
	Gtk::FileChooserButton       existing_session_chooser;
	int redisplay_recent_sessions ();
	void recent_session_row_selected ();
	void recent_session_sort_changed ();
	void recent_row_activated (const Gtk::TreePath& path, Gtk::TreeViewColumn* col);
	bool recent_button_press (GdkEventButton*);
	void recent_context_mennu (GdkEventButton*);
	void recent_remove_selected ();

	void existing_session_selected ();
	void session_selected ();

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
		}

		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<std::string> path;
		Gtk::TreeModelColumn<std::string> description;
		Gtk::TreeModelColumn<std::string> modified_with_short;
		Gtk::TreeModelColumn<std::string> modified_with_long;
	};

	SessionTemplateColumns session_template_columns;

	Glib::RefPtr<Gtk::TreeStore>  template_model;
	Gtk::TreeView                 template_chooser;
	Gtk::ScrolledWindow           template_scroller;

	void template_row_selected ();

	Gtk::TextView template_desc;
	Gtk::Frame    template_desc_frame;

	Gtk::VBox session_new_vbox;
	Gtk::VBox session_existing_vbox;
	std::string load_template_override;

	void new_name_changed ();
	void new_name_activated ();
	void populate_session_templates ();

	/* --disable plugins UI */
	Gtk::CheckButton _disable_plugins;
	void disable_plugins_clicked ();

	/* meta-template */
	static uint32_t meta_master_bus_profile (std::string script);

	/* always there */

	Glib::RefPtr<Pango::Layout> layout;

	bool _existing_session_chooser_used; ///< set to true when the existing session chooser has been used

	Gtk::Label info_scroller_label;
	std::string::size_type info_scroller_count;
	bool info_scroller_update();
	sigc::connection info_scroller_connection;
	void updates_button_clicked ();

	int inital_height;
	int inital_width;
};

#endif /* __gtk2_ardour_session_dialog_h__ */
