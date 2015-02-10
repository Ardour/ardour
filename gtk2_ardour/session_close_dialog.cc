/*
 Copyright (C) 2014 Waves Audio Ltd.
 
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

#include "session_close_dialog.h"


#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <fstream>
#include <algorithm>

#include "waves_button.h"

#include <gtkmm/filechooser.h>
#include "dbg_msg.h"
#include "utils.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gdk;
using namespace Glib;
using namespace PBD;
using namespace ARDOUR;


SessionCloseDialog::SessionCloseDialog ()
: WavesDialog (_("session_close_dialog.xml"), true, false)
, _cancel_button (get_waves_button ("cancel_button"))
, _dont_save_button (get_waves_button ("dont_save_button"))
, _save_button (get_waves_button ("save_button"))
, _top_label (get_label("top_label"))
, _bottom_label (get_label("bottom_label"))
{
	set_keep_above (true);
	set_position (WIN_POS_CENTER);
    
    _cancel_button.signal_clicked.connect (sigc::mem_fun (*this, &SessionCloseDialog::on_cancel));
    _dont_save_button.signal_clicked.connect (sigc::mem_fun (*this, &SessionCloseDialog::on_dont_save));
    _save_button.signal_clicked.connect (sigc::mem_fun (*this, &SessionCloseDialog::on_save));
}

void
SessionCloseDialog::on_cancel (WavesButton*)
{
	hide();
	response (Gtk::RESPONSE_CANCEL);
}

void
SessionCloseDialog::on_dont_save (WavesButton*)
{
	hide();
	response (Gtk::RESPONSE_NO);
}

void
SessionCloseDialog::on_save (WavesButton*)
{
	hide();
	response (Gtk::RESPONSE_YES);
}

void
SessionCloseDialog::set_top_label (std::string message)
{
    const size_t n_characters_in_line = 400 / 7; // 400 - size of the label, see session_close_dialog.xml, 7 - average width of the one character
    _top_label.set_text ( ARDOUR_UI_UTILS::split_on_lines (message, n_characters_in_line) );
}

void
SessionCloseDialog::set_bottom_label (std::string message)
{
    const size_t n_characters_in_line = 400 / 6; // 400 - size of the label, see session_close_dialog.xml, 6 - average width of the one character
    _bottom_label.set_text ( ARDOUR_UI_UTILS::split_on_lines (message,  n_characters_in_line) );
}
