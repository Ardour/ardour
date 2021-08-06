/*
 * Copyright (C) 2006-2007 John Anderson
 * Copyright (C) 2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2012-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2015 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2017 Len Ovens <len@ovenwerks.net>
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

#include <gtkmm/comboboxtext.h>
#include <gtkmm/box.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/table.h>
#include <gtkmm/treeview.h>
#include <gtkmm/liststore.h>
#include <gtkmm/treestore.h>
#include <gtkmm/notebook.h>
#include <gtkmm/cellrenderercombo.h>
#include <gtkmm/scale.h>
#include <gtkmm/alignment.h>

#include "pbd/error.h"
#include "pbd/unwind.h"
#include "pbd/strsplit.h"

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/action_model.h"
#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"

#include "ardour/audioengine.h"
#include "ardour/port.h"
#include "ardour/rc_configuration.h"

#include "mackie_control_protocol.h"
#include "device_info.h"
#include "gui.h"
#include "surface.h"
#include "surface_port.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace ArdourSurface;
using namespace Mackie;

void*
MackieControlProtocol::get_gui () const
{
	if (!_gui) {
		const_cast<MackieControlProtocol*>(this)->build_gui ();
	}
	static_cast<Gtk::Notebook*>(_gui)->show_all();
	return _gui;
}

void
MackieControlProtocol::tear_down_gui ()
{
	if (_gui) {
		Gtk::Widget *w = static_cast<Gtk::Widget*>(_gui)->get_parent();
		if (w) {
			w->hide();
			delete w;
		}
	}
	delete (MackieControlProtocolGUI*) _gui;
	_gui = 0;
}

void
MackieControlProtocol::build_gui ()
{
	_gui = (void *) new MackieControlProtocolGUI (*this);
}

MackieControlProtocolGUI::MackieControlProtocolGUI (MackieControlProtocol& p)
	: _cp (p)
	, table (2, 9)
	, action_model (ActionManager::ActionModel::instance ())
	, touch_sensitivity_adjustment (0, 0, 9, 1, 4)
	, touch_sensitivity_scale (touch_sensitivity_adjustment)
	, recalibrate_fader_button (_("Recalibrate Faders"))
	, ipmidi_base_port_adjustment (_cp.ipmidi_base(), 0, 32767, 1, 1000)
	, discover_button (_("Discover Mackie Devices"))
	, _device_dependent_widget (0)
	, _ignore_profile_changed (false)
	, ignore_active_change (false)
{
	Gtk::Label* l;
	Gtk::Alignment* align;
	int row = 0;

	set_border_width (12);

	table.set_row_spacings (4);
	table.set_col_spacings (6);
	table.set_border_width (12);
	table.set_homogeneous (false);

	l = manage (new Gtk::Label (_("Device Type:")));
	l->set_alignment (1.0, 0.5);
	table.attach (*l, 0, 1, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	table.attach (_surface_combo, 1, 2, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	row++;

	vector<string> surfaces;

	for (std::map<std::string,DeviceInfo>::iterator i = DeviceInfo::device_info.begin(); i != DeviceInfo::device_info.end(); ++i) {
		surfaces.push_back (i->first);
	}
	Gtkmm2ext::set_popdown_strings (_surface_combo, surfaces);
	_surface_combo.signal_changed().connect (sigc::mem_fun (*this, &MackieControlProtocolGUI::surface_combo_changed));

	_cp.DeviceChanged.connect (device_change_connection, invalidator (*this), boost::bind (&MackieControlProtocolGUI::device_changed, this), gui_context());

	/* catch future changes to connection state */
	ARDOUR::AudioEngine::instance()->PortRegisteredOrUnregistered.connect (_port_connections, invalidator (*this), boost::bind (&MackieControlProtocolGUI::connection_handler, this), gui_context());
	ARDOUR::AudioEngine::instance()->PortPrettyNameChanged.connect (_port_connections, invalidator (*this), boost::bind (&MackieControlProtocolGUI::connection_handler, this), gui_context());
	_cp.ConnectionChange.connect (_port_connections, invalidator (*this), boost::bind (&MackieControlProtocolGUI::connection_handler, this), gui_context());

	ipmidi_base_port_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &MackieControlProtocolGUI::ipmidi_spinner_changed));

	/* device-dependent part */

	device_dependent_row = row;

	if (_device_dependent_widget) {
		table.remove (*_device_dependent_widget);
		_device_dependent_widget = 0;
	}

	_device_dependent_widget = device_dependent_widget ();
	table.attach (*_device_dependent_widget, 0, 12, row, row+1, AttachOptions(0), AttachOptions(0), 0, 0);
	row++;

	/* back to the boilerplate */

	RadioButtonGroup rb_group = absolute_touch_mode_button.get_group();
	touch_move_mode_button.set_group (rb_group);

	recalibrate_fader_button.signal_clicked().connect (sigc::mem_fun (*this, &MackieControlProtocolGUI::recalibrate_faders));
	backlight_button.signal_clicked().connect (sigc::mem_fun (*this, &MackieControlProtocolGUI::toggle_backlight));

	touch_sensitivity_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &MackieControlProtocolGUI::touch_sensitive_change));
	touch_sensitivity_scale.set_update_policy (Gtk::UPDATE_DISCONTINUOUS);

	l = manage (new Gtk::Label (_("Button click")));
	l->set_alignment (1.0, 0.5);
	table.attach (*l, 0, 1, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	align = manage (new Alignment);
	align->set (0.0, 0.5);
	align->add (relay_click_button);
	table.attach (*align, 1, 2, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	row++;

	l = manage (new Gtk::Label (_("Backlight")));
	l->set_alignment (1.0, 0.5);
	table.attach (*l, 0, 1, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	align = manage (new Alignment);
	align->set (0.0, 0.5);
	align->add (backlight_button);
	table.attach (*align, 1, 2, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	row++;

	l = manage (new Gtk::Label (_("Send Fader Position Only When Touched")));
	l->set_alignment (1.0, 0.5);
	table.attach (*l, 0, 1, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	align = manage (new Alignment);
	align->set (0.0, 0.5);
	align->add (absolute_touch_mode_button);
	table.attach (*align, 1, 2, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	row++;

	l = manage (new Gtk::Label (_("Send Fader Position When Moved")));
	l->set_alignment (1.0, 0.5);
	table.attach (*l, 0, 1, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	align = manage (new Alignment);
	align->set (0.0, 0.5);
	align->add (touch_move_mode_button);
	table.attach (*align, 1, 2, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	row++;

	l = manage (new Gtk::Label (_("Fader Touch Sense Sensitivity")));
	l->set_alignment (1.0, 0.5);
	table.attach (*l, 0, 1, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	touch_sensitivity_scale.property_digits() = 0;
	touch_sensitivity_scale.property_draw_value() = false;
	table.attach (touch_sensitivity_scale, 1, 2, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	row++;
	table.attach (recalibrate_fader_button, 1, 2, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	row++;


	table.attach (discover_button, 1, 2, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
	discover_button.signal_clicked().connect (sigc::mem_fun (*this, &MackieControlProtocolGUI::discover_clicked));
	row++;

	vector<string> profiles;

	for (std::map<std::string,DeviceProfile>::iterator i = DeviceProfile::device_profiles.begin(); i != DeviceProfile::device_profiles.end(); ++i) {
		cerr << "add discovered profile " << i->first << endl;
		profiles.push_back (i->first);
	}
	Gtkmm2ext::set_popdown_strings (_profile_combo, profiles);
	cerr << "set active profile from " << p.device_profile().name() << endl;
	_profile_combo.set_active_text (p.device_profile().name());
	_profile_combo.signal_changed().connect (sigc::mem_fun (*this, &MackieControlProtocolGUI::profile_combo_changed));

	append_page (table, _("Device Setup"));
	table.show_all();

	/* function key editor */

	VBox* fkey_packer = manage (new VBox);
	HBox* profile_packer = manage (new HBox);
	HBox* observation_packer = manage (new HBox);

	l = manage (new Gtk::Label (_("Profile/Settings:")));
	profile_packer->pack_start (*l, false, false);
	profile_packer->pack_start (_profile_combo, true, true);
	profile_packer->set_spacing (12);
	profile_packer->set_border_width (12);

	l = manage (new Gtk::Label (_("* Button available at the original Mackie MCU PRO or current device if enabled (NOT implemented yet). Device specific name presented.")));
	observation_packer->pack_start (*l, false, false);

	fkey_packer->pack_start (*profile_packer, false, false);
	fkey_packer->pack_start (function_key_scroller, true, true);
	fkey_packer->pack_start (*observation_packer, false, false);
	fkey_packer->set_spacing (12);
	function_key_scroller.property_shadow_type() = Gtk::SHADOW_NONE;
	function_key_scroller.add (function_key_editor);
	append_page (*fkey_packer, _("Function Keys"));

	build_function_key_editor ();
	refresh_function_key_editor ();
	fkey_packer->show_all();
}

void
MackieControlProtocolGUI::connection_handler ()
{
	/* ignore all changes to combobox active strings here, because we're
	   updating them to match a new ("external") reality - we were called
	   because port connections have changed.
	*/

	PBD::Unwinder<bool> ici (ignore_active_change, true);

	vector<Gtk::ComboBox*>::iterator ic;
	vector<Gtk::ComboBox*>::iterator oc;

	vector<string> midi_inputs;
	vector<string> midi_outputs;

	ARDOUR::AudioEngine::instance()->get_ports ("", ARDOUR::DataType::MIDI, ARDOUR::PortFlags (ARDOUR::IsOutput|ARDOUR::IsTerminal), midi_inputs);
	ARDOUR::AudioEngine::instance()->get_ports ("", ARDOUR::DataType::MIDI, ARDOUR::PortFlags (ARDOUR::IsInput|ARDOUR::IsTerminal), midi_outputs);

	for (ic = input_combos.begin(), oc = output_combos.begin(); ic != input_combos.end() && oc != output_combos.end(); ++ic, ++oc) {

		boost::shared_ptr<Surface> surface = _cp.get_surface_by_raw_pointer ((*ic)->get_data ("surface"));

		if (surface) {
			update_port_combos (midi_inputs, midi_outputs, *ic, *oc, surface);
		}
	}
}

void
MackieControlProtocolGUI::update_port_combos (vector<string> const& midi_inputs, vector<string> const& midi_outputs,
                                              Gtk::ComboBox* input_combo,
                                              Gtk::ComboBox* output_combo,
                                              boost::shared_ptr<Surface> surface)
{
	Glib::RefPtr<Gtk::ListStore> input = build_midi_port_list (midi_inputs, true);
	Glib::RefPtr<Gtk::ListStore> output = build_midi_port_list (midi_outputs, false);
	bool input_found = false;
	bool output_found = false;
	int n;

	input_combo->set_model (input);
	output_combo->set_model (output);

	Gtk::TreeModel::Children children = input->children();
	Gtk::TreeModel::Children::iterator i;
	i = children.begin();
	++i; /* skip "Disconnected" */


	for (n = 1;  i != children.end(); ++i, ++n) {
		string port_name = (*i)[midi_port_columns.full_name];
		if (surface->port().input().connected_to (port_name)) {
			input_combo->set_active (n);
			input_found = true;
			break;
		}
	}

	if (!input_found) {
		input_combo->set_active (0); /* disconnected */
	}

	children = output->children();
	i = children.begin();
	++i; /* skip "Disconnected" */

	for (n = 1;  i != children.end(); ++i, ++n) {
		string port_name = (*i)[midi_port_columns.full_name];
		if (surface->port().output().connected_to (port_name)) {
			output_combo->set_active (n);
			output_found = true;
			break;
		}
	}

	if (!output_found) {
		output_combo->set_active (0); /* disconnected */
	}
}

Gtk::Widget*
MackieControlProtocolGUI::device_dependent_widget ()
{
	Gtk::Table* dd_table;
	Gtk::Label* l;
	int row = 0;

	uint32_t n_surfaces = 1 + _cp.device_info().extenders();
	uint32_t main_pos = _cp.device_info().master_position();

	if (!_cp.device_info().uses_ipmidi()) {
		dd_table = Gtk::manage (new Gtk::Table (n_surfaces, 2));
	} else {
		dd_table = Gtk::manage (new Gtk::Table (1, 2));
	}

	dd_table = Gtk::manage (new Gtk::Table (2, n_surfaces));
	dd_table->set_row_spacings (4);
	dd_table->set_col_spacings (6);
	dd_table->set_border_width (12);

	_surface_combo.set_active_text (_cp.device_info().name());

	vector<string> midi_inputs;
	vector<string> midi_outputs;

	ARDOUR::AudioEngine::instance()->get_ports ("", ARDOUR::DataType::MIDI, ARDOUR::PortFlags (ARDOUR::IsOutput|ARDOUR::IsPhysical), midi_inputs);
	ARDOUR::AudioEngine::instance()->get_ports ("", ARDOUR::DataType::MIDI, ARDOUR::PortFlags (ARDOUR::IsInput|ARDOUR::IsPhysical), midi_outputs);

	input_combos.clear ();
	output_combos.clear ();

	if (!_cp.device_info().uses_ipmidi()) {

		for (uint32_t n = 0; n < n_surfaces; ++n) {

			boost::shared_ptr<Surface> surface = _cp.nth_surface (n);

			if (!surface) {
				PBD::fatal << string_compose (_("programming error: %1\n"), string_compose ("n=%1 surface not found!", n)) << endmsg;
				abort (); /*NOTREACHED*/
			}

			Gtk::ComboBox* input_combo = manage (new Gtk::ComboBox);
			Gtk::ComboBox* output_combo = manage (new Gtk::ComboBox);

			update_port_combos (midi_inputs, midi_outputs, input_combo, output_combo, surface);

			input_combo->pack_start (midi_port_columns.short_name);
			input_combo->set_data ("surface", surface.get());
			input_combos.push_back (input_combo);
			output_combo->pack_start (midi_port_columns.short_name);
			output_combo->set_data ("surface", surface.get());
			output_combos.push_back (output_combo);

			boost::weak_ptr<Surface> ws (surface);
			input_combo->signal_changed().connect (sigc::bind (sigc::mem_fun (*this, &MackieControlProtocolGUI::active_port_changed), input_combo, ws, true));
			output_combo->signal_changed().connect (sigc::bind (sigc::mem_fun (*this, &MackieControlProtocolGUI::active_port_changed), output_combo, ws, false));

			string send_string;
			string receive_string;

			if (n_surfaces > 1) {
				if (n == main_pos) {
					send_string = string_compose(_("Main surface at position %1 sends via:"), n + 1);
					receive_string = string_compose(_("Main surface at position %1 receives via:"), n + 1);
				} else {
					send_string = string_compose (_("Extender at position %1 sends via:"), n + 1);
					receive_string = string_compose (_("Extender at position %1 receives via:"), n + 1);
				}
			} else {
				send_string = _("Surface sends via:");
				receive_string = _("Surface receives via:");
			}

			l = manage (new Gtk::Label (send_string));
			l->set_alignment (1.0, 0.5);
			dd_table->attach (*l, 0, 1, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
			dd_table->attach (*input_combo, 1, 2, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
			row++;

			l = manage (new Gtk::Label (receive_string));
			l->set_alignment (1.0, 0.5);
			dd_table->attach (*l, 0, 1, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
			dd_table->attach (*output_combo, 1, 2, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
			row++;
		}

	} else {

		l = manage (new Gtk::Label (_("ipMIDI Port (lowest)")));
		l->set_alignment (1.0, 0.5);

		Gtk::SpinButton*  ipmidi_base_port_spinner = manage (new Gtk::SpinButton (ipmidi_base_port_adjustment));
		dd_table->attach (*l, 0, 1, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
		dd_table->attach (*ipmidi_base_port_spinner, 1, 2, row, row+1, AttachOptions(FILL|EXPAND), AttachOptions (0));
		row++;
	}

	return dd_table;
}

CellRendererCombo*
MackieControlProtocolGUI::make_action_renderer (Glib::RefPtr<TreeStore> model, Gtk::TreeModelColumnBase column)
{
	CellRendererCombo* renderer = manage (new CellRendererCombo);
	renderer->property_model() = model;
	renderer->property_editable() = true;
	renderer->property_text_column() = 0;
	renderer->property_has_entry() = false;
	renderer->signal_changed().connect (sigc::bind (sigc::mem_fun(*this, &MackieControlProtocolGUI::action_changed), column));

	return renderer;
}

void
MackieControlProtocolGUI::build_function_key_editor ()
{
	function_key_editor.append_column (_("Key"), function_key_columns.name);

	TreeViewColumn* col;
	CellRendererCombo* renderer;

	renderer = make_action_renderer (action_model.model(), function_key_columns.plain);
	col = manage (new TreeViewColumn (_("Plain"), *renderer));
	col->add_attribute (renderer->property_text(), function_key_columns.plain);
	function_key_editor.append_column (*col);

	renderer = make_action_renderer (action_model.model(), function_key_columns.shift);
	col = manage (new TreeViewColumn (_("Shift"), *renderer));
	col->add_attribute (renderer->property_text(), function_key_columns.shift);
	function_key_editor.append_column (*col);

	renderer = make_action_renderer (action_model.model(), function_key_columns.control);
	col = manage (new TreeViewColumn (_("Control"), *renderer));
	col->add_attribute (renderer->property_text(), function_key_columns.control);
	function_key_editor.append_column (*col);

	renderer = make_action_renderer (action_model.model(), function_key_columns.option);
	col = manage (new TreeViewColumn (_("Option"), *renderer));
	col->add_attribute (renderer->property_text(), function_key_columns.option);
	function_key_editor.append_column (*col);

	renderer = make_action_renderer (action_model.model(), function_key_columns.cmdalt);
	col = manage (new TreeViewColumn (_("Cmd/Alt"), *renderer));
	col->add_attribute (renderer->property_text(), function_key_columns.cmdalt);
	function_key_editor.append_column (*col);

	renderer = make_action_renderer (action_model.model(), function_key_columns.shiftcontrol);
	col = manage (new TreeViewColumn (_("Shift+Control"), *renderer));
	col->add_attribute (renderer->property_text(), function_key_columns.shiftcontrol);
	function_key_editor.append_column (*col);

	function_key_model = ListStore::create (function_key_columns);
	function_key_editor.set_model (function_key_model);
}

void
MackieControlProtocolGUI::refresh_function_key_editor ()
{
	function_key_editor.set_model (Glib::RefPtr<TreeModel>());
	function_key_model->clear ();

	/* now fill with data */

	TreeModel::Row row;
	DeviceProfile dp (_cp.device_profile());
	DeviceInfo di (_cp.device_info());

	for (int n = 0; n < Mackie::Button::FinalGlobalButton; ++n) {

		Mackie::Button::ID bid = (Mackie::Button::ID) n;

		row = *(function_key_model->append());
		if (di.global_buttons().find (bid) == di.global_buttons().end()) {
			row[function_key_columns.name] = Mackie::Button::id_to_name (bid);
		} else {
			row[function_key_columns.name] = di.get_global_button_name (bid) + "*";
		}
		row[function_key_columns.id] = bid;

		Glib::RefPtr<Gtk::Action> act;
		string action;
		const string defstring = "\u2022";

		/* We only allow plain bindings for Fn keys. All others are
		 * reserved for hard-coded actions.
		 */

		if (bid >= Mackie::Button::F1 && bid <= Mackie::Button::F8) {

			action = dp.get_button_action (bid, 0);
			if (action.empty()) {
				row[function_key_columns.plain] = defstring;
			} else {
				if (action.find ('/') == string::npos) {
					/* Probably a key alias */
					row[function_key_columns.plain] = action;
				} else {

					act = ActionManager::get_action (action, false);
					if (act) {
						row[function_key_columns.plain] = act->get_label();
					} else {
						row[function_key_columns.plain] = defstring;
					}
				}
			}
		}

		/* We only allow plain bindings for Fn keys. All others are
		 * reserved for hard-coded actions.
		 */

		if (bid >= Mackie::Button::F1 && bid <= Mackie::Button::F8) {

			action = dp.get_button_action (bid, MackieControlProtocol::MODIFIER_SHIFT);
			if (action.empty()) {
				row[function_key_columns.shift] = defstring;
			} else {
				if (action.find ('/') == string::npos) {
					/* Probably a key alias */
					row[function_key_columns.shift] = action;
				} else {
					act = ActionManager::get_action (action, false);
					if (act) {
						row[function_key_columns.shift] = act->get_label();
					} else {
						row[function_key_columns.shift] = defstring;
					}
				}
			}
		}

		action = dp.get_button_action (bid, MackieControlProtocol::MODIFIER_CONTROL);
		if (action.empty()) {
			row[function_key_columns.control] = defstring;
		} else {
			if (action.find ('/') == string::npos) {
				/* Probably a key alias */
				row[function_key_columns.control] = action;
			} else {
				act = ActionManager::get_action (action, false);
				if (act) {
					row[function_key_columns.control] = act->get_label();
				} else {
					row[function_key_columns.control] = defstring;
				}
			}
		}

		action = dp.get_button_action (bid, MackieControlProtocol::MODIFIER_OPTION);
		if (action.empty()) {
			row[function_key_columns.option] = defstring;
		} else {
			if (action.find ('/') == string::npos) {
				/* Probably a key alias */
				row[function_key_columns.option] = action;
			} else {
				act = ActionManager::get_action (action, false);
				if (act) {
					row[function_key_columns.option] = act->get_label();
				} else {
					row[function_key_columns.option] = defstring;
				}
			}
		}

		action = dp.get_button_action (bid, MackieControlProtocol::MODIFIER_CMDALT);
		if (action.empty()) {
			row[function_key_columns.cmdalt] = defstring;
		} else {
			if (action.find ('/') == string::npos) {
				/* Probably a key alias */
				row[function_key_columns.cmdalt] = action;
			} else {
				act = ActionManager::get_action (action, false);
				if (act) {
					row[function_key_columns.cmdalt] = act->get_label();
				} else {
					row[function_key_columns.cmdalt] = defstring;
				}
			}
		}

		action = dp.get_button_action (bid, (MackieControlProtocol::MODIFIER_SHIFT|MackieControlProtocol::MODIFIER_CONTROL));
		if (action.empty()) {
			row[function_key_columns.shiftcontrol] = defstring;
		} else {
			act = ActionManager::get_action (action, false);
			if (act) {
				row[function_key_columns.shiftcontrol] = act->get_label();
			} else {
				row[function_key_columns.shiftcontrol] = defstring;
			}
		}
	}

	function_key_editor.set_model (function_key_model);
}

void
MackieControlProtocolGUI::action_changed (const Glib::ustring &sPath, const TreeModel::iterator & iter, TreeModelColumnBase col)
{
	string action_path = (*iter)[action_model.columns().path];

	// Remove Binding is not in the action map but still valid

	bool remove = false;

	if (action_path == "Remove Binding") {
		remove = true;
	}

	Gtk::TreePath path(sPath);
	Gtk::TreeModel::iterator row = function_key_model->get_iter(path);

	if (row) {

		Glib::RefPtr<Gtk::Action> act = ActionManager::get_action (action_path, false);

		if (!act) {
			cerr << action_path << " not found in action map\n";
			if (!remove) {
				return;
			}
		}

		if (act || remove) {
			/* update visible text, using string supplied by
			   available action model so that it matches and is found
			   within the model.
			*/
			if (remove) {
				Glib::ustring dot = "\u2022";
				(*row).set_value (col.index(), dot);
			} else {
				(*row).set_value (col.index(), act->get_label());
			}

			/* update the current DeviceProfile, using the full
			 * path
			 */

			int modifier;

			switch (col.index()) {
			case 3:
				modifier = MackieControlProtocol::MODIFIER_SHIFT;
				break;
			case 4:
				modifier = MackieControlProtocol::MODIFIER_CONTROL;
				break;
			case 5:
				modifier = MackieControlProtocol::MODIFIER_OPTION;
				break;
			case 6:
				modifier = MackieControlProtocol::MODIFIER_CMDALT;
				break;
			case 7:
				modifier = (MackieControlProtocol::MODIFIER_SHIFT|MackieControlProtocol::MODIFIER_CONTROL);
				break;
			default:
				modifier = 0;
			}

			if (remove) {
				_cp.device_profile().set_button_action ((*row)[function_key_columns.id], modifier, "");
			} else {
				_cp.device_profile().set_button_action ((*row)[function_key_columns.id], modifier, action_path);
			}

			_ignore_profile_changed = true;
			_profile_combo.set_active_text ( _cp.device_profile().name() );
			_ignore_profile_changed = false;

		} else {
			std::cerr << "no such action\n";
		}
	}
}

void
MackieControlProtocolGUI::surface_combo_changed ()
{
	_cp.set_device (_surface_combo.get_active_text(), false);
}

void
MackieControlProtocolGUI::device_changed ()
{
	if (_device_dependent_widget) {
		table.remove (*_device_dependent_widget);
		_device_dependent_widget = 0;
	}

	_device_dependent_widget = device_dependent_widget ();
	_device_dependent_widget->show_all ();

	table.attach (*_device_dependent_widget, 0, 12, device_dependent_row, device_dependent_row+1, AttachOptions(0), AttachOptions(0), 0, 0);

	refresh_function_key_editor ();
}

void
MackieControlProtocolGUI::profile_combo_changed ()
{
	if (!_ignore_profile_changed) {
		string profile = _profile_combo.get_active_text();

		_cp.set_profile (profile);

		refresh_function_key_editor ();
	}
}

void
MackieControlProtocolGUI::ipmidi_spinner_changed ()
{
	_cp.set_ipmidi_base ((int16_t) lrintf (ipmidi_base_port_adjustment.get_value()));
}

void
MackieControlProtocolGUI::discover_clicked ()
{
	/* this should help to get things started */
	_cp.ping_devices ();
}

void
MackieControlProtocolGUI::recalibrate_faders ()
{
	_cp.recalibrate_faders ();
}

void
MackieControlProtocolGUI::toggle_backlight ()
{
	_cp.toggle_backlight ();
}

void
MackieControlProtocolGUI::touch_sensitive_change ()
{
	int sensitivity = (int) touch_sensitivity_adjustment.get_value ();
	_cp.set_touch_sensitivity (sensitivity);
}

Glib::RefPtr<Gtk::ListStore>
MackieControlProtocolGUI::build_midi_port_list (vector<string> const & ports, bool for_input)
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
MackieControlProtocolGUI::active_port_changed (Gtk::ComboBox* combo, boost::weak_ptr<Surface> ws, bool for_input)
{
	if (ignore_active_change) {
		return;
	}

	boost::shared_ptr<Surface> surface = ws.lock();

	if (!surface) {
		return;
	}

	TreeModel::iterator active = combo->get_active ();
	string new_port = (*active)[midi_port_columns.full_name];

	if (new_port.empty()) {
		if (for_input) {
			surface->port().input().disconnect_all ();
		} else {
			surface->port().output().disconnect_all ();
		}

		return;
	}

	if (for_input) {
		if (!surface->port().input().connected_to (new_port)) {
			surface->port().input().disconnect_all ();
			surface->port().input().connect (new_port);
		}
	} else {
		if (!surface->port().output().connected_to (new_port)) {
			surface->port().output().disconnect_all ();
			surface->port().output().connect (new_port);
		}
	}
}
