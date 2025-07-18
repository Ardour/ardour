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

#include <ytkmm/filechooser.h>
#include <ytkmm/stock.h>

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

SessionDialog::SessionDialog (DialogTab initial_tab, const std::string& session_name, const std::string& session_path, const std::string& template_name, bool cancel_not_quit)
	: ArdourDialog (initial_tab == New ? _("Session Setup") : _("Recent Sessions"), true, true)
	, _initial_tab (initial_tab)
	, new_name_was_edited (false)
	, new_folder_chooser (FILE_CHOOSER_ACTION_SELECT_FOLDER)
{
	set_position (WIN_POS_CENTER);
	get_vbox()->set_spacing (6);
	get_vbox()->pack_start (_open_table, false, false);

	string image_path;
	Searchpath rc (ARDOUR::ardour_data_search_path());
	rc.add_subdirectory_to_paths ("resources");

	/* Possible update message */
	if (ARDOUR_UI::instance()->announce_string() != "") {
		_info_box.set_border_width (12);
		_info_box.set_spacing (6);

		_info_box.pack_start (info_scroller_label, false, false);

		info_scroller_count = 0;
		info_scroller_connection = Glib::signal_timeout().connect (mem_fun(*this, &SessionDialog::info_scroller_update), 50);

		ArdourButton *updates_button = manage (new ArdourButton (_("Check the website for more...")));

		updates_button->signal_clicked.connect (mem_fun(*this, &SessionDialog::updates_button_clicked));
		set_tooltip (*updates_button, _("Click to open the program website in your web browser"));

		_info_box.pack_start (*updates_button, false, false);

		_info_box.show_all ();
	}
#ifndef LIVETRAX
	/* no update message for trax, show license here */
	_open_table.attach (_info_box, 1,3, 0,1, FILL, FILL, 0, 6);
#endif
	
	new_button.set_text("NEW");
	new_button.set_name ("tab button");
	new_button.signal_button_press_event().connect (sigc::mem_fun (*this, &SessionDialog::new_button_pressed), false);
	new_button.set_tweaks(ArdourButton::Tweaks(ArdourButton::ForceFlat));

	recent_button.set_text("RECENT");
	recent_button.set_name ("tab button");
	recent_button.signal_button_press_event().connect (sigc::mem_fun (*this, &SessionDialog::recent_button_pressed), false);
	recent_button.set_tweaks(ArdourButton::Tweaks(ArdourButton::ForceFlat));

	existing_button.set_text("OPEN");
	existing_button.set_name ("tab button");
	existing_button.signal_button_press_event().connect (sigc::mem_fun (*this, &SessionDialog::existing_button_pressed), false);
	existing_button.set_tweaks(ArdourButton::Tweaks(ArdourButton::ForceFlat));

	prefs_button.set_text("SETTINGS");
	prefs_button.set_name ("tab button");
	prefs_button.signal_button_press_event().connect (sigc::mem_fun (*this, &SessionDialog::prefs_button_pressed), false);
	prefs_button.set_tweaks(ArdourButton::Tweaks(ArdourButton::ForceFlat));

	Glib::RefPtr<SizeGroup> grp = SizeGroup::create (Gtk::SIZE_GROUP_BOTH);
	grp->add_widget(new_button);
	grp->add_widget(recent_button);
	grp->add_widget(existing_button);

	int top = 0;
	int row = 0;

	if (find_file (rc, PROGRAM_NAME "-small-splash.png", image_path)) {
		Gtk::Image* image;
		if ((image = manage (new Gtk::Image (image_path))) != 0) {
			_open_table.attach (*image, 0,1,  row , row + 1, FILL, FILL); ++row;
			grp->add_widget (*image);
		}
	}

	_open_table.attach (new_button,        0,1, row, row + 1, FILL, FILL); ++row;
	_open_table.attach (recent_button,     0,1, row, row + 1, FILL, FILL); ++row;
	_open_table.attach (existing_button,   0,1, row, row + 1, FILL, FILL); ++row;

	++row;
	Label *vspacer = manage (new Label());
	vspacer->set_size_request(8, -1);
	_open_table.attach (*vspacer,          1,2, top, row, FILL,        FILL|EXPAND, 0, 0);
	_open_table.attach (_tabs,             2,3, top, row, FILL|EXPAND, FILL|EXPAND, 0, 0);

	_tabs.set_show_tabs(false);
	_tabs.set_show_border(false);

	_tabs.append_page(session_new_vbox);
	_tabs.append_page(recent_vbox);
	_tabs.append_page(existing_session_chooser);

	session_new_vbox.show_all();
	recent_vbox.show_all();
	existing_session_chooser.show_all();

	_tabs.show_all();

	cancel_button = add_button ((cancel_not_quit ? Stock::CANCEL : Stock::QUIT), RESPONSE_CANCEL);

	open_button = add_button (Stock::OPEN, RESPONSE_ACCEPT);
	open_button->signal_button_press_event().connect (sigc::mem_fun (*this, &SessionDialog::open_button_pressed), false);

	_disable_plugins.set_label (_("Safe Mode: Disable all Plugins"));
	_disable_plugins.set_can_focus ();
	_disable_plugins.set_relief (Gtk::RELIEF_NORMAL);
	_disable_plugins.set_mode (true);
	_disable_plugins.set_active (ARDOUR::Session::get_disable_all_loaded_plugins());
	_disable_plugins.set_border_width(0);
#ifndef LIVETRAX
	_disable_plugins.signal_clicked().connect (sigc::mem_fun (*this, &SessionDialog::disable_plugins_clicked));

	cancel_button->get_parent ()->remove (*cancel_button);
	open_button->get_parent ()->remove (*open_button);
	ButtonBox* bbox = manage (new HButtonBox (BUTTONBOX_DEFAULT_STYLE, 5));
	bbox->add (*cancel_button);
	bbox->add (*open_button);

	HBox* abx = manage (new HBox (false, 5));
	abx->pack_end (*bbox, false, false);
	abx->pack_start (_disable_plugins, true, true);

	get_action_area ()->add (*abx);
#endif

	if (!template_name.empty()) {
		load_template_override = template_name;
	}

	/* fill data models and show/hide accordingly */

	setup_new_session_page ();
	setup_existing_box ();
	populate_session_templates ();
	setup_untitled_session ();
	setup_recent_sessions ();

	recent_vbox.pack_start (recent_scroller, true, true);

	get_vbox()->show_all ();

	if (recent_session_model) {
		int cnt = redisplay_recent_sessions ();
		if (cnt > 0) {
			recent_scroller.show();
			recent_label.show ();
			recent_scroller.set_size_request (-1, 300);
		} else {
			recent_scroller.hide();
			recent_label.hide ();
		}
	}

	_tabs.signal_switch_page().connect (sigc::mem_fun (*this, &SessionDialog::tab_page_switched));
	disallow_idle ();
}

SessionDialog::~SessionDialog()
{
}

void
SessionDialog::on_show ()
{
	ArdourDialog::on_show ();

	_tabs.set_current_page(3); // force change
	switch (_initial_tab) {
		case New:
			_tabs.set_current_page(0);
			break;
		case Open:
			_tabs.set_current_page(2);
			break;
		default:
			_tabs.set_current_page(1);
			break;
	}
}

void
SessionDialog::tab_page_switched(GtkNotebookPage*, guint page_number)
{
	/* clang-format off */
	new_button.set_active_state      (page_number==0 ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	recent_button.set_active_state   (page_number==1 ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	existing_button.set_active_state (page_number==2 ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	prefs_button.set_active_state    (page_number==3 ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);
	/* clang-format on */

	//check the status of each tab and sensitize the 'open' button appropriately
	open_button->set_sensitive(false);
	switch (page_number) {
		case 0:
			new_name_changed();
			new_name_entry.select_region (0, -1);
			new_name_entry.grab_focus ();
			_disable_plugins.hide ();
			break;
		case 1:
			recent_session_row_selected();
			_disable_plugins.show ();
			break;
		case 2:
			existing_file_selected();
			_disable_plugins.show ();
			break;
	}
}

uint32_t
SessionDialog::meta_master_bus_profile (std::string script_path)
{
	if (!Glib::file_test (script_path, Glib::FILE_TEST_EXISTS | Glib::FILE_TEST_IS_REGULAR)) {
		return UINT32_MAX;
	}

	LuaState lua (true, true);
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
	switch (_tabs.get_current_page()) {
	case 0: {
		should_be_new = true;
		string val = new_name_entry.get_text ();
		strip_whitespace_edges (val);
		return val;
	} break;
	case 1: {
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
	} break;
	case 2: {
		/* existing session chosen from file chooser */
		should_be_new = false;
		return existing_session_chooser.get_filename ();
	} break;
	}

	return "";
}

std::string
SessionDialog::session_folder ()
{
	switch (_tabs.get_current_page()) {
		case 0:
			{
				std::string val = new_name_entry.get_text();
				strip_whitespace_edges (val);
				std::string legal_session_folder_name = legalize_for_path (val);
				return Glib::build_filename (new_folder_chooser.get_filename (), legal_session_folder_name);
			}
		case 1:
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
			}
			break;
		case 2:
			/* existing session chosen from file chooser */
			return Glib::path_get_dirname (existing_session_chooser.get_current_folder ());
		default:
			break;
	}
	assert (0);
	return "";
}

Temporal::TimeDomain
SessionDialog::session_domain () const
{
	return timebase_chooser.get_active_row_number() == 1 ? Temporal::BeatTime : Temporal::AudioTime;
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
SessionDialog::setup_existing_box ()
{
	/* Browse button */

	existing_session_chooser.set_size_request (450, 300);
	existing_session_chooser.set_current_folder(poor_mans_glob (Config->get_default_session_parent_dir()));

	FileFilter session_filter;
	session_filter.add_pattern (string_compose(X_("*%1"), ARDOUR::statefile_suffix));
	session_filter.set_name (string_compose (_("%1 sessions"), PROGRAM_NAME));
	existing_session_chooser.add_filter (session_filter);

	FileFilter archive_filter;
	archive_filter.add_pattern (string_compose(X_("*%1"), ARDOUR::session_archive_suffix));
	archive_filter.set_name (_("Session Archives"));
	existing_session_chooser.add_filter (archive_filter);

	FileFilter aaf_filter;
	aaf_filter.add_pattern (string_compose(X_("*%1"), ARDOUR::advanced_authoring_format_suffix));
	aaf_filter.set_name (_("Advanced Authoring Format (AAF)"));
	existing_session_chooser.add_filter (aaf_filter);

	FileFilter all_filter;
	all_filter.add_pattern (string_compose(X_("*%1"), ARDOUR::statefile_suffix));
	all_filter.add_pattern (string_compose(X_("*%1"), ARDOUR::session_archive_suffix));
	all_filter.add_pattern (string_compose(X_("*%1"), ARDOUR::advanced_authoring_format_suffix));
	all_filter.set_name (_("All supported files"));
	existing_session_chooser.add_filter (all_filter);

	existing_session_chooser.set_filter (session_filter);

	Gtkmm2ext::add_volume_shortcuts (existing_session_chooser);

	existing_session_chooser.signal_selection_changed().connect (mem_fun (this, &SessionDialog::existing_file_selected));
	existing_session_chooser.signal_file_activated().connect (sigc::mem_fun (*this, &SessionDialog::existing_file_activated));
}

void
SessionDialog::existing_file_selected ()
{
	if (_tabs.get_current_page()!=2) {
		//gtk filechooser is threaded; don't allow it to mess with open_button sensitivity when it's not actually visible
		return;
	}

	open_button->set_sensitive(false);

	float sr;
	SampleFormat sf;
	string pv;
	XMLNode   engine_hints ("EngineHints");

	std::string s = existing_session_chooser.get_filename ();
	if (Glib::file_test (s, Glib::FILE_TEST_IS_REGULAR)) {
		switch (Session::get_info_from_path (s, sr, sf, pv, &engine_hints)) {
			case 0: {
				//TODO: display the rate somewhere? check that our engine can open this rate?
				/* OK */
			} break;
			case -1:
				error << string_compose (_("Session file %1 does not exist"), s) << endmsg;
				return;
			break;
			case -3:
				error << string_compose (_("Session %1 is from a newer version of %2"), s, PROGRAM_NAME) << endmsg;
				return;
			break;
			default:
				error << string_compose (_("Cannot get existing session information from %1"), s) << endmsg;
				//fallthrough
		}
		open_button->set_sensitive(true);  //still potentially openable; checks for session archives, .ptf, and .aaf will have to occur later
	}
}

void
SessionDialog::session_selected ()
{
}

bool
SessionDialog::new_button_pressed (GdkEventButton*)
{
	_tabs.set_current_page(0);

	return true;
}

bool
SessionDialog::recent_button_pressed (GdkEventButton*)
{
	_tabs.set_current_page(1);

	return true;
}

bool
SessionDialog::existing_button_pressed (GdkEventButton*)
{
	_tabs.set_current_page(2);

	return true;
}

bool
SessionDialog::prefs_button_pressed (GdkEventButton*)
{
	_tabs.set_current_page(3);

	open_button->set_sensitive(false);  //do not allow to open a session from this page

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
	new_name_was_edited = false;
}

void
SessionDialog::delete_selected_template ()
{
	Gtk::TreeModel::const_iterator current_selection = template_chooser.get_selection()->get_selected ();

	if (!current_selection) {
		return;
	}

	if (!current_selection->get_value (session_template_columns.removable)) {
		ArdourMessageDialog msg (("This type of template cannot be deleted"));
		msg.run ();
		return;  //cannot delete built-in scripts
	}

	PBD::remove_directory (current_selection->get_value (session_template_columns.path));

	template_model->erase (current_selection);

	populate_session_templates ();
}
bool
SessionDialog::template_button_press (GdkEventButton* ev)
{
	if (Gtkmm2ext::Keyboard::is_context_menu_event (ev)) {
		show_template_context_menu (ev->button, ev->time);
		/* return false to select item under the mouse */
	}
	return false;
}

void
SessionDialog::show_template_context_menu (int button, int time)
{
	using namespace Gtk::Menu_Helpers;
	Gtk::Menu* menu = ARDOUR_UI::instance()->shared_popup_menu ();
	MenuList&  items = menu->items ();
	items.push_back (MenuElem (_("Delete the selected Template"), hide_return (sigc::mem_fun (*this, &SessionDialog::delete_selected_template))));
	menu->popup (button, time);
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
		row[session_template_columns.removable] = false;
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
		row[session_template_columns.removable] = true;
	}

	//Add an explicit 'Empty Template' item
	TreeModel::Row row = *template_model->prepend ();
	row[session_template_columns.name] = (_("Empty Template"));
	row[session_template_columns.path] = string();
	row[session_template_columns.description] = _("An empty session with factory default settings.\n\nSelect this option if you are importing files to mix.");
	row[session_template_columns.modified_with_short] = ("");
	row[session_template_columns.modified_with_long] = ("");
	row[session_template_columns.removable] = false;

	//auto-select the first item in the list
	Gtk::TreeModel::Row first = template_model->children()[0];
	if(first) {
		template_chooser.get_selection()->select(first);
	}
}

void
SessionDialog::setup_new_session_page ()
{
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

	//Timebase for the new session
	Label* session_domain_label = manage (new Label);
	session_domain_label->set_text (_("Default Time Domain:"));
	HBox* timebase_box = manage (new HBox);
	timebase_box->set_spacing (8);
	timebase_box->pack_start (*session_domain_label, false, false);
	timebase_box->pack_start (timebase_chooser, true, true);

	timebase_chooser.append (_("Audio Time"));
	timebase_chooser.append (_("Beat Time"));
	timebase_chooser.set_active (Config->get_preferred_time_domain() == Temporal::BeatTime ? 1 : 0);

	set_tooltip (timebase_chooser, _(
	"The time domain controls how some items on the timeline respond to tempo map editing.\n\n"
	"If you choose Beat Time, some items (like markers) will move when you change tempo.\n\n"
	"If you choose Audio Time, these items will not move when you change tempo.\n\n"
	"The time domain also affects which ruler lanes will be initially visible.\n\n"
	"You can change the session's timebase anytime in Session -> Properties."
		));

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
		template_hbox->pack_start (template_desc_frame, false, false);
	}

	//template_desc is the textview that displays the currently selected template's description
	template_desc.set_editable (false);
	template_desc.set_wrap_mode (Gtk::WRAP_WORD);
	template_desc.set_size_request (200,300);
	template_desc.set_name (X_("TextOnBackground"));
	template_desc.set_border_width (6);

	//template_chooser is the treeview showing available templates
	template_model = TreeStore::create (session_template_columns);
	template_chooser.set_model (template_model);
	template_chooser.append_column (_("Template"), session_template_columns.name);
#ifdef MIXBUS
	template_chooser.append_column (_("Modified With"), session_template_columns.modified_with_short);
	template_chooser.set_headers_visible (true);
#else
	template_chooser.set_headers_visible (false);  //there is only one column and its purpose should be obvious
#endif
	template_chooser.get_selection()->set_mode (SELECTION_SINGLE);
	template_chooser.get_selection()->signal_changed().connect (sigc::mem_fun (*this, &SessionDialog::template_row_selected));
	template_chooser.signal_button_press_event ().connect (sigc::mem_fun (*this, &SessionDialog::template_button_press), false);
	template_chooser.set_sensitive (true);
	if (UIConfiguration::instance().get_use_tooltips()) {
		template_chooser.set_tooltip_column(4); // modified_with_long
	}
	template_chooser.set_size_request (150,300);

	session_new_vbox.pack_start (*template_hbox, true, true);
#ifndef LIVETRAX
	session_new_vbox.pack_start (*timebase_box, false, true);
#endif
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
		vector<string> state_file_names = Session::possible_states (dirname);

		if (state_file_names.empty()) {
			/* no state file? */
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

#ifdef LIVETRAX
		/* check 'modified-with' */
		if (program_version.empty()) {
			continue;
		}
		if (program_version.rfind (PROGRAM_NAME, 0) != 0) {
			continue;
		}
#endif

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

	//auto-select the first item in the list
	Gtk::TreeModel::Row first = recent_session_model->children()[0];
	if(first) {
		recent_session_display.get_selection()->select(first);
	}

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
	if ((ev->type == GDK_BUTTON_PRESS) && (ev->button == 3)) {

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
SessionDialog::existing_file_activated ()
{
	std::string s = existing_session_chooser.get_filename ();
	if (Glib::file_test (s, Glib::FILE_TEST_IS_REGULAR)) {
		response (RESPONSE_ACCEPT);
	}
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
	snprintf (buf, std::min(info_scroller_count,sizeof(buf)-1), "%s", ARDOUR_UI::instance()->announce_string().c_str());
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
