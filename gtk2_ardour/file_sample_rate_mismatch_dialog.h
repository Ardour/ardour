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

#ifndef __sample_rate_mismatch_dialog_h__
#define __sample_rate_mismatch_dialog_h__

#include "waves_dialog.h"
#include "ardour_button.h"
#include <string.h>
#include "utils.h"

class FileSampleRateMismatchDialog : public WavesDialog
{
public:
    FileSampleRateMismatchDialog(std::string file_name);
    ~FileSampleRateMismatchDialog();
    
protected:
    void on_esc_pressed ();
    void on_enter_pressed ();
    
private:
    void cancel_button_pressed (WavesButton*);
    void ignore_button_pressed (WavesButton*);
    
    WavesButton& _cancel_button;
    WavesButton& _ignore_button;
    Gtk::Label& _info_label_1;
    Gtk::Label& _info_label_2;
};

#endif /* __sample_rate_mismatch_dialog_h__ */
