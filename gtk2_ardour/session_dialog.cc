/*
 * Copyright (C) 2013-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2015 Nick Mainsbridge <mainsbridge@gmail.com>
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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <algorithm>

#include <glib.h>
#include "pbd/gstdio_compat.h"
#include <glibmm.h>

#include <gtkmm/filechooser.h>
#include <gtkmm/stock.h>

#include "pbd/basename.h"
#include "pbd/failed_constructor.h"
#include "pbd/file_utils.h"
#include "pbd/replace_all.h"
#include "pbd/whitespace.h"
#include "pbd/stl_delete.h"
#include "pbd/openuri.h"

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/keyboard.h"

#include "widgets/tooltips.h"

#include "ardour/audioengine.h"
#include "ardour/filesystem_paths.h"
#include "ardour/luascripting.h"
#include "ardour/recent_sessions.h"
#include "ardour/session.h"
#include "ardour/session_state_utils.h"
#include "ardour/template_utils.h"
#include "ardour/filename_extensions.h"

#include "LuaBridge/LuaBridge.h"

#include "ardour_message.h"
#include "ardour_ui.h"
#include "session_dialog.h"
#include "opts.h"
#include "engine_dialog.h"
#include "pbd/i18n.h"
#include "ui_config.h"
#include "utils.h"

using namespace std;
using namespace Gtk;
using namespace Gdk;
using namespace Glib;
using namespace PBD;
using namespace ARDOUR;
using namespace ArdourWidgets;
using namespace ARDOUR_UI_UTILS;

SessionDialog::SessionDialog (bool require_new, const std::string& session_name, const std::string& session_path, const std::string& template_name, bool cancel_not_quit)
	: ArdourDialog (_("Session Setup"), true, true)
	, new_only (require_new)
	, new_name_was_edited (false)
	, new_folder_chooser (FILE_CHOOSER_ACTION_SELECT_FOLDER)
	, _existing_session_chooser_used (false)
{
	set_position (WIN_POS_CENTER);
	get_vbox()->set_spacing (6);

	cancel_button = add_button ((cancel_not_quit ? Stock::CANCEL : Stock::QUIT), RESPONSE_CANCEL);
	back_button = add_button (Stock::GO_BACK, RESPONSE_NO);
	open_button = add_button (Stock::OPEN, RESPONSE_ACCEPT);

	back_button->signal_button_press_event().connect (sigc::mem_fun (*this, &SessionDialog::back_button_pressed), false);
	open_button->signal_button_press_event().connect (sigc::mem_fun (*this, &SessionDialog::open_button_pressed), false);

	open_button->set_sensitive (false);
	back_button->set_sensitive (false);

	/* this is where announcements will be displayed, but it may be empty
	 * and invisible most of the time.
	 */

	info_frame.set_shadow_type(SHADOW_ETCHED_OUT);
	info_frame.set_no_show_all (true);
	info_frame.set_border_width (12);
	get_vbox()->pack_start (info_frame, false, false);

	if (!template_name.empty()) {
		load_template_override = template_name;
	}

	setup_new_session_page ();

	if (!require_new) {
		setup_initial_choice_box ();
		get_vbox()->pack_start (ic_vbox, true, true);
	} else {
		get_vbox()->pack_start (session_new_vbox, true, true);
	}

	get_vbox()->show_all ();

	/* fill data models and show/hide accordingly */

	populate_session_templates ();

	if (recent_session_model) {
		int cnt = redisplay_recent_sessions ();
		if (cnt > 0) {
			recent_scroller.show();
			recent_label.show ();

			if (cnt > 4) {
				recent_scroller.set_size_request (-1, 300);
			} else {
				recent_scroller.set_size_request (-1, 80);
			}
		} else {
			recent_scroller.hide();
			recent_label.hide ();
		}
	}
	inital_height = get_height();
	inital_width = get_width();

	if (require_new) {
		setup_untitled_session ();
	}
}

SessionDialog::SessionDialog ()
	: ArdourDialog (_("Recent Sessions"), true, true)
	, new_only (false)
	, _existing_session_chooser_used (false) // caller must check should_be_new
{
	get_vbox()->set_spacing (6);

	cancel_button = add_button (Stock::CANCEL, RESPONSE_CANCEL);
	open_button = add_button (Stock::OPEN, RESPONSE_ACCEPT);
	open_button->set_sensitive (false);

	setup_recent_sessions ();

	get_vbox()->pack_start (recent_scroller, true, true);
	get_vbox()->show_all ();

	recent_scroller.show();

	int cnt = redisplay_recent_sessions ();
	if (cnt > 4) {
		recent_scroller.set_size_request (-1, 300);
	} else {
		recent_scroller.set_size_request (-1, 80);
	}

}

SessionDialog::~SessionDialog()
{
}

uint32_t
SessionDialog::meta_master_bus_profile (std::string script_path)
{
	if (!Glib::file_test (script_path, Glib::FILE_TEST_EXISTS | Glib::FILE_TEST_IS_REGULAR)) {
		return UINT32_MAX;
	}

	LuaState lua;
	lua.sandbox (true);
	lua_State* L = lua.getState();

	lua.do_command (
			"ardourluainfo = {}"
			"function ardour (entry)"
			"  ardourluainfo['type'] = assert(entry['type'])"
			"  ardourluainfo['master_bus'] = entry['master_bus'] or 2"
			" end"
			);

	int err = -1;

	try {
		err = lua.do_file (script_path);
	} catch (luabridge::LuaException const& e) {
#ifndef NDEBUG
		cerr << "LuaException:" << e.what () << endl;
#endif
		PBD::warning << "LuaException: " << e.what () << endmsg;
		err = -1;
	}  catch (...) {
		err = -1;
	}

	if (err) {
		return UINT32_MAX;
	}

	luabridge::LuaRef nfo = luabridge::getGlobal (L, "ardourluainfo");
	if (nfo.type() != LUA_TTABLE) {
		return UINT32_MAX;
	}

	if (nfo["master_bus"].type() != LUA_TNUMBER || nfo["type"].type() != LUA_TSTRING) {
		return UINT32_MAX;
	}

	LuaScriptInfo::ScriptType type = LuaScriptInfo::str2type (nfo["type"].cast<std::string>());
	if (type != LuaScriptInfo::SessionInit) {
		return UINT32_MAX;
	}

	return nfo["master_bus"].cast<uint32_t>();
}

uint32_t
SessionDialog::master_channel_count ()
{
	if (use_session_template ()) {
		std::string tn = session_template_name();
		if (tn.substr (0, 11) == "urn:ardour:") {
			uint32_t mc = meta_master_bus_profile (tn.substr (11));
			if (mc != UINT32_MAX) {
				return mc;
			}
		}
	}
	return 2;
}

bool
SessionDialog::use_session_template () const
{
	if (!back_button->sensitive () && !new_only) {
		/* open session -- not create a new one */
		return false;
	}

	if (template_chooser.get_selection()->count_selected_rows() > 0) {
		return true;
	}

	return false;
}

std::string
SessionDialog::session_template_name ()
{
	if (template_chooser.get_selection()->count_selected_rows() > 0) {

		TreeIter const iter = template_chooser.get_selection()->get_selected();

		if (iter) {
			string s = (*iter)[session_template_columns.path];
			return s;
		}
	}

	return string();
}

void
SessionDialog::clear_name ()
{
	recent_session_display.get_selection()->unselect_all();
	new_name_entry.set_text (string());
}

std::string
SessionDialog::session_name (bool& should_be_new)
{
	/* Try recent session selection */

	TreeIter iter = recent_session_display.get_selection()->get_selected();

	if (iter) {
		should_be_new = false;
		string s = (*iter)[recent_session_columns.fullpath];
		if (Glib::file_test (s, Glib::FILE_TEST_IS_REGULAR)) {
			return PBD::basename_nosuffix (s);
		}
		return (*iter)[recent_session_columns.visible_name];
	}

	if (_existing_session_chooser_used) {
		/* existing session chosen from file chooser */
		should_be_new = false;
		return existing_session_chooser.get_filename ();
	} else {
		should_be_new = true;
		string val = new_name_entry.get_text ();
		strip_whitespace_edges (val);
		return val;
	}
}

std::string
SessionDialog::session_folder ()
{
	/* Try recent session selection */

	TreeIter iter = recent_session_display.get_selection()->get_selected();

	if (iter) {
		string s = (*iter)[recent_session_columns.fullpath];
		if (Glib::file_test (s, Glib::FILE_TEST_IS_REGULAR)) {
			return Glib::path_get_dirname (s);
		}
		return s;
	}

	if (_existing_session_chooser_used) {
		/* existing session chosen from file chooser */
		return Glib::path_get_dirname (existing_session_chooser.get_current_folder ());
	} else {
		std::string val = new_name_entry.get_text();
		strip_whitespace_edges (val);
		std::string legal_session_folder_name = legalize_for_path (val);
		return Glib::build_filename (new_folder_chooser.get_filename (), legal_session_folder_name);
	}
}

void
SessionDialog::setup_recent_sessions ()
{
	recent_session_model = TreeStore::create (recent_session_columns);
	recent_session_model->signal_sort_column_changed().connect (sigc::mem_fun (*this, &SessionDialog::recent_session_sort_changed));

	recent_session_display.set_model (recent_session_model);
	recent_session_display.append_column (_("Session Name"), recent_session_columns.visible_name);
	recent_session_display.append_column (_("Sample Rate"), recent_session_columns.sample_rate);
#ifdef MIXBUS
	recent_session_display.append_column (_("Modified With"), recent_session_columns.modified_with);
#else
	recent_session_display.append_column (_("File Resolution"), recent_session_columns.disk_format);
#endif
	recent_session_display.append_column (_("Last Modified"), recent_session_columns.time_formatted);
	recent_session_display.set_headers_visible (true);
	recent_session_display.get_selection()->set_mode (SELECTION_SINGLE);

	recent_session_display.get_selection()->signal_changed().connect (sigc::mem_fun (*this, &SessionDialog::recent_session_row_selected));

	recent_scroller.add (recent_session_display);
	recent_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
	recent_scroller.set_shadow_type	(Gtk::SHADOW_IN);

	recent_session_display.show();
	recent_session_display.signal_row_activated().connect (sigc::mem_fun (*this, &SessionDialog::recent_row_activated));
	recent_session_display.signal_button_press_event().connect (sigc::mem_fun (*this, &SessionDialog::recent_button_press), false);
}

void
SessionDialog::setup_initial_choice_box ()
{
	ic_vbox.set_spacing (6);

	HBox* centering_hbox = manage (new HBox);
	VBox* centering_vbox = manage (new VBox);

	centering_vbox->set_spacing (6);

	Label* new_label = manage (new Label);
	new_label->set_markup (string_compose ("<span weight=\"bold\" size=\"large\">%1</span>", _("New Session")));
	new_label->set_justify (JUSTIFY_CENTER);

	ic_new_session_button.add (*new_label);
	ic_new_session_button.signal_clicked().connect (sigc::mem_fun (*this, &SessionDialog::new_session_button_clicked));

	Gtk::HBox* hbox = manage (new HBox);
	Gtk::VBox* vbox = manage (new VBox);
	hbox->set_spacing (12);
	vbox->set_spacing (12);

	string image_path;

	Searchpath rc (ARDOUR::ardour_data_search_path());
	rc.add_subdirectory_to_paths ("resources");

	if (find_file (rc, PROGRAM_NAME "-small-splash.png", image_path)) {
		Gtk::Image* image;
		if ((image = manage (new Gtk::Image (image_path))) != 0) {
			hbox->pack_start (*image, false, false);
		}
	}

	vbox->pack_start (ic_new_session_button, true, true, 20);
	hbox->pack_start (*vbox, true, true, 20);

	centering_vbox->pack_start (*hbox, false, false);

	/* Possible update message */

	if (ARDOUR_UI::instance()->announce_string() != "" ) {

		Box *info_box = manage (new VBox);
		info_box->set_border_width (12);
		info_box->set_spacing (6);

		info_box->pack_start (info_scroller_label, false, false);

		info_scroller_count = 0;
		info_scroller_connection = Glib::signal_timeout().connect (mem_fun(*this, &SessionDialog::info_scroller_update), 50);

		Gtk::Button *updates_button = manage (new Gtk::Button (_("Check the website for more...")));

		updates_button->signal_clicked().connect (mem_fun(*this, &SessionDialog::updates_button_clicked) );
		set_tooltip (*updates_button, _("Click to open the program website in your web browser"));

		info_box->pack_start (*updates_button, false, false);

		info_frame.add (*info_box);
		info_box->show_all ();
		info_frame.show ();
	}

	/* recent session scroller */
	setup_recent_sessions ();

	recent_label.set_no_show_all (true);
	recent_scroller.set_no_show_all (true);

	recent_label.set_markup (string_compose ("<span weight=\"bold\" size=\"large\">%1</span>", _("Recent Sessions")));

	centering_vbox->pack_start (recent_label, false, false, 12);
	centering_vbox->pack_start (recent_scroller, true, true);

	/* Browse button */

	existing_session_chooser.set_title (_("Select session file"));
	existing_session_chooser.signal_file_set().connect (sigc::mem_fun (*this, &SessionDialog::existing_session_selected));
	existing_session_chooser.set_current_folder(poor_mans_glob (Config->get_default_session_parent_dir()));

	FileFilter session_filter;
	session_filter.add_pattern (string_compose(X_("*%1"), ARDOUR::statefile_suffix));
	session_filter.set_name (string_compose (_("%1 sessions"), PROGRAM_NAME));
	existing_session_chooser.add_filter (session_filter);

	FileFilter archive_filter;
	archive_filter.add_pattern (string_compose(X_("*%1"), ARDOUR::session_archive_suffix));
	archive_filter.set_name (_("Session Archives"));
	existing_session_chooser.add_filter (archive_filter);

	existing_session_chooser.set_filter (session_filter);

	Gtkmm2ext::add_volume_shortcuts (existing_session_chooser);

	Label* browse_label = manage (new Label);
	browse_label->set_markup (string_compose ("<span weight=\"bold\" size=\"large\">%1</span>", _("Other Sessions")));

	centering_vbox->pack_start (*browse_label, false, false, 12);
	centering_vbox->pack_start (existing_session_chooser, false, false);

	/* --disable plugins UI */

	_disable_plugins.set_label (_("Safe Mode: Disable all Plugins"));
	_disable_plugins.set_flags (Gtk::CAN_FOCUS);
	_disable_plugins.set_relief (Gtk::RELIEF_NORMAL);
	_disable_plugins.set_mode (true);
	_disable_plugins.set_active (ARDOUR::Session::get_disable_all_loaded_plugins());
	_disable_plugins.set_border_width(0);
	_disable_plugins.signal_clicked().connect (sigc::mem_fun (*this, &SessionDialog::disable_plugins_clicked));
	centering_vbox->pack_start (_disable_plugins, false, false);

	/* pack it all up */

	centering_hbox->pack_start (*centering_vbox, true, true);
	ic_vbox.pack_start (*centering_hbox, true, true);
	ic_vbox.show_all ();
}

void
SessionDialog::session_selected ()
{
	/* HACK HACK HACK ... change the "Apply" button label
	   to say "Open"
	*/

	Gtk::Widget* tl = ic_vbox.get_toplevel();
	Gtk::Window* win;
	if ((win = dynamic_cast<Gtk::Window*>(tl)) != 0) {
		/* ::get_default_widget() is not wrapped in gtkmm */
		Gtk::Widget* def = wrap (gtk_window_get_default_widget (win->gobj()));
		Gtk::Button* button;
		if ((button = dynamic_cast<Gtk::Button*>(def)) != 0) {
			button->set_label (_("Open"));
		}
	}
}

void
SessionDialog::new_session_button_clicked ()
{
	_existing_session_chooser_used = false;
	recent_session_display.get_selection()->unselect_all ();

	get_vbox()->remove (ic_vbox);
	get_vbox()->pack_start (session_new_vbox, true, true);

	back_button->set_sensitive (true);
	setup_untitled_session ();
}

bool
SessionDialog::back_button_pressed (GdkEventButton*)
{
	get_vbox()->remove (session_new_vbox);
	back_button->set_sensitive (false);
	get_vbox()->pack_start (ic_vbox);
	resize(inital_height, inital_width);

	return true;
}

bool
SessionDialog::open_button_pressed (GdkEventButton* ev)
{
	if (Gtkmm2ext::Keyboard::modifier_state_equals (ev->state, Gtkmm2ext::Keyboard::PrimaryModifier)) {
		_disable_plugins.set_active();
	}
	response (RESPONSE_ACCEPT);
	return true;
}

void
SessionDialog::setup_untitled_session ()
{
	new_name_entry.set_text (string_compose (_("Untitled-%1"), Glib::DateTime::create_now_local().format ("%F-%H-%M-%S")));
	new_name_entry.select_region (0, -1);
	new_name_was_edited = false;

	back_button->set_sensitive (true);
	new_name_entry.grab_focus ();
}

void
SessionDialog::populate_session_templates ()
{
	vector<TemplateInfo> templates;

	find_session_templates (templates, true);

	template_model->clear ();

	/* Get Lua Scripts dedicated to session-setup */
	LuaScriptList scripts (LuaScripting::instance ().scripts (LuaScriptInfo::SessionInit));

	/* Add Lua Action Scripts which can also be used for session-setup */
	LuaScriptList& as (LuaScripting::instance ().scripts (LuaScriptInfo::EditorAction));
	for (LuaScriptList::const_iterator s = as.begin(); s != as.end(); ++s) {
		if ((*s)->subtype & LuaScriptInfo::SessionSetup) {
			scripts.push_back (*s);
		}
	}

	std::sort (scripts.begin(), scripts.end(), LuaScripting::Sorter());

	for (LuaScriptList::const_iterator s = scripts.begin(); s != scripts.end(); ++s) {
		TreeModel::Row row = *(template_model->append ());
		row[session_template_columns.name] = (*s)->name;
		row[session_template_columns.path] = "urn:ardour:" + (*s)->path;
		row[session_template_columns.description] = (*s)->description;
		row[session_template_columns.modified_with_short] = string_compose ("{%1}", _("Factory Template"));
		row[session_template_columns.modified_with_long] = string_compose ("{%1}", _("Factory Template"));
	}

	//Add any "template sessions" found in the user's preferences folder
	for (vector<TemplateInfo>::iterator x = templates.begin(); x != templates.end(); ++x) {
		TreeModel::Row row;

		row = *(template_model->append ());

		row[session_template_columns.name] = (*x).name;
		row[session_template_columns.path] = (*x).path;
		row[session_template_columns.description] = (*x).description;
		row[session_template_columns.modified_with_long] = (*x).modified_with;
		row[session_template_columns.modified_with_short] = (*x).modified_with.substr(0, (*x).modified_with.find(" "));
	}

	//Add an explicit 'Empty Template' item
	TreeModel::Row row = *template_model->prepend ();
	row[session_template_columns.name] = (_("Empty Template"));
	row[session_template_columns.path] = string();
	row[session_template_columns.description] = _("An empty session with factory default settings.\n\nSelect this option if you are importing files to mix.");
	row[session_template_columns.modified_with_short] = ("");
	row[session_template_columns.modified_with_long] = ("");

	//auto-select the first item in the list
	Gtk::TreeModel::Row first = template_model->children()[0];
	if(first) {
		template_chooser.get_selection()->select(first);
	}
}

void
SessionDialog::setup_new_session_page ()
{
	session_new_vbox.set_border_width (8);
	session_new_vbox.set_spacing (8);

	Label* name_label = manage (new Label);
	name_label->set_text (_("Session name:"));

	HBox* name_hbox = manage (new HBox);
	name_hbox->set_spacing (8);
	name_hbox->pack_start (*name_label, false, true);
	name_hbox->pack_start (new_name_entry, true, true);

	new_name_entry.signal_key_press_event().connect (sigc::mem_fun (*this, &SessionDialog::new_name_edited), false);
	new_name_entry.signal_changed().connect (sigc::mem_fun (*this, &SessionDialog::new_name_changed));
	new_name_entry.signal_activate().connect (sigc::mem_fun (*this, &SessionDialog::new_name_activated));

	//Folder location for the new session
	Label* new_folder_label = manage (new Label);
	new_folder_label->set_text (_("Create session folder in:"));
	HBox* folder_box = manage (new HBox);
	folder_box->set_spacing (8);
	folder_box->pack_start (*new_folder_label, false, false);
	folder_box->pack_start (new_folder_chooser, true, true);

	if (ARDOUR_UI::instance()->the_session ()) {
		// point the new session file chooser at the parent directory of the current session
		string session_parent_dir = Glib::path_get_dirname(ARDOUR_UI::instance()->the_session()->path());
		new_folder_chooser.set_current_folder (session_parent_dir);
		string default_session_folder = poor_mans_glob (Config->get_default_session_parent_dir());

		try {
			/* add_shortcut_folder throws an exception if the folder being added already has a shortcut */
			new_folder_chooser.add_shortcut_folder (default_session_folder);
		}
		catch (Glib::Error & e) {
			std::cerr << "new_folder_chooser.add_shortcut_folder (" << default_session_folder << ") threw Glib::Error " << e.what() << std::endl;
		}
	} else {
		new_folder_chooser.set_current_folder (poor_mans_glob (Config->get_default_session_parent_dir()));
	}
	new_folder_chooser.show ();
	new_folder_chooser.set_title (_("Select folder for session"));
	Gtkmm2ext::add_volume_shortcuts (new_folder_chooser);

	//Template & Template Description area
	HBox* template_hbox = manage (new HBox);

	//if the "template override" is provided, don't give the user any template selections   (?)
	if (load_template_override.empty()) {
		template_hbox->set_spacing (8);

		Gtk::ScrolledWindow *template_scroller = manage (new Gtk::ScrolledWindow());
		template_scroller->set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
		template_scroller->add (template_chooser);

		Gtk::ScrolledWindow *desc_scroller = manage (new Gtk::ScrolledWindow());
		desc_scroller->set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
		desc_scroller->add (template_desc);

		template_hbox->pack_start (*template_scroller, true, true);

		template_desc_frame.set_name (X_("TextHighlightFrame"));
		template_desc_frame.add (*desc_scroller);
		template_hbox->pack_start (template_desc_frame, true, true);
	}

	//template_desc is the textview that displays the currently selected template's description
	template_desc.set_editable (false);
	template_desc.set_wrap_mode (Gtk::WRAP_WORD);
	template_desc.set_size_request (300,400);
	template_desc.set_name (X_("TextOnBackground"));
	template_desc.set_border_width (6);

	//template_chooser is the treeview showing available templates
	template_model = TreeStore::create (session_template_columns);
	template_chooser.set_model (template_model);
	template_chooser.append_column (_("Template"), session_template_columns.name);
#ifdef MIXBUS
	template_chooser.append_column (_("Modified With"), session_template_columns.modified_with_short);
#endif
	template_chooser.set_headers_visible (true);
	template_chooser.get_selection()->set_mode (SELECTION_SINGLE);
	template_chooser.get_selection()->signal_changed().connect (sigc::mem_fun (*this, &SessionDialog::template_row_selected));
	template_chooser.set_sensitive (true);
	if (UIConfiguration::instance().get_use_tooltips()) {
		template_chooser.set_tooltip_column(4); // modified_with_long
	}

	session_new_vbox.pack_start (*template_hbox, true, true);
	session_new_vbox.pack_start (*folder_box, false, true);
	session_new_vbox.pack_start (*name_hbox, false, true);
	session_new_vbox.show_all ();
}

bool
SessionDialog::new_name_edited (GdkEventKey* ev)
{
	switch (ev->keyval) {
	case GDK_KP_Enter:
	case GDK_3270_Enter:
	case GDK_Return:
		break;
	default:
		new_name_was_edited = true;
	}

	return false;
}


static bool is_invalid_session_char (char c)
{
	/* see also Session::session_name_is_legal */
	return iscntrl (c) || c == '/' || c == '\\' || c == ':' || c == ';';
}

void
SessionDialog::new_name_changed ()
{
	std::string new_name = new_name_entry.get_text();

	std::string const& illegal = Session::session_name_is_legal (new_name);
	if (!illegal.empty()) {
		ArdourMessageDialog msg (string_compose (_("To ensure compatibility with various systems\nsession names may not contain a '%1' character"), illegal));
		msg.run ();
		new_name.erase (remove_if (new_name.begin(), new_name.end(), is_invalid_session_char), new_name.end());
		new_name_entry.set_text (new_name);
	}

	if (!new_name_entry.get_text().empty()) {
		session_selected ();
		open_button->set_sensitive (true);
	} else {
		open_button->set_sensitive (false);
	}
}

void
SessionDialog::new_name_activated ()
{
	response (RESPONSE_ACCEPT);
}

int
SessionDialog::redisplay_recent_sessions ()
{
	std::vector<std::string> session_directories;
	RecentSessionsSorter cmp;

	recent_session_display.set_model (Glib::RefPtr<TreeModel>(0));
	recent_session_model->clear ();

	ARDOUR::RecentSessions rs;
	ARDOUR::read_recent_sessions (rs);

	if (rs.empty()) {
		recent_session_display.set_model (recent_session_model);
		return 0;
	}

	// sort them alphabetically
	sort (rs.begin(), rs.end(), cmp);

	for (ARDOUR::RecentSessions::iterator i = rs.begin(); i != rs.end(); ++i) {
		session_directories.push_back ((*i).second);
	}

	int session_snapshot_count = 0;

	for (vector<std::string>::const_iterator i = session_directories.begin(); i != session_directories.end(); ++i) {
		std::vector<std::string> state_file_paths;

		// now get available states for this session

		get_state_files_in_directory (*i, state_file_paths);

		vector<string> states;
		vector<const gchar*> item;
		string dirname = *i;

		/* remove any trailing / */

		if (dirname.empty()) {
			continue;
		}

		if (dirname[dirname.length()-1] == '/') {
			dirname = dirname.substr (0, dirname.length()-1);
		}

		/* check whether session still exists */
		if (!Glib::file_test(dirname.c_str(), Glib::FILE_TEST_EXISTS)) {
			/* session doesn't exist */
			continue;
		}

		/* now get available states for this session */

		states = Session::possible_states (dirname);

		if (states.empty()) {
			/* no state file? */
			continue;
		}

		std::vector<string> state_file_names(get_file_names_no_extension (state_file_paths));

		if (state_file_names.empty()) {
			continue;
		}

		float sr;
		SampleFormat sf;
		std::string program_version;

		std::string state_file_basename;

		if (state_file_names.size() > 1) {
			state_file_basename = Session::get_snapshot_from_instant (dirname);
			std::string s = Glib::build_filename (dirname, state_file_basename + statefile_suffix);
			if (!Glib::file_test (s, Glib::FILE_TEST_IS_REGULAR)) {
				state_file_basename = "";
			}
		}

		if (state_file_basename.empty()) {
			state_file_basename = state_file_names.front();
		}

		std::string s = Glib::build_filename (dirname, state_file_basename + statefile_suffix);

		int err = Session::get_info_from_path (s, sr, sf, program_version);
		if (err < 0) {
			// XML cannot be parsed, or unsuppored version
			continue;
		}

		GStatBuf gsb;
		g_stat (s.c_str(), &gsb);

		Gtk::TreeModel::Row row = *(recent_session_model->append());
		row[recent_session_columns.fullpath] = s;
		row[recent_session_columns.time_modified] = gsb.st_mtime;


		if (err == 0) {
			row[recent_session_columns.sample_rate] = rate_as_string (sr);
			switch (sf) {
			case FormatFloat:
				row[recent_session_columns.disk_format] = _("32-bit float");
				break;
			case FormatInt24:
				row[recent_session_columns.disk_format] = _("24-bit");
				break;
			case FormatInt16:
				row[recent_session_columns.disk_format] = _("16-bit");
				break;
			}
		} else {
			row[recent_session_columns.sample_rate] = "??";
			row[recent_session_columns.disk_format] = "--";
		}

		if (program_version.empty()) {
			row[recent_session_columns.tip] = Gtkmm2ext::markup_escape_text (dirname);
		} else {
			row[recent_session_columns.tip] = Gtkmm2ext::markup_escape_text (dirname + "\n" + string_compose (_("Last modified with: %1"), program_version));
			row[recent_session_columns.modified_with] = program_version;
		}

		++session_snapshot_count;

		if (state_file_names.size() > 1) {
			// multiple session files in the session directory - show the directory name.
			// if there's not a session file with the same name as the session directory,
			// opening the parent item will fail, but expanding it will show the session
			// files that actually exist, and the right one can then be opened.
			row[recent_session_columns.visible_name] = Glib::path_get_basename (dirname);
			int64_t most_recent = 0;

			// add the children
			int kidcount = 0;
			for (std::vector<std::string>::iterator i2 = state_file_names.begin(); i2 != state_file_names.end(); ++i2) {

				s = Glib::build_filename (dirname, *i2 + statefile_suffix);
				Gtk::TreeModel::Row child_row = *(recent_session_model->append (row.children()));

				child_row[recent_session_columns.visible_name] = *i2;
				child_row[recent_session_columns.fullpath] = s;
				child_row[recent_session_columns.tip] = Gtkmm2ext::markup_escape_text (dirname);
				g_stat (s.c_str(), &gsb);
				child_row[recent_session_columns.time_modified] = gsb.st_mtime;

				Glib::DateTime gdt(Glib::DateTime::create_now_local (gsb.st_mtime));
				child_row[recent_session_columns.time_formatted] = gdt.format ("%F %H:%M");

				if (gsb.st_mtime > most_recent) {
					most_recent = gsb.st_mtime;
				}

				if (++kidcount < 5) {
					// parse "modified with" for the first 5 snapshots
					if (Session::get_info_from_path (s, sr, sf, program_version) == 0) {
#if 0
						child_row[recent_session_columns.sample_rate] = rate_as_string (sr);
						switch (sf) {
						case FormatFloat:
							child_row[recent_session_columns.disk_format] = _("32-bit float");
							break;
						case FormatInt24:
							child_row[recent_session_columns.disk_format] = _("24-bit");
							break;
						case FormatInt16:
							child_row[recent_session_columns.disk_format] = _("16-bit");
							break;
						}
#else
						child_row[recent_session_columns.sample_rate] = "";
						child_row[recent_session_columns.disk_format] = "";
#endif
					} else {
						child_row[recent_session_columns.sample_rate] = "??";
						child_row[recent_session_columns.disk_format] = "--";
					}
					if (!program_version.empty()) {
						child_row[recent_session_columns.tip] = Gtkmm2ext::markup_escape_text (string_compose (_("Last modified with: %1"), program_version));
					}
				} else {
					child_row[recent_session_columns.sample_rate] = "";
					child_row[recent_session_columns.disk_format] = "";
				}

				++session_snapshot_count;
			}

			assert (most_recent >= row[recent_session_columns.time_modified]);
			row[recent_session_columns.time_modified] = most_recent;

		} else {
			// only a single session file in the directory - show its actual name.
			row[recent_session_columns.visible_name] = state_file_basename;
		}

		Glib::DateTime gdt(Glib::DateTime::create_now_local (row[recent_session_columns.time_modified]));
		row[recent_session_columns.time_formatted] = gdt.format ("%F %H:%M");
	}

	if (UIConfiguration::instance().get_use_tooltips()) {
		recent_session_display.set_tooltip_column(1); // recent_session_columns.tip
	}
	recent_session_display.set_model (recent_session_model);

	// custom sort
	Gtk::TreeView::Column* pColumn;
	if ((pColumn = recent_session_display.get_column (0))) { // name
		pColumn->set_sort_column (recent_session_columns.visible_name);
	}
	if ((pColumn = recent_session_display.get_column (3))) { // date
		pColumn->set_sort_column (recent_session_columns.time_modified); // unixtime
	}

	int32_t sort = UIConfiguration::instance().get_recent_session_sort();
	if (abs(sort) != 1 + recent_session_columns.visible_name.index () &&
	    abs(sort) != 1 + recent_session_columns.time_modified.index ()) {
		sort = 1 + recent_session_columns.visible_name.index();
	}
	recent_session_model->set_sort_column (abs (sort) -1, sort < 0 ? Gtk::SORT_DESCENDING : Gtk::SORT_ASCENDING);

	return session_snapshot_count;
}

void
SessionDialog::recent_session_sort_changed ()
{
	int column;
	SortType order;
	if (recent_session_model->get_sort_column_id (column, order)) {
		int32_t sort = (column + 1) * (order == Gtk::SORT_DESCENDING ? -1 : 1);
		if (sort != UIConfiguration::instance().get_recent_session_sort()) {
			UIConfiguration::instance().set_recent_session_sort(sort);
		}
	}
}

void
SessionDialog::recent_session_row_selected ()
{
	if (recent_session_display.get_selection()->count_selected_rows() > 0) {
		open_button->set_sensitive (true);
		session_selected ();
	} else {
		open_button->set_sensitive (false);
	}
}

void
SessionDialog::template_row_selected ()
{
	if (template_chooser.get_selection()->count_selected_rows() > 0) {
		TreeIter iter = template_chooser.get_selection()->get_selected();

		if (iter) {
			string s = (*iter)[session_template_columns.description];
			template_desc.get_buffer()->set_text (s);
		}
	}
}

void
SessionDialog::recent_row_activated (const Gtk::TreePath&, Gtk::TreeViewColumn*)
{
	response (RESPONSE_ACCEPT);
}

bool
SessionDialog::recent_button_press (GdkEventButton* ev)
{
	if ((ev->type == GDK_BUTTON_PRESS) && (ev->button == 3) ) {

		TreeModel::Path path;
		TreeViewColumn* column;
		int cellx, celly;
		if (recent_session_display.get_path_at_pos ((int)ev->x, (int)ev->y, path, column, cellx, celly)) {
			Glib::RefPtr<Gtk::TreeView::Selection> selection = recent_session_display.get_selection();
			if (selection) {
				selection->unselect_all();
				selection->select(path);
			}
		}

		if (recent_session_display.get_selection()->count_selected_rows() > 0) {
			recent_context_mennu (ev);
		}
	}
	return false;
}

void
SessionDialog::recent_context_mennu (GdkEventButton *ev)
{
	using namespace Gtk::Menu_Helpers;

	TreeIter iter = recent_session_display.get_selection()->get_selected();
	assert (iter);
	string s = (*iter)[recent_session_columns.fullpath];
	if (Glib::file_test (s, Glib::FILE_TEST_IS_REGULAR)) {
		s = Glib::path_get_dirname (s);
	}
	if (!Glib::file_test (s, Glib::FILE_TEST_IS_DIR)) {
		return;
	}

	Gtk::TreeModel::Path tpath = recent_session_model->get_path(iter);
	const bool is_child = tpath.up () && tpath.up ();

	Gtk::Menu* m = ARDOUR_UI::instance()->shared_popup_menu ();
	MenuList& items = m->items ();
	items.push_back (MenuElem (s, sigc::bind (sigc::hide_return (sigc::ptr_fun (&PBD::open_folder)), s)));
	if (!is_child) {
		items.push_back (SeparatorElem());
		items.push_back (MenuElem (_("Remove session from recent list"), sigc::mem_fun (*this, &SessionDialog::recent_remove_selected)));
	}
	m->popup (ev->button, ev->time);
}

void
SessionDialog::recent_remove_selected ()
{
	TreeIter iter = recent_session_display.get_selection()->get_selected();
	assert (iter);
	string s = (*iter)[recent_session_columns.fullpath];
	if (Glib::file_test (s, Glib::FILE_TEST_IS_REGULAR)) {
		s = Glib::path_get_dirname (s);
	}
	ARDOUR::remove_recent_sessions (s);
	redisplay_recent_sessions ();
}

void
SessionDialog::disable_plugins_clicked ()
{
	ARDOUR::Session::set_disable_all_loaded_plugins (_disable_plugins.get_active());
}

void
SessionDialog::existing_session_selected ()
{
	_existing_session_chooser_used = true;
	recent_session_display.get_selection()->unselect_all();
	/* mark this sensitive in case we come back here after a failed open
	 * attempt and the user has hacked up the fix. sigh.
	 */
	open_button->set_sensitive (true);
	response (RESPONSE_ACCEPT);
}

void
SessionDialog::updates_button_clicked ()
{
	//now open a browser window so user can see more
	PBD::open_uri (Config->get_updates_url());
}

bool
SessionDialog::info_scroller_update()
{
	info_scroller_count++;

	char buf[512];
	snprintf (buf, std::min(info_scroller_count,sizeof(buf)-1), "%s", ARDOUR_UI::instance()->announce_string().c_str() );
	buf[info_scroller_count] = 0;
	info_scroller_label.set_text (buf);
	info_scroller_label.show();

	if (info_scroller_count > ARDOUR_UI::instance()->announce_string().length()) {
		info_scroller_connection.disconnect();
	}

	return true;
}

bool
SessionDialog::on_delete_event (GdkEventAny* ev)
{
	response (RESPONSE_CANCEL);
	return ArdourDialog::on_delete_event (ev);
}

void
SessionDialog::set_provided_session (string const & name, string const & path)
{
	/* Note: path is required to be the full path to the session file, not
	   just the folder name
	*/
	new_name_entry.set_text (name);
	existing_session_chooser.set_current_folder (Glib::path_get_dirname (path));
}
