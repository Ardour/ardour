#include <algorithm>

#include <gtkmm/main.h>
#include <gtkmm/filechooser.h>

#include "pbd/failed_constructor.h"
#include "pbd/file_utils.h"
#include "pbd/filesystem.h"

#include "ardour/filesystem_paths.h"
#include "ardour/recent_sessions.h"
#include "ardour/session.h"
#include "ardour/session_state_utils.h"
#include "ardour/template_utils.h"

#include "startup.h"
#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gdk;
using namespace Glib;
using namespace PBD;
using namespace ARDOUR;

ArdourStartup* ArdourStartup::the_startup = 0;

ArdourStartup::ArdourStartup ()
	: applying (false)
	, ic_new_session_button (_("Open a new session"))
	, ic_existing_session_button (_("Open an existing session"))
	, more_new_session_options_button (_("I'd like more options for this session"))
	, new_folder_chooser (FILE_CHOOSER_ACTION_SELECT_FOLDER)
{
	set_keep_above (true);
	set_position (WIN_POS_CENTER);

	sys::path icon_file;

	if (!find_file_in_search_path (ardour_search_path() + system_data_search_path(), "ardour_icon_48px.png", icon_file)) {
		throw failed_constructor();
	}

	try {
		icon_pixbuf = Gdk::Pixbuf::create_from_file (icon_file.to_string());
	}

	catch (...) {
		throw failed_constructor();
	}

	sys::path been_here_before = user_config_directory();
	been_here_before /= ".a3"; // XXXX use more specific version so we can catch upgrades
	
	if (!exists (been_here_before)) {
		// XXX touch been_here_before;
		setup_new_user_page ();
		setup_first_time_config_page ();
	} else {
		setup_initial_choice_page ();
	}

	setup_session_page ();
	setup_more_options_page ();
	setup_final_page ();

	the_startup = this;
}

ArdourStartup::~ArdourStartup ()
{
}

void
ArdourStartup::setup_new_user_page ()
{
	Label* foomatic = manage (new Label (_("\
Ardour is a digital audio workstation. You can use it to\n\
record, edit and mix multi-track audio. You can produce your\n\
own CDs, mix video soundtracks, or just experiment with new\n\
ideas about music and sound.\n\
\n\
There are a few things that need to configured before you start\n\
using the program.\
")));
	
	HBox* hbox = manage (new HBox);
	HBox* vbox = manage (new HBox);

	hbox->set_border_width (12);
	vbox->set_border_width (12);

	hbox->pack_start (*foomatic, false, true);
	vbox->pack_start (*hbox, false, true);

	foomatic->show ();
	hbox->show ();
	vbox->show ();

	append_page (*vbox);
	set_page_type (*vbox, ASSISTANT_PAGE_INTRO);
	set_page_title (*vbox, _("Welcome to Ardour"));
	set_page_header_image (*vbox, icon_pixbuf);
	set_page_complete (*vbox, true);
}

void
ArdourStartup::setup_first_time_config_page ()
{
	Gtk::FileChooserButton* fcb = manage (new FileChooserButton (_("Default session folder"), FILE_CHOOSER_ACTION_SELECT_FOLDER));
	Gtk::Label* txt = manage (new Label);
	HBox* hbox1 = manage (new HBox);
	VBox* vbox = manage (new VBox);
	
	txt->set_markup (_("\
Each project that you work on with Ardour has its own folder.\n\
These can require a lot of disk space if you are recording audio.\n\
\n\
Where would you like new Ardour sessions to be stored by default?\n\
<i>(You can put new sessions anywhere - this is just a default)</i>"));

	hbox1->set_border_width (6);
	vbox->set_border_width (6);

	hbox1->pack_start (*fcb, false, true);
	vbox->pack_start (*txt, false, true);
	vbox->pack_start (*hbox1, false, true);

	fcb->show ();
	txt->show ();
	hbox1->show ();
	vbox->show ();

	append_page (*vbox);
	set_page_title (*vbox, _("Default folder for new sessions"));
	set_page_header_image (*vbox, icon_pixbuf);
	set_page_type (*vbox, ASSISTANT_PAGE_CONTENT);

	/* user can just skip all these settings if they want to */

	set_page_complete (*vbox, true);
}

void
ArdourStartup::setup_initial_choice_page ()
{
	ic_vbox.set_spacing (6);
	ic_vbox.set_border_width (6);

	RadioButton::Group g (ic_new_session_button.get_group());
	ic_existing_session_button.set_group (g);

	ic_vbox.pack_start (ic_new_session_button);
	ic_vbox.pack_start (ic_existing_session_button);

	ic_new_session_button.show ();
	ic_existing_session_button.show ();
	ic_vbox.show ();

	append_page (ic_vbox);
	set_page_title (ic_vbox, _("What would you like to do?"));
	set_page_header_image (ic_vbox, icon_pixbuf);

	/* user could just click on "Forward" if default
	 * choice is correct.
	 */

	set_page_complete (ic_vbox, true);
}

void
ArdourStartup::setup_session_page ()
{
	session_hbox.set_border_width (12);
	session_vbox.set_border_width (12);

	session_vbox.pack_start (session_hbox, true, true);
	session_vbox.show ();
	session_hbox.show ();

	append_page (session_vbox);
}

void
ArdourStartup::setup_final_page ()
{
	final_page.set_text ("Ardour is ready for use");
	final_page.show ();
	append_page (final_page);
	set_page_complete (final_page, true);
	set_page_header_image (final_page, icon_pixbuf);
	set_page_type (final_page, ASSISTANT_PAGE_CONFIRM);
}

void
ArdourStartup::on_cancel ()
{
	exit (1);
}

void
ArdourStartup::on_close ()
{
	if (!applying) {
		exit (1);
	}
}

void
ArdourStartup::on_apply ()
{
	applying = true;

	// XXX do stuff and then ....

	gtk_main_quit ();
}

void
ArdourStartup::on_prepare (Gtk::Widget* page)
{
	if (page == &session_vbox) {
		if (ic_new_session_button.get_active()) {
			/* new session requested */
			setup_new_session_page ();
		} else {
			/* existing session requested */
			setup_existing_session_page ();
		}
	}
}

void
ArdourStartup::setup_new_session_page ()
{
	if (!session_hbox.get_children().empty()) {
		session_hbox.remove (**session_hbox.get_children().begin());
	}

	if (session_new_vbox.get_children().empty()) {
		
		HBox* hbox1 = manage (new HBox);
		Label* label1 = manage (new Label);
		
		hbox1->set_spacing (6);
		hbox1->pack_start (*label1, false, false);
		hbox1->pack_start (new_name_entry, true, true);
		
		label1->set_text (_("Session name:"));
		
		hbox1->show();
		label1->show();
		new_name_entry.show ();
		
		new_name_entry.signal_changed().connect (mem_fun (*this, &ArdourStartup::new_name_changed));
		
		HBox* hbox2 = manage (new HBox);
		Label* label2 = manage (new Label);
		
		hbox2->set_spacing (6);
		hbox2->pack_start (*label2, false, false);
		hbox2->pack_start (new_folder_chooser, true, true);
		
		label2->set_text (_("Create session folder in:"));
		new_folder_chooser.set_current_folder(getenv ("HOME"));
		new_folder_chooser.set_title (_("Select folder for session"));
		
		hbox2->show();
		label2->show();
		new_folder_chooser.show ();
		
		if (is_directory (user_template_directory ())) {
			session_template_chooser.set_current_folder (user_template_directory().to_string());
		} else if (is_directory (system_template_directory ())) {
			session_template_chooser.set_current_folder (system_template_directory().to_string());
		} else {
			/* hmm, no templates ... what to do? */
		}
		
		if (is_directory (system_template_directory ())) {
			session_template_chooser.add_shortcut_folder (system_template_directory().to_string());
		}
		
		HBox* hbox3 = manage (new HBox);
		Label* label3 = manage (new Label);
		
		hbox3->set_spacing (6);
		hbox3->pack_start (*label3, false, false);
		hbox3->pack_start (session_template_chooser, true, true);
		
		label3->set_text (_("Use this template:"));
		
		hbox3->show ();
		label3->show ();
		session_template_chooser.show ();
		
		Gtk::FileFilter* template_filter = manage (new (Gtk::FileFilter));
		template_filter->add_pattern(X_("*.template"));
		session_template_chooser.set_filter (*template_filter);
		session_template_chooser.set_title (_("Select template"));
		
		
		HBox* hbox4 = manage (new HBox);
	
		hbox4->set_spacing (6);
		hbox4->pack_start (more_new_session_options_button, false, false);
		
		hbox4->show ();
		more_new_session_options_button.show ();
		more_new_session_options_button.signal_clicked().connect (mem_fun (*this, &ArdourStartup::more_new_session_options_button_clicked));
		session_new_vbox.set_spacing (12);
	
		session_new_vbox.pack_start (*hbox1, false, false);
		session_new_vbox.pack_start (*hbox2, false, false);
		session_new_vbox.pack_start (*hbox3, false, false);
		session_new_vbox.pack_start (*hbox4, false, false);
	}

	session_new_vbox.show ();
	session_hbox.pack_start (session_new_vbox, false, false);
	set_page_title (session_vbox, _("New Session"));
}

void
ArdourStartup::new_name_changed ()
{
	if (!new_name_entry.get_text().empty()) {
		set_page_complete (session_vbox, true);
	} else {
		set_page_complete (session_vbox, false);
	}
}

void
ArdourStartup::redisplay_recent_sessions ()
{
	std::vector<sys::path> session_directories;
	RecentSessionsSorter cmp;
	
	recent_session_display.set_model (Glib::RefPtr<TreeModel>(0));
	recent_session_model->clear ();

	ARDOUR::RecentSessions rs;
	ARDOUR::read_recent_sessions (rs);

	if (rs.empty()) {
		recent_session_display.set_model (recent_session_model);
		return;
	}
	//
	// sort them alphabetically
	sort (rs.begin(), rs.end(), cmp);
	
	for (ARDOUR::RecentSessions::iterator i = rs.begin(); i != rs.end(); ++i) {
	        session_directories.push_back ((*i).second);
	}
	
	for (vector<sys::path>::const_iterator i = session_directories.begin();
			i != session_directories.end(); ++i)
	{
		std::vector<sys::path> state_file_paths;
	    
		// now get available states for this session

		get_state_files_in_directory (*i, state_file_paths);

		vector<string*>* states;
		vector<const gchar*> item;
		string fullpath = (*i).to_string();
		
		/* remove any trailing / */

		if (fullpath[fullpath.length()-1] == '/') {
			fullpath = fullpath.substr (0, fullpath.length()-1);
		}

		/* check whether session still exists */
		if (!Glib::file_test(fullpath.c_str(), Glib::FILE_TEST_EXISTS)) {
			/* session doesn't exist */
			cerr << "skipping non-existent session " << fullpath << endl;
			continue;
		}		
		
		/* now get available states for this session */

		if ((states = Session::possible_states (fullpath)) == 0) {
			/* no state file? */
			continue;
		}
	  
		std::vector<string> state_file_names(get_file_names_no_extension (state_file_paths));

		Gtk::TreeModel::Row row = *(recent_session_model->append());

		row[recent_session_columns.visible_name] = Glib::path_get_basename (fullpath);
		row[recent_session_columns.fullpath] = fullpath;
		
		if (state_file_names.size() > 1) {

			// add the children

			for (std::vector<std::string>::iterator i2 = state_file_names.begin();
					i2 != state_file_names.end(); ++i2)
			{

				Gtk::TreeModel::Row child_row = *(recent_session_model->append (row.children()));

				child_row[recent_session_columns.visible_name] = *i2;
				child_row[recent_session_columns.fullpath] = fullpath;
			}
		}
	}

	recent_session_display.set_model (recent_session_model);
}

void
ArdourStartup::recent_session_row_selected ()
{
	if (recent_session_display.get_selection()->count_selected_rows() > 0) {
		set_page_complete (session_vbox, true);
	} else {
		set_page_complete (session_vbox, false);
	}
}

void
ArdourStartup::setup_existing_session_page ()
{
	if (!session_hbox.get_children().empty()) {
		session_hbox.remove (**session_hbox.get_children().begin());
	}

	if (recent_scroller.get_children().empty()) {

		recent_session_model = TreeStore::create (recent_session_columns);
		recent_session_display.set_model (recent_session_model);
		recent_session_display.append_column (_("Recent Sessions"), recent_session_columns.visible_name);
		recent_session_display.set_headers_visible (false);
		recent_session_display.get_selection()->set_mode (SELECTION_BROWSE);
		
		recent_session_display.get_selection()->signal_changed().connect (mem_fun (*this, &ArdourStartup::recent_session_row_selected));
		
		recent_scroller.add (recent_session_display);
		recent_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
		
		recent_session_display.show();
	}

	recent_scroller.show();
	redisplay_recent_sessions ();

	session_hbox.pack_start (recent_scroller, true, true);
	set_page_title (session_vbox, _("Select a session"));
	set_page_type (session_vbox, ASSISTANT_PAGE_CONFIRM);
}

void
ArdourStartup::more_new_session_options_button_clicked ()
{
	if (more_new_session_options_button.get_active()) {
		more_options_vbox.show ();
	} else {
		more_options_vbox.hide ();
	}
}

void
ArdourStartup::setup_more_options_page ()
{
	Label* foomatic = manage (new Label);
	foomatic->set_text (_("Here be more options...."));
	foomatic->show ();

	more_options_vbox.set_border_width (12);
	more_options_hbox.set_border_width (12);
	
	more_options_hbox.pack_start (*foomatic, true, true);
	more_options_vbox.pack_start (more_options_hbox, true, true);

	more_options_hbox.show ();

	/* note that more_options_vbox is NOT visible by
	 * default. this is entirely by design - this page
	 * should be skipped unless explicitly requested.
	 */
	
	append_page (more_options_vbox);
	set_page_title (more_options_vbox, _("Advanced Session Options"));
	set_page_complete (more_options_vbox, true);
}
