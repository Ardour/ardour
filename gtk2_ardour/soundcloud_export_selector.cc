/* soundcloud_export_selector.cpp ***************************************************

	Adapted for Ardour by Ben Loftis, March 2012

	Licence GPL:

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.


*************************************************************************************/
#include "ardour/soundcloud_upload.h"
#include "soundcloud_export_selector.h"

#include <pbd/error.h>
#include "pbd/openuri.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <iostream>
#include <glib/gstdio.h>

#include "i18n.h"

using namespace PBD;

#include "ardour/session_metadata.h"
#include "utils.h"

SoundcloudExportSelector::SoundcloudExportSelector() :
	  sc_table (4, 3),
	  soundcloud_public_checkbox (_("Make file(s) public")),
	  soundcloud_username_label (_("User Email"), 1.0, 0.5),
	  soundcloud_password_label (_("Password"), 1.0, 0.5),
	  soundcloud_open_checkbox (_("Open uploaded files in browser")),
	  progress_bar()
{


	soundcloud_public_checkbox.set_name ("ExportCheckbox");
	soundcloud_username_label.set_name ("ExportFormatLabel");
	soundcloud_username_entry.set_name ("ExportFormatDisplay");
	soundcloud_password_label.set_name ("ExportFormatLabel");
	soundcloud_password_entry.set_name ("ExportFormatDisplay");

	soundcloud_username_entry.set_text (ARDOUR::SessionMetadata::Metadata()->user_email());
	soundcloud_password_entry.set_visibility(false);

	Gtk::Frame *sc_frame = manage(new Gtk::Frame);
	sc_frame->set_border_width(4);
	sc_frame->set_shadow_type(Gtk::SHADOW_ETCHED_OUT);
	sc_frame->set_name("soundcloud_export_box");
	pack_start(*sc_frame, false, false);

	sc_table.set_border_width(4);
	sc_table.set_col_spacings (5);
	sc_table.set_row_spacings (5);
	sc_frame->add (sc_table);

	//		sc_table.attach ( *( manage (new EventBox (::get_icon (X_("soundcloud"))))) , 0, 1,  0, 1);
	sc_table.attach ( *(Gtk::manage (new Gtk::Image (get_icon (X_("soundcloud"))))) , 0, 1,  0, 2);

	sc_table.attach (soundcloud_public_checkbox, 2, 3,  1, 2);
	sc_table.attach (soundcloud_username_label, 0, 1,  3, 4);
	sc_table.attach (soundcloud_username_entry, 1, 3,  3, 4);
	sc_table.attach (soundcloud_password_label, 0, 1,  5, 6);
	sc_table.attach (soundcloud_password_entry, 1, 3,  5, 6);
	sc_table.attach (soundcloud_open_checkbox, 2, 3,  7, 8);

	pack_end(progress_bar, false, false);
	sc_frame->show_all();
}


int
SoundcloudExportSelector::do_progress_callback(double ultotal, double ulnow, const std::string &filename)
{
	std::cerr << "SoundcloudExportSelector::do_progress_callback(" << ultotal << ", " << ulnow << ", " << filename << ")..." << std::endl; 
	if (soundcloud_cancel) {
		progress_bar.set_fraction (0);
		// cancel_button.set_label ("");
		return -1;
	}

	double fraction = 0.0;
	if (ultotal != 0) {
		fraction = ulnow / ultotal;
	}

	progress_bar.set_fraction ( fraction );

	std::string prog;
	prog = string_compose (_("%1: %2 of %3 bytes uploaded"), filename, ulnow, ultotal);
	progress_bar.set_text( prog );


	return 0;
}

