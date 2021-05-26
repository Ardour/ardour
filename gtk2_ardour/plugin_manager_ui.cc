/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
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

#include "ardour/types_convert.h"

#include "gtkmm2ext/gui_thread.h"

#include "plugin_manager_ui.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

PluginManagerUI::PluginManagerUI ()
	: ArdourWindow (_("Plugin Manager"))
	, _btn_rescan_all (_("Re-scan Faulty"))
	, _btn_rescan_sel (_("Re-scan Selected"))
	, _btn_clear (_("Clear Stale Scan Log"))
{
	//plugin_model = Gtk::TreeStore::create (plugin_columns);
	plugin_model = Gtk::ListStore::create (plugin_columns);

	plugin_display.append_column (_("Status"), plugin_columns.status);
	plugin_display.append_column (_("BL"), plugin_columns.blacklisted);
	plugin_display.append_column (_("Name"), plugin_columns.name);
	plugin_display.append_column (_("Creator"), plugin_columns.creator);
	plugin_display.append_column (_("Type"), plugin_columns.type);
	plugin_display.append_column (_("Path"), plugin_columns.path);

	plugin_display.set_model (plugin_model);
	plugin_display.set_headers_visible (true);
	plugin_display.set_headers_clickable (true);
	plugin_display.set_reorderable (false);
	plugin_display.set_rules_hint (true);

	for (int i = 0; i < 5; ++i) {
		Gtk::TreeView::Column* column = plugin_display.get_column(i);
		if (column) {
			column->set_sort_column(i);
		}
	}

	plugin_model->set_sort_column (plugin_columns.name.index(), Gtk::SORT_ASCENDING);
	plugin_display.set_name("PluginSelectorDisplay");

	plugin_display.get_selection()->signal_changed().connect (sigc::mem_fun(*this, &PluginManagerUI::selection_changed));
#if 0
	plugin_display.get_selection()->set_mode (SELECTION_SINGLE);
	plugin_display.signal_row_activated().connect_notify (sigc::mem_fun(*this, &PluginManagerUI::row_activated));
#endif

	_scroller.add (plugin_display);
	_scroller.set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

	_log.set_editable (false);
	_log.set_wrap_mode (Gtk::WRAP_WORD);

	_log_scroller.set_shadow_type(Gtk::SHADOW_NONE);
	_log_scroller.set_border_width(0);
	_log_scroller.add (_log);
	_log_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	_pane.add (_scroller);
	_pane.add (_log_scroller);
	_pane.set_divider (0, .85);

	Gtk::Label* lbl = new Gtk::Label (""); // spacer

	/* top level packing */
	_top.attach (*lbl,            0, 1, 0, 1, Gtk::SHRINK, Gtk::EXPAND | Gtk::FILL, 4, 0);
	_top.attach (_btn_clear,      0, 1, 1, 2, Gtk::FILL | Gtk::SHRINK, Gtk::SHRINK, 4, 4);
	_top.attach (_btn_rescan_sel, 0, 1, 2, 3, Gtk::FILL | Gtk::SHRINK, Gtk::SHRINK, 4, 4);
	_top.attach (_btn_rescan_all, 0, 1, 3, 4, Gtk::FILL | Gtk::SHRINK, Gtk::SHRINK, 4, 4);
	_top.attach (_pane,           1, 2, 0, 4, Gtk::EXPAND | Gtk::FILL, Gtk::EXPAND | Gtk::FILL, 4, 0);

	add (_top);

	_log.set_size_request (400, -1);
	set_size_request (-1, 600);

	/* connect to signals */

	PluginManager::instance ().PluginListChanged.connect (_manager_connection, invalidator (*this), boost::bind (&PluginManagerUI::refill, this), gui_context());
	_btn_rescan_all.signal_clicked.connect (sigc::mem_fun (*this, &PluginManagerUI::rescan_all));
	_btn_rescan_sel.signal_clicked.connect (sigc::mem_fun (*this, &PluginManagerUI::rescan_selected));
	_btn_clear.signal_clicked.connect (sigc::mem_fun (*this, &PluginManagerUI::clear_log));
}

PluginManagerUI::~PluginManagerUI ()
{
}

void
PluginManagerUI::on_show ()
{
	refill (); // XXX -> only once in c'tor?
	ArdourWindow::on_show ();
}

static std::string
status_text (PluginScanLogEntry const& psle)
{
	if (!psle.recent ()) {
		return "Stale";
	}

	PluginScanLogEntry::PluginScanResult sr = psle.result ();
	if (sr == PluginScanLogEntry::OK) {
		return "OK";
	}
	// TODO pick most relevant, show others on tooltip only
	std::string rv;
	if ((int)sr & PluginScanLogEntry::New) {
		rv += "New ";
	}
	if ((int)sr & PluginScanLogEntry::Updated) {
		rv += "Updated ";
	}
	if ((int)sr & PluginScanLogEntry::Error) {
		rv += "Error ";
	}
	if ((int)sr & PluginScanLogEntry::Incompatible) {
		rv += "Incompatible ";
	}
	return rv;
}

static bool
is_blacklisted (PluginScanLogEntry const& psle)
{
	return (int) psle.result () & PluginScanLogEntry::Blacklisted;
}

static std::string
plugin_type (const PluginType t)
{
	/* see also PluginManager::to_generic_vst */
	switch (t) {
		case Windows_VST:
		case LXVST:
		case MacVST:
			return "VST2.x";
		default:
			return enum_2_string (t);
	}
}

void
PluginManagerUI::refill ()
{
	std::vector<boost::shared_ptr<PluginScanLogEntry> > psl;
	PluginManager& mgr (PluginManager::instance ());
	mgr.scan_log (psl);

	plugin_display.set_model (Glib::RefPtr<Gtk::TreeStore>(0));

	int sort_col;
	Gtk::SortType sort_type;
	bool sorted = plugin_model->get_sort_column_id (sort_col, sort_type);
	plugin_model->set_sort_column (-2, Gtk::SORT_ASCENDING);

	bool have_err   = false;
	bool have_stale = false;

	plugin_model->clear ();

	for (std::vector<boost::shared_ptr <PluginScanLogEntry> >::const_iterator i = psl.begin(); i != psl.end(); ++i) {
		PluginInfoList const& plugs = (*i)->nfo ();

		if (!(*i)->recent ()) {
			have_stale = true;
		} else if ((*i)->result () == PluginScanLogEntry::Blacklisted) {
			// OK, but manually blacklisted
		} else if ((*i)->result () != PluginScanLogEntry::OK) {
			have_err = true;
		}

		// TODO show "hidden" status
		// PluginManager::PluginStatusType status = manager.get_status (*i);
		if (plugs.size () == 0) {
			Gtk::TreeModel::Row newrow = *(plugin_model->append());
			newrow[plugin_columns.path] = (*i)->path ();
			newrow[plugin_columns.type] = plugin_type ((*i)->type ());
			newrow[plugin_columns.name] = "-";
			newrow[plugin_columns.creator] = "-";
			newrow[plugin_columns.status] = status_text (**i); // XXX
			newrow[plugin_columns.blacklisted] = is_blacklisted (**i);
			newrow[plugin_columns.psle] = *i;
		} else if (plugs.size () == 1) {
			Gtk::TreeModel::Row newrow = *(plugin_model->append());
			newrow[plugin_columns.path] = (*i)->path ();
			newrow[plugin_columns.type] = plugin_type ((*i)->type ());
			newrow[plugin_columns.name] = plugs.front()->name;
			newrow[plugin_columns.creator] = plugs.front()->creator;
			newrow[plugin_columns.status] = status_text (**i);
			newrow[plugin_columns.blacklisted] = is_blacklisted (**i);
			newrow[plugin_columns.psle] = *i;
		} else {
			for (PluginInfoList::const_iterator j = plugs.begin(); j != plugs.end(); ++j) {
				Gtk::TreeModel::Row newrow = *(plugin_model->append());
				newrow[plugin_columns.path] = (*i)->path ();
				newrow[plugin_columns.type] = plugin_type ((*i)->type ());
				newrow[plugin_columns.name] = (*j)->name;
				newrow[plugin_columns.creator] = (*j)->creator;
				newrow[plugin_columns.status] = status_text (**i);
				newrow[plugin_columns.blacklisted] = is_blacklisted (**i);
				newrow[plugin_columns.psle] = *i;
			}
		}
	}
	plugin_display.set_model (plugin_model);
	if (sorted) {
		plugin_model->set_sort_column (sort_col, sort_type);
	}

	_btn_clear.set_sensitive (have_stale);
	_btn_rescan_all.set_sensitive (have_err);
}

void
PluginManagerUI::selection_changed ()
{
	if (plugin_display.get_selection()->count_selected_rows() != 1) {
		_log.get_buffer()->set_text ("-");
		return;
	}
	Gtk::TreeIter iter = plugin_display.get_selection ()->get_selected ();
	boost::shared_ptr<PluginScanLogEntry> const& psle ((*iter)[plugin_columns.psle]);
	_log.get_buffer()->set_text (psle->log ());

	PluginScanLogEntry::PluginScanResult sr = psle->result ();
	if (sr == PluginScanLogEntry::OK) {
		_btn_rescan_sel.set_sensitive (false);
	} else {
		_btn_rescan_sel.set_sensitive (true);
	}
}

void
PluginManagerUI::rescan_all ()
{
}

void
PluginManagerUI::rescan_selected ()
{
}

void
PluginManagerUI::clear_log ()
{
}
