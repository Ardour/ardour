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

#ifndef __gtk_ardour_video_server_dialog_h__
#define __gtk_ardour_video_server_dialog_h__

#include <string>

#include <gtkmm.h>

#include "ardour/types.h"
#include "ardour/template_utils.h"
#include "ardour_dialog.h"

/** @class VideoServerDialog
 *  @brief dialog box to collect video-server settings
 */
class VideoServerDialog : public ArdourDialog
{
  public:
	VideoServerDialog (ARDOUR::Session*);
	~VideoServerDialog ();

	std::string get_exec_path () { return path_entry.get_text(); }
	std::string get_docroot ();
	std::string get_listenaddr () { return listenaddr_combo.get_active_text();}
	int get_listenport () { return listenport_spinner.get_value_as_int();}
	int get_cachesize () { return cachesize_spinner.get_value_as_int();}
	bool show_again () { return showagain_checkbox.get_active();}

  private:
	void on_show ();
	void open_path_dialog ();
	void open_docroot_dialog ();

	Gtk::Label        path_label;
	Gtk::Entry        path_entry;
	Gtk::Button       path_browse_button;

	Gtk::Label        docroot_label;
	Gtk::Entry        docroot_entry;
	Gtk::Button       docroot_browse_button;

	Gtk::ComboBoxText listenaddr_combo;
	Gtk::Adjustment   listenport_adjustment;
	Gtk::SpinButton   listenport_spinner;
	Gtk::Adjustment   cachesize_adjustment;
	Gtk::SpinButton   cachesize_spinner;
	Gtk::CheckButton  showagain_checkbox;
};

#endif /* __gtk_ardour_video_server_dialog_h__ */

#endif /* WITH_VIDEOTIMELINE */
