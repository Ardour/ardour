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
#include "new_plugin_preset_dialog.h"
#include "i18n.h"

using namespace std;
using namespace Gtk;

NewPluginPresetDialog::NewPluginPresetDialog (boost::shared_ptr<ARDOUR::Plugin> p)
	: ArdourDialog (_("New Preset"))
	, _replace (_("Replace existing preset with this name"))
{
	HBox* h = manage (new HBox);
	h->set_spacing (6);
	h->pack_start (*manage (new Label (_("Name of new preset"))));
	h->pack_start (_name);

	get_vbox()->set_spacing (6);
	get_vbox()->pack_start (*h);

	get_vbox()->pack_start (_replace);

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	_add = add_button (Stock::ADD, RESPONSE_ACCEPT);
	set_default_response (RESPONSE_ACCEPT);
	_name.set_activates_default(true);

	show_all ();

	_presets = p->get_presets ();

	_name.signal_changed().connect (sigc::mem_fun (*this, &NewPluginPresetDialog::setup_sensitivity));
	_replace.signal_toggled().connect (sigc::mem_fun (*this, &NewPluginPresetDialog::setup_sensitivity));

	setup_sensitivity ();
}

void
NewPluginPresetDialog::setup_sensitivity ()
{
	if (_name.get_text().empty()) {
		_replace.set_sensitive (false);
		_add->set_sensitive (false);
		return;
	}

	vector<ARDOUR::Plugin::PresetRecord>::const_iterator i = _presets.begin ();
	while (i != _presets.end() && i->label != _name.get_text()) {
		++i;
	}

	if (i != _presets.end ()) {
		_replace.set_sensitive (true);
		_add->set_sensitive (_replace.get_active ());
	} else {
		_replace.set_sensitive (false);
		_add->set_sensitive (true);
	}
}

string
NewPluginPresetDialog::name () const
{
	return _name.get_text ();
}

bool
NewPluginPresetDialog::replace () const
{
	return _replace.get_active ();
}

