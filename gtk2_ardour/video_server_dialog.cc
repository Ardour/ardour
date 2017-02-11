/*
    Copyright (C) 2010 Paul Davis
    Author: Robin Gareus <robin@gareus.org>

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
#include <cstdio>
#include <cmath>

#include <sigc++/bind.h>

#include "pbd/error.h"
#include "pbd/file_utils.h"
#include "ardour/session_directory.h"
#include "gtkmm2ext/utils.h"
#include "ardour/template_utils.h"
#include "ardour/session.h"

#ifdef interface
#undef interface
#endif

#include "video_server_dialog.h"
#include "utils_videotl.h"
#include "video_tool_paths.h"
#include "pbd/i18n.h"

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#endif

using namespace Gtk;
using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace VideoUtils;

VideoServerDialog::VideoServerDialog (Session* s)
	: ArdourDialog (_("Launch Video Server"))
	, path_label (_("Server Executable:"), Gtk::ALIGN_LEFT)
	, path_browse_button (_("Browse"))
	, docroot_label (_("Server Docroot:"), Gtk::ALIGN_LEFT)
	, docroot_browse_button (_("Browse"))
	, listenport_adjustment (1554, 1025, 65536, 1, 10, 0)
	, listenport_spinner (listenport_adjustment)
	, cachesize_adjustment (256, 32, 32768, 1, 32, 0)
	, cachesize_spinner (cachesize_adjustment)
	, showagain_checkbox (_("Don't show this dialog again. (Reset in Edit->Preferences)."))
{
	set_session (s);

	set_name ("VideoServerDialog");
	set_modal (true);
	set_skip_taskbar_hint (true);
	set_resizable (false);

	Gtk::Label* l;
	VBox* vbox = manage (new VBox);
	VBox* options_box = manage (new VBox);
	HBox* path_hbox = manage (new HBox);
	HBox* docroot_hbox = manage (new HBox);

	path_entry.set_width_chars(38);
	path_entry.set_text("/usr/bin/harvid");
	docroot_entry.set_width_chars(38);
	docroot_entry.set_text(video_get_docroot (Config));

#ifndef __APPLE__
	/* Note: on OSX icsd is not able to bind to IPv4 localhost */
	listenaddr_combo.append_text("127.0.0.1");
#endif
	listenaddr_combo.append_text("0.0.0.0");
	listenaddr_combo.set_active(0);

	std::string harvid_exe;
	if (ArdourVideoToolPaths::harvid_exe(harvid_exe)) {
		path_entry.set_text(harvid_exe);
	} else {
		PBD::warning <<
			string_compose(
					_("The external video server 'harvid' can not be found.\n"
						"The tool is included with the %1 releases from ardour.org, "
						"alternatively you can download it from http://x42.github.com/harvid/ "
						"or acquire it from your distribution.\n"
						"\n"
						"see also http://manual.ardour.org/video-timeline/setup/"
					 ), PROGRAM_NAME)
			<< endmsg;
	}

#ifdef PLATFORM_WINDOWS
	if (VideoUtils::harvid_version >= 0x000802) {
		/* empty docroot -> all drive letters */
	} else
#endif
	if (docroot_entry.get_text().empty()) {
	  std::string docroot =  Glib::path_get_dirname(_session->session_directory().root_path());
	  if ((docroot.empty() || docroot.at(docroot.length()-1) != '/')) { docroot += "/"; }
		docroot_entry.set_text(docroot);
	}

	path_hbox->pack_start (path_label, false, false, 3);
	path_hbox->pack_start (path_entry, true, true, 3);
	path_hbox->pack_start (path_browse_button, false, false, 3);

	docroot_hbox->pack_start (docroot_label, false, false, 3);
	docroot_hbox->pack_start (docroot_entry, true, true, 3);
	docroot_hbox->pack_start (docroot_browse_button, false, false, 3);

	l = manage (new Label (_("<b>Options</b>"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	l->set_use_markup ();
	options_box->pack_start (*l, false, true, 4);

	Table* t = manage (new Table (2, 3));
	t->set_spacings (4);
	options_box->pack_start (*t, true, true, 4);

	l = manage (new Label (_("Listen Address:")));
	l->set_alignment (0, 0.5);
	t->attach (*l, 0, 1, 0, 1, FILL);
	t->attach (listenaddr_combo, 1, 2, 0, 1);

	l = manage (new Label (_("Listen Port:")));
	l->set_alignment (0, 0.5);
	t->attach (*l, 0, 1, 1, 2, FILL);
	t->attach (listenport_spinner, 1, 2, 1, 2);

	l = manage (new Label (_("Cache Size:")));
	l->set_alignment (0, 0.5);
	t->attach (*l, 0, 1, 2, 3, FILL);
	t->attach (cachesize_spinner, 1, 2, 2, 3);

	l = manage (new Label (string_compose(
					_("%1 relies on an external video server for the videotimeline.\nThe server configured in Edit -> Preferences -> Video is not reachable.\nDo you want %1 to launch 'harvid' on this machine?"), PROGRAM_NAME)
				, Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	l->set_max_width_chars(80);
	l->set_line_wrap();
	vbox->pack_start (*l, true, true, 4);
	vbox->pack_start (*path_hbox, false, false);
	if (Config->get_video_advanced_setup()){
		vbox->pack_start (*docroot_hbox, false, false);
	} else {
		listenport_spinner.set_sensitive(false);
	}
	vbox->pack_start (*options_box, false, true);

	get_vbox()->set_spacing (4);
	get_vbox()->pack_start (*vbox, false, false);
	get_vbox()->pack_start (showagain_checkbox, false, false);
	showagain_checkbox.set_active(!Config->get_show_video_server_dialog());

	path_browse_button.signal_clicked().connect (sigc::mem_fun (*this, &VideoServerDialog::open_path_dialog));
	docroot_browse_button.signal_clicked().connect (sigc::mem_fun (*this, &VideoServerDialog::open_docroot_dialog));

	show_all_children ();
	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::EXECUTE, RESPONSE_ACCEPT);
}

VideoServerDialog::~VideoServerDialog ()
{
}

void
VideoServerDialog::on_show ()
{
	Dialog::on_show ();
}

void
VideoServerDialog::open_path_dialog ()
{
	Gtk::FileChooserDialog dialog(_("Set Video Server Executable"), Gtk::FILE_CHOOSER_ACTION_OPEN);
	dialog.set_filename (path_entry.get_text());

	dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	dialog.add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);

	int result = dialog.run();

	if (result == Gtk::RESPONSE_OK) {
		std::string filename = dialog.get_filename();

		if (filename.length()) {
			path_entry.set_text (filename);
		}
	}
}

void
VideoServerDialog::open_docroot_dialog ()
{
	Gtk::FileChooserDialog dialog(_("Server docroot"), Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
	dialog.set_filename (docroot_entry.get_text());

	dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	dialog.add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);

	int result = dialog.run();

	if (result == Gtk::RESPONSE_OK) {
		std::string dirname = dialog.get_filename();

		if (dirname.empty() || dirname.at(dirname.length()-1) != G_DIR_SEPARATOR) {
			dirname += "/";
		}

		if (dirname.length()) {
			docroot_entry.set_text (dirname);
		}
	}
}

std::string
VideoServerDialog::get_docroot () {
	return docroot_entry.get_text();
}
