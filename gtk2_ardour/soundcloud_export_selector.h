/*soundcloud_export_selector.h***********************************************

	Adapted for Ardour by Ben Loftis, March 2012

*****************************************************************************/
#pragma once

#include <string>
#include <stdio.h>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>

#include <ytkmm/box.h>
#include <ytkmm/checkbutton.h>
#include <ytkmm/entry.h>
#include <ytkmm/label.h>
#include <ytkmm/progressbar.h>
#include <ytkmm/table.h>

#include "ardour/session_handle.h"

class SoundcloudExportSelector : public Gtk::VBox, public ARDOUR::SessionHandlePtr
{
public:
	SoundcloudExportSelector ();
	int do_progress_callback (double ultotal, double ulnow, const std::string &filename);
	std::string username () { return soundcloud_username_entry.get_text (); }
	std::string password () { return soundcloud_password_entry.get_text (); }
	bool make_public  () { return soundcloud_public_checkbox.get_active (); }
	bool open_page    () { return soundcloud_open_checkbox.get_active (); }
	bool downloadable () { return soundcloud_download_checkbox.get_active (); }
	void cancel () { soundcloud_cancel = true; }

private:
	Gtk::Table  sc_table;
	Gtk::Label soundcloud_username_label;
	Gtk::Entry soundcloud_username_entry;
	Gtk::Label soundcloud_password_label;
	Gtk::Entry soundcloud_password_entry;
	Gtk::CheckButton soundcloud_public_checkbox;
	Gtk::CheckButton soundcloud_open_checkbox;
	Gtk::CheckButton soundcloud_download_checkbox;
	bool soundcloud_cancel;
	Gtk::ProgressBar progress_bar;

};

