/*
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __gtk_ardour_instrument_selector_h__
#define __gtk_ardour_instrument_selector_h__

#include <string>

#include <gtkmm/combobox.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/liststore.h>

#include "pbd/signals.h"

#include "ardour/plugin.h"
#include "ardour/types.h"
#include "ardour/template_utils.h"

#include "ardour_dialog.h"

class Editor;

class InstrumentSelector : public Gtk::ComboBox
{
public:
	enum InstrumentListDisposition {
		ForAuditioner,    /* this will always return some synth, never 'None' */
		ForTrackDefault,  /* functionally same as ForAuditioner, but for tracks */
		ForTrackSelector  /* this provides the 'None' option so the user can add a synth later */
	};

	InstrumentSelector (InstrumentListDisposition);

	ARDOUR::PluginInfoPtr selected_instrument () const;
	std::string selected_instrument_name () const;

private:
	struct InstrumentListColumns : public Gtk::TreeModel::ColumnRecord {
		InstrumentListColumns() {
			add(name);
			add(info_ptr);
		}
		Gtk::TreeModelColumn<std::string>           name;
		Gtk::TreeModelColumn<ARDOUR::PluginInfoPtr> info_ptr;
	};

	void build_instrument_list();
	void refill();

	std::string					 _longest_instrument_name;

	Glib::RefPtr<Gtk::ListStore> _instrument_list;
	InstrumentListColumns        _instrument_list_columns;
	uint32_t                     _reasonable_synth_id;
	uint32_t                     _gmsynth_id;
	InstrumentListDisposition    _disposition;
	PBD::ScopedConnection        _update_connection;
};

#endif /* __gtk_ardour_instrument_selector_h__ */
