/*
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2017 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2018 Len Ovens <len@ovenwerks.net>
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

#ifndef __gtk_ardour_add_route_dialog_h__
#define __gtk_ardour_add_route_dialog_h__

#include <string>

#include <gtkmm/entry.h>
#include <gtkmm/dialog.h>
#include <gtkmm/frame.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/button.h>
#include <gtkmm/combobox.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/textview.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treeview.h>
#include <gtkmm/treestore.h>
#include <gtkmm/liststore.h>
#include <gtkmm/scrolledwindow.h>

#include "ardour/plugin.h"
#include "ardour/types.h"
#include "ardour/template_utils.h"

#include "ardour_dialog.h"
#include "instrument_selector.h"
#include "route_dialogs.h"

class Editor;
class RouteGroupDialog;

class AddRouteDialog : public ArdourDialog
{
public:
	AddRouteDialog ();
	~AddRouteDialog ();

	enum ResponseId {
		Add,
		AddAndClose,
	};

	enum TypeWanted {
		AudioTrack,
		MidiTrack,
		AudioBus,
		MidiBus,
		VCAMaster,
		FoldbackBus,
	};
	TypeWanted type_wanted();

	ARDOUR::ChanCount channels ();
	uint32_t channel_count ();
	int count ();

	std::string name_template () const;
	bool name_template_is_default () const;
	ARDOUR::PluginInfoPtr requested_instrument ();

	ARDOUR::TrackMode mode();
	ARDOUR::RouteGroup* route_group ();

	RouteDialogs::InsertAt insert_at();
	bool use_strict_io();

	std::string get_template_path();

	void reset_name_edited () { name_edited_by_user = false; }

private:
	Gtk::Entry name_template_entry;
	Gtk::Adjustment routes_adjustment;
	Gtk::SpinButton routes_spinner;
	Gtk::ComboBoxText channel_combo;
	Gtk::Label configuration_label;
	Gtk::Label manual_label;
	Gtk::Label add_label;
	Gtk::Label name_label;
	Gtk::Label group_label;
	Gtk::Label insert_label;
	Gtk::Label strict_io_label;
	Gtk::Label mode_label;
	Gtk::Label instrument_label;
	Gtk::ComboBoxText mode_combo;
	Gtk::ComboBoxText route_group_combo;
	InstrumentSelector instrument_combo;
	Gtk::ComboBoxText insert_at_combo;
	Gtk::ComboBoxText strict_io_combo;

	void track_type_chosen ();
	void refill_channel_setups ();
	void refill_route_groups ();
	void refill_track_modes ();
	void add_route_group (ARDOUR::RouteGroup *);
	void group_changed ();
	void channel_combo_changed ();
	bool channel_separator (const Glib::RefPtr<Gtk::TreeModel> &m, const Gtk::TreeModel::iterator &i);
	bool route_separator (const Glib::RefPtr<Gtk::TreeModel> &m, const Gtk::TreeModel::iterator &i);
	void maybe_update_name_template_entry ();
	void instrument_changed ();

	struct TrackTemplateColumns : public Gtk::TreeModel::ColumnRecord {
		TrackTemplateColumns () {
			add (name);
			add (path);
			add (description);
			add (modified_with);
		}

		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<std::string> path;
		Gtk::TreeModelColumn<std::string> description;
		Gtk::TreeModelColumn<std::string> modified_with;
	};

	TrackTemplateColumns track_template_columns;

	Glib::RefPtr<Gtk::TreeStore>  trk_template_model;
	Gtk::TreeView                 trk_template_chooser;

	void trk_template_row_selected ();

	Gtk::TextView trk_template_desc;
	Gtk::Frame    trk_template_outer_frame;
	Gtk::Frame    trk_template_desc_frame;

	void reset_template_option_visibility ();
	void new_group_dialog_finished (int, RouteGroupDialog*);
	void on_show ();
	void on_response (int);

	struct ChannelSetup {
		std::string name;
		uint32_t    channels;
	};

	typedef std::vector<ChannelSetup> ChannelSetups;
	ChannelSetups channel_setups;

	int last_route_count;
	bool route_count_set_by_template;

	static std::vector<std::pair<std::string, std::string> > builtin_types;
	static std::vector<std::string> channel_combo_strings;
	static std::vector<std::string> bus_mode_strings;

	bool name_edited_by_user;
	void name_template_entry_insertion (Glib::ustring const &,int*);
	void name_template_entry_deletion (int, int);
};

#endif /* __gtk_ardour_add_route_dialog_h__ */
