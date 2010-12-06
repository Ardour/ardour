/*
    Copyright (C) 2010 Paul Davis
    Author: Carl Hetherington <cth@carlh.net>

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

#include <gtkmm/stock.h>
#include <gtkmm/listviewtext.h>
#include "gtkmm2ext/gui_thread.h"
#include "ardour/plugin.h"
#include "edit_plugin_presets_dialog.h"

using namespace std;
using namespace Gtk;

EditPluginPresetsDialog::EditPluginPresetsDialog (boost::shared_ptr<ARDOUR::Plugin> plugin)
	: ArdourDialog (_("Edit Presets"))
	, _plugin (plugin)
	, _list (1, false, SELECTION_MULTIPLE)
	, _delete (_("Delete"))
{
	_list.set_headers_visible (false);

	setup_list ();

	HBox* hbox = manage (new HBox);
	hbox->set_spacing (6);
	hbox->pack_start (_list);

	VBox* vbox = manage (new VBox);
	vbox->pack_start (_delete, false, false);

	hbox->pack_start (*vbox, false, false);

	get_vbox()->pack_start (*hbox);

	add_button (Stock::CLOSE, RESPONSE_ACCEPT);

	set_size_request (250, 300);
	update_sensitivity ();
	
	show_all ();

	_list.get_selection()->signal_changed().connect (sigc::mem_fun (*this, &EditPluginPresetsDialog::update_sensitivity));
	_delete.signal_clicked().connect (sigc::mem_fun (*this, &EditPluginPresetsDialog::delete_presets));

	_plugin->PresetAdded.connect (_preset_added_connection, invalidator (*this), boost::bind (&EditPluginPresetsDialog::setup_list, this), gui_context ());
	_plugin->PresetRemoved.connect (_preset_removed_connection, invalidator (*this), boost::bind (&EditPluginPresetsDialog::setup_list, this), gui_context ());
}

void
EditPluginPresetsDialog::update_sensitivity ()
{
	_delete.set_sensitive (!_list.get_selected().empty());
}

void
EditPluginPresetsDialog::delete_presets ()
{
	ListViewText::SelectionList const s = _list.get_selected ();
	for (ListViewText::SelectionList::const_iterator i = s.begin(); i != s.end(); ++i) {
		_plugin->remove_preset (_list.get_text (*i));
	}

	setup_list ();
}

void
EditPluginPresetsDialog::setup_list ()
{
	_list.clear_items ();
	
	vector<ARDOUR::Plugin::PresetRecord> presets = _plugin->get_presets ();
	for (vector<ARDOUR::Plugin::PresetRecord>::const_iterator i = presets.begin(); i != presets.end(); ++i) {
		_list.append_text (i->label);
	}
}
