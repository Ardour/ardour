/*
 * Copyright (C) 2017-2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2018-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2019 Johannes Mueller <github@johannes-mueller.org>
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
#include <gtkmm/separator.h>

#include "pbd/unwind.h"
#include "pbd/strsplit.h"
#include "pbd/file_utils.h"

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/action_model.h"
#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"

#include "ardour/audioengine.h"
#include "ardour/filesystem_paths.h"

#include "faderport8.h"
#include "gui.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace ARDOUR;
using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ArdourSurface::FP_NAMESPACE;

void*
FaderPort8::get_gui () const
{
	if (!gui) {
		const_cast<FaderPort8*>(this)->build_gui ();
	}
	static_cast<Gtk::VBox*>(gui)->show_all();
	return gui;
}

void
FaderPort8::tear_down_gui ()
{
	if (gui) {
		Gtk::Widget *w = static_cast<Gtk::VBox*>(gui)->get_parent();
		if (w) {
			w->hide();
			delete w;
		}
	}
	delete static_cast<FP8GUI*> (gui);
	gui = 0;
}

void
FaderPort8::build_gui ()
{
	gui = (void*) new FP8GUI (*this);
}

/* ****************************************************************************/

FP8GUI::FP8GUI (FaderPort8& p)
	: fp (p)
	, table (2, 3)
	, ignore_active_change (false)
	, two_line_text_cb (_("Two Line Trackname"))
	, auto_pluginui_cb (_("Auto Show/Hide Plugin GUIs"))
	, action_model (ActionManager::ActionModel::instance ())
{
	set_border_width (12);

	table.set_row_spacings (4);
	table.set_col_spacings (6);
	table.set_border_width (12);
	table.set_homogeneous (false);

	std::string data_file_path;
#ifdef FADERPORT16
	string name = "faderport16-small.png";
#elif defined FADERPORT2
	string name = "faderport2018-small.png";
#else
	string name = "faderport8-small.png";
#endif
	Searchpath spath(ARDOUR::ardour_data_search_path());
	spath.add_subdirectory_to_paths ("icons");
	find_file (spath, name, data_file_path);
	if (!data_file_path.empty()) {
		image.set (data_file_path);
		hpacker.pack_start (image, false, false);
	}

	Gtk::Label* l;
	int row = 0;

	input_combo.pack_start (midi_port_columns.short_name);
	output_combo.pack_start (midi_port_columns.short_name);

	build_prefs_combos ();
	update_prefs_combos ();

	input_combo.signal_changed().connect (sigc::bind (sigc::mem_fun (*this, &FP8GUI::active_port_changed), &input_combo, true));
	output_combo.signal_changed().connect (sigc::bind (sigc::mem_fun (*this, &FP8GUI::active_port_changed), &output_combo, false));

	clock_combo.signal_changed().connect (sigc::mem_fun (*this, &FP8GUI::clock_mode_changed));
	scribble_combo.signal_changed().connect (sigc::mem_fun (*this, &FP8GUI::scribble_mode_changed));
	two_line_text_cb.signal_toggled().connect(sigc::mem_fun (*this, &FP8GUI::twolinetext_toggled));
	auto_pluginui_cb.signal_toggled().connect(sigc::mem_fun (*this, &FP8GUI::auto_pluginui_toggled));

	l = manage (new Gtk::Label);
	l->set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Incoming MIDI on:")));
	l->set_alignment (1.0, 0.5);
	table.attach (*l, 1, 4, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	table.attach (input_combo, 4, 8, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	row++;

	l = manage (new Gtk::Label);
	l->set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Outgoing MIDI on:")));
	l->set_alignment (1.0, 0.5);
	table.attach (*l, 1, 4, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	table.attach (output_combo, 4, 8, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	row++;

	Gtk::HSeparator *hsep = manage(new Gtk::HSeparator);
	table.attach (*hsep, 0, 8, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 6);
	row++;

	hpacker.pack_start (table, true, true);
	pack_start (hpacker, false, false);

	/* actions */

	int action_row = 0;
	int action_col = 0;
	Gtk::Alignment* align;

	for (FP8Controls::UserButtonMap::const_iterator i = fp.control().user_buttons ().begin ();
			i != fp.control().user_buttons ().end (); ++i) {
		Gtk::ComboBox* user_combo = manage (new Gtk::ComboBox);
		build_action_combo (*user_combo, i->first);
		l = manage (new Gtk::Label);
		l->set_markup (string_compose ("<span weight=\"bold\">%1:</span>", i->second));
		l->set_alignment (1.0, 0.5);
		table.attach (*l, 3 * action_col, 3 * action_col + 1, row + action_row, row + action_row + 1, AttachOptions(FILL|EXPAND), AttachOptions (0));
		align = manage (new Alignment);
		align->set (0.0, 0.5);
		align->add (*user_combo);
		table.attach (*align, 3 * action_col + 1, 3 * action_col + 2, row + action_row, row + action_row + 1, AttachOptions(FILL|EXPAND), AttachOptions (0));

#ifdef FADERPORT2
		if (++action_row == 2)
#else
		if (++action_row == 4)
#endif
		{
			action_row = 0;
			++action_col;
		}
	}

	for (int c = 0; c < 2; ++c) {
		Gtk::VSeparator *vsep = manage(new Gtk::VSeparator);
		table.attach (*vsep, 3 * c + 2, 3 * c + 3, row, row + 4, AttachOptions(0), AttachOptions(FILL), 6, 0);
	}

	row += 4;

#ifndef FADERPORT2
	hsep = manage(new Gtk::HSeparator);
	table.attach (*hsep, 0, 8, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 6);
	row++;

	l = manage (new Gtk::Label);
	l->set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Clock:")));
	l->set_alignment (1.0, 0.5);
	table.attach (*l, 0, 1, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	table.attach (clock_combo, 1, 4, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);

	table.attach (two_line_text_cb, 4, 8, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	row++;

	l = manage (new Gtk::Label);
	l->set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Display:")));
	l->set_alignment (1.0, 0.5);
	table.attach (*l, 0, 1, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	table.attach (scribble_combo, 1, 4, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);

	table.attach (auto_pluginui_cb, 4, 8, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	row++;
#endif

	/* update the port connection combos */
	update_port_combos ();

	/* catch future changes to connection state */
	ARDOUR::AudioEngine::instance()->PortRegisteredOrUnregistered.connect (_port_connections, invalidator (*this), boost::bind (&FP8GUI::connection_handler, this), gui_context());
	ARDOUR::AudioEngine::instance()->PortPrettyNameChanged.connect (_port_connections, invalidator (*this), boost::bind (&FP8GUI::connection_handler, this), gui_context());
	fp.ConnectionChange.connect (_port_connections, invalidator (*this), boost::bind (&FP8GUI::connection_handler, this), gui_context());
}

FP8GUI::~FP8GUI ()
{
}

void
FP8GUI::connection_handler ()
{
	PBD::Unwinder<bool> ici (ignore_active_change, true);
	update_port_combos ();
}

void
FP8GUI::update_port_combos ()
{
	vector<string> midi_inputs;
	vector<string> midi_outputs;

	ARDOUR::AudioEngine::instance()->get_ports ("", ARDOUR::DataType::MIDI, ARDOUR::PortFlags (ARDOUR::IsOutput|ARDOUR::IsTerminal), midi_inputs);
	ARDOUR::AudioEngine::instance()->get_ports ("", ARDOUR::DataType::MIDI, ARDOUR::PortFlags (ARDOUR::IsInput|ARDOUR::IsTerminal), midi_outputs);

	Glib::RefPtr<Gtk::ListStore> input = build_midi_port_list (midi_inputs, true);
	Glib::RefPtr<Gtk::ListStore> output = build_midi_port_list (midi_outputs, false);
	bool input_found = false;
	bool output_found = false;
	int n;

	input_combo.set_model (input);
	output_combo.set_model (output);

	Gtk::TreeModel::Children children = input->children();
	Gtk::TreeModel::Children::iterator i;
	i = children.begin();
	++i; /* skip "Disconnected" */

	for (n = 1;  i != children.end(); ++i, ++n) {
		string port_name = (*i)[midi_port_columns.full_name];
		if (fp.input_port()->connected_to (port_name)) {
			input_combo.set_active (n);
			input_found = true;
			break;
		}
	}

	if (!input_found) {
		input_combo.set_active (0); /* disconnected */
	}

	children = output->children();
	i = children.begin();
	++i; /* skip "Disconnected" */

	for (n = 1;  i != children.end(); ++i, ++n) {
		string port_name = (*i)[midi_port_columns.full_name];
		if (fp.output_port()->connected_to (port_name)) {
			output_combo.set_active (n);
			output_found = true;
			break;
		}
	}

	if (!output_found) {
		output_combo.set_active (0); /* disconnected */
	}
}


Glib::RefPtr<Gtk::ListStore>
FP8GUI::build_midi_port_list (vector<string> const & ports, bool for_input)
{
	Glib::RefPtr<Gtk::ListStore> store = ListStore::create (midi_port_columns);
	TreeModel::Row row;

	row = *store->append ();
	row[midi_port_columns.full_name] = string();
	row[midi_port_columns.short_name] = _("Disconnected");

	for (vector<string>::const_iterator p = ports.begin(); p != ports.end(); ++p) {
		row = *store->append ();
		row[midi_port_columns.full_name] = *p;
		std::string pn = ARDOUR::AudioEngine::instance()->get_pretty_name_by_name (*p);
		if (pn.empty ()) {
			pn = (*p).substr ((*p).find (':') + 1);
		}
		row[midi_port_columns.short_name] = pn;
	}

	return store;
}

void
FP8GUI::active_port_changed (Gtk::ComboBox* combo, bool for_input)
{
	if (ignore_active_change) {
		return;
	}

	TreeModel::iterator active = combo->get_active ();
	string new_port = (*active)[midi_port_columns.full_name];

	if (new_port.empty()) {
		if (for_input) {
			fp.input_port()->disconnect_all ();
		} else {
			fp.output_port()->disconnect_all ();
		}

		return;
	}

	if (for_input) {
		if (!fp.input_port()->connected_to (new_port)) {
			fp.input_port()->disconnect_all ();
			fp.input_port()->connect (new_port);
		}
	} else {
		if (!fp.output_port()->connected_to (new_port)) {
			fp.output_port()->disconnect_all ();
			fp.output_port()->connect (new_port);
		}
	}
}

void
FP8GUI::build_action_combo (Gtk::ComboBox& cb, FP8Controls::ButtonId id)
{
	/* set the active "row" to the right value for the current button binding */
	const string current_action = fp.get_button_action (id, false); /* lookup release action */
	action_model.build_action_combo(cb, current_action);
	cb.signal_changed().connect (sigc::bind (sigc::mem_fun (*this, &FP8GUI::action_changed), &cb, id));
}

void
FP8GUI::action_changed (Gtk::ComboBox* cb, FP8Controls::ButtonId id)
{
	TreeModel::const_iterator row = cb->get_active ();
	string action_path = (*row)[action_model.path()];
	fp.set_button_action (id, false, action_path);
}


void
FP8GUI::build_prefs_combos ()
{
	vector<string> clock_strings;
	vector<string> scribble_strings;

	//clock_strings.push_back (_("Off"));
	clock_strings.push_back (_("Timecode"));
	clock_strings.push_back (_("BBT"));
	clock_strings.push_back (_("Timecode + BBT"));

	scribble_strings.push_back (_("Off"));
	scribble_strings.push_back (_("Meter"));
	scribble_strings.push_back (_("Pan"));
	scribble_strings.push_back (_("Meter + Pan"));

	set_popdown_strings (clock_combo, clock_strings);
	set_popdown_strings (scribble_combo, scribble_strings);
}

void
FP8GUI::update_prefs_combos ()
{
	switch (fp.clock_mode()) {
		default:
			clock_combo.set_active_text (_("Off"));
			break;
		case 1:
			clock_combo.set_active_text (_("Timecode"));
			break;
		case 2:
			clock_combo.set_active_text (_("BBT"));
			break;
		case 3:
			clock_combo.set_active_text (_("Timecode + BBT"));
			break;
	}

	switch (fp.scribble_mode()) {
		default:
			scribble_combo.set_active_text (_("Off"));
			break;
		case 1:
			scribble_combo.set_active_text (_("Meter"));
			break;
		case 2:
			scribble_combo.set_active_text (_("Pan"));
			break;
		case 3:
			scribble_combo.set_active_text (_("Meter + Pan"));
			break;
	}
	two_line_text_cb.set_active (fp.twolinetext ());
	auto_pluginui_cb.set_active (fp.auto_pluginui ());
}

void
FP8GUI::clock_mode_changed ()
{
	string str = clock_combo.get_active_text();
	if (str == _("BBT")) {
		fp.set_clock_mode (2);
	} else if (str == _("Timecode + BBT")) {
		fp.set_clock_mode (3);
	} else {
		fp.set_clock_mode (1);
	}
}

void
FP8GUI::scribble_mode_changed ()
{
	string str = scribble_combo.get_active_text();
	if (str == _("Off")) {
		fp.set_scribble_mode (0);
	} else if (str == _("Meter")) {
		fp.set_scribble_mode (1);
	} else if (str == _("Pan")) {
		fp.set_scribble_mode (2);
	} else {
		fp.set_scribble_mode (3);
	}
}

void
FP8GUI::twolinetext_toggled ()
{
	fp.set_two_line_text (two_line_text_cb.get_active ());
}


void
FP8GUI::auto_pluginui_toggled ()
{
	fp.set_auto_pluginui (auto_pluginui_cb.get_active ());
}
