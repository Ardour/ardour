/*
    Copyright (C) 2009-2013 Paul Davis
    Author: Johannes Mueller

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

#include <gtkmm/label.h>

#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/gui_thread.h"

#include "pbd/i18n.h"
#include "pbd/strsplit.h"
#include "pbd/signals.h"

#include "button_config_widget.h"

using namespace std;
using namespace Gtk;
using namespace ArdourSurface;

class ActionModel
{
public:
	static const ActionModel& instance ();

	const Glib::RefPtr<TreeStore> model () const { return _available_action_model; }

	const TreeModelColumn<string>& name () const { return _action_columns.name; }
	const TreeModelColumn<string>& path () const { return _action_columns.path; }

private:
	ActionModel ();
	struct ActionColumns : public TreeModel::ColumnRecord {
		ActionColumns() {
			add (name);
			add (path);
		}
		TreeModelColumn<string> name;
		TreeModelColumn<string> path;
	};

	const ActionColumns _action_columns;
	Glib::RefPtr<TreeStore> _available_action_model;
};


ButtonConfigWidget::ButtonConfigWidget ()
	: HBox ()
	, _choice_jump (_("Jump: "))
	, _choice_action (_("Other action: "))
	, _jump_distance (JumpDistance ({ .value = 1, .unit = BEATS }))
	, _action_model (ActionModel::instance ())
{
	RadioButtonGroup cbg = _choice_jump.get_group ();
	_choice_action.set_group (cbg);
	_choice_jump.signal_toggled().connect (sigc::mem_fun (*this, &ButtonConfigWidget::update_choice));

	_jump_distance.Changed.connect (sigc::mem_fun (*this, &ButtonConfigWidget::update_config));
	_action_cb.set_model (_action_model.model());
	_action_cb.signal_changed().connect (sigc::mem_fun (*this, &ButtonConfigWidget::update_config));
	_action_cb.pack_start (_action_model.name (), true);

	HBox* jump_box = manage (new HBox);
	jump_box->pack_start (_choice_jump, false, true);
	jump_box->pack_start (_jump_distance, false, true);

	HBox* action_box = manage (new HBox);
	action_box->pack_start (_choice_action, false, true);
	action_box->pack_start (_action_cb, false, true);

	set_spacing (25);
	pack_start (*jump_box, false, true);
	pack_start (*action_box, false, true);
}

bool
ButtonConfigWidget::find_action_in_model (const TreeModel::iterator& iter, string const& action_path, TreeModel::iterator* found)
{
	TreeModel::Row row = *iter;

	if (action_path == string(row[_action_model.path ()])) {
		*found = iter;
		return true;
	}

	return false;
}

void
ButtonConfigWidget::set_current_config (boost::shared_ptr<ButtonBase> btn_cnf)
{
	const ButtonAction* ba = dynamic_cast<const ButtonAction*> (btn_cnf.get());
	if (ba) {
		set_current_action (ba->get_path ());
		_action_cb.set_sensitive (true);
		_jump_distance.set_sensitive (false);
	} else {
		const ButtonJump* bj = static_cast<const ButtonJump*> (btn_cnf.get());
		set_jump_distance (bj->get_jump_distance());
		_action_cb.set_sensitive (false);
		_jump_distance.set_sensitive (true);
	}
}

boost::shared_ptr<ButtonBase>
ButtonConfigWidget::get_current_config (ShuttleproControlProtocol& scp) const
{
	if (_choice_jump.get_active ()) {
		return boost::shared_ptr<ButtonBase> (new ButtonJump (JumpDistance (_jump_distance.get_distance ()), scp));
	}

	TreeModel::const_iterator row = _action_cb.get_active ();
	string action_path = (*row)[_action_model.path ()];

	return boost::shared_ptr<ButtonBase> (new ButtonAction (action_path, scp));
}


void
ButtonConfigWidget::set_current_action (std::string action_string)
{
	_choice_action.set_active (true);
	_choice_jump.set_active (false);
	if (action_string.empty()) {
		_action_cb.set_active (0);
		return;
	}

	TreeModel::iterator iter = _action_model.model()->children().end();

	_action_model.model()->foreach_iter (sigc::bind (sigc::mem_fun (*this, &ButtonConfigWidget::find_action_in_model),  action_string, &iter));

	if (iter != _action_model.model()->children().end()) {
		_action_cb.set_active (iter);
	} else {
		_action_cb.set_active (0);
	}
}

void
ButtonConfigWidget::set_jump_distance (JumpDistance dist)
{
	_choice_jump.set_active (true);
	_choice_action.set_active (false);
	_jump_distance.set_distance (dist);

	Changed (); /* emit signal */
}

void
ButtonConfigWidget::update_choice ()
{
	_jump_distance.set_sensitive (_choice_jump.get_active ());
	_action_cb.set_sensitive (_choice_action.get_active ());

	Changed (); /* emit signal */
}


void
ButtonConfigWidget::update_config ()
{
	Changed (); /* emit signal */
}



const ActionModel&
ActionModel::instance ()
{
	static ActionModel am;
	return am;
}

ActionModel::ActionModel ()
{
	_available_action_model = TreeStore::create (_action_columns);
	_available_action_model->clear ();

	typedef std::map<string,TreeIter> NodeMap;
	NodeMap nodes;
	NodeMap::iterator r;

	TreeIter rowp;
	TreeModel::Row parent;


	rowp = _available_action_model->append ();
	parent = *(rowp);
	parent[_action_columns.name] = _("Disabled");

	vector<string> paths;
	vector<string> labels;
	vector<string> tooltips;
	vector<string> keys;
	vector<Glib::RefPtr<Gtk::Action> > actions;

	Gtkmm2ext::ActionMap::get_all_actions (paths, labels, tooltips, keys, actions);

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
		if ( parts[1] == _("Main_menu") )
			continue;
		if ( parts[1] == _("JACK") )
			continue;
		if ( parts[1] == _("redirectmenu") )
			continue;
		if ( parts[1] == _("Editor_menus") )
			continue;
		if ( parts[1] == _("RegionList") )
			continue;
		if ( parts[1] == _("ProcessorMenu") )
			continue;

		if ((r = nodes.find (parts[1])) == nodes.end()) {
			/* top level is missing */

			TreeIter rowp;
			TreeModel::Row parent;
			rowp = _available_action_model->append();
			nodes[parts[1]] = rowp;
			parent = *(rowp);
			parent[_action_columns.name] = parts[1];

			row = *(_available_action_model->append (parent.children()));
		} else {
			row = *(_available_action_model->append ((*r->second)->children()));
		}

		/* add this action */

		if (l->empty ()) {
			row[_action_columns.name] = *t;
		} else {
			row[_action_columns.name] = *l;
		}

		string path = (*p);
		/* ControlProtocol::access_action() is not interested in the
		   legacy "<Actions>/" prefix part of a path.
		*/
		path = path.substr (strlen ("<Actions>/"));

		row[_action_columns.path] = path;
	}
}
