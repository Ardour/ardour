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

#include "waves_ambiguous_file_dialog.h"
#include "i18n.h"


WavesAmbiguousFileDialog::WavesAmbiguousFileDialog (const std::string& file, const std::vector<std::string>& radio_items)
  : WavesDialog ("waves_ambiguous_file_dialog.xml", true, false )
  , _top_label (get_label("top_label"))
  , _radio_items_home (get_box("radio_items_home"))
  , _done_button (get_waves_button ("done_button"))
{
    _done_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesAmbiguousFileDialog::on_done_button));
    _top_label.set_markup (string_compose (_("%1 has found the file <i>%2</i> in the following places:"), PROGRAM_NAME, file));
    
    for (std::size_t i = 0; i < radio_items.size(); ++i) {
        boost::shared_ptr<WavesRadioItem> ri = boost::shared_ptr<WavesRadioItem> (manage (new WavesRadioItem(radio_items[i])));
        _radio_items.push_back(ri);
        _radio_items_home.pack_start (*ri, false, false);
        ri->_button.signal_clicked.connect (sigc::mem_fun (*this, &WavesAmbiguousFileDialog::on_radio_item_clicked));
        ri->show ();
    }
    
    if ( _radio_items[0] )
        _radio_items[0]->_button.set_active_state ( Gtkmm2ext::ExplicitActive );
}

void
WavesAmbiguousFileDialog::on_radio_item_clicked (WavesButton* button)
{
    for (std::size_t i = 0; i < _radio_items.size (); ++i) {
        WavesButton* button_i = &_radio_items[i]->_button;
        button_i->set_active_state ( button == button_i ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off );
    }
}

int
WavesAmbiguousFileDialog::get_selected_num ()
{
    for (std::size_t i = 0; i < _radio_items.size (); ++i) {
        WavesButton* button_i = &_radio_items[i]->_button;
        if ( button_i->get_active () == Gtkmm2ext::ExplicitActive )
            return i;
    }
    
    return -1;
}

void
WavesAmbiguousFileDialog::on_done_button (WavesButton*)
{
	response (Gtk::RESPONSE_OK);
}

WavesAmbiguousFileDialog::WavesRadioItem::WavesRadioItem (std::string message)
: WavesUI ("waves_radio_item.xml", *this)
, _button (get_waves_button("button"))
, _label (get_label("label"))
{
    _button.set_text(message);
}