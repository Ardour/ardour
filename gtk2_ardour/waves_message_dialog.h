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

#ifndef __waves_message_dialog_h__
#define __waves_message_dialog_h__

#include "waves_dialog.h"

class WavesMessageDialog : public WavesDialog
{
public:
	WavesMessageDialog(const std::string& layout_script_file,
					   const std::string& title, 
					   const std::string& message,
					   unsigned buttons = WavesMessageDialog::BUTTON_OK);
	WavesMessageDialog(const std::string& title,
					   const std::string& message,
					   unsigned buttons = WavesMessageDialog::BUTTON_OK);

	enum
	{
	  BUTTON_OK = 1 << 2,
	  BUTTON_CLOSE = 1 << 3,
	  BUTTON_ACCEPT = 1 << 4,
	  BUTTON_CANCEL = 1 << 5,
	  BUTTON_YES = 1 << 6,
	  BUTTON_NO = 1 << 7,
	};
    
    void set_markup (std::string markup);

protected:
	void init (const std::string& title,
			   const std::string& message,
			   unsigned buttons);
   
private:
    void _on_button_clicked (WavesButton*);

	WavesButton& _ok_button;
	WavesButton& _close_button;
	WavesButton& _accept_button;
	WavesButton& _cancel_button;
	WavesButton& _yes_button;
	WavesButton& _no_button;
    Gtk::Label& _message_label;
};

#endif /* __waves_message_dialog_h__ */
