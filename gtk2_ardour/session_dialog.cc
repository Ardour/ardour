/*
    Copyright (C) 2013 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <fstream>
#include <algorithm>

#include <gtkmm/filechooser.h>

#include "pbd/failed_constructor.h"
#include "pbd/file_utils.h"
#include "pbd/replace_all.h"
#include "pbd/whitespace.h"
#include "pbd/stacktrace.h"
#include "pbd/openuri.h"

#include "ardour/audioengine.h"
#include "ardour/filesystem_paths.h"
#include "ardour/recent_sessions.h"
#include "ardour/session.h"
#include "ardour/session_state_utils.h"
#include "ardour/template_utils.h"
#include "ardour/filename_extensions.h"

#include "ardour_ui.h"
#include "session_dialog.h"
#include "opts.h"
#include "engine_dialog.h"
#include "i18n.h"
#include "utils.h"

using namespace std;
using namespace Gtk;
using namespace Gdk;
using namespace Glib;
using namespace PBD;
using namespace ARDOUR;

static string poor_mans_glob (string path)
{
	string copy = path;
	replace_all (copy, "~", Glib::get_home_dir());
	return copy;
}

SessionDialog::SessionDialog (bool require_new, const std::string& session_name, const std::string& session_path, const std::string& template_name, bool cancel_not_quit)
	: ArdourDialog (_("Session Setup"), true, true)
	, new_only (require_new)
	, _provided_session_name (session_name)
	, _provided_session_path (session_path)
	, new_folder_chooser (FILE_CHOOSER_ACTION_SELECT_FOLDER)
	, more_new_session_options_button (_("Advanced options ..."))
	, _output_limit_count_adj (1, 0, 100, 1, 10, 0)
	, _input_limit_count_adj (1, 0, 100, 1, 10, 0)
	, _master_bus_channel_count_adj (2, 0, 100, 1, 10, 0)
	, _existing_session_chooser_used (false)
{
	set_keep_above (true);
	set_position (WIN_POS_CENTER);
	get_vbox()->set_spacing (6);

	cancel_button = add_button ((cancel_not_quit ? Stock::CANCEL : Stock::QUIT), RESPONSE_CANCEL);
	back_button = add_button (Stock::GO_BACK, RESPONSE_NO);
	open_button = add_button (Stock::OPEN, RESPONSE_ACCEPT);

	back_button->signal_button_press_event().connect (sigc::mem_fun (*this, &SessionDialog::back_button_pressed), false);

	open_button->set_sensitive (false);
	back_button->set_sensitive (false);

	/* this is where announcements will be displayed, but it may be empty
	 * and invisible most of the time.
	 */

	info_frame.set_shadow_type(SHADOW_ETCHED_OUT);
	info_frame.set_no_show_all (true);
	info_frame.set_border_width (12);
	get_vbox()->pack_start (info_frame, false, false);

	setup_new_session_page ();
	
	if (!new_only) {
		setup_initial_choice_box ();
		get_vbox()->pack_start (ic_vbox, true, true);
	} else {
		get_vbox()->pack_start (session_new_vbox, true, true);
	}
	
	if (!template_name.empty()) {
		use_template_button.set_active (false);
		load_template_override = template_name;
	}
	
	get_vbox()->show_all ();

	/* fill data models and how/hide accordingly */

	populate_session_templates ();

	if (!template_model->children().empty()) {
		use_template_button.show();
		template_chooser.show ();
	} else {
		use_template_button.hide();
		template_chooser.hide ();
	}

	if (recent_session_model) {
		int cnt = redisplay_recent_sessions ();
		if (cnt > 0) {
			recent_scroller.show();
			recent_label.show ();
			
			if (cnt > 4) {
				recent_scroller.set_size_request (-1, 300);
			}
		} else {
			recent_scroller.hide();
			recent_label.hide ();
		}
	}

	/* possibly get out of here immediately if everything is ready to go.
	   We still need to set up the whole dialog because of the way
	   ARDOUR_UI::get_session_parameters() might skip it on a first
	   pass then require it for a second pass (e.g. when there
	   is an error with session loading and we have to ask the user
	   what to do next).
	 */

	if (!session_name.empty() && !require_new) {
		response (RESPONSE_OK);
		return;
	}
}

SessionDialog::~SessionDialog()
{
}

void
SessionDialog::clear_given ()
{
	_provided_session_path = "";
	_provided_session_name = "";
}

bool
SessionDialog::use_session_template ()
{
	if (!load_template_override.empty()) {
		return true;
	}

	if (use_template_button.get_active()) {
		return true;
	}

	return false;
}

std::string
SessionDialog::session_template_name ()
{
	if (!load_template_override.empty()) {
		string the_path (ARDOUR::user_template_directory());
		return Glib::build_filename (the_path, load_template_override + ARDOUR::template_suffix);
	}

	if (use_template_button.get_active()) {
		TreeModel::iterator iter = template_chooser.get_active ();
		TreeModel::Row row = (*iter);
		string s = row[session_template_columns.path];
		return s;
	} 

	return string();
}

std::string
SessionDialog::session_name (bool& should_be_new)
{
	if (!_provided_session_name.empty() && !new_only) {
		should_be_new = false;
		return _provided_session_name;
	}

	/* Try recent session selection */

	TreeIter iter = recent_session_display.get_selection()->get_selected();
	
	if (iter) {
		should_be_new = false;
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
	if (!_provided_session_path.empty() && !new_only) {
		return _provided_session_path;
	}

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
		std::string legal_session_folder_name = legalize_for_path (new_name_entry.get_text());
		return Glib::build_filename (new_folder_chooser.get_current_folder(), legal_session_folder_name);
	}
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

	if (find_file_in_search_path (ardour_data_search_path(), "small-splash.png", image_path)) {
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
		ARDOUR_UI::instance()->tooltips().set_tip (*updates_button, _("Click to open the program website in your web browser"));

		info_box->pack_start (*updates_button, false, false);

		info_frame.add (*info_box);
		info_box->show_all ();
		info_frame.show ();
	}

	/* recent session scroller */

	recent_label.set_no_show_all (true);
	recent_scroller.set_no_show_all (true);
	
	recent_label.set_markup (string_compose ("<span weight=\"bold\" size=\"large\">%1</span>", _("Recent Sessions")));
	
	recent_session_model = TreeStore::create (recent_session_columns);
	
	recent_session_display.set_model (recent_session_model);
	recent_session_display.append_column (_("Recent Sessions"), recent_session_columns.visible_name);
	recent_session_display.append_column (_("Sample Rate"), recent_session_columns.sample_rate);
	recent_session_display.append_column (_("Disk Format"), recent_session_columns.disk_format);
	recent_session_display.set_headers_visible (false);
	recent_session_display.get_selection()->set_mode (SELECTION_SINGLE);
	
	recent_session_display.get_selection()->signal_changed().connect (sigc::mem_fun (*this, &SessionDialog::recent_session_row_selected));
	
	recent_scroller.add (recent_session_display);
	recent_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
	recent_scroller.set_shadow_type	(Gtk::SHADOW_IN);
	
	recent_session_display.show();
	recent_session_display.signal_row_activated().connect (sigc::mem_fun (*this, &SessionDialog::recent_row_activated));
	
	centering_vbox->pack_start (recent_label, false, false, 12);
	centering_vbox->pack_start (recent_scroller, false, true);

	/* Browse button */
	
	existing_session_chooser.set_title (_("Select session file"));
	existing_session_chooser.signal_file_set().connect (sigc::mem_fun (*this, &SessionDialog::existing_session_selected));
	existing_session_chooser.set_current_folder(poor_mans_glob (Config->get_default_session_parent_dir()));
	
	FileFilter session_filter;
	session_filter.add_pattern ("*.ardour");
	session_filter.set_name (string_compose (_("%1 sessions"), PROGRAM_NAME));
	existing_session_chooser.add_filter (session_filter);
	existing_session_chooser.set_filter (session_filter);
	
#ifdef GTKOSX
	existing_session_chooser.add_shortcut_folder ("/Volumes");
#endif
	
	Label* browse_label = manage (new Label);
	browse_label->set_markup (string_compose ("<span weight=\"bold\" size=\"large\">%1</span>", _("Other Sessions")));
	
	centering_vbox->pack_start (*browse_label, false, false, 12);
	centering_vbox->pack_start (existing_session_chooser, false, false);

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
	new_name_entry.grab_focus ();
}

bool
SessionDialog::back_button_pressed (GdkEventButton*)
{
	get_vbox()->remove (session_new_vbox);
	back_button->set_sensitive (false);
	get_vbox()->pack_start (ic_vbox);

	return true;
}

void
SessionDialog::populate_session_templates ()
{
	vector<TemplateInfo> templates;

	find_session_templates (templates);

	template_model->clear ();

	for (vector<TemplateInfo>::iterator x = templates.begin(); x != templates.end(); ++x) {
		TreeModel::Row row;

		row = *(template_model->append ());

		row[session_template_columns.name] = (*x).name;
		row[session_template_columns.path] = (*x).path;
	}

	if (!templates.empty()) {
		/* select first row */
		template_chooser.set_active (0);
	}
}

void
SessionDialog::setup_new_session_page ()
{
	session_new_vbox.set_border_width (12);
	session_new_vbox.set_spacing (18);

	VBox *vbox1 = manage (new VBox);
	HBox* hbox1 = manage (new HBox);
	Label* label1 = manage (new Label);

	vbox1->set_spacing (6);

	hbox1->set_spacing (6);
	hbox1->pack_start (*label1, false, false);
	hbox1->pack_start (new_name_entry, true, true);

	label1->set_text (_("Session name:"));

	if (!ARDOUR_COMMAND_LINE::session_name.empty()) {
		new_name_entry.set_text  (Glib::path_get_basename (ARDOUR_COMMAND_LINE::session_name));
		/* name provided - they can move right along */
		open_button->set_sensitive (true);
	}

	new_name_entry.signal_changed().connect (sigc::mem_fun (*this, &SessionDialog::new_name_changed));
	new_name_entry.signal_activate().connect (sigc::mem_fun (*this, &SessionDialog::new_name_activated));

	vbox1->pack_start (*hbox1, true, true);

	/* --- */

	HBox* hbox2 = manage (new HBox);
	Label* label2 = manage (new Label);

	hbox2->set_spacing (6);
	hbox2->pack_start (*label2, false, false);
	hbox2->pack_start (new_folder_chooser, true, true);

	label2->set_text (_("Create session folder in:"));

	if (!ARDOUR_COMMAND_LINE::session_name.empty()) {
		new_folder_chooser.set_current_folder (poor_mans_glob (Glib::path_get_dirname (ARDOUR_COMMAND_LINE::session_name)));
	} else if (ARDOUR_UI::instance()->session_loaded) {
		// point the new session file chooser at the parent directory of the current session
		string session_parent_dir = Glib::path_get_dirname(ARDOUR_UI::instance()->the_session()->path());
		string::size_type last_dir_sep = session_parent_dir.rfind(G_DIR_SEPARATOR);
		session_parent_dir = session_parent_dir.substr(0, last_dir_sep);
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

#ifdef __APPLE__
	new_folder_chooser.add_shortcut_folder ("/Volumes");
#endif

	vbox1->pack_start (*hbox2, false, false);
		
	session_new_vbox.pack_start (*vbox1, false, false);

	/* --- */

	VBox *vbox2 = manage (new VBox);
	HBox* hbox3 = manage (new HBox);
	template_model = ListStore::create (session_template_columns);

	vbox2->set_spacing (6);

	VBox *vbox3 = manage (new VBox);

	vbox3->set_spacing (6);

	/* we may want to hide this and show it at various
	   times depending on the existence of templates.
	*/
	template_chooser.set_no_show_all (true);
	use_template_button.set_no_show_all (true);

	HBox* hbox4a = manage (new HBox);
	use_template_button.set_label (_("Use this template"));
		
	TreeModel::Row row = *template_model->prepend ();
	row[session_template_columns.name] = (_("no template"));
	row[session_template_columns.path] = string();
		
	hbox4a->set_spacing (6);
	hbox4a->pack_start (use_template_button, false, false);
	hbox4a->pack_start (template_chooser, true, true);
		
	template_chooser.set_model (template_model);
		
	Gtk::CellRendererText* text_renderer = Gtk::manage (new Gtk::CellRendererText);
	text_renderer->property_editable() = false;
		
	template_chooser.pack_start (*text_renderer);
	template_chooser.add_attribute (text_renderer->property_text(), session_template_columns.name);
	template_chooser.set_active (0);

	vbox3->pack_start (*hbox4a, false, false);

	/* --- */
	
	HBox* hbox5 = manage (new HBox);
	
	hbox5->set_spacing (6);
	hbox5->pack_start (more_new_session_options_button, false, false);
	
	setup_more_options_box ();
	more_new_session_options_button.add (more_options_vbox);
	
	vbox3->pack_start (*hbox5, false, false);
	hbox3->pack_start (*vbox3, true, true, 8);
	vbox2->pack_start (*hbox3, false, false);
	
	/* --- */
	
	session_new_vbox.pack_start (*vbox2, false, false);
	session_new_vbox.show_all ();
}

void
SessionDialog::new_name_changed ()
{
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
	//
	// sort them alphabetically
	sort (rs.begin(), rs.end(), cmp);

	for (ARDOUR::RecentSessions::iterator i = rs.begin(); i != rs.end(); ++i) {
		session_directories.push_back ((*i).second);
	}
	
	int session_snapshot_count = 0;

	for (vector<std::string>::const_iterator i = session_directories.begin(); i != session_directories.end(); ++i)
	{
		std::vector<std::string> state_file_paths;

		// now get available states for this session

		get_state_files_in_directory (*i, state_file_paths);

		vector<string*>* states;
		vector<const gchar*> item;
		string dirname = *i;

		/* remove any trailing / */

		if (dirname[dirname.length()-1] == '/') {
			dirname = dirname.substr (0, dirname.length()-1);
		}

		/* check whether session still exists */
		if (!Glib::file_test(dirname.c_str(), Glib::FILE_TEST_EXISTS)) {
			/* session doesn't exist */
			continue;
		}

		/* now get available states for this session */

		if ((states = Session::possible_states (dirname)) == 0) {
			/* no state file? */
			continue;
		}

		std::vector<string> state_file_names(get_file_names_no_extension (state_file_paths));

		if (state_file_names.empty()) {
			continue;
		}

		Gtk::TreeModel::Row row = *(recent_session_model->append());

		float sr;
		SampleFormat sf;
		std::string s = Glib::build_filename (dirname, state_file_names.front() + statefile_suffix);

		row[recent_session_columns.visible_name] = Glib::path_get_basename (dirname);
		row[recent_session_columns.fullpath] = dirname; /* just the dir, but this works too */
		row[recent_session_columns.tip] = Glib::Markup::escape_text (dirname);

		if (Session::get_info_from_path (s, sr, sf) == 0) {
			row[recent_session_columns.sample_rate] = rate_as_string (sr);
			switch (sf) {
			case FormatFloat:
				row[recent_session_columns.disk_format] = _("32 bit float");
				break;
			case FormatInt24:
				row[recent_session_columns.disk_format] = _("24 bit");
				break;
			case FormatInt16:
				row[recent_session_columns.disk_format] = _("16 bit");
				break;
			}
		} else {
			row[recent_session_columns.sample_rate] = "??";
			row[recent_session_columns.disk_format] = "--";
		}

		++session_snapshot_count;

		if (state_file_names.size() > 1) {

			// add the children

			for (std::vector<std::string>::iterator i2 = state_file_names.begin(); i2 != state_file_names.end(); ++i2) {

				Gtk::TreeModel::Row child_row = *(recent_session_model->append (row.children()));
				
				child_row[recent_session_columns.visible_name] = *i2;
				child_row[recent_session_columns.fullpath] = Glib::build_filename (dirname, *i2 + statefile_suffix);
				child_row[recent_session_columns.tip] = Glib::Markup::escape_text (dirname);
				
				if (Session::get_info_from_path (s, sr, sf) == 0) {
					child_row[recent_session_columns.sample_rate] = rate_as_string (sr);
					switch (sf) {
					case FormatFloat:
						child_row[recent_session_columns.disk_format] = _("32 bit float");
						break;
					case FormatInt24:
						child_row[recent_session_columns.disk_format] = _("24 bit");
						break;
					case FormatInt16:
						child_row[recent_session_columns.disk_format] = _("16 bit");
						break;
					}
				} else {
					child_row[recent_session_columns.sample_rate] = "??";
					child_row[recent_session_columns.disk_format] = "--";
				}
				

				++session_snapshot_count;
			}
		}
	}

	recent_session_display.set_tooltip_column(1); // recent_session_columns.tip 
	recent_session_display.set_model (recent_session_model);
	return session_snapshot_count;
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
SessionDialog::setup_more_options_box ()
{
	more_options_vbox.set_border_width (24);

	_output_limit_count.set_adjustment (_output_limit_count_adj);
	_input_limit_count.set_adjustment (_input_limit_count_adj);
	_master_bus_channel_count.set_adjustment (_master_bus_channel_count_adj);

	chan_count_label_1.set_text (_("channels"));
	chan_count_label_3.set_text (_("channels"));
	chan_count_label_4.set_text (_("channels"));

	chan_count_label_1.set_alignment(0,0.5);
	chan_count_label_1.set_padding(0,0);
	chan_count_label_1.set_line_wrap(false);

	chan_count_label_3.set_alignment(0,0.5);
	chan_count_label_3.set_padding(0,0);
	chan_count_label_3.set_line_wrap(false);

	chan_count_label_4.set_alignment(0,0.5);
	chan_count_label_4.set_padding(0,0);
	chan_count_label_4.set_line_wrap(false);

	bus_label.set_markup (_("<b>Busses</b>"));
	input_label.set_markup (_("<b>Inputs</b>"));
	output_label.set_markup (_("<b>Outputs</b>"));

	_master_bus_channel_count.set_flags(Gtk::CAN_FOCUS);
	_master_bus_channel_count.set_update_policy(Gtk::UPDATE_ALWAYS);
	_master_bus_channel_count.set_numeric(true);
	_master_bus_channel_count.set_digits(0);
	_master_bus_channel_count.set_wrap(false);

	_create_master_bus.set_label (_("Create master bus"));
	_create_master_bus.set_flags(Gtk::CAN_FOCUS);
	_create_master_bus.set_relief(Gtk::RELIEF_NORMAL);
	_create_master_bus.set_mode(true);
	_create_master_bus.set_active(true);
	_create_master_bus.set_border_width(0);

	advanced_table.set_row_spacings(0);
	advanced_table.set_col_spacings(0);

	_connect_inputs.set_label (_("Automatically connect to physical inputs"));
	_connect_inputs.set_flags(Gtk::CAN_FOCUS);
	_connect_inputs.set_relief(Gtk::RELIEF_NORMAL);
	_connect_inputs.set_mode(true);
	_connect_inputs.set_active(Config->get_input_auto_connect() != ManualConnect);
	_connect_inputs.set_border_width(0);

	_limit_input_ports.set_label (_("Use only"));
	_limit_input_ports.set_flags(Gtk::CAN_FOCUS);
	_limit_input_ports.set_relief(Gtk::RELIEF_NORMAL);
	_limit_input_ports.set_mode(true);
	_limit_input_ports.set_sensitive(true);
	_limit_input_ports.set_border_width(0);

	_input_limit_count.set_flags(Gtk::CAN_FOCUS);
	_input_limit_count.set_update_policy(Gtk::UPDATE_ALWAYS);
	_input_limit_count.set_numeric(true);
	_input_limit_count.set_digits(0);
	_input_limit_count.set_wrap(false);
	_input_limit_count.set_sensitive(false);

	bus_hbox.pack_start (bus_table, Gtk::PACK_SHRINK, 18);

	bus_label.set_alignment(0, 0.5);
	bus_label.set_padding(0,0);
	bus_label.set_line_wrap(false);
	bus_label.set_selectable(false);
	bus_label.set_use_markup(true);
	bus_frame.set_shadow_type(Gtk::SHADOW_NONE);
	bus_frame.set_label_align(0,0.5);
	bus_frame.add(bus_hbox);
	bus_frame.set_label_widget(bus_label);

	bus_table.set_row_spacings (0);
	bus_table.set_col_spacings (0);
	bus_table.attach (_create_master_bus, 0, 1, 0, 1, Gtk::EXPAND|Gtk::FILL, Gtk::EXPAND|Gtk::FILL, 0, 0);
	bus_table.attach (_master_bus_channel_count, 1, 2, 0, 1, Gtk::EXPAND|Gtk::FILL, Gtk::EXPAND|Gtk::FILL, 0, 0);
	bus_table.attach (chan_count_label_1, 2, 3, 0, 1, Gtk::EXPAND|Gtk::FILL, Gtk::EXPAND|Gtk::FILL, 6, 0);

	input_port_limit_hbox.pack_start(_limit_input_ports, Gtk::PACK_SHRINK, 6);
	input_port_limit_hbox.pack_start(_input_limit_count, Gtk::PACK_SHRINK, 0);
	input_port_limit_hbox.pack_start(chan_count_label_3, Gtk::PACK_SHRINK, 6);
	input_port_vbox.pack_start(_connect_inputs, Gtk::PACK_SHRINK, 0);
	input_port_vbox.pack_start(input_port_limit_hbox, Gtk::PACK_EXPAND_PADDING, 0);
	input_table.set_row_spacings(0);
	input_table.set_col_spacings(0);
	input_table.attach(input_port_vbox, 0, 1, 0, 1, Gtk::EXPAND|Gtk::FILL, Gtk::EXPAND|Gtk::FILL, 6, 6);

	input_hbox.pack_start (input_table, Gtk::PACK_SHRINK, 18);

	input_label.set_alignment(0, 0.5);
	input_label.set_padding(0,0);
	input_label.set_line_wrap(false);
	input_label.set_selectable(false);
	input_label.set_use_markup(true);
	input_frame.set_shadow_type(Gtk::SHADOW_NONE);
	input_frame.set_label_align(0,0.5);
	input_frame.add(input_hbox);
	input_frame.set_label_widget(input_label);

	_connect_outputs.set_label (_("Automatically connect outputs"));
	_connect_outputs.set_flags(Gtk::CAN_FOCUS);
	_connect_outputs.set_relief(Gtk::RELIEF_NORMAL);
	_connect_outputs.set_mode(true);
	_connect_outputs.set_active(Config->get_output_auto_connect() != ManualConnect);
	_connect_outputs.set_border_width(0);
	_limit_output_ports.set_label (_("Use only"));
	_limit_output_ports.set_flags(Gtk::CAN_FOCUS);
	_limit_output_ports.set_relief(Gtk::RELIEF_NORMAL);
	_limit_output_ports.set_mode(true);
	_limit_output_ports.set_sensitive(true);
	_limit_output_ports.set_border_width(0);
	_output_limit_count.set_flags(Gtk::CAN_FOCUS);
	_output_limit_count.set_update_policy(Gtk::UPDATE_ALWAYS);
	_output_limit_count.set_numeric(false);
	_output_limit_count.set_digits(0);
	_output_limit_count.set_wrap(false);
	_output_limit_count.set_sensitive(false);
	output_port_limit_hbox.pack_start(_limit_output_ports, Gtk::PACK_SHRINK, 6);
	output_port_limit_hbox.pack_start(_output_limit_count, Gtk::PACK_SHRINK, 0);
	output_port_limit_hbox.pack_start(chan_count_label_4, Gtk::PACK_SHRINK, 6);

	_connect_outputs_to_master.set_label (_("... to master bus"));
	_connect_outputs_to_master.set_flags(Gtk::CAN_FOCUS);
	_connect_outputs_to_master.set_relief(Gtk::RELIEF_NORMAL);
	_connect_outputs_to_master.set_mode(true);
	_connect_outputs_to_master.set_active(Config->get_output_auto_connect() == AutoConnectMaster);
	_connect_outputs_to_master.set_border_width(0);

	_connect_outputs_to_master.set_group (connect_outputs_group);
	_connect_outputs_to_physical.set_group (connect_outputs_group);

	_connect_outputs_to_physical.set_label (_("... to physical outputs"));
	_connect_outputs_to_physical.set_flags(Gtk::CAN_FOCUS);
	_connect_outputs_to_physical.set_relief(Gtk::RELIEF_NORMAL);
	_connect_outputs_to_physical.set_mode(true);
	_connect_outputs_to_physical.set_active(Config->get_output_auto_connect() == AutoConnectPhysical);
	_connect_outputs_to_physical.set_border_width(0);

	output_conn_vbox.pack_start(_connect_outputs, Gtk::PACK_SHRINK, 0);
	output_conn_vbox.pack_start(_connect_outputs_to_master, Gtk::PACK_SHRINK, 0);
	output_conn_vbox.pack_start(_connect_outputs_to_physical, Gtk::PACK_SHRINK, 0);
	output_vbox.set_border_width(6);

	output_port_vbox.pack_start(output_port_limit_hbox, Gtk::PACK_SHRINK, 0);

	output_vbox.pack_start(output_conn_vbox);
	output_vbox.pack_start(output_port_vbox);

	output_label.set_alignment(0, 0.5);
	output_label.set_padding(0,0);
	output_label.set_line_wrap(false);
	output_label.set_selectable(false);
	output_label.set_use_markup(true);
	output_frame.set_shadow_type(Gtk::SHADOW_NONE);
	output_frame.set_label_align(0,0.5);

	output_hbox.pack_start (output_vbox, Gtk::PACK_SHRINK, 18);

	output_frame.add(output_hbox);
	output_frame.set_label_widget(output_label);

	more_options_vbox.pack_start(advanced_table, Gtk::PACK_SHRINK, 0);
	more_options_vbox.pack_start(bus_frame, Gtk::PACK_SHRINK, 6);
	more_options_vbox.pack_start(input_frame, Gtk::PACK_SHRINK, 6);
	more_options_vbox.pack_start(output_frame, Gtk::PACK_SHRINK, 0);

	/* signals */

	_connect_inputs.signal_clicked().connect (sigc::mem_fun (*this, &SessionDialog::connect_inputs_clicked));
	_connect_outputs.signal_clicked().connect (sigc::mem_fun (*this, &SessionDialog::connect_outputs_clicked));
	_limit_input_ports.signal_clicked().connect (sigc::mem_fun (*this, &SessionDialog::limit_inputs_clicked));
	_limit_output_ports.signal_clicked().connect (sigc::mem_fun (*this, &SessionDialog::limit_outputs_clicked));
	_create_master_bus.signal_clicked().connect (sigc::mem_fun (*this, &SessionDialog::master_bus_button_clicked));

	/* note that more_options_vbox is "visible" by default even
	 * though it may not be displayed to the user, this is so the dialog
	 * doesn't resize.
	 */
	more_options_vbox.show_all ();
}

bool
SessionDialog::create_master_bus() const
{
	return _create_master_bus.get_active();
}

int
SessionDialog::master_channel_count() const
{
	return _master_bus_channel_count.get_value_as_int();
}

bool
SessionDialog::connect_inputs() const
{
	return _connect_inputs.get_active();
}

bool
SessionDialog::limit_inputs_used_for_connection() const
{
	return _limit_input_ports.get_active();
}

int
SessionDialog::input_limit_count() const
{
	return _input_limit_count.get_value_as_int();
}

bool
SessionDialog::connect_outputs() const
{
	return _connect_outputs.get_active();
}

bool
SessionDialog::limit_outputs_used_for_connection() const
{
	return _limit_output_ports.get_active();
}

int
SessionDialog::output_limit_count() const
{
	return _output_limit_count.get_value_as_int();
}

bool
SessionDialog::connect_outs_to_master() const
{
	return _connect_outputs_to_master.get_active();
}

bool
SessionDialog::connect_outs_to_physical() const
{
	return _connect_outputs_to_physical.get_active();
}

void
SessionDialog::connect_inputs_clicked ()
{
	_limit_input_ports.set_sensitive(_connect_inputs.get_active());

	if (_connect_inputs.get_active() && _limit_input_ports.get_active()) {
		_input_limit_count.set_sensitive(true);
	} else {
		_input_limit_count.set_sensitive(false);
	}
}

void
SessionDialog::connect_outputs_clicked ()
{
	bool const co = _connect_outputs.get_active ();
	_limit_output_ports.set_sensitive(co);
	_connect_outputs_to_master.set_sensitive(co);
	_connect_outputs_to_physical.set_sensitive(co);

	if (co && _limit_output_ports.get_active()) {
		_output_limit_count.set_sensitive(true);
	} else {
		_output_limit_count.set_sensitive(false);
	}
}

void
SessionDialog::limit_inputs_clicked ()
{
	_input_limit_count.set_sensitive(_limit_input_ports.get_active());
}

void
SessionDialog::limit_outputs_clicked ()
{
	_output_limit_count.set_sensitive(_limit_output_ports.get_active());
}

void
SessionDialog::master_bus_button_clicked ()
{
	bool const yn = _create_master_bus.get_active();

	_master_bus_channel_count.set_sensitive(yn);
	_connect_outputs_to_master.set_sensitive(yn);
}

void
SessionDialog::recent_row_activated (const Gtk::TreePath&, Gtk::TreeViewColumn*)
{
	response (RESPONSE_ACCEPT);
}

void
SessionDialog::existing_session_selected ()
{
	_existing_session_chooser_used = true;
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

