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

#ifndef __Tracks__session_close_dialog__
#define __Tracks__session_close_dialog__

#include <string>
#include "waves_dialog.h"

class EngineControl;

class SessionCloseDialog : public WavesDialog {
public:
    
    SessionCloseDialog ();
    void set_top_label (std::string message);
    void set_bottom_label (std::string message);
    
private:
	WavesButton& _cancel_button;
    WavesButton& _dont_save_button;
    WavesButton& _save_button;
    
	void on_cancel(WavesButton*);
    void on_dont_save(WavesButton*);
    void on_save(WavesButton*);

    Gtk::Label& _top_label;
    Gtk::Label& _bottom_label;
};


#endif /* defined(__Tracks__session_close_dialog__) */
