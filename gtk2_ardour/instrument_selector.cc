/*
  Copyright (C) 2003-2014 Paul Davis

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

#include "ardour/plugin_manager.h"

#include "instrument_selector.h"

#include "i18n.h"

using namespace Gtk;
using namespace ARDOUR;

InstrumentSelector::InstrumentSelector()
	: _reasonable_synth_id(0)
{
	build_instrument_list();
	set_model(_instrument_list);
	pack_start(_instrument_list_columns.name);
	set_active(_reasonable_synth_id);
	set_button_sensitivity(Gtk::SENSITIVITY_AUTO);
}

void
InstrumentSelector::build_instrument_list()
{
	PluginManager& manager = PluginManager::instance();

	PluginInfoList all_plugs;
	all_plugs.insert(all_plugs.end(), manager.ladspa_plugin_info().begin(), manager.ladspa_plugin_info().end());
#ifdef WINDOWS_VST_SUPPORT
	all_plugs.insert(all_plugs.end(), manager.windows_vst_plugin_info().begin(), manager.windows_vst_plugin_info().end());
#endif
#ifdef LXVST_SUPPORT
	all_plugs.insert(all_plugs.end(), manager.lxvst_plugin_info().begin(), manager.lxvst_plugin_info().end());
#endif
#ifdef AUDIOUNIT_SUPPORT
	all_plugs.insert(all_plugs.end(), manager.au_plugin_info().begin(), manager.au_plugin_info().end());
#endif
#ifdef LV2_SUPPORT
	all_plugs.insert(all_plugs.end(), manager.lv2_plugin_info().begin(), manager.lv2_plugin_info().end());
#endif

	_instrument_list = ListStore::create(_instrument_list_columns);

	TreeModel::Row row = *(_instrument_list->append());
	row[_instrument_list_columns.info_ptr] = PluginInfoPtr();
	row[_instrument_list_columns.name]     = _("-none-");

	uint32_t n = 1;
	for (PluginInfoList::const_iterator i = all_plugs.begin(); i != all_plugs.end(); ++i) {
		if (manager.get_status(*i) == PluginManager::Hidden) {
			continue;
		}

		if ((*i)->is_instrument()) {
			row                                    = *(_instrument_list->append());
			row[_instrument_list_columns.name]     = (*i)->name;
			row[_instrument_list_columns.info_ptr] = *i;
			if ((*i)->unique_id == "https://community.ardour.org/node/7596") {
				_reasonable_synth_id = n;
			}
			n++;
		}
	}
}

PluginInfoPtr
InstrumentSelector::selected_instrument()
{
	TreeModel::iterator iter = get_active();
	if (!iter) {
		return PluginInfoPtr();
	}

	const TreeModel::Row& row = (*iter);
	return row[_instrument_list_columns.info_ptr];
}
