/*
 * Copyright (C) 2018-2019 Damien Zammit <damien@zamaudio.com>
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

#include <pbd/error.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <iostream>

#include "pbd/gstdio_compat.h"
#include "pbd/file_utils.h"

#include "ptformat/ptformat.h"

#include "ardour/session_handle.h"

#include "gtkmm2ext/utils.h"

#include "pt_import_selector.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using namespace PBD;

PTImportSelector::PTImportSelector (PTFFormat& ptf) :
	ArdourDialog (_("Import PT Session")),
	ptimport_ptf_chooser (FILE_CHOOSER_ACTION_OPEN),
	ptimport_import_button (_("Import")),
	ptimport_cancel_button (_("Cancel"))
{
	_ptf = &ptf;

	set_size_request (800, 450);
	ptimport_import_button.set_size_request (90, 35);
	ptimport_cancel_button.set_size_request (90, 35);

	Gtk::FileFilter match_pt_filter;

	ptimport_info_text.set_editable (false);
	ptimport_info_text.set_wrap_mode (Gtk::WRAP_NONE);
	ptimport_info_text.get_buffer ()->set_text ("Select a PT session\n");
	ptimport_info_text.set_sensitive (false);

	match_pt_filter.add_pattern ("*.pt5");
	match_pt_filter.add_pattern ("*.pt6");
	match_pt_filter.add_pattern ("*.pt7");
	match_pt_filter.add_pattern ("*.pts");
	match_pt_filter.add_pattern ("*.ptf");
	match_pt_filter.add_pattern ("*.ptx");
	match_pt_filter.set_name (_("All PT sessions"));

	Gtkmm2ext::add_volume_shortcuts (ptimport_ptf_chooser);
	ptimport_ptf_chooser.add_filter (match_pt_filter);
	ptimport_ptf_chooser.set_select_multiple (false);
	//XXX ptimport_ptf_chooser.set_current_folder (dstdir);


	HBox* buttons = manage (new HBox);
	buttons->set_spacing (2);
	buttons->set_border_width (10);
	buttons->pack_start (ptimport_import_button, false, false);
	buttons->pack_start (ptimport_cancel_button, false, false);

	HBox* infobox = manage (new HBox);
	infobox->set_spacing (1);
	infobox->set_border_width (50);
	infobox->pack_start (ptimport_info_text, false, false);

	HBox* toplevel = manage (new HBox);
	toplevel->set_spacing (2);
	toplevel->set_border_width (10);
	toplevel->pack_start (ptimport_ptf_chooser, true, true);
	toplevel->pack_start (*infobox, false, false);

	get_vbox()->pack_start (*toplevel, true, true);
	get_vbox()->pack_start (*buttons, false, false);

	ptimport_ptf_chooser.signal_selection_changed ().connect (sigc::mem_fun (*this, &PTImportSelector::update_ptf));

	ptimport_import_button.set_sensitive(false);
	ptimport_cancel_button.set_sensitive(true);

	ptimport_cancel_button.signal_clicked ().connect (sigc::bind (sigc::mem_fun (*this, &Gtk::Dialog::response), RESPONSE_CANCEL));
	ptimport_import_button.signal_clicked ().connect (sigc::bind (sigc::mem_fun (*this, &Gtk::Dialog::response), RESPONSE_ACCEPT));

	show_all ();
}

void
PTImportSelector::update_ptf()
{
	if (ptimport_ptf_chooser.get_filename ().size () > 0) {
		int err = 0;
                std::string path = ptimport_ptf_chooser.get_filename ();
		bool ok = Glib::file_test(path.c_str(), Glib::FILE_TEST_IS_REGULAR | Glib::FILE_TEST_IS_SYMLINK)
				&& !Glib::file_test(path.c_str(), Glib::FILE_TEST_IS_DIR);
		if (ok) {
			err = _ptf->load (path, _session_rate);
			if (err == -1) {
				ptimport_info_text.get_buffer ()->set_text ("Cannot decrypt PT session\n");
				ptimport_import_button.set_sensitive(false);
			} else if (err == -2) {
				ptimport_info_text.get_buffer ()->set_text ("Cannot detect PT session\n");
				ptimport_import_button.set_sensitive(false);
			} else if (err == -3) {
				ptimport_info_text.get_buffer ()->set_text ("Incompatible PT version\n");
				ptimport_import_button.set_sensitive(false);
			} else if (err == -4) {
				ptimport_info_text.get_buffer ()->set_text ("Cannot parse PT session\n");
				ptimport_import_button.set_sensitive(false);
			} else {
				std::string ptinfo = string_compose (_("PT Session [ VALID ]\n\nSession Info:\n\n\nPT v%1 Session @ %2Hz\n\n%3 audio files\n%4 audio regions\n%5 active audio regions\n%6 midi regions\n%7 active midi regions\n\n"),
					(int)_ptf->version (),
					_ptf->sessionrate (),
					_ptf->audiofiles ().size (),
					_ptf->regions ().size (),
					_ptf->tracks ().size (),
					_ptf->midiregions ().size (),
					_ptf->miditracks ().size ()
				);
				if (_session_rate != _ptf->sessionrate ()) {
					ptinfo = string_compose (_("%1WARNING:\n\nSample rate mismatch,\nwill be resampling\n"), ptinfo);
				}
				ptimport_info_text.get_buffer ()->set_text (ptinfo);
				ptimport_import_button.set_sensitive(true);
			}
		}
	}
}

void
PTImportSelector::set_session (Session* s)
{
        ArdourDialog::set_session (s);
	_session_rate = s->sample_rate ();
}
