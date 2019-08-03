/*
 * Copyright (C) 2019 Johannes Mueller <github@johannes-mueller.org>
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

#include <vector>

#include  <gtkmm/combobox.h>
#include <gtkmm/liststore.h>

#include "pbd/i18n.h"
#include "pbd/strsplit.h"

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/action_model.h"

using namespace std;
using namespace Gtk;

namespace ActionManager {

const ActionModel&
ActionModel::instance ()
{
	static ActionModel am;
	return am;
}

ActionModel::ActionModel ()
{
	_model = TreeStore::create (_columns);
	_model->clear ();

	typedef std::map<string,TreeIter> NodeMap;
	NodeMap nodes;
	NodeMap::iterator r;

	TreeIter rowp;
	TreeModel::Row parent;

	rowp = _model->append ();
	parent = *(rowp);
	parent[_columns.name] = _("Disabled");

	vector<string> paths;
	vector<string> labels;
	vector<string> tooltips;
	vector<string> keys;
	vector<Glib::RefPtr<Gtk::Action> > actions;

	get_all_actions (paths, labels, tooltips, keys, actions);

	vector<string>::iterator k;
	vector<string>::iterator p;
	vector<string>::iterator t;
	vector<string>::iterator l;

	for (l = labels.begin(), k = keys.begin(), p = paths.begin(), t = tooltips.begin(); l != labels.end(); ++k, ++p, ++t, ++l) {

		TreeModel::Row row;
		vector<string> parts;
		parts.clear ();
		split (*p, parts, '/');

		if (parts.empty()) {
			continue;
		}

		//kinda kludgy way to avoid displaying menu items as mappable
		if ( parts[0] == _("Main_menu") )
			continue;
		if ( parts[0] == _("JACK") )
			continue;
		if ( parts[0] == _("redirectmenu") )
			continue;
		if ( parts[0] == _("Editor_menus") )
			continue;
		if ( parts[0] == _("RegionList") )
			continue;
		if ( parts[0] == _("ProcessorMenu") )
			continue;

		if ((r = nodes.find (parts[0])) == nodes.end()) {
			/* top level is missing */

			TreeIter rowp;
			TreeModel::Row parent;
			rowp = _model->append();
			nodes[parts[0]] = rowp;
			parent = *(rowp);
			parent[_columns.name] = parts[0];

			row = *(_model->append (parent.children()));
		} else {
			row = *(_model->append ((*r->second)->children()));
		}

		/* add this action */

		if (l->empty ()) {
			row[_columns.name] = *t;
		} else {
			row[_columns.name] = *l;
		}

		row[_columns.path] = *p;
	}
}

bool
ActionModel::find_action_in_model (const TreeModel::iterator& iter, std::string const & action_path, TreeModel::iterator* found) const
{
	TreeModel::Row row = *iter;
	string path = row[_columns.path];

	if (path == action_path) {
		*found = iter;
		return true;
	}

	return false;
}

void
ActionModel::build_action_combo (ComboBox& cb, string const& current_action) const
{
	cb.set_model (_model);
	cb.pack_start (_columns.name);

	if (current_action.empty()) {
		cb.set_active (0); /* "disabled" */
		return;
	}

	TreeModel::iterator iter = _model->children().end();

	_model->foreach_iter (sigc::bind (sigc::mem_fun (*this, &ActionModel::find_action_in_model), current_action, &iter));

	if (iter != _model->children().end()) {
		cb.set_active (iter);
	} else {
		cb.set_active (0);
	}
}

void
ActionModel::build_custom_action_combo (ComboBox& cb, const vector<pair<string,string> >& actions, const string& current_action) const
{
	Glib::RefPtr<Gtk::ListStore> model (Gtk::ListStore::create (_columns));
	TreeIter rowp;
	TreeModel::Row row;
	int active_row = -1;
	int n;
	vector<pair<string,string> >::const_iterator i;

	rowp = model->append();
	row = *(rowp);
	row[_columns.name] = _("Disabled");
	row[_columns.path] = string();

	if (current_action.empty()) {
		active_row = 0;
	}

	for (i = actions.begin(), n = 0; i != actions.end(); ++i, ++n) {
		rowp = model->append();
		row = *(rowp);
		row[_columns.name] = i->first;
		row[_columns.path] = i->second;
		if (current_action == i->second) {
			active_row = n+1;
		}
	}

	cb.set_model (model);
	cb.pack_start (_columns.name);

	if (active_row >= 0) {
		cb.set_active (active_row);
	}
}

}
