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

#ifndef __crash_recovery_dialog_h__
#define __crash_recovery_dialog_h__

#include "waves_dialog.h"

class CrashRecoveryDialog : public WavesDialog
{
public:
    CrashRecoveryDialog();
    ~CrashRecoveryDialog();
    
protected:
    void on_esc_pressed ();
    void on_enter_pressed ();
    
private:
    void ignore_button_pressed (WavesButton*);
    void recover_button_pressed (WavesButton*);
    
    WavesButton& _ignore_button;
    WavesButton& _recover_button;
    Gtk::Label& _info_label;
};

#endif /* __crash_recovery_dialog_h__ */
