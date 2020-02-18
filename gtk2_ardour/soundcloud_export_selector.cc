/*
 * Copyright (C) 2013-2014 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
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
#include <gtkmm/frame.h>

#include "ardour/debug.h"
#include "ardour/soundcloud_upload.h"
#include "soundcloud_export_selector.h"

#include <pbd/error.h>
#include "pbd/openuri.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <iostream>
#include "pbd/gstdio_compat.h"

#include "pbd/i18n.h"

using namespace PBD;

#include "ardour/session_metadata.h"
#include "utils.h"

SoundcloudExportSelector::SoundcloudExportSelector () :
	  sc_table (4, 3),
	  soundcloud_username_label (_("User Email"), 1.0, 0.5),
	  soundcloud_password_label (_("Password"), 1.0, 0.5),
	  soundcloud_public_checkbox (_("Make files public")),
	  soundcloud_open_checkbox (_("Open uploaded files in browser")),
	  soundcloud_download_checkbox (_("Make files downloadable")),
	  progress_bar()
{


	soundcloud_public_checkbox.set_name ("ExportCheckbox");
	soundcloud_download_checkbox.set_name ("ExportCheckbox");
	soundcloud_username_label.set_name ("ExportFormatLabel");
	soundcloud_username_entry.set_name ("ExportFormatDisplay");
	soundcloud_password_label.set_name ("ExportFormatLabel");
	soundcloud_password_entry.set_name ("ExportFormatDisplay");

	soundcloud_username_entry.set_text (ARDOUR::SessionMetadata::Metadata()->user_email());
	soundcloud_password_entry.set_visibility (false);

	Gtk::Frame *sc_frame = manage (new Gtk::Frame);
	sc_frame->set_border_width (4);
	sc_frame->set_shadow_type (Gtk::SHADOW_ETCHED_OUT);
	sc_frame->set_name ("soundcloud_export_box");
	pack_start (*sc_frame, false, false);

	sc_table.set_border_width (4);
	sc_table.set_col_spacings (5);
	sc_table.set_row_spacings (5);
	sc_frame->add (sc_table);

	sc_table.attach ( *(Gtk::manage (new Gtk::Image (ARDOUR_UI_UTILS::get_icon (X_("soundcloud"))))) , 0, 1,  0, 2);

	sc_table.attach (soundcloud_username_label,    0, 1,  1, 2);
	sc_table.attach (soundcloud_username_entry,    1, 3,  1, 2);
	sc_table.attach (soundcloud_password_label,    0, 1,  2, 3);
	sc_table.attach (soundcloud_password_entry,    1, 3,  2, 3);
	sc_table.attach (soundcloud_public_checkbox,   2, 3,  3, 4);
	sc_table.attach (soundcloud_open_checkbox,     2, 3,  4, 5);
	sc_table.attach (soundcloud_download_checkbox, 2, 3,  5, 6);

	pack_end (progress_bar, false, false);
	sc_frame->show_all ();
}


int
SoundcloudExportSelector::do_progress_callback (double ultotal, double ulnow, const std::string &filename)
{
	DEBUG_TRACE (DEBUG::Soundcloud, string_compose ("SoundcloudExportSelector::do_progress_callback(%1, %2, %3)\n", ultotal, ulnow, filename));
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
	progress_bar.set_text (prog);


	return 0;
}
