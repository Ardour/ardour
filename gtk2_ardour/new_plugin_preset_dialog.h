/*
 * Copyright (C) 2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <gtkmm/entry.h>
#include <gtkmm/checkbutton.h>
#include "ardour/plugin.h"
#include "ardour_dialog.h"

class NewPluginPresetDialog : public ArdourDialog
{
public:
	NewPluginPresetDialog (boost::shared_ptr<ARDOUR::Plugin>, std::string, bool fav = false);

	std::string name () const;
	bool replace () const;

private:
	void setup_sensitivity ();

	Gtk::Entry _name;
	Gtk::CheckButton _replace;
	Gtk::Button* _add;
	std::vector<ARDOUR::Plugin::PresetRecord> _presets;
};
