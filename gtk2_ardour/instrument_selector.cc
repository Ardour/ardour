/*
 * Copyright (C) 2016-2018 Robin Gareus <robin@gareus.org>
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

#include "pbd/convert.h"
#include "pbd/enumwriter.h"

#include "ardour/plugin_manager.h"
#include "gtkmm2ext/gui_thread.h"

#include "instrument_selector.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;

InstrumentSelector::InstrumentSelector (InstrumentListDisposition disp)
	: _reasonable_synth_id (0)
	, _gmsynth_id (UINT32_MAX)
	, _disposition (disp)
{
	refill ();

	PluginManager::instance ().PluginListChanged.connect (_update_connection, invalidator (*this), boost::bind (&InstrumentSelector::refill, this), gui_context());
}

void
InstrumentSelector::refill()
{
	TreeModel::iterator iter = get_active();
	std::string selected;
	if (iter) {
		const TreeModel::Row& row = (*iter);
		selected = row[_instrument_list_columns.name];
	}

	unset_model ();
	clear ();
	build_instrument_list();
	set_model(_instrument_list);
	pack_start(_instrument_list_columns.name);
	if (selected.empty ()) {
		if (_gmsynth_id != UINT32_MAX) {
			set_active(_gmsynth_id);
		} else {
			set_active(_reasonable_synth_id);
		}
	} else {
		TreeModel::Children rows = _instrument_list->children();
		TreeModel::Children::iterator i;
		for (i = rows.begin(); i != rows.end(); ++i) {
			std::string cn = (*i)[_instrument_list_columns.name];
			if (cn == selected) {
				set_active(*i);
				break;
			}
		}
	}
	set_button_sensitivity(Gtk::SENSITIVITY_AUTO);
}

static bool
pluginsort (const PluginInfoPtr& a, const PluginInfoPtr& b)
{
	return PBD::downcase(a->name) < PBD::downcase(b->name);
}

static bool
invalid_instrument (PluginInfoPtr p) {
	const PluginManager& manager = PluginManager::instance();
	if (manager.get_status(p) == PluginManager::Hidden) {
		return true;
	}
	if (manager.get_status(p) == PluginManager::Concealed) {
		return true;
	}
	return !p->is_instrument();
}

void
InstrumentSelector::build_instrument_list()
{
	PluginManager& manager = PluginManager::instance();

	PluginInfoList all_plugs;
	all_plugs.insert(all_plugs.end(), manager.ladspa_plugin_info().begin(), manager.ladspa_plugin_info().end());
	all_plugs.insert(all_plugs.end(), manager.lua_plugin_info().begin(), manager.lua_plugin_info().end());
	all_plugs.insert(all_plugs.end(), manager.lv2_plugin_info().begin(), manager.lv2_plugin_info().end());
#ifdef WINDOWS_VST_SUPPORT
	all_plugs.insert(all_plugs.end(), manager.windows_vst_plugin_info().begin(), manager.windows_vst_plugin_info().end());
#endif
#ifdef LXVST_SUPPORT
	all_plugs.insert(all_plugs.end(), manager.lxvst_plugin_info().begin(), manager.lxvst_plugin_info().end());
#endif
#ifdef MACVST_SUPPORT
	all_plugs.insert(all_plugs.end(), manager.mac_vst_plugin_info().begin(), manager.mac_vst_plugin_info().end());
#endif
#ifdef AUDIOUNIT_SUPPORT
	all_plugs.insert(all_plugs.end(), manager.au_plugin_info().begin(), manager.au_plugin_info().end());
#endif
#ifdef VST3_SUPPORT
	all_plugs.insert(all_plugs.end(), manager.vst3_plugin_info().begin(), manager.vst3_plugin_info().end());
#endif

	all_plugs.remove_if (invalid_instrument);
	all_plugs.sort (pluginsort);

	_instrument_list = ListStore::create(_instrument_list_columns);

	if (_disposition==ForTrackSelector) {
		TreeModel::Row row = *(_instrument_list->append());
		row[_instrument_list_columns.info_ptr] = PluginInfoPtr();
		row[_instrument_list_columns.name]     = _("-none-");
	}

	_longest_instrument_name = "";

	uint32_t n = (_disposition==ForTrackSelector) ? 1 : 0;
	std::string prev;
	for (PluginInfoList::const_iterator i = all_plugs.begin(); i != all_plugs.end(); ++i, ++n) {
		PluginInfoPtr p = *i;
		
		if (p->name.length() > _longest_instrument_name.length()) {
			_longest_instrument_name = p->name;
		}

		std::string suffix;

#ifdef MIXBUS
		uint32_t n_outs = p->max_configurable_ouputs ();
		if (n_outs > 2) {
			if (p->reconfigurable_io ()) {
				suffix = string_compose(_("\u2264 %1 outs"), n_outs);
			} else {
				suffix = string_compose(_("%1 outs"), n_outs);
			}
		}
#else
		if (p->multichannel_name_ambiguity) {
			uint32_t n_outs = p->max_configurable_ouputs ();
			if (n_outs > 2) {
				if (p->reconfigurable_io ()) {
					suffix = string_compose(_("\u2264 %1 outs"), n_outs);
				} else {
					suffix = string_compose(_("%1 outs"), n_outs);
				}
			} else if (n_outs == 2) {
				suffix = _("stereo");
			}
		}
#endif

		if (p->plugintype_name_ambiguity) {
			std::string pt = PluginManager::plugin_type_name (p->type);
			if (!suffix.empty ()) {
				suffix += ", ";
			}
			suffix += pt;
		}

		std::string name = p->name;
		if (!suffix.empty ()) {
			name += " (" + suffix + ")";
		}

		TreeModel::Row row = *(_instrument_list->append());
		row[_instrument_list_columns.name] = name;

		row[_instrument_list_columns.info_ptr] = p;
		if (p->unique_id == "https://community.ardour.org/node/7596") {
			_reasonable_synth_id = n;
		}
		if (p->unique_id == "http://gareus.org/oss/lv2/gmsynth") {
			_gmsynth_id = n;
		}
		prev = p->name;
	}
}

PluginInfoPtr
InstrumentSelector::selected_instrument() const
{
	TreeModel::iterator iter = get_active();
	if (!iter) {
		return PluginInfoPtr();
	}

	const TreeModel::Row& row = (*iter);
	return row[_instrument_list_columns.info_ptr];
}

std::string
InstrumentSelector::selected_instrument_name () const
{
	PluginInfoPtr pip = selected_instrument ();
	if (!pip) {
		return "";
	}
	return pip->name;
}
