/*
 * Copyright (C) 2005-2006 Doug McLain <doug@nostar.net>
 * Copyright (C) 2005-2007 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2007-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2014 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
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
#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <cstdio>
#include <map>

#include <algorithm>

#include <gtkmm/button.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/frame.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/notebook.h>
#include <gtkmm/stock.h>
#include <gtkmm/table.h>
#include <gtkmm/treestore.h>

#include "gtkmm2ext/utils.h"

#include "widgets/tooltips.h"

#include "pbd/convert.h"
#include "pbd/tokenizer.h"

#include "ardour/utils.h"
#include "ardour/rc_configuration.h"

#include "ardour_message.h"
#include "plugin_scan_dialog.h"
#include "plugin_selector.h"
#include "ardour_ui.h"
#include "plugin_utils.h"
#include "gui_thread.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace std;
using namespace ArdourWidgets;
using namespace ARDOUR_PLUGIN_UTILS;

static const uint32_t MAX_CREATOR_LEN = 24;

PluginSelector::PluginSelector (PluginManager& mgr)
	: ArdourDialog (_("Plugin Selector"), true, false)
	, search_clear_button (Stock::CLEAR)
	, manager (mgr)
	, _need_tag_save (false)
	, _need_status_save (false)
	, _need_menu_rebuild (false)
	, _inhibit_refill (false)
{
	set_name ("PluginSelectorWindow");
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);

	_plugin_menu = 0;
	in_row_change = false;

	manager.PluginListChanged.connect (plugin_list_changed_connection, invalidator (*this), boost::bind (&PluginSelector::build_plugin_menu, this), gui_context());
	manager.PluginStatusChanged.connect (plugin_list_changed_connection, invalidator (*this), boost::bind (&PluginSelector::build_plugin_menu, this), gui_context());
	manager.PluginTagChanged.connect (plugin_list_changed_connection, invalidator (*this), boost::bind (&PluginSelector::build_plugin_menu, this), gui_context());

	manager.PluginStatusChanged.connect (plugin_list_changed_connection, invalidator (*this), boost::bind (&PluginSelector::plugin_status_changed, this, _1, _2, _3), gui_context());
	manager.PluginTagChanged.connect(plugin_list_changed_connection, invalidator (*this), boost::bind (&PluginSelector::tags_changed, this, _1, _2, _3), gui_context());

	plugin_model = Gtk::ListStore::create (plugin_columns);
	plugin_display.set_model (plugin_model);
	plugin_display.append_column (S_("Favorite|Fav"), plugin_columns.favorite);
	plugin_display.append_column (_("Name"), plugin_columns.name);
	plugin_display.append_column (_("Tags"), plugin_columns.tags);
	plugin_display.append_column (_("Creator"), plugin_columns.creator);
	plugin_display.append_column (_("Type"), plugin_columns.type_name);
	plugin_display.append_column (_("Audio I/O"),plugin_columns.audio_io);
	plugin_display.append_column (_("MIDI I/O"), plugin_columns.midi_io);
	plugin_display.set_headers_visible (true);
	plugin_display.set_headers_clickable (true);
	plugin_display.set_reorderable (false);
	plugin_display.set_rules_hint (true);
	plugin_display.add_object_drag (plugin_columns.plugin.index(), "x-ardour/plugin.info");
	plugin_display.set_drag_column (plugin_columns.name.index());

	// setting a sort-column prevents re-ordering via Drag/Drop
	plugin_model->set_sort_column (plugin_columns.name.index(), Gtk::SORT_ASCENDING);

	plugin_display.set_name("PluginSelectorDisplay");
	plugin_display.signal_row_activated().connect_notify (sigc::mem_fun(*this, &PluginSelector::row_activated));
	plugin_display.get_selection()->signal_changed().connect (sigc::mem_fun(*this, &PluginSelector::display_selection_changed));

	CellRendererToggle* fav_cell = dynamic_cast<CellRendererToggle*>(plugin_display.get_column_cell_renderer (0));
	fav_cell->property_activatable() = true;
	fav_cell->signal_toggled().connect (sigc::mem_fun (*this, &PluginSelector::favorite_changed));

	scroller.set_border_width(10);
	scroller.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	scroller.add(plugin_display);

	amodel = Gtk::ListStore::create(acols);
	added_list.set_model (amodel);
	added_list.append_column (_("Plugins to be connected"), acols.text);
	added_list.set_headers_visible (true);
	added_list.set_reorderable (false);

	for (int i = 2; i <= 7; ++i) {
		Gtk::TreeView::Column* column = plugin_display.get_column(i);
		if (column) {
			column->set_sort_column(i);
		}
	}

	ascroller.set_border_width(10);
	ascroller.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	ascroller.add(added_list);
	btn_add = manage(new Gtk::Button(Stock::ADD));
	set_tooltip(*btn_add, _("Add a plugin to the effect list"));
	btn_add->set_sensitive (false);
	btn_remove = manage(new Gtk::Button(Stock::REMOVE));
	btn_remove->set_sensitive (false);
	set_tooltip(*btn_remove, _("Remove a plugin from the effect list"));

	btn_add->set_name("PluginSelectorButton");
	btn_remove->set_name("PluginSelectorButton");

	/* SEARCH */

	Gtk::Table* search_table = manage(new Gtk::Table(2, 2));

	search_entry.signal_changed().connect (sigc::mem_fun (*this, &PluginSelector::search_entry_changed));
	search_clear_button.signal_clicked().connect (sigc::mem_fun (*this, &PluginSelector::search_clear_button_clicked));

	_search_name_checkbox = manage (new ArdourButton (_("Name"), ArdourButton::led_default_elements, true));
	_search_name_checkbox->set_active(true);
	_search_name_checkbox->set_name ("pluginlist filter button");

	_search_tags_checkbox = manage (new ArdourButton (_("Tags"), ArdourButton::led_default_elements, true));
	_search_tags_checkbox->set_active(true);
	_search_tags_checkbox->set_name ("pluginlist filter button");

	_search_ignore_checkbox = manage (new ArdourButton(_("Ignore Filters when searching"), ArdourButton::led_default_elements, true));
	_search_ignore_checkbox->set_active(true);
	_search_ignore_checkbox->set_name ("pluginlist filter button");

	Gtk::Label* search_help_label1 = manage (new Label(
		_("All search terms must be matched."), Gtk::ALIGN_LEFT));

	Gtk::Label* search_help_label2 = manage (new Label(
		_("Ex: \"ess dyn\" will find \"dynamic de-esser\" but not \"de-esser\"."), Gtk::ALIGN_LEFT));

	search_table->attach (search_entry,            0, 3, 0, 1, FILL|EXPAND, FILL);
	search_table->attach (search_clear_button,     3, 4, 0, 1, FILL, FILL);
	search_table->attach (*_search_name_checkbox,  0, 1, 1, 2, FILL, FILL);
	search_table->attach (*_search_tags_checkbox,  1, 2, 1, 2, FILL, FILL);
	search_table->attach (*_search_ignore_checkbox,2, 3, 1, 2, FILL, FILL);
	search_table->attach (*search_help_label1,     0, 3, 2, 3, FILL, FILL);
	search_table->attach (*search_help_label2,     0, 3, 3, 4, FILL, FILL);

	search_table->set_border_width (4);
	search_table->set_col_spacings (4);
	search_table->set_row_spacings (4);

	Frame* search_frame = manage (new Frame);
	search_frame->set_name ("BaseFrame");
	search_frame->set_label (_("Search"));
	search_frame->add (*search_table);
	search_frame->show_all ();

	_search_name_checkbox->signal_clicked.connect (sigc::mem_fun (*this, &PluginSelector::refill));
	_search_tags_checkbox->signal_clicked.connect (sigc::mem_fun (*this, &PluginSelector::refill));
	_search_ignore_checkbox->signal_clicked.connect (sigc::mem_fun (*this, &PluginSelector::set_sensitive_widgets));

	/* FILTER */

	Gtk::RadioButtonGroup fil_radio_group;

	_fil_effects_radio = manage (new RadioButton (fil_radio_group, _("Show Effects Only")));
	_fil_instruments_radio = manage (new RadioButton (fil_radio_group, _("Show Instruments Only")));
	_fil_utils_radio = manage (new RadioButton (fil_radio_group, _("Show Utilities Only")));
	_fil_favorites_radio = manage (new RadioButton (fil_radio_group, _("Show Favorites Only")));
	_fil_hidden_radio = manage (new RadioButton (fil_radio_group, _("Show Hidden Only")));
	_fil_all_radio = manage (new RadioButton (fil_radio_group, _("Show All")));

	//_fil_type_combo = manage (new ComboBoxText);
	_fil_type_combo.append_text_item (_("Show All Formats"));

#if (defined WINDOWS_VST_SUPPORT || defined LXVST_SUPPORT || defined MACVST_SUPPORT)
	_fil_type_combo.append_text_item (X_("VST"));
#endif
#ifdef VST3_SUPPORT
	_fil_type_combo.append_text_item (X_("VST3"));
#endif
#ifdef AUDIOUNIT_SUPPORT
	_fil_type_combo.append_text_item (X_("AudioUnit"));
#endif
	_fil_type_combo.append_text_item (X_("LV2"));
	_fil_type_combo.append_text_item (X_("Lua"));
	_fil_type_combo.append_text_item (X_("LADSPA"));
	_fil_type_combo.set_text (_("Show All Formats"));

	/* note: _fil_creator_combo menu gets filled in build_plugin_menu */
	_fil_creator_combo.set_text_ellipsize (Pango::ELLIPSIZE_END);
	_fil_creator_combo.set_layout_ellipsize_width (PANGO_SCALE * 160 * UIConfiguration::instance ().get_ui_scale ());

	VBox* filter_vbox = manage (new VBox);
	filter_vbox->pack_start (*_fil_effects_radio,     false, false);
	filter_vbox->pack_start (*_fil_instruments_radio, false, false);
	filter_vbox->pack_start (*_fil_utils_radio,       false, false);
	filter_vbox->pack_start (*_fil_favorites_radio,   false, false);
	filter_vbox->pack_start (*_fil_hidden_radio,      false, false);
	filter_vbox->pack_start (*_fil_all_radio,         false, false);
	filter_vbox->pack_start (_fil_type_combo,         false, false);
	filter_vbox->pack_start (_fil_creator_combo,      false, false);

	filter_vbox->set_border_width (4);
	filter_vbox->set_spacing (4);

	Frame* filter_frame = manage (new Frame);
	filter_frame->set_name ("BaseFrame");
	filter_frame->set_label (_("Filter"));
	filter_frame->add (*filter_vbox);
	filter_frame->show_all ();

	_fil_effects_radio->signal_clicked().connect (sigc::mem_fun (*this, &PluginSelector::refill));
	_fil_instruments_radio->signal_clicked().connect (sigc::mem_fun (*this, &PluginSelector::refill));
	_fil_utils_radio->signal_clicked().connect (sigc::mem_fun (*this, &PluginSelector::refill));
	_fil_favorites_radio->signal_clicked().connect (sigc::mem_fun (*this, &PluginSelector::refill));
	_fil_hidden_radio->signal_clicked().connect (sigc::mem_fun (*this, &PluginSelector::refill));

	_fil_type_combo.StateChanged.connect (sigc::mem_fun (*this, &PluginSelector::refill));
	_fil_creator_combo.StateChanged.connect (sigc::mem_fun (*this, &PluginSelector::refill));

	/* TAG entry */

	Gtk::Table* tagging_table = manage(new Gtk::Table(1, 2));
	tagging_table->set_border_width (4);
	tagging_table->set_col_spacings (4);
	tagging_table->set_row_spacings (4);

	tag_entry = manage (new Gtk::Entry);
	tag_entry_connection = tag_entry->signal_changed().connect (sigc::mem_fun (*this, &PluginSelector::tag_entry_changed));

	tag_reset_button = manage (new Button (_("Reset")));
	tag_reset_button->signal_clicked().connect (sigc::mem_fun (*this, &PluginSelector::tag_reset_button_clicked));

	Gtk::Label* tagging_help_label1 = manage (new Label(
		_("Enter space-separated, one-word Tags for the selected plugin."), Gtk::ALIGN_LEFT));

	Gtk::Label* tagging_help_label2 = manage (new Label(
		_("You can include dashes, colons or underscores in a Tag."), Gtk::ALIGN_LEFT));

	Gtk::Label* tagging_help_label3 = manage (new Label(
		_("Ex: \"dynamic de-esser vocal\" applies 3 Tags."), Gtk::ALIGN_LEFT));

	int p = 0;
	tagging_table->attach (*tag_entry,           0, 1, p, p+1, FILL|EXPAND, FILL);
	tagging_table->attach (*tag_reset_button,    1, 2, p, p+1, FILL, FILL); p++;
	tagging_table->attach (*tagging_help_label1, 0, 2, p, p+1, FILL, FILL); p++;
	tagging_table->attach (*tagging_help_label2, 0, 2, p, p+1, FILL, FILL); p++;
	tagging_table->attach (*tagging_help_label3, 0, 2, p, p+1, FILL, FILL); p++;

	Frame* tag_frame = manage (new Frame);
	tag_frame->set_name ("BaseFrame");
	tag_frame->set_label (_("Tags for Selected Plugin"));
	tag_frame->add (*tagging_table);
	tag_frame->show_all ();

	/* Add & remove buttons */

	HBox* add_remove = manage (new HBox);
	add_remove->pack_start (*btn_add, true, true);
	add_remove->pack_start (*btn_remove, true, true);

	btn_add->signal_clicked().connect(sigc::mem_fun(*this, &PluginSelector::btn_add_clicked));
	btn_remove->signal_clicked().connect(sigc::mem_fun(*this, &PluginSelector::btn_remove_clicked));
	added_list.get_selection()->signal_changed().connect (sigc::mem_fun(*this, &PluginSelector::added_list_selection_changed));
	added_list.signal_button_press_event().connect_notify (mem_fun(*this, &PluginSelector::added_row_clicked));

	added_list.set_name("PluginSelectorList");

	/* Top-level Layout */

	VBox* to_be_inserted_vbox = manage (new VBox);
	to_be_inserted_vbox->pack_start (ascroller);
	to_be_inserted_vbox->pack_start (*add_remove, false, false);

	int min_width  = std::max (200.f, rintf(200.f * UIConfiguration::instance().get_ui_scale()));
	int min_height = std::max (600.f, rintf(600.f * UIConfiguration::instance().get_ui_scale()));

	to_be_inserted_vbox->set_size_request (min_width, -1);

	Gtk::Table* table = manage(new Gtk::Table(3, 3));
	table->set_size_request(-1, min_height);

	table->attach (scroller,               0, 3, 0, 5); /* this is the main plugin list */
	table->attach (*search_frame,          0, 1, 6, 7, FILL, FILL, 5, 5);
	table->attach (*tag_frame,             0, 1, 7, 8, FILL, FILL, 5, 5);
	table->attach (*filter_frame,          1, 2, 6, 8, FILL, FILL, 5, 5);
	table->attach (*to_be_inserted_vbox,   2, 3, 6, 8, FILL|EXPAND, FILL, 5, 5); /* to be inserted... */

	add_button (Stock::CLOSE, RESPONSE_CLOSE);
	add_button (_("Insert Plugin(s)"), RESPONSE_APPLY);
	set_default_response (RESPONSE_APPLY);
	set_response_sensitive (RESPONSE_APPLY, false);
	get_vbox()->pack_start (*table);

	table->set_name("PluginSelectorTable");

	plugin_display.grab_focus();

	build_plugin_menu ();
	display_selection_changed ();
}

PluginSelector::~PluginSelector ()
{
	delete _plugin_menu;
}

void
PluginSelector::row_activated(Gtk::TreeModel::Path, Gtk::TreeViewColumn*)
{
	btn_add_clicked();
}

void
PluginSelector::added_row_clicked(GdkEventButton* event)
{
	if (event->type == GDK_2BUTTON_PRESS)
		btn_remove_clicked();
}

bool
PluginSelector::show_this_plugin (const PluginInfoPtr& info, const std::string& searchstr)
{
	string mode;
	bool maybe_show = false;
	PluginManager::PluginStatusType status = manager.get_status (info);

	if (!searchstr.empty()) {

		if (_search_name_checkbox->get_active()) { /* name contains */
			std::string compstr = info->name;
			setup_search_string (compstr);
			maybe_show |= match_search_strings (compstr, searchstr);
		}

		if (_search_tags_checkbox->get_active()) { /* tag contains */
			std::string compstr = manager.get_tags_as_string (info);
			setup_search_string (compstr);
			maybe_show |= match_search_strings (compstr, searchstr);
		}

		if (!maybe_show) {
			return false;
		}

		/* user asked to ignore filters */
		if (maybe_show && _search_ignore_checkbox->get_active()) {
			if (status == PluginManager::Hidden) {
				return false;
			}
			if (status == PluginManager::Concealed) {
				return false;
			}
			return true;
		}
	}

	if (_fil_effects_radio->get_active() && !info->is_effect()) {
		return false;
	}

	if (_fil_instruments_radio->get_active() && !info->is_instrument()) {
		return false;
	}

	if (_fil_utils_radio->get_active() && !(info->is_utility() || info->is_analyzer())) {
		return false;
	}

	if (_fil_favorites_radio->get_active() && status != PluginManager::Favorite) {
		return false;
	}

	if (_fil_hidden_radio->get_active() && (status != PluginManager::Hidden && status != PluginManager::Concealed)) {
		return false;
	}

	if (!_fil_hidden_radio->get_active() && status == PluginManager::Hidden) {
		return false;
	}

	if (!_fil_hidden_radio->get_active() && status == PluginManager::Concealed) {
		return false;
	}

	/* Filter "type" combobox */

	if (_fil_type_combo.get_text() == X_("VST") && PluginManager::to_generic_vst(info->type) != LXVST) {
		return false;
	}

	if (_fil_type_combo.get_text() == X_("AudioUnit") && info->type != AudioUnit) {
		return false;
	}

	if (_fil_type_combo.get_text() == X_("VST3") && info->type != VST3) {
		return false;
	}

	if (_fil_type_combo.get_text() == X_("LV2") && info->type != LV2) {
		return false;
	}

	if (_fil_type_combo.get_text() == X_("Lua") && info->type != Lua) {
		return false;
	}

	if (_fil_type_combo.get_text() == X_("LADSPA") && info->type != LADSPA) {
		return false;
	}

	/* Filter "creator" combobox */

	if (_fil_creator_combo.get_text() != _("Show All Creators")) {
		if (_fil_creator_combo.get_text() != info->creator) {
			return false;
		}
	}

	return true;
}

void
PluginSelector::set_sensitive_widgets ()
{
	if (_search_ignore_checkbox->get_active() && !search_entry.get_text().empty()) {
		_fil_effects_radio->set_sensitive(false);
		_fil_instruments_radio->set_sensitive(false);
		_fil_utils_radio->set_sensitive(false);
		_fil_favorites_radio->set_sensitive(false);
		_fil_hidden_radio->set_sensitive(false);
		_fil_all_radio->set_sensitive(false);
		_inhibit_refill = true;
		_fil_type_combo.set_sensitive(false);
		_fil_creator_combo.set_sensitive(false);
		_inhibit_refill = false;
	} else {
		_fil_effects_radio->set_sensitive(true);
		_fil_instruments_radio->set_sensitive(true);
		_fil_utils_radio->set_sensitive(true);
		_fil_favorites_radio->set_sensitive(true);
		_fil_hidden_radio->set_sensitive(true);
		_fil_all_radio->set_sensitive(true);
		_inhibit_refill = true;
		_fil_type_combo.set_sensitive(true);
		_fil_creator_combo.set_sensitive(true);
		_inhibit_refill = false;
	}
	if (!search_entry.get_text().empty()) {
		refill ();
	}
}

void
PluginSelector::refill ()
{
	if (_inhibit_refill) {
		return;
	}

	in_row_change = true;

	plugin_display.set_model (Glib::RefPtr<Gtk::TreeStore>(0));

	int sort_col;
	SortType sort_type;
	bool sorted = plugin_model->get_sort_column_id (sort_col, sort_type);

	/* Disable sorting to gain performance */
	plugin_model->set_sort_column (-2, SORT_ASCENDING);

	plugin_model->clear ();

	std::string searchstr = search_entry.get_text ();
	setup_search_string (searchstr);

	ladspa_refiller (searchstr);
	lv2_refiller (searchstr);
	vst_refiller (searchstr);
	lxvst_refiller (searchstr);
	mac_vst_refiller (searchstr);
	au_refiller (searchstr);
	lua_refiller (searchstr);
	vst3_refiller (searchstr);

	in_row_change = false;

	plugin_display.set_model (plugin_model);
	if (sorted) {
		plugin_model->set_sort_column (sort_col, sort_type);
	}
}

void
PluginSelector::refiller (const PluginInfoList& plugs, const::std::string& searchstr, const char* type)
{
	char buf[16];

	for (PluginInfoList::const_iterator i = plugs.begin(); i != plugs.end(); ++i) {

		if (show_this_plugin (*i, searchstr)) {

			TreeModel::Row newrow = *(plugin_model->append());

			PluginManager::PluginStatusType status = manager.get_status (*i);
			newrow[plugin_columns.favorite] = status == PluginManager::Favorite;

			string name = (*i)->name;
			if (name.length() > 48) {
				name = name.substr (0, 48);
				name.append("...");
			}
			newrow[plugin_columns.name] = name;

			newrow[plugin_columns.type_name] = type;

			/* Creator */
			string creator = (*i)->creator;
			string::size_type pos = 0;
			if ((*i)->type == ARDOUR::LADSPA) {
				/* stupid LADSPA creator strings */
#ifdef PLATFORM_WINDOWS
				while (pos < creator.length() && creator[pos] > -2 && creator[pos] < 256 && (isalnum (creator[pos]) || isspace (creator[pos]))) ++pos;
#else
				while (pos < creator.length() && (isalnum (creator[pos]) || isspace (creator[pos]))) ++pos;
#endif
			} else {
				pos = creator.length ();
			}
			// If there were too few characters to create a
			// meaningful name, mark this creator as 'Unknown'
			if (creator.length() < 2 || pos < 3) {
				creator = "Unknown";
			} else{
				creator = creator.substr (0, pos);
			}

			if (creator.length() > MAX_CREATOR_LEN) {
				creator = creator.substr (0, MAX_CREATOR_LEN);
				creator.append("...");
			}
			newrow[plugin_columns.creator] = creator;

			/* Tags */
			string tags = manager.get_tags_as_string(*i);
			if (tags.length() > 32) {
				tags = tags.substr (0, 32);
				tags.append("...");
			}
			newrow[plugin_columns.tags] = tags;

			if ((*i)->reconfigurable_io ()) {
				newrow[plugin_columns.audio_io] = "* / *";
				newrow[plugin_columns.midi_io] = "* / *";
			} else {
				snprintf (buf, sizeof(buf), "%d / %d", (*i)->n_inputs.n_audio(), (*i)->n_outputs.n_audio());
				newrow[plugin_columns.audio_io] = buf;
				snprintf (buf, sizeof(buf), "%d / %d", (*i)->n_inputs.n_midi(), (*i)->n_outputs.n_midi());
				newrow[plugin_columns.midi_io] = buf;
			}

			newrow[plugin_columns.plugin] = *i;
		}
	}
}

void
PluginSelector::ladspa_refiller (const std::string& searchstr)
{
	refiller (manager.ladspa_plugin_info(), searchstr, "LADSPA");
}

void
PluginSelector::lua_refiller (const std::string& searchstr)
{
	refiller (manager.lua_plugin_info(), searchstr, "Lua");
}

void
PluginSelector::lv2_refiller (const std::string& searchstr)
{
	refiller (manager.lv2_plugin_info(), searchstr, "LV2");
}

void
PluginSelector::vst_refiller (const std::string& searchstr)
{
#ifdef WINDOWS_VST_SUPPORT
	refiller (manager.windows_vst_plugin_info(), searchstr, "VST");
#endif
}

void
PluginSelector::lxvst_refiller (const std::string& searchstr)
{
#ifdef LXVST_SUPPORT
	refiller (manager.lxvst_plugin_info(), searchstr, "LXVST");
#endif
}

void
PluginSelector::mac_vst_refiller (const std::string& searchstr)
{
#ifdef MACVST_SUPPORT
	refiller (manager.mac_vst_plugin_info(), searchstr, "MacVST");
#endif
}

void
PluginSelector::vst3_refiller (const std::string& searchstr)
{
#ifdef VST3_SUPPORT
	refiller (manager.vst3_plugin_info(), searchstr, "VST3");
#endif
}

void
PluginSelector::au_refiller (const std::string& searchstr)
{
#ifdef AUDIOUNIT_SUPPORT
	refiller (manager.au_plugin_info(), searchstr, "AU");
#endif
}

PluginPtr
PluginSelector::load_plugin (PluginInfoPtr pi)
{
	if (_session == 0) {
		return PluginPtr();
	}

	return pi->load (*_session);
}

void
PluginSelector::btn_add_clicked()
{
	if (plugin_display.get_selection()->count_selected_rows() == 0) {
		/* may happen with ctrl + double-click un-selecting but activating a row */
		return;
	}
	std::string name;
	PluginInfoPtr pi;
	TreeModel::Row newrow = *(amodel->append());
	TreeModel::Row row;

	row = *(plugin_display.get_selection()->get_selected());
	name = row[plugin_columns.name];
	pi = row[plugin_columns.plugin];

	newrow[acols.text] = name;
	newrow[acols.plugin] = pi;

	if (!amodel->children().empty()) {
		set_response_sensitive (RESPONSE_APPLY, true);
	}
}

void
PluginSelector::btn_remove_clicked()
{
	TreeModel::iterator iter = added_list.get_selection()->get_selected();

	amodel->erase(iter);
	if (amodel->children().empty()) {
		set_response_sensitive (RESPONSE_APPLY, false);
	}
}

void
PluginSelector::display_selection_changed()
{
	tag_entry_connection.block ();
	if (plugin_display.get_selection()->count_selected_rows() != 0) {

		/* a plugin row is selected; allow the user to edit the "tags" on it. */
		TreeModel::Row row = *(plugin_display.get_selection()->get_selected());
		string tags = manager.get_tags_as_string (row[plugin_columns.plugin]);
		tag_entry->set_text (tags);

		tag_entry->set_sensitive (true);
		tag_reset_button->set_sensitive (true);
		btn_add->set_sensitive (true);

	} else {
		tag_entry->set_text ("");

		tag_entry->set_sensitive (false);
		tag_reset_button->set_sensitive (false);
		btn_add->set_sensitive (false);
	}
	tag_entry_connection.unblock ();
}

void
PluginSelector::added_list_selection_changed()
{
	if (added_list.get_selection()->count_selected_rows() != 0) {
		btn_remove->set_sensitive (true);
	} else {
		btn_remove->set_sensitive (false);
	}
}

int
PluginSelector::run ()
{
	ResponseType r;
	TreeModel::Children::iterator i;

	bool finish = false;

	while (!finish) {

		SelectedPlugins plugins;
		r = (ResponseType) Dialog::run ();

		switch (r) {
		case RESPONSE_APPLY:
			for (i = amodel->children().begin(); i != amodel->children().end(); ++i) {
				PluginInfoPtr pp = (*i)[acols.plugin];
				PluginPtr p = load_plugin (pp);
				if (p) {
					plugins.push_back (p);
				} else {
					MessageDialog msg (string_compose (_("The plugin \"%1\" could not be loaded\n\nSee the Log window for more details (maybe)"), pp->name));
					msg.run ();
				}
			}
			if (interested_object && !plugins.empty()) {
				finish = !interested_object->use_plugins (plugins);
			}

			break;

		default:
			finish = true;
			break;
		}
	}


	hide();
	amodel->clear();
	interested_object = 0;

	if (_need_tag_save) {
		manager.save_tags();
	}

	if (_need_status_save) {
		manager.save_statuses();
	}
	if (_need_menu_rebuild) {
		build_plugin_menu ();
	}

	return (int) r;
}

void
PluginSelector::search_clear_button_clicked ()
{
	search_entry.set_text ("");
}

void
PluginSelector::tag_reset_button_clicked ()
{
	if (plugin_display.get_selection()->count_selected_rows() != 0) {
		TreeModel::Row row = *(plugin_display.get_selection()->get_selected());
		ARDOUR::PluginInfoPtr pi = row[plugin_columns.plugin];
		manager.reset_tags (pi);
		display_selection_changed ();
		_need_tag_save = true;
	}
}

void
PluginSelector::search_entry_changed ()
{
	set_sensitive_widgets();
	if (search_entry.get_text().empty()) {
		refill ();
	}
}

void
PluginSelector::tag_entry_changed ()
{
	if (plugin_display.get_selection()->count_selected_rows() != 0) {
		TreeModel::Row row = *(plugin_display.get_selection()->get_selected());

		ARDOUR::PluginInfoPtr pi = row[plugin_columns.plugin];
		manager.set_tags (pi->type, pi->unique_id, tag_entry->get_text(), pi->name, PluginManager::FromGui);

		_need_tag_save = true;
	}
}

void
PluginSelector::tags_changed (PluginType t, std::string unique_id, std::string tags)
{
	if (plugin_display.get_selection()->count_selected_rows() != 0) {
		TreeModel::Row row = *(plugin_display.get_selection()->get_selected());
		if (tags.length() > 32) {
			tags = tags.substr (0, 32);
			tags.append ("...");
		}
		row[plugin_columns.tags] = tags;
	}
}

void
PluginSelector::plugin_status_changed (PluginType t, std::string uid, PluginManager::PluginStatusType stat)
{
	Gtk::TreeModel::iterator i;
	for (i = plugin_model->children().begin(); i != plugin_model->children().end(); ++i) {
		PluginInfoPtr pp = (*i)[plugin_columns.plugin];
		if ((pp->type == t) && (pp->unique_id == uid)) {
			(*i)[plugin_columns.favorite] = (stat == PluginManager::Favorite) ? true : false;

			/* if plug was hidden, remove it from the view */
			if (stat == PluginManager::Hidden || stat == PluginManager::Concealed) {
				if (!_fil_hidden_radio->get_active() && !_fil_all_radio->get_active()) {
					plugin_model->erase(i);
				}
			} else if (_fil_hidden_radio->get_active()) {
				plugin_model->erase(i);
			}
			/* if no longer a favorite, remove it from the view */
			if (stat != PluginManager::Favorite && _fil_favorites_radio->get_active()) {
					plugin_model->erase(i);
			}

			return;
		}
	}
}

void
PluginSelector::on_show ()
{
	ArdourDialog::on_show ();
	search_entry.grab_focus ();

	refill ();

	_need_tag_save = false;
	_need_status_save = false;
}

struct PluginMenuCompareByCreator {
	bool operator() (PluginInfoPtr a, PluginInfoPtr b) const {
		int cmp;

		cmp = cmp_nocase_utf8 (a->creator, b->creator);

		if (cmp < 0) {
			return true;
		} else if (cmp == 0) {
			/* same creator ... compare names */
			if (cmp_nocase_utf8 (a->name, b->name) < 0) {
				return true;
			}
		}
		return false;
	}
};

/** @return Plugin menu. The caller should not delete it */
Gtk::Menu*
PluginSelector::plugin_menu()
{
	return _plugin_menu;
}

void
PluginSelector::build_plugin_menu ()
{
	if (is_visible ()) {
		_need_menu_rebuild = true;
		return;
	}
	_need_menu_rebuild = false;
	PluginInfoList all_plugs;

	all_plugs.insert (all_plugs.end(), manager.ladspa_plugin_info().begin(), manager.ladspa_plugin_info().end());
	all_plugs.insert (all_plugs.end(), manager.lua_plugin_info().begin(), manager.lua_plugin_info().end());
	all_plugs.insert (all_plugs.end(), manager.lv2_plugin_info().begin(), manager.lv2_plugin_info().end());
#ifdef WINDOWS_VST_SUPPORT
	all_plugs.insert (all_plugs.end(), manager.windows_vst_plugin_info().begin(), manager.windows_vst_plugin_info().end());
#endif
#ifdef LXVST_SUPPORT
	all_plugs.insert (all_plugs.end(), manager.lxvst_plugin_info().begin(), manager.lxvst_plugin_info().end());
#endif
#ifdef MACVST_SUPPORT
	all_plugs.insert (all_plugs.end(), manager.mac_vst_plugin_info().begin(), manager.mac_vst_plugin_info().end());
#endif
#ifdef VST3_SUPPORT
	all_plugs.insert (all_plugs.end(), manager.vst3_plugin_info().begin(), manager.vst3_plugin_info().end());
#endif
#ifdef AUDIOUNIT_SUPPORT
	all_plugs.insert (all_plugs.end(), manager.au_plugin_info().begin(), manager.au_plugin_info().end());
#endif

	using namespace Menu_Helpers;

	delete _plugin_menu;

	_plugin_menu = new Menu;
	_plugin_menu->set_name("ArdourContextMenu");

	MenuList& items = _plugin_menu->items();
	items.clear ();

	Gtk::Menu* favs = create_favs_menu(all_plugs);
	items.push_back (MenuElem (_("Favorites"), *manage (favs)));

	items.push_back (MenuElem (_("Plugin Selector..."), sigc::mem_fun (*this, &PluginSelector::show_manager)));
	items.push_back (SeparatorElem ());

	Menu* charts = create_charts_menu(all_plugs);
	items.push_back (MenuElem (_("By Popularity"), *manage (charts)));

	Menu* by_creator = create_by_creator_menu(all_plugs);
	items.push_back (MenuElem (_("By Creator"), *manage (by_creator)));

	Menu* by_tags = create_by_tags_menu(all_plugs);
	items.push_back (MenuElem (_("By Tags"), *manage (by_tags)));
}

string
GetPluginTypeStr(PluginInfoPtr info)
{
	return string_compose (" (%1)", PluginManager::plugin_type_name (info->type, false));
}

Gtk::Menu*
PluginSelector::create_favs_menu (PluginInfoList& all_plugs)
{
	using namespace Menu_Helpers;

	Menu* favs = new Menu();
	favs->set_name("ArdourContextMenu");

	PluginABCSorter cmp_by_name;
	all_plugs.sort (cmp_by_name);

	for (PluginInfoList::const_iterator i = all_plugs.begin(); i != all_plugs.end(); ++i) {
		if (manager.get_status (*i) == PluginManager::Favorite) {
			string typ = GetPluginTypeStr(*i);
			MenuElem elem ((*i)->name + typ, (sigc::bind (sigc::mem_fun (*this, &PluginSelector::plugin_chosen_from_menu), *i)));
			elem.get_child()->set_use_underline (false);
			favs->items().push_back (elem);
		}
	}
	return favs;
}

Gtk::Menu*
PluginSelector::create_charts_menu (PluginInfoList& all_plugs)
{
	using namespace Menu_Helpers;

	Menu* charts = new Menu();
	charts->set_name("ArdourContextMenu");

	PluginInfoList plugs;

	for (PluginInfoList::const_iterator i = all_plugs.begin(); i != all_plugs.end(); ++i) {
		int64_t lru;
		uint64_t use_count;
		if (manager.stats (*i, lru, use_count)) {
			plugs.push_back (*i);
		}
	}
	PluginChartsSorter cmp;
	plugs.sort (cmp);
	plugs.resize (std::min (plugs.size(), size_t(UIConfiguration::instance().get_max_plugin_chart())));

	PluginABCSorter abc;
	plugs.sort (abc);

	for (PluginInfoList::const_iterator i = plugs.begin(); i != plugs.end(); ++i) {
		string typ = GetPluginTypeStr(*i);
		MenuElem elem ((*i)->name + typ, (sigc::bind (sigc::mem_fun (*this, &PluginSelector::plugin_chosen_from_menu), *i)));
		elem.get_child()->set_use_underline (false);
		charts->items().push_back (elem);
	}
	return charts;
}

Gtk::Menu*
PluginSelector::create_by_creator_menu (ARDOUR::PluginInfoList& all_plugs)
{
	_inhibit_refill = true;
	_fil_creator_combo.clear_items ();
	_fil_creator_combo.append_text_item (_("Show All Creators"));
	_fil_creator_combo.set_text (_("Show All Creators"));
	_inhibit_refill = false;

	using namespace Menu_Helpers;

	typedef std::map<std::string,Gtk::Menu*> SubmenuMap;
	SubmenuMap creator_submenu_map;

	Menu* by_creator = new Menu();
	by_creator->set_name("ArdourContextMenu");

	MenuList& by_creator_items = by_creator->items();
	PluginMenuCompareByCreator cmp_by_creator;
	all_plugs.sort (cmp_by_creator);

	for (PluginInfoList::const_iterator i = all_plugs.begin(); i != all_plugs.end(); ++i) {

		PluginManager::PluginStatusType status = manager.get_status (*i);
		if (status == PluginManager::Hidden) continue;
		if (status == PluginManager::Concealed) continue;

		string creator = (*i)->creator;

		/* If there were too few characters to create a
		 * meaningful name, mark this creator as 'Unknown'
		 */
		if (creator.length() < 2) {
			creator = "Unknown";
		}

		SubmenuMap::iterator x;
		Gtk::Menu* submenu;
		if ((x = creator_submenu_map.find (creator)) != creator_submenu_map.end()) {
			submenu = x->second;
		} else {

			_fil_creator_combo.append_text_item (creator);

			submenu = new Gtk::Menu;
			by_creator_items.push_back (MenuElem (creator, *manage (submenu)));
			creator_submenu_map.insert (pair<std::string,Menu*> (creator, submenu));
			submenu->set_name("ArdourContextMenu");
		}
		string typ = GetPluginTypeStr(*i);
		MenuElem elem ((*i)->name+typ, (sigc::bind (sigc::mem_fun (*this, &PluginSelector::plugin_chosen_from_menu), *i)));
		elem.get_child()->set_use_underline (false);
		submenu->items().push_back (elem);
	}

	return by_creator;
}

Gtk::Menu*
PluginSelector::create_by_tags_menu (ARDOUR::PluginInfoList& all_plugs)
{
	using namespace Menu_Helpers;

	typedef std::map<std::string,Gtk::Menu*> SubmenuMap;
	SubmenuMap tags_submenu_map;

	Menu* by_tags = new Menu();
	by_tags->set_name("ArdourContextMenu");
	MenuList& by_tags_items = by_tags->items();

	std::vector<std::string> all_tags = manager.get_all_tags (PluginManager::NoHidden);
	for (vector<string>::iterator t = all_tags.begin(); t != all_tags.end(); ++t) {
		Gtk::Menu *submenu = new Gtk::Menu;
		by_tags_items.push_back (MenuElem (*t, *manage (submenu)));
		tags_submenu_map.insert (pair<std::string,Menu*> (*t, submenu));
		submenu->set_name("ArdourContextMenu");
	}

	PluginABCSorter cmp_by_name;
	all_plugs.sort (cmp_by_name);

	for (PluginInfoList::const_iterator i = all_plugs.begin(); i != all_plugs.end(); ++i) {

		PluginManager::PluginStatusType status = manager.get_status (*i);
		if (status == PluginManager::Hidden) continue;
		if (status == PluginManager::Concealed) continue;

		/* for each tag in the plugins tag list, add it to that submenu */
		vector<string> tokens = manager.get_tags(*i);
		for (vector<string>::iterator t = tokens.begin(); t != tokens.end(); ++t) {
			SubmenuMap::iterator x;
			Gtk::Menu* submenu;
			if ((x = tags_submenu_map.find (*t)) != tags_submenu_map.end()) {
				submenu = x->second;
				string typ = GetPluginTypeStr(*i);
				MenuElem elem ((*i)->name + typ, (sigc::bind (sigc::mem_fun (*this, &PluginSelector::plugin_chosen_from_menu), *i)));
				elem.get_child()->set_use_underline (false);
				submenu->items().push_back (elem);
			}
		}
	}
	return by_tags;
}

void
PluginSelector::plugin_chosen_from_menu (const PluginInfoPtr& pi)
{
	PluginPtr p = load_plugin (pi);

	if (p && interested_object) {
		SelectedPlugins plugins;
		plugins.push_back (p);
		interested_object->use_plugins (plugins);
	}

	interested_object = 0;
}

void
PluginSelector::favorite_changed (const std::string& path)
{
	PluginInfoPtr pi;

	if (in_row_change) {
		return;
	}

	in_row_change = true;

	TreeModel::iterator iter = plugin_model->get_iter (path);

	if (iter) {

		bool favorite = !(*iter)[plugin_columns.favorite];

		/* change state */

		PluginManager::PluginStatusType status = (favorite ? PluginManager::Favorite : PluginManager::Normal);

		/* save new statuses list */

		pi = (*iter)[plugin_columns.plugin];

		manager.set_status (pi->type, pi->unique_id, status);

		_need_status_save = true;
	}
	in_row_change = false;
}

void
PluginSelector::show_manager ()
{
	bool scan_now = false;
	if (!manager.cache_valid ()) {
		ArdourMessageDialog q (
#ifdef __APPLE__
				_("Scan VST2/3 and AudioUnit plugins now?")
#else
				_("Scan VST2/3 Plugins now?")
#endif
				, false, MESSAGE_QUESTION, BUTTONS_YES_NO);

		q.set_title (string_compose (_("Discover %1 Plugins?"),
#ifdef __APPLE__
					_("VST/AU")
#else
					_("VST")
#endif
					));

		q.set_secondary_text (string_compose (_("Third party plugins have not yet been indexed. %1 plugins have to be scanned before they can be used. This can also be done manually from Window > Plugin Manager. Depending on the number of installed plugins the process can take several minutes."),
#ifdef __APPLE__
					_("AudioUnit and VST")
#else
					_("VST")
#endif
					));

		if (q.run () == RESPONSE_YES) {
			scan_now = true;
		}
	}

	if (scan_now) {
		PluginScanDialog psd (false, true);
		psd.start ();
		ARDOUR_UI::instance()->show_plugin_manager ();
	}

	show_all();
	run ();
}

void
PluginSelector::set_interested_object (PluginInterestedObject& obj)
{
	interested_object = &obj;
}
