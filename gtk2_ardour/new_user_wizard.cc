/*
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2012-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2013 Michael R. Fisher <mfisher@bketech.com>
 * Copyright (C) 2015 John Emmas <john@creativepost.co.uk>
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
#include "gtk2ardour-version.h"
#endif

#include <algorithm>
#include <fcntl.h>

#include "pbd/gstdio_compat.h"

#include <gtkmm.h>

#include "pbd/basename.h"
#include "pbd/failed_constructor.h"
#include "pbd/scoped_file_descriptor.h"
#include "pbd/file_utils.h"
#include "pbd/replace_all.h"
#include "pbd/whitespace.h"
#include "pbd/openuri.h"

#include "ardour/audioengine.h"
#include "ardour/filesystem_paths.h"
#include "ardour/filename_extensions.h"
#include "ardour/plugin_manager.h"
#include "ardour/recent_sessions.h"
#include "ardour/session.h"
#include "ardour/session_state_utils.h"
#include "ardour/template_utils.h"
#include "ardour/profile.h"

#include "gtkmm2ext/utils.h"

#include "new_user_wizard.h"
#include "opts.h"
#include "splash.h"
#include "ui_config.h"
#include "pbd/i18n.h"
#include "utils.h"

using namespace std;
using namespace Gtk;
using namespace Gdk;
using namespace Glib;
using namespace PBD;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;

NewUserWizard::NewUserWizard ()
	: _splash_pushed (false)
	, config_modified (false)
	, default_dir_chooser (0)
	, monitor_via_hardware_button (string_compose (_("Use an external mixer or the hardware mixer of your audio interface.\n"
							 "%1 will play NO role in monitoring"), PROGRAM_NAME))
	, monitor_via_ardour_button (string_compose (_("Ask %1 to play back material as it is being recorded"), PROGRAM_NAME))
	, audio_page_index (-1)
	, new_user_page_index (-1)
	, default_folder_page_index (-1)
	, monitoring_page_index (-1)
	, final_page_index (-1)
{
	set_position (WIN_POS_CENTER);
	set_border_width (12);

	if (! (icon_pixbuf = ::get_icon (PROGRAM_NAME "-icon_48px"))) {
		throw failed_constructor();
	}

	list<Glib::RefPtr<Gdk::Pixbuf> > window_icons;
	Glib::RefPtr<Gdk::Pixbuf> icon;

	if ((icon = ::get_icon (PROGRAM_NAME "-icon_16px"))) {
		window_icons.push_back (icon);
	}
	if ((icon = ::get_icon (PROGRAM_NAME "-icon_22px"))) {
		window_icons.push_back (icon);
	}
	if ((icon = ::get_icon (PROGRAM_NAME "-icon_32px"))) {
		window_icons.push_back (icon);
	}
	if ((icon = ::get_icon (PROGRAM_NAME "-icon_48px"))) {
		window_icons.push_back (icon);
	}
	if (!window_icons.empty ()) {
		set_default_icon_list (window_icons);
	}

	setup_new_user_page ();
	setup_first_time_config_page ();
	setup_monitoring_choice_page ();
	setup_monitor_section_choice_page ();
	setup_final_page ();
}

NewUserWizard::~NewUserWizard ()
{
	pop_splash ();
}

bool
NewUserWizard::required ()
{
	if (Glib::file_test (ARDOUR::been_here_before_path (), Glib::FILE_TEST_EXISTS)) {
		return false;
	}

	return true;
}

void
NewUserWizard::setup_new_user_page ()
{
	Label* foomatic = manage (new Label);

	foomatic->set_markup (string_compose (_("\
<span size=\"larger\">%1 is a digital audio workstation. You can use it to \
record, edit and mix multi-track audio. You can produce your \
own CDs, mix video soundtracks, or experiment with new \
ideas about music and sound. \
\n\n\
There are a few things that need to be configured before you start \
using the program.</span> \
"), PROGRAM_NAME));
	foomatic->set_justify (JUSTIFY_FILL);
	foomatic->set_line_wrap ();

	VBox* vbox = manage (new VBox);
	vbox->set_border_width (24);
	vbox->pack_start (*foomatic, true, true, 12);

#ifndef __APPLE__
	Label* barmatic = manage (new Label);
	barmatic->set_text (_("GUI and Font scaling:"));

	Label* bazmatic = manage (new Label);
	bazmatic->set_markup (_("<small><i>This can later be changed in Preferences &gt; Appearance.</i></small>"));

	ui_font_scale.append_text (_("100%"));
	ui_font_scale.append_text (_("150%"));
	ui_font_scale.append_text (_("200%"));
	ui_font_scale.append_text (_("250%"));
	ui_font_scale.set_active_text (_("100%"));

	HBox* hbox = manage (new HBox);
	HBox* cbox = manage (new HBox);

	hbox->pack_start (*barmatic, false, false);
	hbox->pack_start (ui_font_scale, false, false);
	cbox->pack_start (*hbox, true, false);

	vbox->pack_start (*cbox, false, false, 2);
	vbox->pack_start (*bazmatic, false, false);

	ui_font_scale.show ();
	barmatic->show ();
	bazmatic->show ();
	hbox->show ();
	cbox->show ();

	guess_default_ui_scale ();
	ui_font_scale.signal_changed ().connect (sigc::mem_fun (*this, &NewUserWizard::rescale_ui));
#endif

	foomatic->show ();
	vbox->show ();

	new_user_page_index = append_page (*vbox);
	set_page_type (*vbox, ASSISTANT_PAGE_INTRO);
	set_page_title (*vbox, string_compose (_("Welcome to %1"), PROGRAM_NAME));
	set_page_header_image (*vbox, icon_pixbuf);
	set_page_complete (*vbox, true);
}

void
NewUserWizard::rescale_ui ()
{
	int rn = ui_font_scale.get_active_row_number ();
	if (rn < 0 ) {
		return;
	}
	float ui_scale = 100 + rn * 50;
	UIConfiguration::instance ().set_font_scale (1024 * ui_scale);
	UIConfiguration::instance ().reset_dpi ();
}

void
NewUserWizard::guess_default_ui_scale ()
{
	gint width = 0;
	gint height = 0;
	GdkScreen* screen = gdk_display_get_screen (gdk_display_get_default (), 0);
	gint n_monitors = gdk_screen_get_n_monitors (screen);

	if (!screen) {
		return;
	}

	for (gint i = 0; i < n_monitors; ++i) {
		GdkRectangle rect;
		gdk_screen_get_monitor_geometry (screen, i, &rect);
		width = std::max (width, rect.width);
		height = std::max (height, rect.height);
	}

	float wx = width  / 1920.f;
	float hx = height / 1080.f;
	float sx = std::min (wx, hx);

	if (sx < 1.25) {
		ui_font_scale.set_active (0); // 100%
	} else if (sx < 1.6) {
		ui_font_scale.set_active (1); // 150%
	} else if (sx < 2.1) {
		ui_font_scale.set_active (2); // 200%
	} else {
		ui_font_scale.set_active (3); // 250%
	}
	rescale_ui ();
}

void
NewUserWizard::default_dir_changed ()
{
	Config->set_default_session_parent_dir (default_dir_chooser->get_filename());
	// make new session folder chooser point to the new default
	new_folder_chooser.set_current_folder (Config->get_default_session_parent_dir());
	config_changed ();
}

void
NewUserWizard::config_changed ()
{
	config_modified = true;
}

void
NewUserWizard::setup_first_time_config_page ()
{
	default_dir_chooser = manage (new FileChooserButton (string_compose (_("Default folder for %1 sessions"), PROGRAM_NAME),
							     FILE_CHOOSER_ACTION_SELECT_FOLDER));
	Gtk::Label* txt = manage (new Label);
	HBox* hbox = manage (new HBox);
	VBox* vbox = manage (new VBox);

	txt->set_markup (string_compose (_("\
Each project that you work on with %1 has its own folder.\n\
These can require a lot of disk space if you are recording audio.\n\
\n\
Where would you like new %1 sessions to be stored by default?\n\n\
<i>(You can put new sessions anywhere, this is just a default)</i>"), PROGRAM_NAME));
	txt->set_alignment (0.0, 0.0);

	vbox->set_spacing (18);
	vbox->set_border_width (24);

	hbox->pack_start (*default_dir_chooser, false, true, 8);
	vbox->pack_start (*txt, false, false);
	vbox->pack_start (*hbox, false, true);

	cerr << "set default folder to " << poor_mans_glob (Config->get_default_session_parent_dir()) << endl;
	Gtkmm2ext::add_volume_shortcuts (*default_dir_chooser);
	default_dir_chooser->set_current_folder (poor_mans_glob (Config->get_default_session_parent_dir()));
	default_dir_chooser->signal_current_folder_changed().connect (sigc::mem_fun (*this, &NewUserWizard::default_dir_changed));
	default_dir_chooser->show ();

	vbox->show_all ();

	default_folder_page_index = append_page (*vbox);
	set_page_title (*vbox, _("Default folder for new sessions"));
	set_page_header_image (*vbox, icon_pixbuf);
	set_page_type (*vbox, ASSISTANT_PAGE_CONTENT);

	/* user can just skip all these settings if they want to */

	set_page_complete (*vbox, true);
}

void
NewUserWizard::setup_monitoring_choice_page ()
{
	mon_vbox.set_spacing (18);
	mon_vbox.set_border_width (24);

	HBox* hbox = manage (new HBox);
	VBox* vbox = manage (new VBox);
	/* first button will be on by default */
	RadioButton::Group g (monitor_via_ardour_button.get_group());
	monitor_via_hardware_button.set_group (g);

	monitor_label.set_markup(_("\
While recording instruments or vocals, you probably want to listen to the\n\
signal as well as record it. This is called \"monitoring\". There are\n\
different ways to do this depending on the equipment you have and the\n\
configuration of that equipment. The two most common are presented here.\n\
Please choose whichever one is right for your setup.\n\n\
<i>(You can change this preference at any time, via the Preferences dialog)</i>\n\n\
<i>If you do not understand what this is about, just accept the default.</i>"));
	monitor_label.set_alignment (0.0, 0.0);

	vbox->set_spacing (6);

	vbox->pack_start (monitor_via_hardware_button, false, true);
	vbox->pack_start (monitor_via_ardour_button, false, true);
	hbox->pack_start (*vbox, true, true, 8);
	mon_vbox.pack_start (monitor_label, false, false);
	mon_vbox.pack_start (*hbox, false, false);

	mon_vbox.show_all ();

	monitoring_page_index = append_page (mon_vbox);
	set_page_title (mon_vbox, _("Monitoring Choices"));
	set_page_header_image (mon_vbox, icon_pixbuf);

	monitor_via_hardware_button.signal_toggled().connect (sigc::mem_fun (*this, &NewUserWizard::config_changed));
	monitor_via_ardour_button.signal_toggled().connect (sigc::mem_fun (*this, &NewUserWizard::config_changed));

	/* user could just click on "Forward" if default
	 * choice is correct.
	 */

	set_page_complete (mon_vbox, true);
}

void
NewUserWizard::setup_monitor_section_choice_page ()
{
	mon_sec_vbox.set_spacing (18);
	mon_sec_vbox.set_border_width (24);

	HBox* hbox = manage (new HBox);
	VBox* main_vbox = manage (new VBox);
	VBox* vbox;
	Label* l = manage (new Label);

	main_vbox->set_spacing (32);

	no_monitor_section_button.set_label (_("Use a Master bus directly"));
	l->set_alignment (0.0, 1.0);
	l->set_markup(_("Connect the Master bus directly to your hardware outputs. This is preferable for simple usage."));

	vbox = manage (new VBox);
	vbox->set_spacing (6);
	vbox->pack_start (no_monitor_section_button, false, true);
	vbox->pack_start (*l, false, true);

	main_vbox->pack_start (*vbox, false, false);

	use_monitor_section_button.set_label (_("Use an additional Monitor bus"));
	l = manage (new Label);
	l->set_alignment (0.0, 1.0);
	l->set_text (_("Use a Monitor bus between Master bus and hardware outputs for \n\
greater control in monitoring without affecting the mix."));

	vbox = manage (new VBox);
	vbox->set_spacing (6);
	vbox->pack_start (use_monitor_section_button, false, true);
	vbox->pack_start (*l, false, true);

	main_vbox->pack_start (*vbox, false, false);

	RadioButton::Group g (use_monitor_section_button.get_group());
	no_monitor_section_button.set_group (g);

	if (Config->get_use_monitor_bus()) {
		use_monitor_section_button.set_active (true);
	} else {
		no_monitor_section_button.set_active (true);
	}

	use_monitor_section_button.signal_toggled().connect (sigc::mem_fun (*this, &NewUserWizard::config_changed));
	no_monitor_section_button.signal_toggled().connect (sigc::mem_fun (*this, &NewUserWizard::config_changed));

	monitor_section_label.set_markup(_("<i>You can change this preference at any time via the Preferences dialog.\nYou can also add or remove the monitor section to/from any session.</i>\n\n\
<i>If you do not understand what this is about, just accept the default.</i>"));
	monitor_section_label.set_alignment (0.0, 0.0);

	hbox->pack_start (*main_vbox, true, true, 8);
	mon_sec_vbox.pack_start (*hbox, false, false);
	mon_sec_vbox.pack_start (monitor_section_label, false, false);

	mon_sec_vbox.show_all ();

	monitor_section_page_index = append_page (mon_sec_vbox);
	set_page_title (mon_sec_vbox, _("Monitor Section"));
	set_page_header_image (mon_sec_vbox, icon_pixbuf);

	/* user could just click on "Forward" if default
	 * choice is correct.
	 */

	set_page_complete (mon_sec_vbox, true);
}

void
NewUserWizard::setup_final_page ()
{
	string msg = string_compose (_("%1 is ready for use"), PROGRAM_NAME);

	Gtk::Label* final_label = manage (new Label);
	final_label->set_markup (string_compose ("<span weight=\"bold\" size=\"large\">%1</span>", msg));
	final_label->show ();

	VBox* vbox = manage (new VBox);
	vbox->pack_start (*final_label, true, true);
	vbox->show ();

	final_page_index = append_page (*vbox);
	set_page_complete (*vbox, true);
	set_page_header_image (*vbox, icon_pixbuf);
	set_page_type (*vbox, ASSISTANT_PAGE_CONFIRM);
}

void
NewUserWizard::on_cancel ()
{
	_signal_response (int (RESPONSE_CANCEL));
}

bool
NewUserWizard::on_delete_event (GdkEventAny*)
{
	_signal_response (int (RESPONSE_CLOSE));
	return true;
}

void
NewUserWizard::on_apply ()
{
	/* file-chooser button does not emit 'current_folder_changed' signal
	 * when a folder from the dropdown or the sidebar is chosen.
	 * -> explicitly poll for the dir as suggested by the gtk documentation.
	 */
	if (default_dir_chooser && default_dir_chooser->get_filename() != Config->get_default_session_parent_dir ()) {
		config_modified = true;
	}

	if (config_modified) {

		if (default_dir_chooser) {
			Config->set_default_session_parent_dir (default_dir_chooser->get_filename());
		}

		if (monitor_via_hardware_button.get_active()) {
			Config->set_monitoring_model (ExternalMonitoring);
		} else if (monitor_via_ardour_button.get_active()) {
			Config->set_monitoring_model (SoftwareMonitoring);
		}

		Config->set_use_monitor_bus (use_monitor_section_button.get_active());

		Config->save_state ();

	}

	{
		/* "touch" the been-here-before path now we've successfully
		   made it through the first time setup (at least)
		*/
		PBD::ScopedFileDescriptor fout (g_open (been_here_before_path ().c_str(), O_CREAT|O_TRUNC|O_RDWR, 0666));

	}

	if (ARDOUR::Profile->get_mixbus () && Config->get_copy_demo_sessions ()) {
		std::string dspd (Config->get_default_session_parent_dir());
		Searchpath ds (ARDOUR::ardour_data_search_path());
		ds.add_subdirectory_to_paths ("sessions");
		vector<string> demos;
		find_files_matching_pattern (demos, ds, ARDOUR::session_archive_suffix);

		ARDOUR::RecentSessions rs;
		ARDOUR::read_recent_sessions (rs);

		for (vector<string>::iterator i = demos.begin(); i != demos.end (); ++i) {
			/* "demo-session" must be inside "demo-session.<session_archive_suffix>" */
			std::string name = basename_nosuffix (basename_nosuffix (*i));
			std::string path = Glib::build_filename (dspd, name);
			/* skip if session-dir already exists */
			if (Glib::file_test(path.c_str(), Glib::FILE_TEST_IS_DIR)) {
				continue;
			}
			/* skip sessions that are already in 'recent'.
			 * eg. a new user changed <session-default-dir> shortly after installation
			 */
			for (ARDOUR::RecentSessions::iterator r = rs.begin(); r != rs.end(); ++r) {
				if ((*r).first == name) {
					continue;
				}
			}
			try {
				PBD::FileArchive ar (*i);
				if (0 == ar.inflate (dspd)) {
					store_recent_sessions (name, path);
					info << string_compose (_("Copied Demo Session %1."), name) << endmsg;
				}
			} catch (...) {}
		}
	}

	_signal_response (int (RESPONSE_OK));
}


void
NewUserWizard::move_along_now ()
{
	on_apply ();
}

void
NewUserWizard::on_show ()
{
	Gtk::Assistant::on_show ();
	push_splash ();
}

void
NewUserWizard::on_unmap ()
{
	pop_splash ();
	Gtk::Assistant::on_unmap ();
}

void
NewUserWizard::pop_splash ()
{
	if (_splash_pushed) {
		Splash* spl = Splash::exists () ? Splash::instance() : NULL;
		if (spl) {
			spl->pop_front_for (*this);
		}
		_splash_pushed = false;
	}
}


void
NewUserWizard::push_splash ()
{
	if (Splash::exists()) {
		Splash* spl = Splash::instance();
		if (spl->is_visible()) {
			spl->pop_back_for (*this);
			_splash_pushed = true;
		}
	}
}
