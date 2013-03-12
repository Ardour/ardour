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

#ifndef __gtk_ardour_video_copy_dialog_h__
#define __gtk_ardour_video_copy_dialog_h__

#include <string>

#include <gtkmm.h>

#include "ardour/types.h"
#include "ardour/template_utils.h"
#include "ardour_dialog.h"

/** @class ExportVideoDialog
 *  @brief dialog box and progress report for linking and copying video-files to the session.
 */
class VideoCopyDialog : public ArdourDialog , public PBD::ScopedConnectionList
{
  public:
	/** @param infile absolute-path to the file to copy or link */
	VideoCopyDialog (ARDOUR::Session*, std::string infile);
	~VideoCopyDialog ();
	/** if set to true before calling dialog->show()
	 * the dialog will only show the progres report and
	 * start copying or linking immediatly
	 * @param destfn destination path to copy or link the infile to.
	 */
	void setup_non_interactive_copy(std::string destfn ="");
	std::string get_filename () { return outfn; }

	/*
	 * Note: it's actually 'private' function but used
	 * by the internal pthread, which only has a pointer
	 * to this instance and thus can only access public fn.
	 */
	void do_copy ();

  private:
	void on_show ();
	void abort_clicked ();
	bool aborted;
	bool autostart;
	bool finished;
	pthread_t thread;

	void launch_copy ();
	std::string infn;
	std::string outfn;

	gint progress_timeout ();
	sigc::connection p_connection;
	ssize_t p_cur;
	off_t  p_tot;

	void open_browse_dialog ();
	Gtk::Label        path_label;
	Gtk::Entry        path_entry;
	Gtk::Button       browse_button;
	Gtk::Button      *cancel_button;
	Gtk::Button       copy_button;

	Gtk::HBox*  path_hbox;
	Gtk::VBox*  progress_box;
	Gtk::Button abort_button;
	Gtk::Label  progress_label;
	Gtk::ProgressBar pbar;
};

#endif /* __gtk_ardour_video_copy_dialog_h__ */

#endif /* WITH_VIDEOTIMELINE */
