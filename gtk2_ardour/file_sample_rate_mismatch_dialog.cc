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

#include "file_sample_rate_mismatch_dialog.h"
#include "utils.h"
#include <string.h>
#include "i18n.h"

using namespace Gtk;
using namespace Gdk;
using namespace std;
using namespace ARDOUR;
using namespace PBD;

FileSampleRateMismatchDialog::FileSampleRateMismatchDialog ( std::string file_name )
	: WavesDialog (_("file_sample_rate_mismatch_dialog.xml"), true, false)
    , _cancel_button ( get_waves_button ("cancel_button") )
    , _ignore_button ( get_waves_button ("ignore_button") )
    , _info_label_1 ( get_label("info_label_1") )
    , _info_label_2 ( get_label("info_label_2") )
{
	set_modal (true);
	set_resizable (false);
    
    _cancel_button.signal_clicked.connect (sigc::mem_fun (*this, &FileSampleRateMismatchDialog::cancel_button_pressed));
    _ignore_button.signal_clicked.connect (sigc::mem_fun (*this, &FileSampleRateMismatchDialog::ignore_button_pressed));
    
    _info_label_1.set_text ( file_name );
    _info_label_2.set_text ( _("This audiofile's sample rate doesn't match the session sample rate!") );
    
    show_all ();
}

void
FileSampleRateMismatchDialog::on_esc_pressed ()
{
    hide ();
    response (Gtk::RESPONSE_CANCEL);
}

void
FileSampleRateMismatchDialog::on_enter_pressed ()
{
    hide ();
    response (Gtk::RESPONSE_ACCEPT);
}

void
FileSampleRateMismatchDialog::cancel_button_pressed (WavesButton*)
{
    hide ();
    response (Gtk::RESPONSE_CANCEL);
}

void
FileSampleRateMismatchDialog::ignore_button_pressed (WavesButton*)
{
    hide ();
    response (Gtk::RESPONSE_ACCEPT);
}

FileSampleRateMismatchDialog::~FileSampleRateMismatchDialog ()
{
}
