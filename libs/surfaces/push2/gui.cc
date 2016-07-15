/*
    Copyright (C) 2015 Paul Davis

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

#include "push2.h"
#include "gui.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace ARDOUR;
using namespace ArdourSurface;
using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

void*
Push2::get_gui () const
{
	if (!gui) {
		const_cast<Push2*>(this)->build_gui ();
	}
	static_cast<Gtk::VBox*>(gui)->show_all();
	return gui;
}

void
Push2::tear_down_gui ()
{
	if (gui) {
		Gtk::Widget *w = static_cast<Gtk::VBox*>(gui)->get_parent();
		if (w) {
			w->hide();
			delete w;
		}
	}
	delete gui;
	gui = 0;
}

void
Push2::build_gui ()
{
	gui = new P2GUI (*this);
}

/*--------------------*/

P2GUI::P2GUI (Push2& p)
	: p2 (p)
	, table (2, 5)
	, action_table (5, 4)
	, ignore_active_change (false)
	, pad_table (8, 8)
	, root_note_octave_adjustment (p2.root_octave(), 0, 10, 1, 1)
	, root_note_octave (root_note_octave_adjustment)
	, root_note_octave_label (_("Octave"))
	, root_note_label (_("Root"))
	, mode_label (_("Mode (Scale)"))
	, inkey_button (_("In-Key Mode"))
	, mode_packer (3, 3)
{
	set_border_width (12);

	table.set_row_spacings (4);
	table.set_col_spacings (6);
	table.set_border_width (12);
	table.set_homogeneous (false);

	std::string data_file_path;
	string name = "push2-small.png";
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

	input_combo.signal_changed().connect (sigc::bind (sigc::mem_fun (*this, &P2GUI::active_port_changed), &input_combo, true));
	output_combo.signal_changed().connect (sigc::bind (sigc::mem_fun (*this, &P2GUI::active_port_changed), &output_combo, false));

	l = manage (new Gtk::Label);
	l->set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Incoming MIDI on:")));
	l->set_alignment (1.0, 0.5);
	table.attach (*l, 0, 1, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	table.attach (input_combo, 1, 2, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	row++;

	l = manage (new Gtk::Label);
	l->set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Outgoing MIDI on:")));
	l->set_alignment (1.0, 0.5);
	table.attach (*l, 0, 1, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	table.attach (output_combo, 1, 2, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	row++;

	hpacker.pack_start (table, true, true);

	pad_table.set_spacings (3);
	build_pad_table ();

	root_note_selector.set_model (build_note_columns());
	root_note_selector.pack_start (note_columns.name);
	root_note_selector.set_active (p2.scale_root());

	mode_selector.set_model (build_mode_columns());
	mode_selector.pack_start (mode_columns.name);
	mode_selector.set_active ((int) p2.mode());

	mode_packer.set_border_width (12);
	mode_packer.set_spacings (12);

	mode_packer.attach (root_note_label, 0, 1, 0, 1, AttachOptions (FILL|EXPAND), SHRINK);
	mode_packer.attach (root_note_selector, 1, 2, 0, 1, AttachOptions (FILL|EXPAND), SHRINK);

	mode_packer.attach (root_note_octave_label, 0, 1, 1, 2, AttachOptions (FILL|EXPAND), SHRINK);
	mode_packer.attach (root_note_octave, 1, 2, 1, 2, AttachOptions (FILL|EXPAND), SHRINK);

	mode_packer.attach (mode_label, 0, 1, 2, 3, AttachOptions (FILL|EXPAND), SHRINK);
	mode_packer.attach (mode_selector, 1, 2, 2, 3, AttachOptions (FILL|EXPAND), SHRINK);

	inkey_button.set_active (p2.in_key());
	mode_packer.attach (inkey_button, 1, 2, 3, 4, AttachOptions (FILL|EXPAND), SHRINK);

	pad_notebook.append_page (pad_table, _("Pad Layout"));
	pad_notebook.append_page (mode_packer, _("Modes/Scales"));
	pad_notebook.append_page (custom_packer, _("Custom"));

	root_note_octave_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &P2GUI::reprogram_pad_scale));
	root_note_selector.signal_changed().connect (sigc::mem_fun (*this, &P2GUI::reprogram_pad_scale));
	mode_selector.signal_changed().connect (sigc::mem_fun (*this, &P2GUI::reprogram_pad_scale));
	inkey_button.signal_clicked().connect (sigc::mem_fun (*this, &P2GUI::reprogram_pad_scale));

	set_spacing (12);

	pack_start (hpacker, false, false);
	pack_start (pad_notebook);

	/* update the port connection combos */

	update_port_combos ();

	/* catch future changes to connection state */

	// p2.ConnectionChange.connect (connection_change_connection, invalidator (*this), boost::bind (&P2GUI::connection_handler, this), gui_context());
	p2.PadChange.connect (p2_connections, invalidator (*this), boost::bind (&P2GUI::build_pad_table, this), gui_context());
}

P2GUI::~P2GUI ()
{
}

void
P2GUI::connection_handler ()
{
	/* ignore all changes to combobox active strings here, because we're
	   updating them to match a new ("external") reality - we were called
	   because port connections have changed.
	*/

	PBD::Unwinder<bool> ici (ignore_active_change, true);

	update_port_combos ();
}

void
P2GUI::update_port_combos ()
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
		if (p2.input_port()->connected_to (port_name)) {
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
		if (p2.output_port()->connected_to (port_name)) {
			output_combo.set_active (n);
			output_found = true;
			break;
		}
	}

	if (!output_found) {
		output_combo.set_active (0); /* disconnected */
	}
}

void
P2GUI::build_available_action_menu ()
{
	/* build a model of all available actions (needs to be tree structured
	 * more)
	 */

	available_action_model = TreeStore::create (action_columns);

	vector<string> paths;
	vector<string> labels;
	vector<string> tooltips;
	vector<string> keys;
	vector<Glib::RefPtr<Gtk::Action> > actions;

	Gtkmm2ext::ActionMap::get_all_actions (paths, labels, tooltips, keys, actions);

	typedef std::map<string,TreeIter> NodeMap;
	NodeMap nodes;
	NodeMap::iterator r;


	vector<string>::iterator k;
	vector<string>::iterator p;
	vector<string>::iterator t;
	vector<string>::iterator l;

	available_action_model->clear ();

	TreeIter rowp;
	TreeModel::Row parent;

	/* Disabled item (row 0) */

	rowp = available_action_model->append();
	parent = *(rowp);
	parent[action_columns.name] = _("Disabled");

	/* Key aliasing */

	rowp = available_action_model->append();
	parent = *(rowp);
	parent[action_columns.name] = _("Shift");
	rowp = available_action_model->append();
	parent = *(rowp);
	parent[action_columns.name] = _("Control");
	rowp = available_action_model->append();
	parent = *(rowp);
	parent[action_columns.name] = _("Option");
	rowp = available_action_model->append();
	parent = *(rowp);
	parent[action_columns.name] = _("CmdAlt");


	for (l = labels.begin(), k = keys.begin(), p = paths.begin(), t = tooltips.begin(); l != labels.end(); ++k, ++p, ++t, ++l) {

		TreeModel::Row row;
		vector<string> parts;

		parts.clear ();

		split (*p, parts, '/');

		if (parts.empty()) {
			continue;
		}

		//kinda kludgy way to avoid displaying menu items as mappable
		if ( parts[1] == _("Main_menu") )
			continue;
		if ( parts[1] == _("JACK") )
			continue;
		if ( parts[1] == _("redirectmenu") )
			continue;
		if ( parts[1] == _("Editor_menus") )
			continue;
		if ( parts[1] == _("RegionList") )
			continue;
		if ( parts[1] == _("ProcessorMenu") )
			continue;

		if ((r = nodes.find (parts[1])) == nodes.end()) {

			/* top level is missing */

			TreeIter rowp;
			TreeModel::Row parent;
			rowp = available_action_model->append();
			nodes[parts[1]] = rowp;
			parent = *(rowp);
			parent[action_columns.name] = parts[1];

			row = *(available_action_model->append (parent.children()));

		} else {

			row = *(available_action_model->append ((*r->second)->children()));

		}

		/* add this action */

		if (l->empty ()) {
			row[action_columns.name] = *t;
			action_map[*t] = *p;
		} else {
			row[action_columns.name] = *l;
			action_map[*l] = *p;
		}

		string path = (*p);
		/* ControlProtocol::access_action() is not interested in the
		   legacy "<Actions>/" prefix part of a path.
		*/
		path = path.substr (strlen ("<Actions>/"));

		row[action_columns.path] = path;
	}
}


bool
P2GUI::find_action_in_model (const TreeModel::iterator& iter, std::string const & action_path, TreeModel::iterator* found)
{
	TreeModel::Row row = *iter;
	string path = row[action_columns.path];

	if (path == action_path) {
		*found = iter;
		return true;
	}

	return false;
}

Glib::RefPtr<Gtk::ListStore>
P2GUI::build_midi_port_list (vector<string> const & ports, bool for_input)
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
P2GUI::active_port_changed (Gtk::ComboBox* combo, bool for_input)
{
	if (ignore_active_change) {
		return;
	}

	TreeModel::iterator active = combo->get_active ();
	string new_port = (*active)[midi_port_columns.full_name];

	if (new_port.empty()) {
		if (for_input) {
			p2.input_port()->disconnect_all ();
		} else {
			p2.output_port()->disconnect_all ();
		}

		return;
	}

	if (for_input) {
		if (!p2.input_port()->connected_to (new_port)) {
			p2.input_port()->disconnect_all ();
			p2.input_port()->connect (new_port);
		}
	} else {
		if (!p2.output_port()->connected_to (new_port)) {
			p2.output_port()->disconnect_all ();
			p2.output_port()->connect (new_port);
		}
	}
}

void
P2GUI::build_pad_table ()
{
	container_clear (pad_table);

	for (int row = 7; row >= 0; --row) {
		for (int col = 0; col < 8; ++col) {

			int n = p2.pad_note (row, col);

			Gtk::Button* b = manage (new Button (string_compose ("%1 (%2)", ParameterDescriptor::midi_note_name (n), n)));
			b->show ();

			pad_table.attach (*b, col, col+1, (7-row), (8-row));
		}
	}
}

Glib::RefPtr<Gtk::ListStore>
P2GUI::build_mode_columns ()
{
	Glib::RefPtr<Gtk::ListStore> store = ListStore::create (mode_columns);
	TreeModel::Row row;

	row = *store->append();
	row[mode_columns.name] = _("Dorian");
	row[mode_columns.mode] = MusicalMode::Dorian;

	row = *store->append();
	row[mode_columns.name] = _("Ionian (\"Major\")");
	row[mode_columns.mode] = MusicalMode::IonianMajor;

	row = *store->append();
	row[mode_columns.name] = _("Minor");
	row[mode_columns.mode] = MusicalMode::Minor;

	row = *store->append();
	row[mode_columns.name] = _("Harmonic Minor");
	row[mode_columns.mode] = MusicalMode::HarmonicMinor;

	row = *store->append();
	row[mode_columns.name] = _("Melodic Minor Ascending");
	row[mode_columns.mode] = MusicalMode::MelodicMinorAscending;

	row = *store->append();
	row[mode_columns.name] = _("Melodic Minor Descending");
	row[mode_columns.mode] = MusicalMode::MelodicMinorDescending;

	row = *store->append();
	row[mode_columns.name] = _("Phrygian");
	row[mode_columns.mode] = MusicalMode::Phrygian;

	row = *store->append();
	row[mode_columns.name] = _("Lydian");
	row[mode_columns.mode] = MusicalMode::Lydian;

	row = *store->append();
	row[mode_columns.name] = _("Mixolydian");
	row[mode_columns.mode] = MusicalMode::Mixolydian;

	row = *store->append();
	row[mode_columns.name] = _("Aeolian (\"Major\")");
	row[mode_columns.mode] = MusicalMode::Aeolian;

	row = *store->append();
	row[mode_columns.name] = _("Locrian");
	row[mode_columns.mode] = MusicalMode::Locrian;

	row = *store->append();
	row[mode_columns.name] = _("Pentatonic Major");
	row[mode_columns.mode] = MusicalMode::PentatonicMajor;

	row = *store->append();
	row[mode_columns.name] = _("Pentatonic Minor");
	row[mode_columns.mode] = MusicalMode::PentatonicMinor;

	row = *store->append();
	row[mode_columns.name] = _("Chromatic");
	row[mode_columns.mode] = MusicalMode::Chromatic;

	row = *store->append();
	row[mode_columns.name] = _("Blues Scale");
	row[mode_columns.mode] = MusicalMode::BluesScale;

	row = *store->append();
	row[mode_columns.name] = _("Neapolitan Minor");
	row[mode_columns.mode] = MusicalMode::NeapolitanMinor;

	row = *store->append();
	row[mode_columns.name] = _("Neapolitan Major");
	row[mode_columns.mode] = MusicalMode::NeapolitanMajor;

	row = *store->append();
	row[mode_columns.name] = _("Oriental");
	row[mode_columns.mode] = MusicalMode::Oriental;

	row = *store->append();
	row[mode_columns.name] = _("Double Harmonic");
	row[mode_columns.mode] = MusicalMode::DoubleHarmonic;

	row = *store->append();
	row[mode_columns.name] = _("Enigmatic");
	row[mode_columns.mode] = MusicalMode::Enigmatic;

	row = *store->append();
	row[mode_columns.name] = _("Hirajoshi");
	row[mode_columns.mode] = MusicalMode::Hirajoshi;

	row = *store->append();
	row[mode_columns.name] = _("Hungarian Minor");
	row[mode_columns.mode] = MusicalMode::HungarianMinor;

	row = *store->append();
	row[mode_columns.name] = _("Hungarian Major");
	row[mode_columns.mode] = MusicalMode::HungarianMajor;

	row = *store->append();
	row[mode_columns.name] = _("Kumoi");
	row[mode_columns.mode] = MusicalMode::Kumoi;

	row = *store->append();
	row[mode_columns.name] = _("Iwato");
	row[mode_columns.mode] = MusicalMode::Iwato;

	row = *store->append();
	row[mode_columns.name] = _("Hindu");
	row[mode_columns.mode] = MusicalMode::Hindu;

	row = *store->append();
	row[mode_columns.name] = _("Spanish 8 Tone");
	row[mode_columns.mode] = MusicalMode::Spanish8Tone;

	row = *store->append();
	row[mode_columns.name] = _("Pelog");
	row[mode_columns.mode] = MusicalMode::Pelog;

	row = *store->append();
	row[mode_columns.name] = _("Hungarian Gypsy");
	row[mode_columns.mode] = MusicalMode::HungarianGypsy;

	row = *store->append();
	row[mode_columns.name] = _("Overtone");
	row[mode_columns.mode] = MusicalMode::Overtone;

	row = *store->append();
	row[mode_columns.name] = _("Leading Whole Tone");
	row[mode_columns.mode] = MusicalMode::LeadingWholeTone;

	row = *store->append();
	row[mode_columns.name] = _("Arabian");
	row[mode_columns.mode] = MusicalMode::Arabian;

	row = *store->append();
	row[mode_columns.name] = _("Balinese");
	row[mode_columns.mode] = MusicalMode::Balinese;

	row = *store->append();
	row[mode_columns.name] = _("Gypsy");
	row[mode_columns.mode] = MusicalMode::Gypsy;

	row = *store->append();
	row[mode_columns.name] = _("Mohammedan");
	row[mode_columns.mode] = MusicalMode::Mohammedan;

	row = *store->append();
	row[mode_columns.name] = _("Javanese");
	row[mode_columns.mode] = MusicalMode::Javanese;

	row = *store->append();
	row[mode_columns.name] = _("Persian");
	row[mode_columns.mode] = MusicalMode::Persian;

	row = *store->append();
	row[mode_columns.name] = _("Algerian");
	row[mode_columns.mode] = MusicalMode::Algerian;

	return store;
}

Glib::RefPtr<Gtk::ListStore>
P2GUI::build_note_columns ()
{
	Glib::RefPtr<Gtk::ListStore> store = ListStore::create (note_columns);
	TreeModel::Row row;

	row = *store->append ();
	row[note_columns.number] = 0;
	row[note_columns.name] = "C";

	row = *store->append ();
	row[note_columns.number] = 1;
	row[note_columns.name] = "C#";

	row = *store->append ();
	row[note_columns.number] = 2;
	row[note_columns.name] = "D";

	row = *store->append ();
	row[note_columns.number] = 3;
	row[note_columns.name] = "D#";

	row = *store->append ();
	row[note_columns.number] = 4;
	row[note_columns.name] = "E";

	row = *store->append ();
	row[note_columns.number] = 5;
	row[note_columns.name] = "F";

	row = *store->append ();
	row[note_columns.number] = 6;
	row[note_columns.name] = "F#";

	row = *store->append ();
	row[note_columns.number] = 7;
	row[note_columns.name] = "G";

	row = *store->append ();
	row[note_columns.number] = 8;
	row[note_columns.name] = "G#";

	row = *store->append ();
	row[note_columns.number] = 9;
	row[note_columns.name] = "A";

	row = *store->append ();
	row[note_columns.number] = 10;
	row[note_columns.name] = "A#";

	row = *store->append ();
	row[note_columns.number] = 11;
	row[note_columns.name] = "B";

	return store;
}

void
P2GUI::reprogram_pad_scale ()
{
	int root;
	int octave;
	MusicalMode::Type mode;
	bool inkey;

	Gtk::TreeModel::iterator iter = root_note_selector.get_active();
	if (iter) {
		Gtk::TreeModel::Row row = *iter;
		if (row) {
			root = row[note_columns.number];
		} else {
			root = 5;
		}
	} else {
		root = 5;
	}

	octave = (int) floor (root_note_octave_adjustment.get_value ());

	iter = mode_selector.get_active();
	if (iter) {
		Gtk::TreeModel::Row row = *iter;
		if (row) {
			mode = row[mode_columns.mode];
		} else {
			mode = MusicalMode::IonianMajor;
		}
	} else {
		mode = MusicalMode::IonianMajor;
	}

	inkey = inkey_button.get_active ();

	p2.set_pad_scale (root, octave, mode, inkey);
}
