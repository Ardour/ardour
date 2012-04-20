/*
    Copyright (C) 2000-2006 Paul Davis

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
#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <cstdio>
#include <lrdf.h>
#include <map>

#include <algorithm>

#include <gtkmm/table.h>
#include <gtkmm/stock.h>
#include <gtkmm/button.h>
#include <gtkmm/notebook.h>

#include <gtkmm2ext/utils.h>

#include "pbd/convert.h"

#include "ardour/plugin_manager.h"
#include "ardour/plugin.h"
#include "ardour/configuration.h"
#include "ardour/session.h"

#include "ardour_ui.h"
#include "plugin_selector.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace std;

static const char* _filter_mode_strings[] = {
	N_("Name contains"),
	N_("Type contains"),
	N_("Category contains"),
	N_("Author contains"),
	N_("Library contains"),
	N_("Favorites only"),
	N_("Hidden only"),
	0
};

PluginSelector::PluginSelector (PluginManager& mgr)
	: ArdourDialog (_("Plugin Manager"), true, false)
	, filter_button (Stock::CLEAR)
	, manager (mgr)
	  
{
	set_position (Gtk::WIN_POS_MOUSE);
	set_name ("PluginSelectorWindow");
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);

	_plugin_menu = 0;
	in_row_change = false;

	manager.PluginListChanged.connect (plugin_list_changed_connection, invalidator (*this), boost::bind (&PluginSelector::build_plugin_menu, this), gui_context());
	build_plugin_menu ();

	plugin_model = Gtk::ListStore::create (plugin_columns);
	plugin_display.set_model (plugin_model);
	/* XXX translators: try to convert "Fav" into a short term
	   related to "favorite" and "Hid" into a short term
	   related to "hidden"
	*/
	plugin_display.append_column (_("Fav"), plugin_columns.favorite);
	plugin_display.append_column (_("Hid"), plugin_columns.hidden);
	plugin_display.append_column (_("Available Plugins"), plugin_columns.name);
	plugin_display.append_column (_("Type"), plugin_columns.type_name);
	plugin_display.append_column (_("Category"), plugin_columns.category);
	plugin_display.append_column (_("Creator"), plugin_columns.creator);
	plugin_display.append_column (_("# Audio In"),plugin_columns.audio_ins);
	plugin_display.append_column (_("# Audio Out"), plugin_columns.audio_outs);
	plugin_display.append_column (_("# MIDI In"),plugin_columns.midi_ins);
	plugin_display.append_column (_("# MIDI Out"), plugin_columns.midi_outs);
	plugin_display.set_headers_visible (true);
	plugin_display.set_headers_clickable (true);
	plugin_display.set_reorderable (false);
	plugin_display.set_rules_hint (true);

	CellRendererToggle* fav_cell = dynamic_cast<CellRendererToggle*>(plugin_display.get_column_cell_renderer (0));
	fav_cell->property_activatable() = true;
	fav_cell->property_radio() = true;
	fav_cell->signal_toggled().connect (sigc::mem_fun (*this, &PluginSelector::favorite_changed));

	CellRendererToggle* hidden_cell = dynamic_cast<CellRendererToggle*>(plugin_display.get_column_cell_renderer (1));
	hidden_cell->property_activatable() = true;
	hidden_cell->property_radio() = true;
	hidden_cell->signal_toggled().connect (sigc::mem_fun (*this, &PluginSelector::hidden_changed));

	scroller.set_border_width(10);
	scroller.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	scroller.add(plugin_display);

	amodel = Gtk::ListStore::create(acols);
	added_list.set_model (amodel);
	added_list.append_column (_("Plugins to be connected"), acols.text);
	added_list.set_headers_visible (true);
	added_list.set_reorderable (false);

	for (int i = 0; i <=8; i++) {
		Gtk::TreeView::Column* column = plugin_display.get_column(i);
		column->set_sort_column(i);
	}

	ascroller.set_border_width(10);
	ascroller.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	ascroller.add(added_list);
	btn_add = manage(new Gtk::Button(Stock::ADD));
	ARDOUR_UI::instance()->set_tip(*btn_add, _("Add a plugin to the effect list"));
	btn_add->set_sensitive (false);
	btn_remove = manage(new Gtk::Button(Stock::REMOVE));
	btn_remove->set_sensitive (false);
	ARDOUR_UI::instance()->set_tip(*btn_remove, _("Remove a plugin from the effect list"));
	Gtk::Button *btn_update = manage(new Gtk::Button(Stock::REFRESH));
	ARDOUR_UI::instance()->set_tip(*btn_update, _("Update available plugins"));

	btn_add->set_name("PluginSelectorButton");
	btn_remove->set_name("PluginSelectorButton");

	Gtk::Table* table = manage(new Gtk::Table(7, 11));
	table->set_size_request(750, 500);
	table->attach(scroller, 0, 7, 0, 5);

	HBox* filter_box = manage (new HBox);

	vector<string> filter_strings = I18N (_filter_mode_strings);
	Gtkmm2ext::set_popdown_strings (filter_mode, filter_strings);
	filter_mode.set_active_text (filter_strings.front());

	filter_box->pack_start (filter_mode, false, false);
	filter_box->pack_start (filter_entry, true, true);
	filter_box->pack_start (filter_button, false, false);

	filter_entry.signal_changed().connect (sigc::mem_fun (*this, &PluginSelector::filter_entry_changed));
	filter_button.signal_clicked().connect (sigc::mem_fun (*this, &PluginSelector::filter_button_clicked));
	filter_mode.signal_changed().connect (sigc::mem_fun (*this, &PluginSelector::filter_mode_changed));

	filter_box->show ();
	filter_mode.show ();
	filter_entry.show ();
	filter_button.show ();

	table->attach (*filter_box, 0, 7, 5, 6, FILL|EXPAND, FILL, 5, 5);

	table->attach(*btn_add, 1, 2, 6, 7, FILL, FILL, 5, 5);
	table->attach(*btn_remove, 3, 4, 6, 7, FILL, FILL, 5, 5);
	table->attach(*btn_update, 5, 6, 6, 7, FILL, FILL, 5, 5);

	table->attach(ascroller, 0, 7, 8, 10);

	add_button (Stock::CLOSE, RESPONSE_CLOSE);
	add_button (_("Insert Plugin(s)"), RESPONSE_APPLY);
	set_default_response (RESPONSE_APPLY);
	set_response_sensitive (RESPONSE_APPLY, false);
	get_vbox()->pack_start (*table);

	table->set_name("PluginSelectorTable");
	plugin_display.set_name("PluginSelectorDisplay");
	//plugin_display.set_name("PluginSelectorList");
	added_list.set_name("PluginSelectorList");

	plugin_display.signal_row_activated().connect_notify (sigc::mem_fun(*this, &PluginSelector::row_activated));
	plugin_display.get_selection()->signal_changed().connect (sigc::mem_fun(*this, &PluginSelector::display_selection_changed));
	plugin_display.grab_focus();

	btn_update->signal_clicked().connect (sigc::mem_fun(*this, &PluginSelector::btn_update_clicked));
	btn_add->signal_clicked().connect(sigc::mem_fun(*this, &PluginSelector::btn_add_clicked));
	btn_remove->signal_clicked().connect(sigc::mem_fun(*this, &PluginSelector::btn_remove_clicked));
	added_list.get_selection()->signal_changed().connect (sigc::mem_fun(*this, &PluginSelector::added_list_selection_changed));

	refill ();
}

PluginSelector::~PluginSelector ()
{
	delete _plugin_menu;
}

void
PluginSelector::row_activated(Gtk::TreeModel::Path path, Gtk::TreeViewColumn* col)
{
	btn_add_clicked();
}

bool
PluginSelector::show_this_plugin (const PluginInfoPtr& info, const std::string& filterstr)
{
	std::string compstr;
	std::string mode = filter_mode.get_active_text ();

	if (mode == _("Favorites only")) {
		return manager.get_status (info) == PluginManager::Favorite;
	}

	if (mode == _("Hidden only")) {
		return manager.get_status (info) == PluginManager::Hidden;
	}

	if (!filterstr.empty()) {

		if (mode == _("Name contains")) {
			compstr = info->name;
		} else if (mode == _("Category contains")) {
			compstr = info->category;
		} else if (mode == _("Type contains")) {

			switch (info->type) {
			case LADSPA:
				compstr = X_("LADSPA");
				break;
			case AudioUnit:
				compstr = X_("AudioUnit");
				break;
			case LV2:
				compstr = X_("LV2");
				break;
			case Windows_VST:
				compstr = X_("VST");
				break;
			case LXVST:
				compstr = X_("LXVST");
				break;
			}

		} else if (mode == _("Author contains")) {
			compstr = info->creator;
		} else if (mode == _("Library contains")) {
			compstr = info->path;
		}

		if (compstr.empty()) {
			return false;
		}

		transform (compstr.begin(), compstr.end(), compstr.begin(), ::toupper);

		if (compstr.find (filterstr) != string::npos) {
			return true;
		} else {
			return false;
		}
	}

	return true;
}

void
PluginSelector::setup_filter_string (string& filterstr)
{
	filterstr = filter_entry.get_text ();
	transform (filterstr.begin(), filterstr.end(), filterstr.begin(), ::toupper);
}

void
PluginSelector::refill ()
{
	std::string filterstr;

	in_row_change = true;

	plugin_model->clear ();

	setup_filter_string (filterstr);

	ladspa_refiller (filterstr);
	lv2_refiller (filterstr);
	vst_refiller (filterstr);
	lxvst_refiller (filterstr);
	au_refiller (filterstr);

	in_row_change = false;
}

void
PluginSelector::refiller (const PluginInfoList& plugs, const::std::string& filterstr, const char* type)
{
	char buf[16];

	for (PluginInfoList::const_iterator i = plugs.begin(); i != plugs.end(); ++i) {

		if (show_this_plugin (*i, filterstr)) {

			TreeModel::Row newrow = *(plugin_model->append());
			newrow[plugin_columns.favorite] = (manager.get_status (*i) == PluginManager::Favorite);
			newrow[plugin_columns.hidden] = (manager.get_status (*i) == PluginManager::Hidden);
			newrow[plugin_columns.name] = (*i)->name;
			newrow[plugin_columns.type_name] = type;
			newrow[plugin_columns.category] = (*i)->category;

			string creator = (*i)->creator;
			string::size_type pos = 0;

			/* stupid LADSPA creator strings */

			while (pos < creator.length() && (isalnum (creator[pos]) || isspace (creator[pos]))) ++pos;
			creator = creator.substr (0, pos);

			newrow[plugin_columns.creator] = creator;

			snprintf (buf, sizeof(buf), "%d", (*i)->n_inputs.n_audio());
			newrow[plugin_columns.audio_ins] = buf;
			snprintf (buf, sizeof(buf), "%d", (*i)->n_inputs.n_midi());
			newrow[plugin_columns.midi_ins] = buf;

			snprintf (buf, sizeof(buf), "%d", (*i)->n_outputs.n_audio());
			newrow[plugin_columns.audio_outs] = buf;
			snprintf (buf, sizeof(buf), "%d", (*i)->n_outputs.n_midi());
			newrow[plugin_columns.midi_outs] = buf;

			newrow[plugin_columns.plugin] = *i;
		}
	}
}

void
PluginSelector::ladspa_refiller (const std::string& filterstr)
{
	refiller (manager.ladspa_plugin_info(), filterstr, "LADSPA");
}

void
PluginSelector::lv2_refiller (const std::string& filterstr)
{
#ifdef LV2_SUPPORT
	refiller (manager.lv2_plugin_info(), filterstr, "LV2");
#endif
}

void
#ifdef WINDOWS_VST_SUPPORT
PluginSelector::vst_refiller (const std::string& filterstr)
#else
PluginSelector::vst_refiller (const std::string&)
#endif
{
#ifdef WINDOWS_VST_SUPPORT
	refiller (manager.windows_vst_plugin_info(), filterstr, "VST");
#endif
}

void
#ifdef LXVST_SUPPORT
PluginSelector::lxvst_refiller (const std::string& filterstr)
#else
PluginSelector::lxvst_refiller (const std::string&)
#endif
{
#ifdef LXVST_SUPPORT
	refiller (manager.lxvst_plugin_info(), filterstr, "LXVST");
#endif
}

void
#ifdef AUDIOUNIT_SUPPORT
PluginSelector::au_refiller (const std::string& filterstr)
#else
PluginSelector::au_refiller (const std::string&)
#endif
{
#ifdef AUDIOUNIT_SUPPORT
	refiller (manager.au_plugin_info(), filterstr, "AU");
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
PluginSelector::btn_update_clicked()
{
	manager.refresh ();
	refill();
}

void
PluginSelector::display_selection_changed()
{
	if (plugin_display.get_selection()->count_selected_rows() != 0) {
		btn_add->set_sensitive (true);
	} else {
		btn_add->set_sensitive (false);
	}
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

	return (int) r;
}

void
PluginSelector::filter_button_clicked ()
{
	filter_entry.set_text ("");
}

void
PluginSelector::filter_entry_changed ()
{
	refill ();
}

void
PluginSelector::filter_mode_changed ()
{
	std::string mode = filter_mode.get_active_text ();

	if (mode == _("Favorites only") || mode == _("Hidden only")) {
		filter_entry.set_sensitive (false);
	} else {
		filter_entry.set_sensitive (true);
	}

	refill ();
}

void
PluginSelector::on_show ()
{
	ArdourDialog::on_show ();
	filter_entry.grab_focus ();
}

struct PluginMenuCompareByCreator {
    bool operator() (PluginInfoPtr a, PluginInfoPtr b) const {
	    int cmp;

	    cmp = strcasecmp (a->creator.c_str(), b->creator.c_str());

	    if (cmp < 0) {
		    return true;
	    } else if (cmp == 0) {
		    /* same creator ... compare names */
		    if (strcasecmp (a->name.c_str(), b->name.c_str()) < 0) {
			    return true;
		    }
	    }
	    return false;
    }
};

struct PluginMenuCompareByName {
    bool operator() (PluginInfoPtr a, PluginInfoPtr b) const {
	    int cmp;

	    cmp = strcasecmp (a->name.c_str(), b->name.c_str());

	    if (cmp < 0) {
		    return true;
	    } else if (cmp == 0) {
		    /* same name ... compare type */
		    if (a->type < b->type) {
			    return true;
		    }
	    }
	    return false;
    }
};

struct PluginMenuCompareByCategory {
    bool operator() (PluginInfoPtr a, PluginInfoPtr b) const {
	    int cmp;

	    cmp = strcasecmp (a->category.c_str(), b->category.c_str());

	    if (cmp < 0) {
		    return true;
	    } else if (cmp == 0) {
		    /* same category ... compare names */
		    if (strcasecmp (a->name.c_str(), b->name.c_str()) < 0) {
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
	PluginInfoList all_plugs;

	all_plugs.insert (all_plugs.end(), manager.ladspa_plugin_info().begin(), manager.ladspa_plugin_info().end());
#ifdef WINDOWS_VST_SUPPORT
	all_plugs.insert (all_plugs.end(), manager.windows_vst_plugin_info().begin(), manager.windows_vst_plugin_info().end());
#endif
#ifdef LXVST_SUPPORT
	all_plugs.insert (all_plugs.end(), manager.lxvst_plugin_info().begin(), manager.lxvst_plugin_info().end());
#endif
#ifdef AUDIOUNIT_SUPPORT
	all_plugs.insert (all_plugs.end(), manager.au_plugin_info().begin(), manager.au_plugin_info().end());
#endif
#ifdef LV2_SUPPORT
	all_plugs.insert (all_plugs.end(), manager.lv2_plugin_info().begin(), manager.lv2_plugin_info().end());
#endif

	using namespace Menu_Helpers;

	delete _plugin_menu;

	_plugin_menu = manage (new Menu);
	_plugin_menu->set_name("ArdourContextMenu");

	MenuList& items = _plugin_menu->items();
	items.clear ();

	Gtk::Menu* favs = create_favs_menu(all_plugs);
	items.push_back (MenuElem (_("Favorites"), *manage (favs)));

	items.push_back (MenuElem (_("Plugin Manager..."), sigc::mem_fun (*this, &PluginSelector::show_manager)));
	items.push_back (SeparatorElem ());

	Menu* by_creator = create_by_creator_menu(all_plugs);
	items.push_back (MenuElem (_("By Creator"), *manage (by_creator)));

	Menu* by_category = create_by_category_menu(all_plugs);
	items.push_back (MenuElem (_("By Category"), *manage (by_category)));
}

Gtk::Menu*
PluginSelector::create_favs_menu (PluginInfoList& all_plugs)
{
	using namespace Menu_Helpers;

	Menu* favs = new Menu();
	favs->set_name("ArdourContextMenu");

	PluginMenuCompareByName cmp_by_name;
	all_plugs.sort (cmp_by_name);

	for (PluginInfoList::const_iterator i = all_plugs.begin(); i != all_plugs.end(); ++i) {
		if (manager.get_status (*i) == PluginManager::Favorite) {
			MenuElem elem ((*i)->name, (sigc::bind (sigc::mem_fun (*this, &PluginSelector::plugin_chosen_from_menu), *i)));
			elem.get_child()->set_use_underline (false);
			favs->items().push_back (elem);
		}
	}
	return favs;
}

Gtk::Menu*
PluginSelector::create_by_creator_menu (ARDOUR::PluginInfoList& all_plugs)
{
	using namespace Menu_Helpers;

	typedef std::map<std::string,Gtk::Menu*> SubmenuMap;
	SubmenuMap creator_submenu_map;

	Menu* by_creator = new Menu();
	by_creator->set_name("ArdourContextMenu");

	MenuList& by_creator_items = by_creator->items();
	PluginMenuCompareByCreator cmp_by_creator;
	all_plugs.sort (cmp_by_creator);

	for (PluginInfoList::const_iterator i = all_plugs.begin(); i != all_plugs.end(); ++i) {

		if (manager.get_status (*i) == PluginManager::Hidden) continue;

		string creator = (*i)->creator;

		/* stupid LADSPA creator strings */
		string::size_type pos = 0;
		while (pos < creator.length() && (isalnum (creator[pos]) || isspace (creator[pos]))) ++pos;
		creator = creator.substr (0, pos);

		SubmenuMap::iterator x;
		Gtk::Menu* submenu;
		if ((x = creator_submenu_map.find (creator)) != creator_submenu_map.end()) {
			submenu = x->second;
		} else {
			submenu = new Gtk::Menu;
			by_creator_items.push_back (MenuElem (creator, *manage (submenu)));
			creator_submenu_map.insert (pair<std::string,Menu*> (creator, submenu));
			submenu->set_name("ArdourContextMenu");
		}
		MenuElem elem ((*i)->name, (sigc::bind (sigc::mem_fun (*this, &PluginSelector::plugin_chosen_from_menu), *i)));
		elem.get_child()->set_use_underline (false);
		submenu->items().push_back (elem);
	}
	return by_creator;
}

Gtk::Menu*
PluginSelector::create_by_category_menu (ARDOUR::PluginInfoList& all_plugs)
{
	using namespace Menu_Helpers;

	typedef std::map<std::string,Gtk::Menu*> SubmenuMap;
	SubmenuMap category_submenu_map;

	Menu* by_category = new Menu();
	by_category->set_name("ArdourContextMenu");

	MenuList& by_category_items = by_category->items();
	PluginMenuCompareByCategory cmp_by_category;
	all_plugs.sort (cmp_by_category);

	for (PluginInfoList::const_iterator i = all_plugs.begin(); i != all_plugs.end(); ++i) {

		if (manager.get_status (*i) == PluginManager::Hidden) continue;

		string category = (*i)->category;

		SubmenuMap::iterator x;
		Gtk::Menu* submenu;
		if ((x = category_submenu_map.find (category)) != category_submenu_map.end()) {
			submenu = x->second;
		} else {
			submenu = new Gtk::Menu;
			by_category_items.push_back (MenuElem (category, *manage (submenu)));
			category_submenu_map.insert (pair<std::string,Menu*> (category, submenu));
			submenu->set_name("ArdourContextMenu");
		}
		MenuElem elem ((*i)->name, (sigc::bind (sigc::mem_fun (*this, &PluginSelector::plugin_chosen_from_menu), *i)));
		elem.get_child()->set_use_underline (false);
		submenu->items().push_back (elem);
	}
	return by_category;
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

		(*iter)[plugin_columns.favorite] = favorite;
		(*iter)[plugin_columns.hidden] = false;
		PluginManager::PluginStatusType status = (favorite ? PluginManager::Favorite : PluginManager::Normal);

		/* save new statuses list */

		pi = (*iter)[plugin_columns.plugin];

		manager.set_status (pi->type, pi->unique_id, status);

		manager.save_statuses ();

		build_plugin_menu ();
	}
	in_row_change = false;
}

void
PluginSelector::hidden_changed (const std::string& path)
{
	PluginInfoPtr pi;

	if (in_row_change) {
		return;
	}

	in_row_change = true;

	TreeModel::iterator iter = plugin_model->get_iter (path);

	if (iter) {

		bool hidden = !(*iter)[plugin_columns.hidden];

		/* change state */

		(*iter)[plugin_columns.favorite] = false;
		(*iter)[plugin_columns.hidden] = hidden;
		PluginManager::PluginStatusType status = (hidden ? PluginManager::Hidden : PluginManager::Normal);

		/* save new statuses list */

		pi = (*iter)[plugin_columns.plugin];

		manager.set_status (pi->type, pi->unique_id, status);

		manager.save_statuses ();
	}
	in_row_change = false;
}

void
PluginSelector::show_manager ()
{
	show_all();
	run ();
}

void
PluginSelector::set_interested_object (PluginInterestedObject& obj)
{
	interested_object = &obj;
}
