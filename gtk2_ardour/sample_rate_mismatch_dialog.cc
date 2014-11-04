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

#include "sample_rate_mismatch_dialog.h"
#include "utils.h"
#include <string.h>
#include "i18n.h"

using namespace Gtk;
using namespace Gdk;
using namespace std;
using namespace ARDOUR;
using namespace PBD;

SampleRateMismatchDialog::SampleRateMismatchDialog (ARDOUR::framecnt_t desired, std::string program_name, ARDOUR::framecnt_t actual)
	: WavesDialog (_("sample_rate_mismatch_dialog.xml"), true, false)
    , _cancel_button ( get_waves_button ("cancel_button") )
    , _accept_button ( get_waves_button ("accept_button") )
    , _info_label_1 ( get_label("info_label_1") )
    , _info_label_2 ( get_label("info_label_2") )
    , _info_label_3 ( get_label("info_label_3") )
    , _info_label_4 ( get_label("info_label_4") )
    , _info_label_5 ( get_label("info_label_5") )
{
	set_modal (true);
	set_resizable (false);
    
    _cancel_button.signal_clicked.connect (sigc::mem_fun (*this, &SampleRateMismatchDialog::cancel_button_pressed));
    _accept_button.signal_clicked.connect (sigc::mem_fun (*this, &SampleRateMismatchDialog::accept_button_pressed));
    
    _info_label_1.set_text (string_compose ( _("This session was created with a sample rate of %1 Hz, but "), desired) );
    _info_label_2.set_text (string_compose ( _("%1 is currently running at %2 Hz. If you load this session, "), program_name, actual) );
    _info_label_3.set_text ( _("device will be switched to the session sample rate value.") );
    _info_label_4.set_text ( _("If an attemp to switch the device is unsuccessful") );
    _info_label_5.set_text ( _("audio may be played at the wrong sample rate.") );
             
    show_all ();
}

void
SampleRateMismatchDialog::on_esc_pressed ()
{
    hide ();
    response (Gtk::RESPONSE_CANCEL);
}

void
SampleRateMismatchDialog::on_enter_pressed ()
{
    hide ();
    response (Gtk::RESPONSE_ACCEPT);
}

void
SampleRateMismatchDialog::cancel_button_pressed (WavesButton*)
{
    hide ();
    response (Gtk::RESPONSE_CANCEL);
}

void
SampleRateMismatchDialog::accept_button_pressed (WavesButton*)
{
    hide ();
    response (Gtk::RESPONSE_ACCEPT);
}

SampleRateMismatchDialog::~SampleRateMismatchDialog ()
{
}
