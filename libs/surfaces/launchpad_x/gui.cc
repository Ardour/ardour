/*
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
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

#include <gtkmm/alignment.h>
#include <gtkmm/label.h>
#include <gtkmm/liststore.h>

#include "pbd/unwind.h"
#include "pbd/strsplit.h"
#include "pbd/file_utils.h"

#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"

#include "ardour/audioengine.h"
#include "ardour/filesystem_paths.h"
#include "ardour/parameter_descriptor.h"

#include "lpx.h"
#include "gui.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace ARDOUR;
using namespace ArdourSurface;
using namespace Gtk;
using namespace Gtkmm2ext;

void*
LaunchPadX::get_gui () const
{
	if (!_gui) {
		const_cast<LaunchPadX*>(this)->build_gui ();
	}

	static_cast<Gtk::VBox*>(_gui)->show_all();
	return _gui;
}

void
LaunchPadX::tear_down_gui ()
{
	if (_gui) {
		Gtk::Widget *w = static_cast<Gtk::VBox*>(_gui)->get_parent();
		if (w) {
			w->hide();
			delete w;
		}
	}
	delete _gui;
	_gui = 0;
}

void
LaunchPadX::build_gui ()
{
	_gui = new LPX_GUI (*this);
}

/*--------------------*/

LPX_GUI::LPX_GUI (LaunchPadX& p)
	: _lp (p)
	, _table (2, 5)
	, _action_table (5, 4)
	, _ignore_active_change (false)
{
	set_border_width (12);

	_table.set_row_spacings (4);
	_table.set_col_spacings (6);
	_table.set_border_width (12);
	_table.set_homogeneous (false);

	std::string data_file_path;
	std::string name = "launchpad-pro.png";
	Searchpath spath(ARDOUR::ardour_data_search_path());
	spath.add_subdirectory_to_paths ("icons");
	find_file (spath, name, data_file_path);
	if (!data_file_path.empty()) {
		_image.set (data_file_path);
		_hpacker.pack_start (_image, false, false);
	}

	Gtk::Label* l;
	int row = 0;

	_input_combo.pack_start (_midi_port_columns.short_name);
	_output_combo.pack_start (_midi_port_columns.short_name);

	_input_combo.signal_changed().connect (sigc::bind (sigc::mem_fun (*this, &LPX_GUI::active_port_changed), &_input_combo, true));
	_output_combo.signal_changed().connect (sigc::bind (sigc::mem_fun (*this, &LPX_GUI::active_port_changed), &_output_combo, false));

	l = manage (new Gtk::Label);
	l->set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Incoming MIDI on:")));
	l->set_alignment (1.0, 0.5);
	_table.attach (*l, 0, 1, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	_table.attach (_input_combo, 1, 2, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	row++;

	l = manage (new Gtk::Label);
	l->set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Outgoing MIDI on:")));
	l->set_alignment (1.0, 0.5);
	_table.attach (*l, 0, 1, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	_table.attach (_output_combo, 1, 2, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	row++;

	_hpacker.pack_start (_table, true, true);

	set_spacing (12);

	pack_start (_hpacker, false, false);

	/* update the port connection combos */

	update_port_combos ();

	/* catch future changes to connection state */

	ARDOUR::AudioEngine::instance()->PortRegisteredOrUnregistered.connect (_port_connections, invalidator (*this), boost::bind (&LPX_GUI::connection_handler, this), gui_context());
	ARDOUR::AudioEngine::instance()->PortPrettyNameChanged.connect (_port_connections, invalidator (*this), boost::bind (&LPX_GUI::connection_handler, this), gui_context());
	_lp.ConnectionChange.connect (_port_connections, invalidator (*this), boost::bind (&LPX_GUI::connection_handler, this), gui_context());
}

LPX_GUI::~LPX_GUI ()
{
}

void
LPX_GUI::connection_handler ()
{
	/* ignore all changes to combobox active strings here, because we're
	   updating them to match a new ("external") reality - we were called
	   because port connections have changed.
	*/

	PBD::Unwinder<bool> ici (_ignore_active_change, true);

	update_port_combos ();
}

void
LPX_GUI::update_port_combos ()
{
	std::vector<std::string> midi_inputs;
	std::vector<std::string> midi_outputs;

	if (!_lp.input_port() || !_lp.output_port()) {
		return;
	}

	ARDOUR::AudioEngine::instance()->get_ports ("", ARDOUR::DataType::MIDI, ARDOUR::PortFlags (ARDOUR::IsOutput|ARDOUR::IsTerminal), midi_inputs);
	ARDOUR::AudioEngine::instance()->get_ports ("", ARDOUR::DataType::MIDI, ARDOUR::PortFlags (ARDOUR::IsInput|ARDOUR::IsTerminal), midi_outputs);

	Glib::RefPtr<Gtk::ListStore> input = build_midi_port_list (midi_inputs, true);
	Glib::RefPtr<Gtk::ListStore> output = build_midi_port_list (midi_outputs, false);
	bool input_found = false;
	bool output_found = false;
	int n;

	_input_combo.set_model (input);
	_output_combo.set_model (output);

	Gtk::TreeModel::Children children = input->children();
	Gtk::TreeModel::Children::iterator i;
	i = children.begin();
	++i; /* skip "Disconnected" */


	for (n = 1;  i != children.end(); ++i, ++n) {
		std::string port_name = (*i)[_midi_port_columns.full_name];
		if (_lp.input_port()->connected_to (port_name)) {
			_input_combo.set_active (n);
			input_found = true;
			break;
		}
	}

	if (!input_found) {
		_input_combo.set_active (0); /* disconnected */
	}

	children = output->children();
	i = children.begin();
	++i; /* skip "Disconnected" */

	for (n = 1;  i != children.end(); ++i, ++n) {
		std::string port_name = (*i)[_midi_port_columns.full_name];
		if (_lp.output_port()->connected_to (port_name)) {
			_output_combo.set_active (n);
			output_found = true;
			break;
		}
	}

	if (!output_found) {
		_output_combo.set_active (0); /* disconnected */
	}
}

Glib::RefPtr<Gtk::ListStore>
LPX_GUI::build_midi_port_list (std::vector<std::string> const & ports, bool for_input)
{
	Glib::RefPtr<Gtk::ListStore> store = ListStore::create (_midi_port_columns);
	TreeModel::Row row;

	row = *store->append ();
	row[_midi_port_columns.full_name] = std::string();
	row[_midi_port_columns.short_name] = _("Disconnected");

	for (std::vector<std::string>::const_iterator p = ports.begin(); p != ports.end(); ++p) {
		row = *store->append ();
		row[_midi_port_columns.full_name] = *p;
		std::string pn = ARDOUR::AudioEngine::instance()->get_pretty_name_by_name (*p);
		if (pn.empty ()) {
			pn = (*p).substr ((*p).find (':') + 1);
		}
		row[_midi_port_columns.short_name] = pn;
	}

	return store;
}

void
LPX_GUI::active_port_changed (Gtk::ComboBox* combo, bool for_input)
{
	if (_ignore_active_change) {
		return;
	}

	TreeModel::iterator active = combo->get_active ();
	std::string new_port = (*active)[_midi_port_columns.full_name];

	if (new_port.empty()) {
		if (for_input) {
			_lp.input_port()->disconnect_all ();
		} else {
			_lp.output_port()->disconnect_all ();
		}

		return;
	}

	if (for_input) {
		if (!_lp.input_port()->connected_to (new_port)) {
			_lp.input_port()->disconnect_all ();
			_lp.input_port()->connect (new_port);
		}
	} else {
		if (!_lp.output_port()->connected_to (new_port)) {
			_lp.output_port()->disconnect_all ();
			_lp.output_port()->connect (new_port);
		}
	}
}
