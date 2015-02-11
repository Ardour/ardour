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

#ifndef __waves_numeric_edit_dialog_h__
#define __waves_numeric_edit_dialog_h__

#include "waves_dialog.h"

class WavesNumericEditDialog : public WavesDialog
{
public:
	WavesNumericEditDialog(const std::string& layout_script_file,
                           const std::string& title);
	WavesNumericEditDialog(const std::string& title);
    
    bool on_key_press_event (GdkEventKey*);
    
    void set_top_label (std::string message);
    void set_bottom_label (std::string message);
    
    void set_count (int);
    int get_count ();
    
protected:
	void init (const std::string& title);
    
private:
    void on_button_clicked (WavesButton*);
    void on_inc_button_clicked (WavesButton*);
    void on_dec_button_clicked (WavesButton*);
    bool value_accepted ();
        
	WavesButton& _ok_button;
	WavesButton& _cancel_button;
    WavesButton& _inc_button;
    WavesButton& _dec_button;
    Gtk::Label& _top_label;
    Gtk::Label& _bottom_label;
    Gtk::Entry& _numeric_entry;
    
    int _min_count;
    int _max_count;
};

#endif /* __waves_numeric_edit_dialog_h__ */
