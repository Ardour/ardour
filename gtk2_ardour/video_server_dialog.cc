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
#ifdef WITH_VIDEOTIMELINE

#include <cstdio>
#include <cmath>

#include <sigc++/bind.h>

#include "pbd/error.h"
#include "pbd/file_utils.h"
#include "ardour/session_directory.h"
#include "gtkmm2ext/utils.h"
#include "ardour/template_utils.h"
#include "ardour/session.h"

#include "video_server_dialog.h"
#include "i18n.h"

using namespace Gtk;
using namespace std;
using namespace PBD;
using namespace ARDOUR;

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
	set_position (Gtk::WIN_POS_MOUSE);
	set_modal (true);
	set_skip_taskbar_hint (true);
	set_resizable (false);

	Gtk::Label* l;
	VBox* vbox = manage (new VBox);
	VBox* options_box = manage (new VBox);
	HBox* path_hbox = manage (new HBox);
	HBox* docroot_hbox = manage (new HBox);

	path_entry.set_width_chars(38);
	path_browse_button.set_name ("PaddedButton");
	path_entry.set_text("/usr/bin/harvid");
	docroot_entry.set_width_chars(38);
	docroot_entry.set_text(Config->get_video_server_docroot());
	docroot_browse_button.set_name ("PaddedButton");

	listenaddr_combo.set_name ("PaddedButton");
#ifndef __APPLE__
	/* Note: on OSX icsd is not able to bind to IPv4 localhost */
	listenaddr_combo.append_text("127.0.0.1");
#endif
	listenaddr_combo.append_text("0.0.0.0");
	listenaddr_combo.set_active(0);

	std::string icsd_file_path;
	if (find_file_in_search_path (PBD::SearchPath(Glib::getenv("PATH")), X_("harvid"), icsd_file_path)) {
		path_entry.set_text(icsd_file_path);
	}
	else if (Glib::file_test(X_("C:\\Program Files\\harvid\\harvid.exe"), Glib::FILE_TEST_EXISTS)) {
		path_entry.set_text(X_("C:\\Program Files\\harvid\\harvid.exe"));
	}
	else {
		PBD::warning << _("The external video server 'harvid' can not be found, see https://github.com/x42/harvid") << endmsg;
	}


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

	l = manage (new Label (_("Ardour relies on an external Video Server for the videotimeline. The server configured in Edit -> Prefereces -> Video is not reachable. Do you want ardour to launch 'harvid' on this machine?"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
	l->set_line_wrap();
	vbox->pack_start (*l, false, true, 4);
	vbox->pack_start (*path_hbox, false, false);
	vbox->pack_start (*docroot_hbox, false, false);
	vbox->pack_start (*options_box, false, true);

	get_vbox()->set_spacing (4);
	get_vbox()->pack_start (*vbox, false, false);
	get_vbox()->pack_start (showagain_checkbox, false, false);
	showagain_checkbox.set_active(false);

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

		if (dirname.length()) {
			docroot_entry.set_text (dirname);
		}
	}
}

#endif /* WITH_VIDEOTIMELINE */
