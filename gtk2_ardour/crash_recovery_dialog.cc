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

#include "crash_recovery_dialog.h"
#include "i18n.h"

CrashRecoveryDialog::CrashRecoveryDialog ()
	: WavesDialog (_("crash_recovery_dialog.xml"), true, false)
    , _ignore_button ( get_waves_button ("ignore_button") )
    , _recover_button ( get_waves_button ("recover_button") )
    , _info_label ( get_label("info_label") )
{
	set_modal (true);
	set_resizable (false);
    
    _ignore_button.signal_clicked.connect (sigc::mem_fun (*this, &CrashRecoveryDialog::ignore_button_pressed));
    _recover_button.signal_clicked.connect (sigc::mem_fun (*this, &CrashRecoveryDialog::recover_button_pressed));
    
    this->set_title ("Crash Recovery");
    _info_label.set_text (string_compose (_("This session appears to have been in the\n\
middle of recording when %1 or\n\
the computer was shutdown.\n\
\n\
%1 can recover any captured audio for\n\
you, or it can ignore it. Please decide\n\
what you would like to do.\n"), PROGRAM_NAME));
             
    show_all ();
}

void
CrashRecoveryDialog::on_esc_pressed ()
{
    hide ();
    response (Gtk::RESPONSE_REJECT);
}

void
CrashRecoveryDialog::on_enter_pressed ()
{
    hide ();
    response (Gtk::RESPONSE_ACCEPT);
}

void
CrashRecoveryDialog::ignore_button_pressed (WavesButton*)
{
    hide ();
    response (Gtk::RESPONSE_REJECT);
}

void
CrashRecoveryDialog::recover_button_pressed (WavesButton*)
{
    hide ();
    response (Gtk::RESPONSE_ACCEPT);
}

CrashRecoveryDialog::~CrashRecoveryDialog ()
{
}
