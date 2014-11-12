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

#include "i18n.h"
#include "yes_no_dialog.h"

using namespace Gtk;
using namespace Gdk;
using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Pango;

namespace  {
    const size_t button_left_padding = 10;
    const size_t button_bottom_padding = 10;
    const size_t font_size = 12;
    const size_t label_top_padding = 10;
    const size_t between_button_padding = 5;
    
    size_t count_lines(const std::string& str)
    {
        std::string::const_iterator beg = str.begin();
        std::string::const_iterator end = str.end();
        std::string::size_type count = 0;
        string delimeter = "\n";
        
        while ((beg + (delimeter.size() - 1)) != end)
        {
            std::string tmp(beg, beg + delimeter.size());
            if (tmp == delimeter)
            {
                ++count;
            }
            ++beg;
        }
        return count+1;
    }
    
    int calculate_window_height (size_t current_window_height, size_t button_height, size_t font_size, size_t current_lines_number)
    {
        int label_max_height = (current_window_height - label_top_padding - button_height - button_bottom_padding);
        int max_lines_number = label_max_height / font_size;
        
        if ( current_lines_number > max_lines_number )
        {
            return current_window_height + (current_lines_number - max_lines_number) * font_size;
        } else
            return current_window_height;
    }
}


/*
 
 */
YesNoDialog::YesNoDialog (std::string window_title, std::string info_lines)
: WavesDialog ( _("yes_no_dialog.xml"), true, false )
, _yes_button ( get_waves_button ("yes_button") )
, _no_button ( get_waves_button ("no_button") )
, _info_label ( get_label("info_label") )
, _layout ( get_layout("layout") )
{
	set_modal (true);
	set_resizable (false);
    
    _info_label.set_text( info_lines );
    this->set_title(window_title);
    
    // Recalculate window height if needed
    std::size_t new_window_height = 0;
    
    size_t current_window_height;
    this->realize();
    current_window_height = this->get_allocation().get_height();
    size_t button_height = _yes_button.get_allocation().get_height();
    
    new_window_height = calculate_window_height( current_window_height, button_height, font_size, count_lines(info_lines) );
    // end recalculating
    
    // Resize window height
    if ( new_window_height > current_window_height ) {
        
        this->realize(); // must be for correct work of get_width,height functions
        
        guint layout_width, layout_height;
        layout_width = _layout.get_allocation().get_width();
        _layout.set_size_request (layout_width, new_window_height);
        
        guint button_width, button_height;
        button_width = _yes_button.get_allocation().get_width();
        button_height = _yes_button.get_allocation().get_height();
        
        _layout.put( _no_button, layout_width - button_width - button_left_padding, new_window_height - button_height - button_bottom_padding);
        _layout.put( _yes_button, layout_width - 2*button_width - button_left_padding - between_button_padding, new_window_height - button_height - button_bottom_padding);
    }
    
    _yes_button.signal_clicked.connect (sigc::mem_fun (*this, &YesNoDialog::yes_button_pressed));
    _no_button.signal_clicked.connect (sigc::mem_fun (*this, &YesNoDialog::no_button_pressed));
	show_all ();
}

void
YesNoDialog::on_esc_pressed ()
{
    hide ();
    response (Gtk::RESPONSE_NO);
}

void
YesNoDialog::on_enter_pressed ()
{
    hide ();
    response (Gtk::RESPONSE_YES);
}

void
YesNoDialog::yes_button_pressed (WavesButton*)
{
    hide ();
    response (Gtk::RESPONSE_YES);
}

void
YesNoDialog::no_button_pressed (WavesButton*)
{
    hide ();
    response (Gtk::RESPONSE_NO);
}

YesNoDialog::~YesNoDialog ()
{
}
