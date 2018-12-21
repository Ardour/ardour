/*
 * Copyright (C) 2018 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "gtkmm2ext/utils.h"

#include "ardour/plugin.h"
#include "gui_thread.h"

#include "plugin_presets_ui.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

PluginPresetsUI::PluginPresetsUI (boost::shared_ptr<PluginInsert> insert)
	: _insert (insert)
	, _load_button (_("Load"))
{
	_plugin_preset_model = Gtk::TreeStore::create (_plugin_preset_columns);
	_plugin_preset_display.set_model (_plugin_preset_model);
	_plugin_preset_display.set_headers_visible (true);
	_plugin_preset_display.get_selection ()->set_mode (Gtk::SELECTION_SINGLE);
	_plugin_preset_display.get_selection ()->signal_changed ().connect (sigc::mem_fun (*this, &PluginPresetsUI::preset_selected));
	_plugin_preset_display.set_sensitive (true);

	Gtk::CellRendererText* label_render = Gtk::manage (new Gtk::CellRendererText());
	Gtk::TreeView::Column* label_col = Gtk::manage (new Gtk::TreeView::Column (_("Preset"), *label_render));
	label_col->add_attribute (label_render->property_markup(), _plugin_preset_columns.name);
	_plugin_preset_display.append_column (*label_col);

	_preset_desc.set_editable (false);
	_preset_desc.set_can_focus (false);
	_preset_desc.set_wrap_mode (Gtk::WRAP_WORD);
	_preset_desc.set_size_request (400,200);
	_preset_desc.set_name (X_("TextOnBackground"));
	_preset_desc.set_border_width (6);

	_preset_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
	_preset_scroller.add (_plugin_preset_display);

	_load_button.set_name ("generic button");
	_load_button.signal_clicked.connect (sigc::mem_fun (*this, &PluginPresetsUI::load_preset));
	_load_button.set_sensitive (false);

	attach (_preset_scroller, 0, 1, 0, 2, Gtk::FILL, Gtk::EXPAND|Gtk::FILL, 2, 0);
	attach (_preset_desc, 1, 2, 0, 1, Gtk::EXPAND|Gtk::FILL, Gtk::FILL, 2, 0);
	attach (_load_button, 1, 2, 1, 2, Gtk::FILL, Gtk::SHRINK, 2, 0);

	boost::shared_ptr<Plugin> plugin (_insert->plugin ());

	plugin->PresetAdded.connect (_preset_connections, invalidator (*this), boost::bind (&PluginPresetsUI::update_preset_list, this), gui_context ());
	plugin->PresetRemoved.connect (_preset_connections, invalidator (*this), boost::bind (&PluginPresetsUI::update_preset_list, this), gui_context ());
	plugin->PresetLoaded.connect (_preset_connections, invalidator (*this), boost::bind (&PluginPresetsUI::update_preset_list, this), gui_context ());

	update_preset_list ();
}

void
PluginPresetsUI::update_preset_list ()
{
	boost::shared_ptr<Plugin> plugin (_insert->plugin ());

	Plugin::PresetRecord const& p = plugin->last_preset ();
	std::vector<Plugin::PresetRecord> presets = plugin->get_presets ();


	std::string selected_uri;
	if (_plugin_preset_display.get_selection ()->count_selected_rows () == 1) {
		Gtk::TreeIter iter = _plugin_preset_display.get_selection ()->get_selected ();
		ARDOUR::Plugin::PresetRecord const& ppr ((*iter)[_plugin_preset_columns.plugin_preset]);
		selected_uri = ppr.uri;
	}

	_plugin_preset_model->clear ();

	bool found_active = false;

	for (std::vector<Plugin::PresetRecord>::const_iterator i = presets.begin (); i != presets.end (); ++i) {
		Gtk::TreeModel::Row row = *(_plugin_preset_model->append ());
		if (p.uri == i->uri) {
			row[_plugin_preset_columns.name] = string_compose ("<span weight=\"bold\"  background=\"green\">%1</span>", Gtkmm2ext::markup_escape_text (i->label));
			found_active = true;
		} else {
			row[_plugin_preset_columns.name] = Gtkmm2ext::markup_escape_text (i->label);
		}
		row[_plugin_preset_columns.description] = i->description;
		row[_plugin_preset_columns.plugin_preset] = *i;
	}

	{
		Gtk::TreeModel::Row row = *(_plugin_preset_model->prepend ());
		if (found_active) {
			row[_plugin_preset_columns.name] = _("(none)");
		} else {
			row[_plugin_preset_columns.name] = string_compose ("<span weight=\"bold\"  background=\"green\">%1</span>", _("(none)"));
		}
		row[_plugin_preset_columns.description] = "";
		row[_plugin_preset_columns.plugin_preset] = Plugin::PresetRecord ();
	}

	Gtk::TreeModel::Children rows = _plugin_preset_model->children ();
	for (Gtk::TreeModel::Children::iterator i = rows.begin (); i != rows.end (); ++i) {
		ARDOUR::Plugin::PresetRecord const& ppr ((*i)[_plugin_preset_columns.plugin_preset]);
		if (ppr.uri == selected_uri) {
			_plugin_preset_display.get_selection ()->select (*i);
			break;
		}
	}

}

void
PluginPresetsUI::preset_selected ()
{
	if (_plugin_preset_display.get_selection ()->count_selected_rows () != 1) {
		return;
	}

	Gtk::TreeIter iter = _plugin_preset_display.get_selection ()->get_selected ();
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
PluginPresetsUI::load_preset ()
{
	if (_plugin_preset_display.get_selection ()->count_selected_rows () != 1) {
		return;
	}

	Gtk::TreeIter iter = _plugin_preset_display.get_selection ()->get_selected ();
	ARDOUR::Plugin::PresetRecord const& ppr ((*iter)[_plugin_preset_columns.plugin_preset]);
	if (ppr.valid) {
		_insert->load_preset (ppr);
	}
}
