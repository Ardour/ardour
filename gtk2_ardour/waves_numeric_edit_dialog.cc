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

#include "waves_numeric_edit_dialog.h"
#include "waves_message_dialog.h"

#include "i18n.h"

WavesNumericEditDialog::WavesNumericEditDialog(const std::string& layout_script_file,
                                               const std::string& title)
: WavesDialog (layout_script_file, true, false )
, _ok_button (get_waves_button ("ok_button"))
, _cancel_button (get_waves_button ("cancel_button"))
, _inc_button (get_waves_button("inc_button"))
, _dec_button (get_waves_button("dec_button"))
, _top_label (get_label("top_label"))
, _bottom_label (get_label("bottom_label"))
, _numeric_entry (get_entry("numeric_entry"))
, _min_count (xml_property (*xml_tree ()->root (), "mincount", 0))
, _max_count (xml_property (*xml_tree ()->root (), "maxcount", 10000))
{
	init (title);
}

WavesNumericEditDialog::WavesNumericEditDialog (const std::string& title)
: WavesDialog ("waves_numeric_edit_dialog.xml", true, false )
, _ok_button (get_waves_button ("ok_button"))
, _cancel_button (get_waves_button ("cancel_button"))
, _inc_button (get_waves_button("inc_button"))
, _dec_button (get_waves_button("dec_button"))
, _top_label (get_label("top_label"))
, _bottom_label (get_label("bottom_label"))
, _numeric_entry (get_entry("numeric_entry"))
, _min_count (xml_property (*xml_tree ()->root (), "mincount", 0))
, _max_count (xml_property (*xml_tree ()->root (), "maxcount", 10000))
{
	init (title);
}

void
WavesNumericEditDialog::init(const std::string& title)
{
	set_modal (true);
	set_resizable (false);
    set_keep_above (true);
    
    _ok_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesNumericEditDialog::on_button_clicked));
    _cancel_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesNumericEditDialog::on_button_clicked));
    
    set_count (1);
    _numeric_entry.select_region (0, -1);
    _numeric_entry.grab_focus();
    
    _inc_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesNumericEditDialog::on_inc_button_clicked));
    _dec_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesNumericEditDialog::on_dec_button_clicked));
    
	set_title (title);
	show_all ();
}

void
WavesNumericEditDialog::on_button_clicked (WavesButton* clicked_button)
{
	if (clicked_button == &_ok_button) {
	    if ( value_accepted() )
            response (WavesDialog::RESPONSE_DEFAULT);
        else
            return;
	} else if (clicked_button == &_cancel_button) {
	    response (Gtk::RESPONSE_CANCEL);
	}
    hide ();
}

int
WavesNumericEditDialog::get_count ()
{
    int entry_value = (int) PBD::atoi(_numeric_entry.get_text());
    entry_value = std::max (entry_value, _min_count);
    entry_value = std::min (entry_value, _max_count);
    
    return entry_value;
}

void
WavesNumericEditDialog::set_count (int counter)
{
    if ( counter < _min_count || counter > _max_count )
        return;
    
    _numeric_entry.set_text (string_compose ("%1", counter));
    _dec_button.set_sensitive (counter > _min_count);
    _inc_button.set_sensitive (counter < _max_count);
}

void
WavesNumericEditDialog::on_inc_button_clicked (WavesButton*)
{
    int counter = get_count ();
    
    set_count (counter + 1);
    _numeric_entry.select_region (0, -1);
}

void
WavesNumericEditDialog::on_dec_button_clicked (WavesButton*)
{
    int counter = get_count ();
    
    set_count (counter - 1);
    _numeric_entry.select_region (0, -1);
}

void
WavesNumericEditDialog::set_top_label (std::string message)
{
    _top_label.set_text (message);
}

void
WavesNumericEditDialog::set_bottom_label (std::string message)
{
    _bottom_label.set_text (message);
}

bool
WavesNumericEditDialog::on_key_press_event (GdkEventKey* ev)
{
    switch (ev->keyval)
    {
        case GDK_Return:
        case GDK_KP_Enter:
            if ( value_accepted () ) {
                hide ();
                response (WavesDialog::RESPONSE_DEFAULT);
            }
            return true;
                
        case GDK_Escape:
            hide ();
            response (Gtk::RESPONSE_CANCEL);
            return true;
    }
    
	return Gtk::Dialog::on_key_press_event (ev);
}

bool
WavesNumericEditDialog::value_accepted ()
{
    int entered_value = (int) PBD::atoi(_numeric_entry.get_text());
    if ( entered_value < _min_count || entered_value > _max_count ) {
        
        std::string error_msg = xml_property (*xml_tree ()->root (), "errormsg", "");
        
        if (error_msg.empty ()) {
            error_msg = string_compose("Incorrect value. Please input value between %1 and %2.", _min_count, _max_count);
        }
        
        WavesMessageDialog dialog ("Error", error_msg);
        dialog.run ();
        _numeric_entry.select_region (0, -1);
        return false;
    }
    
    return true;
}
