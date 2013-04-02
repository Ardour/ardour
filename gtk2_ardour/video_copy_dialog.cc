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
#include <string>
#include <sstream>
#include <iomanip>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sigc++/bind.h>
#include <libgen.h>

#include "pbd/error.h"
#include "pbd/convert.h"
#include "gtkmm2ext/utils.h"
#include "ardour/session_directory.h"
#include "ardour/profile.h"
#include "ardour/template_utils.h"
#include "ardour/session.h"
#include "ardour_ui.h"
#include "gui_thread.h"

#include "utils_videotl.h"
#include "utils.h"
#include "opts.h"
#include "video_copy_dialog.h"
#include "i18n.h"

using namespace Gtk;
using namespace std;
using namespace PBD;
using namespace ARDOUR;

VideoCopyDialog::VideoCopyDialog (Session* s, std::string infile)
	: ArdourDialog (_("Import Video File "))
	, infn (infile)
	, path_label (_("Output File:"), Gtk::ALIGN_LEFT)
	, browse_button (_("Browse"))
	, copy_button (_("Copy/Embed"))
	, abort_button (_("Abort"))
	, progress_label ()
{
	set_session (s);
	autostart = false;

	set_name ("VideoCopyDialog");
	set_position (Gtk::WIN_POS_MOUSE);
	set_modal (true);
	set_skip_taskbar_hint (true);
	set_resizable (false);
	p_connection = sigc::connection();

	std::string dstdir = video_dest_dir(_session->session_directory().video_path(), Config->get_video_server_docroot());
	std::string dstfn  = dstdir + G_DIR_SEPARATOR + Glib::path_get_basename(infile);
	path_entry.set_text (dstfn);

	path_hbox = manage (new HBox);
	path_hbox->pack_start (path_label, false, false, 3);
	path_hbox->pack_start (path_entry, true, true, 3);
	path_hbox->pack_start (browse_button, false, false, 3);
	browse_button.set_name ("PaddedButton");
	path_entry.set_width_chars(38);

	browse_button.signal_clicked().connect (sigc::mem_fun (*this, &VideoCopyDialog::open_browse_dialog));
	copy_button.signal_clicked().connect (sigc::mem_fun (*this, &VideoCopyDialog::launch_copy));
	abort_button.signal_clicked().connect (sigc::mem_fun (*this, &VideoCopyDialog::abort_clicked));

	progress_box = manage (new VBox);
	progress_box->pack_start (progress_label, false, false);
	progress_box->pack_start (pbar, false, false);
	progress_box->pack_start (abort_button, false, false);

	get_vbox()->pack_start (*path_hbox, false, false);
	get_vbox()->pack_start (*progress_box, false, false);


	cancel_button = add_button (Stock::CANCEL, RESPONSE_CANCEL);
	get_action_area()->pack_start (copy_button, false, false);
	show_all_children ();
	progress_box->hide();
}

VideoCopyDialog::~VideoCopyDialog ()
{
}

void
VideoCopyDialog::setup_non_interactive_copy (std::string destfn)
{
	if (destfn.empty()) {
		std::string dstdir = video_dest_dir(_session->session_directory().video_path(), Config->get_video_server_docroot());
		outfn= dstdir + G_DIR_SEPARATOR + Glib::path_get_basename(infn);
	} else {
		outfn=destfn;
	}
	autostart=true;
}

void
VideoCopyDialog::on_show ()
{
	if (autostart) {
	  Glib::signal_timeout().connect_once (sigc::mem_fun(*this, &VideoCopyDialog::launch_copy), 200);
	}
	Dialog::on_show ();
}

void
VideoCopyDialog::abort_clicked ()
{
	aborted = true;
}

gint
VideoCopyDialog::progress_timeout ()
{
	if (p_tot == 0) {
		pbar.set_pulse_step(.5);
		pbar.pulse();
		return 1;
	}
	pbar.set_fraction ((double)p_cur / (double) p_tot);
	return 1;
}

void*
video_copy_thread (void *arg)
{
	VideoCopyDialog *cvd = static_cast<VideoCopyDialog*>(arg);
	cvd->do_copy();
	return 0;
}


void
VideoCopyDialog::launch_copy ()
{
	if (!autostart) {
		outfn = path_entry.get_text();
	}
	if (!confirm_video_outfn(outfn)) { return; }
	p_cur = 0; p_tot = 0;

	p_connection = Glib::signal_timeout().connect (sigc::mem_fun(*this, &VideoCopyDialog::progress_timeout), 80);

	pbar.set_size_request(300,-1);
	progress_box->show();
	path_hbox->hide();
	cancel_button->hide();
	copy_button.hide();
	aborted = false;
	finished = false;

	pthread_create(&thread, NULL, video_copy_thread ,this);
	while (!finished) {
		if (gtk_events_pending()) {
			gtk_main_iteration ();
		} else {
			usleep (10000);
		}
	}
	pthread_join(thread, NULL);

	p_connection.disconnect();

	if (aborted) {
		Gtk::Dialog::response(RESPONSE_CANCEL);
	} else {
		Gtk::Dialog::response(RESPONSE_ACCEPT);
	}
}

void
VideoCopyDialog::do_copy ()
{
	progress_label.set_text (_("Linking File."));

	unlink (outfn.c_str());

	bool try_hardlink = false; // Config->get_try_link_for_embed(); /* XXX */
	struct stat sb;
	if (lstat (infn.c_str(), &sb) == 0) {
		p_tot = sb.st_size;
		/* don't hardlink a symlink */
		if ((sb.st_mode&S_IFMT) == S_IFLNK) {
			try_hardlink = false;
			if (stat (infn.c_str(), &sb) == 0) {
				p_tot = sb.st_size;
			}
		}
	} else {
		/* Can not stat() input file */
		warning << _("Can not read input file.") << endmsg;
		aborted=true;
		finished=true;
		return;
	}

	if ( !try_hardlink || link(infn.c_str(), outfn.c_str()) ) {
		/* hard-link failed , try copy */
		progress_label.set_text (_("Copying File."));
		int infd = open (infn.c_str(), O_RDONLY);
		int outfd = open (outfn.c_str(), O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
		if (infd <0 || outfd <0) {
			if (infd != -1) close(infd);
			warning << _("Can not open files for copy.") << endmsg;
			aborted=true;
			finished=true;
			return;
		}
		char buffer[BUFSIZ];
		ssize_t nrb, ret;
		while ((nrb = read(infd, buffer, BUFSIZ)) > 0  && nrb != -1 ) {
			ret = write(outfd, buffer, nrb);
			if(ret != nrb || aborted) {
				warning << _("File copy failed.") << endmsg;
				unlink(outfn.c_str());
				aborted=true;
				finished=true;
				return;
			}
			p_cur+=ret;
		}
	}
	finished=true;
	return;
}

void
VideoCopyDialog::open_browse_dialog ()
{
	Gtk::FileChooserDialog dialog(_("Video File Copy Destination"), Gtk::FILE_CHOOSER_ACTION_SAVE);
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
#endif /* WITH_VIDEOTIMELINE */
