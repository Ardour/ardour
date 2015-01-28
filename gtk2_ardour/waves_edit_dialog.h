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

#ifndef __waves_edit_dialog_h__
#define __waves_edit_dialog_h__

#include "waves_dialog.h"

class WavesEditDialog : public WavesDialog
{
public:
	WavesEditDialog(const std::string& layout_script_file,
					   const std::string& title, 
					   const std::string& message);
	WavesEditDialog(const std::string& title,
					   const std::string& message);
    
    void set_entry_text (const std::string& message);
    std::string get_entry_text ();

protected:
	void init (const std::string& title,
			   const std::string& message);
   
private:
    void _on_button_clicked (WavesButton*);

	WavesButton& _ok_button;
	WavesButton& _cancel_button;
    Gtk::Label& _message_label;
    Gtk::Entry& _name_entry;
};

#endif /* __waves_edit_dialog_h__ */
