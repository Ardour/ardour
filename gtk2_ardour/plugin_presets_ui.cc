/*
 * Copyright (C) 2018-2019 Robin Gareus <robin@gareus.org>
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

#include <gtkmm/box.h>
#include <gtkmm/frame.h>

#include "gtkmm2ext/utils.h"

#include "ardour/plugin.h"
#include "gui_thread.h"

#include "plugin_presets_ui.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace Gtk;

PluginPresetsUI::PluginPresetsUI (boost::shared_ptr<PluginInsert> insert)
	: _insert (insert)
	, _load_button (_("Load"))
{
	_filter_banks_model = TreeStore::create (_filter_banks_columns);
	_filter_banks_display.set_model (_filter_banks_model);
	_filter_banks_display.set_headers_visible (true);
	_filter_banks_display.get_selection ()->set_mode (SELECTION_BROWSE);
	_filter_banks_display.get_selection ()->signal_changed ().connect (sigc::mem_fun (*this, &PluginPresetsUI::filter_presets));
	_filter_banks_display.set_sensitive (true);
	_filter_banks_display.append_column (_("Bank/Vendor"), _filter_banks_columns.name);
	_banks_scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);
	_banks_scroller.add (_filter_banks_display);
	_banks_scroller.set_no_show_all (true);

	_filter_types_model = TreeStore::create (_filter_types_columns);
	_filter_types_display.set_model (_filter_types_model);
	_filter_types_display.set_headers_visible (true);
	_filter_types_display.get_selection ()->set_mode (SELECTION_BROWSE);
	_filter_types_display.get_selection ()->signal_changed ().connect (sigc::mem_fun (*this, &PluginPresetsUI::filter_presets));
	_filter_types_display.set_sensitive (true);
	_filter_types_display.append_column (_("Type/Category"), _filter_types_columns.name);
	_types_scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);
	_types_scroller.add (_filter_types_display);
	_types_scroller.set_no_show_all (true);

	_plugin_preset_model = TreeStore::create (_plugin_preset_columns);
	_plugin_preset_display.set_model (_plugin_preset_model);
	_plugin_preset_display.set_headers_visible (true);
	_plugin_preset_display.get_selection ()->set_mode (SELECTION_BROWSE);
	_plugin_preset_display.get_selection ()->signal_changed ().connect (sigc::mem_fun (*this, &PluginPresetsUI::preset_selected));
	_plugin_preset_display.signal_row_activated ().connect (sigc::mem_fun (*this, &PluginPresetsUI::preset_row_activated));
	_plugin_preset_display.set_sensitive (true);

	CellRendererText* label_render = manage (new CellRendererText());
	TreeView::Column* label_col = manage (new TreeView::Column (_("Preset"), *label_render));
	label_col->add_attribute (label_render->property_markup(), _plugin_preset_columns.name);
	_plugin_preset_display.append_column (*label_col);

	_preset_desc.set_editable (false);
	_preset_desc.set_can_focus (false);
	_preset_desc.set_wrap_mode (WRAP_WORD);
	_preset_desc.set_size_request (300,200);
	_preset_desc.set_name (X_("TextOnBackground"));
	_preset_desc.set_border_width (15);

	Frame* frame = manage (new Frame);
	frame->set_label (_("Description"));
	frame->add (_preset_desc);

	_preset_scroller.set_policy (POLICY_NEVER, POLICY_AUTOMATIC);
	_preset_scroller.add (_plugin_preset_display);

	_load_button.set_name ("generic button");
	_load_button.signal_clicked.connect (sigc::mem_fun (*this, &PluginPresetsUI::load_preset));
	_load_button.set_sensitive (false);

	Box* filter_box = manage (new VBox ());
	filter_box->pack_start (_banks_scroller);
	filter_box->pack_start (_types_scroller);

	attach (*filter_box,      0, 1, 0, 2, FILL, EXPAND|FILL, 2, 0);
	attach (_preset_scroller, 1, 2, 0, 2, FILL, EXPAND|FILL, 2, 0);
	attach (*frame,           2, 3, 0, 1, EXPAND|FILL, EXPAND|FILL, 2, 4);
	attach (_load_button,     2, 3, 1, 2, FILL, SHRINK, 2, 0);

	boost::shared_ptr<Plugin> plugin (_insert->plugin ());

	plugin->PresetAdded.connect (_preset_connections, invalidator (*this), boost::bind (&PluginPresetsUI::update_preset_list, this), gui_context ());
	plugin->PresetRemoved.connect (_preset_connections, invalidator (*this), boost::bind (&PluginPresetsUI::update_preset_list, this), gui_context ());

	plugin->PresetLoaded.connect (_preset_connections, invalidator (*this), boost::bind (&PluginPresetsUI::filter_presets, this), gui_context ());
	plugin->PresetDirty.connect (_preset_connections, invalidator (*this), boost::bind (&PluginPresetsUI::filter_presets, this), gui_context ());

	update_preset_list ();
}

void
PluginPresetsUI::update_preset_list ()
{
	boost::shared_ptr<Plugin> plugin (_insert->plugin ());
	std::vector<Plugin::PresetRecord> presets = plugin->get_presets ();

	_pps.clear ();
	std::map<std::string, size_t> banks; // or vendors
	std::map<std::string, size_t> types; // or categories

	for (std::vector<Plugin::PresetRecord>::const_iterator i = presets.begin (); i != presets.end (); ++i) {
		++banks[_("-All-")];
		++types[_("-All-")];
		if (i->user) {
			++banks[_("-User-")];
			_pps.push_back (PluginPreset(*i, _("-User-")));
			continue;
		}

		std::string l (i->label);
		std::vector<std::string> cat;
		size_t pos = 0;
		while ((pos = l.find(" - ")) != std::string::npos) {
			cat.push_back (l.substr (0, pos));
			l.erase (0, pos + 3);
			if (cat.size() > 1) {
				break;
			}
		}
		if (cat.size() > 1) {
			++banks[cat.at (0)];
			++types[cat.at (1)];
			_pps.push_back (PluginPreset(*i, cat.at (0), cat.at (1)));
		} else if (cat.size() > 0) {
			++banks[cat.at (0)];
			_pps.push_back (PluginPreset(*i, cat.at (0)));
		} else {
			_pps.push_back (PluginPreset(*i));
		}
	}

	if (types.size() > 2) {
		std::string selected_type;
		if (_filter_types_display.get_selection ()->count_selected_rows () == 1) {
			TreeIter iter = _filter_types_display.get_selection ()->get_selected ();
			selected_type = (*iter)[_filter_types_columns.name];
		} else {
			selected_type = Gtkmm2ext::markup_escape_text(_("-All-"));
		}
		_filter_types_model->clear ();
		for (std::map<std::string, size_t>::const_iterator i = types.begin (); i != types.end(); ++i) {
			TreeModel::Row row = *(_filter_types_model->append ());
			row[_filter_types_columns.name] = Gtkmm2ext::markup_escape_text (i->first);
			row[_filter_types_columns.count] = i->second;
		}
		TreeModel::Children rows = _filter_types_model->children ();
		for (TreeModel::Children::iterator i = rows.begin (); i != rows.end (); ++i) {
			std::string const& name ((*i)[_filter_types_columns.name]);
			if (selected_type == name) {
				_filter_types_display.get_selection ()->select (*i);
				break;
			}
		}
		_filter_types_display.show_all ();
		_types_scroller.show ();
	} else {
		_filter_types_model->clear ();
		_types_scroller.hide ();
	}

	if (banks.size() > 2) {
		std::string selected_bank = Gtkmm2ext::markup_escape_text(_("-All-"));
		if (_filter_banks_display.get_selection ()->count_selected_rows () == 1) {
			TreeIter iter = _filter_banks_display.get_selection ()->get_selected ();
			selected_bank = (*iter)[_filter_banks_columns.name];
		}
		_filter_banks_model->clear ();
		for (std::map<std::string, size_t>::const_iterator i = banks.begin (); i != banks.end(); ++i) {
			TreeModel::Row row = *(_filter_banks_model->append ());
			row[_filter_banks_columns.name] = Gtkmm2ext::markup_escape_text (i->first);
			row[_filter_banks_columns.count] = i->second;
		}
		TreeModel::Children rows = _filter_banks_model->children ();
		for (TreeModel::Children::iterator i = rows.begin (); i != rows.end (); ++i) {
			std::string const& name ((*i)[_filter_banks_columns.name]);
			if (selected_bank == name) {
				_filter_banks_display.get_selection ()->select (*i);
				break;
			}
		}
		_filter_banks_display.show_all ();
		_banks_scroller.show ();
	} else {
		_filter_banks_model->clear ();
		_banks_scroller.hide ();
	}

	std::sort (_pps.begin(), _pps.end());

	filter_presets ();
}

void
PluginPresetsUI::filter_presets ()
{
	bool user_only = false;
	std::string selected_bank;
	if (_filter_banks_display.get_selection ()->count_selected_rows () == 1) {
		TreeIter iter = _filter_banks_display.get_selection ()->get_selected ();
		selected_bank = (*iter)[_filter_banks_columns.name];
		if (_("-All-") == selected_bank) {
			selected_bank = "";
		}
		if (_("-User-") == selected_bank) {
			selected_bank = "";
			user_only = true;
		}
	}

	std::string selected_type;
	if (_filter_types_display.get_selection ()->count_selected_rows () == 1) {
		TreeIter iter = _filter_types_display.get_selection ()->get_selected ();
		selected_type = (*iter)[_filter_types_columns.name];
		if (_("-All-") == selected_type) {
			selected_type = "";
		}
	}

	boost::shared_ptr<Plugin> plugin (_insert->plugin ());
	Plugin::PresetRecord const& p = plugin->last_preset ();

	std::string selected_uri = p.valid ? p.uri : "";
	if (_plugin_preset_display.get_selection ()->count_selected_rows () == 1) {
		TreeIter iter = _plugin_preset_display.get_selection ()->get_selected ();
		ARDOUR::Plugin::PresetRecord const& ppr ((*iter)[_plugin_preset_columns.plugin_preset]);
		selected_uri = ppr.uri;
	}

	_plugin_preset_model->clear ();
	bool const modified = plugin->parameter_changed_since_last_preset ();

	for (std::vector<PluginPreset>::const_iterator i = _pps.begin (); i != _pps.end (); ++i) {
		if (!selected_type.empty() && i->_type != selected_type) {
			continue;
		}
		if (!selected_bank.empty() && i->_bank != selected_bank) {
			continue;
		}

		ARDOUR::Plugin::PresetRecord const& ppr (i->_preset_record);

		if (user_only && !ppr.user) {
			continue;
		}

		TreeModel::Row row = *(_plugin_preset_model->append ());
		if (p.uri == ppr.uri && !modified) {
			row[_plugin_preset_columns.name] = string_compose ("<span weight=\"bold\"  background=\"green\">%1</span>", Gtkmm2ext::markup_escape_text (ppr.label));
		} else {
			row[_plugin_preset_columns.name] = Gtkmm2ext::markup_escape_text (ppr.label);
		}
		row[_plugin_preset_columns.description] = ppr.description;
		row[_plugin_preset_columns.plugin_preset] = ppr;
	}

	int path = 0;
	TreeModel::Children rows = _plugin_preset_model->children ();
	for (TreeModel::Children::iterator i = rows.begin (); i != rows.end (); ++i, ++path) {
		ARDOUR::Plugin::PresetRecord const& ppr ((*i)[_plugin_preset_columns.plugin_preset]);
		if (ppr.uri == selected_uri) {
			_plugin_preset_display.get_selection ()->select (*i);
			char row_path[21];
			snprintf(row_path, 21, "%d", path);
			_plugin_preset_display.scroll_to_row (Gtk::TreePath(row_path));
			break;
		}
	}
}

void
PluginPresetsUI::preset_selected ()
{
	if (_plugin_preset_display.get_selection ()->count_selected_rows () != 1) {
		_preset_desc.get_buffer ()->set_text ("");
		_load_button.set_sensitive (false);
		return;
	}

	TreeIter iter = _plugin_preset_display.get_selection ()->get_selected ();
	assert (iter);
	ARDOUR::Plugin::PresetRecord const& ppr ((*iter)[_plugin_preset_columns.plugin_preset]);

	std::string d;
	if (!ppr.valid) {
		d = "-";
	} else if (ppr.user) {
		d = _("(user preset)");
	} else {
		d = (*iter)[_plugin_preset_columns.description];
	}
	_preset_desc.get_buffer ()->set_text (d);

	Plugin::PresetRecord const& p = _insert->plugin ()->last_preset ();
	_load_button.set_sensitive (ppr.valid && !(p.valid && p.uri == ppr.uri));
}

void
PluginPresetsUI::preset_row_activated (Gtk::TreeModel::Path, Gtk::TreeViewColumn*)
{
	if (_load_button.get_sensitive ()) {
		load_preset ();
	}
}

void
PluginPresetsUI::load_preset ()
{
	if (_plugin_preset_display.get_selection ()->count_selected_rows () != 1) {
		return;
	}

	TreeIter iter = _plugin_preset_display.get_selection ()->get_selected ();
	ARDOUR::Plugin::PresetRecord const& ppr ((*iter)[_plugin_preset_columns.plugin_preset]);
	if (ppr.valid) {
		_insert->load_preset (ppr);
	}
}
