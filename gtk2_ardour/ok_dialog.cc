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
#include "ok_dialog.h"
#include <vector>

using namespace Gtk;
using namespace Gdk;
using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Pango;

namespace  {
    const size_t button_left_padding = 10;
    const size_t button_bottom_padding = 15;
    const size_t font_size = 12;
    const size_t label_top_padding = 10;

    size_t count_lines(const std::string& str)
    {
        std::string::const_iterator beg = str.begin();
        std::string::const_iterator end = str.end();
        std::string::size_type count = 0;
        std::string delimeter = "\n";
        
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
            return current_window_height + (current_lines_number - max_lines_number + 1) * font_size;
        } else
            return current_window_height;
    }
}


/*
 
 */
OkDialog::OkDialog (std::string window_title, std::string info_lines)
	: WavesDialog ( _("ok_dialog.xml"), true, false )
    , _ok_button ( get_waves_button ("ok_button") )
    , _info_label ( get_label("info_label") )
    , _layout ( get_layout("layout") ) 
{
	set_modal (true);
	set_resizable (false);
    this->set_keep_above (true);
    
    _info_label.set_text( info_lines );
    this->set_title(window_title);
    
    // Recalculate window height if needed
    std::size_t new_window_height = 0;
    
    size_t current_window_height;
    this->realize();
    current_window_height = this->get_allocation().get_height();
    size_t button_height = _ok_button.get_allocation().get_height();
    
    new_window_height = calculate_window_height( current_window_height, button_height, font_size, count_lines(info_lines) );
    // end recalculating
    
    // Resize window height
    if ( new_window_height > current_window_height ) {
        
        this->realize(); // must be for correct work of get_width,height functions
        
        guint layout_width, layout_height;
        layout_width = _layout.get_allocation().get_width();
        _layout.set_size_request (layout_width, new_window_height);
        
        guint button_width, button_height;
        button_width = _ok_button.get_allocation().get_width();
        button_height = _ok_button.get_allocation().get_height();
        
        _info_label.set_size_request( layout_width, new_window_height - button_height - button_bottom_padding);
        
        _layout.put( _ok_button, layout_width - button_width - button_left_padding, new_window_height - button_height - button_bottom_padding);
    }
    
    _ok_button.signal_clicked.connect (sigc::mem_fun (*this, &OkDialog::ok_button_pressed));
	show_all ();
}

void
OkDialog::on_esc_pressed ()
{
    hide ();
    response (Gtk::RESPONSE_OK);
}

void
OkDialog::on_enter_pressed ()
{
    hide ();
    response (Gtk::RESPONSE_OK);
}

void
OkDialog::ok_button_pressed (WavesButton*)
{
    hide ();
    response (Gtk::RESPONSE_OK);
}

OkDialog::~OkDialog ()
{
}
