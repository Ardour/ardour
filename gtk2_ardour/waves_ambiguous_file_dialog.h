/*
    Copyright (C) 2005-2006 Paul Davis

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

#ifndef __waves_ambiguous_file_dialog_h__
#define __waves_ambiguous_file_dialog_h__

#include <string>
#include <vector>

#include <sigc++/signal.h>
#include "waves_dialog.h"

class WavesAmbiguousFileDialog : public WavesDialog
{
public:
	WavesAmbiguousFileDialog (const std::string& file, const std::vector<std::string>& radio_items);
    int get_selected_num ();

private:
    Gtk::Label& _top_label;
    Gtk::Box& _radio_items_home;
	WavesButton& _done_button;

	void on_done_button (WavesButton*);
    void on_radio_item_clicked (WavesButton* button);
    
    class WavesRadioItem : public Gtk::EventBox, public WavesUI
    {
    public:
        WavesRadioItem (std::string message);
        WavesButton& _button;
        
    private:
        Gtk::Label& _label;
    };
    
    std::vector<boost::shared_ptr<WavesRadioItem> > _radio_items;
};

#endif // __waves_ambiguous_file_dialog_h__
