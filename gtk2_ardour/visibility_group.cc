/*
    Copyright (C) 2011 Paul Davis
    Author: Carl Hetherington <cth@carlh.net>

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

#include <gtkmm/menu.h>
#include <gtkmm/menushell.h>
#include <gtkmm/treeview.h>
#include "pbd/xml++.h"
#include "visibility_group.h"

#include "i18n.h"

using namespace std;

VisibilityGroup::VisibilityGroup (std::string const & name)
	: _xml_property_name (name)
	, _ignore_list_view_change (false)
{

}

/** Add a widget to the group.
 *  @param widget The widget.
 *  @param id Some single-word ID to be used for the state of this member in XML.
 *  @param name User-visible name for the widget.
 *  @param visible true to default to visible, otherwise false.
 *  @param override A functor to decide whether the visibility specified by the member should be
 *  overridden by some external factor; if the returned optional value is given, it will be used
 *  to override whatever visibility setting the member has.
 */

void
VisibilityGroup::add (Gtk::Widget* widget, string const & id, string const & name, bool visible, boost::function<boost::optional<bool> ()> override)
{
	Member m;
	m.widget = widget;
	m.id = id;
	m.name = name;
	m.visible = visible;
	m.override = override;
	
	_members.push_back (m);
}

/** Pop up a menu (on right-click) to configure visibility of members */
bool
VisibilityGroup::button_press_event (GdkEventButton* ev)
{
	if (ev->button != 3) {
		return false;
	}

	menu()->popup (1, ev->time);
	return true;
}

Gtk::Menu*
VisibilityGroup::menu ()
{
	using namespace Gtk::Menu_Helpers;

	Gtk::Menu* m = Gtk::manage (new Gtk::Menu);
	MenuList& items = m->items ();

	for (vector<Member>::iterator i = _members.begin(); i != _members.end(); ++i) {
		items.push_back (CheckMenuElem (i->name));
		Gtk::CheckMenuItem* j = dynamic_cast<Gtk::CheckMenuItem*> (&items.back ());
		j->set_active (i->visible);
		j->signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &VisibilityGroup::toggle), i));
	}

	return m;
}

/** @return true if the member should be visible, even taking into account any override functor */
bool
VisibilityGroup::should_actually_be_visible (Member const & m) const
{
	if (m.override) {
		boost::optional<bool> o = m.override ();
		if (o) {
			return o.get ();
		}
	}

	return m.visible;
}

/** Update visible consequences of any changes to our _members vector */
void
VisibilityGroup::update ()
{
	for (vector<Member>::iterator i = _members.begin(); i != _members.end(); ++i) {
		if (i->widget) {
			if (should_actually_be_visible (*i)) {
				i->widget->show ();
			} else {
				i->widget->hide ();
			}
		}
	}

	update_list_view ();

	VisibilityChanged (); /* EMIT SIGNAL */
}

void
VisibilityGroup::toggle (vector<Member>::iterator m)
{
	m->visible = !m->visible;
	update ();
}

void
VisibilityGroup::set_state (XMLNode const & node)
{
	XMLProperty const * p = node.property (_xml_property_name);
	if (!p) {
		return;
	}

	set_state (p->value ());
}

void
VisibilityGroup::set_state (string v)
{
	for (vector<Member>::iterator i = _members.begin(); i != _members.end(); ++i) {
		i->visible = false;
	}

	do {
		string::size_type const comma = v.find_first_of (',');
		string segment = v.substr (0, comma);

		for (vector<Member>::iterator i = _members.begin(); i != _members.end(); ++i) {
			if (segment == i->id) {
				i->visible = true;
			}
		}

		if (comma == string::npos) {
			break;
		}

		v = v.substr (comma + 1);
		
	} while (1);

	update ();
}

string
VisibilityGroup::get_state_name () const
{
	return _xml_property_name;
}

string
VisibilityGroup::get_state_value () const
{
	string result;
	for (vector<Member>::const_iterator i = _members.begin(); i != _members.end(); ++i) {
		if (i->visible) {
			if (!result.empty ()) {
				result += ',';
			}
			result += i->id;
		}
	}

	return result;
}

void
VisibilityGroup::update_list_view ()
{
	if (!_model) {
		return;
	}
	
	_ignore_list_view_change = true;

	_model->clear ();
	
	for (vector<Member>::iterator i = _members.begin(); i != _members.end(); ++i) {
		Gtk::TreeModel::iterator j = _model->append ();
		Gtk::TreeModel::Row row = *j;
		row[_model_columns._visible] = i->visible;
		row[_model_columns._name] = i->name;
		row[_model_columns._iterator] = i;
	}

	_ignore_list_view_change = false;
}

Gtk::Widget *
VisibilityGroup::list_view ()
{
	_model = Gtk::ListStore::create (_model_columns);

	update_list_view ();

	Gtk::TreeView* v = Gtk::manage (new Gtk::TreeView (_model));
	v->set_headers_visible (false);
	v->append_column ("", _model_columns._visible);
	v->append_column ("", _model_columns._name);

	Gtk::CellRendererToggle* visible_cell = dynamic_cast<Gtk::CellRendererToggle*> (v->get_column_cell_renderer (0));
	visible_cell->property_activatable() = true;
	visible_cell->signal_toggled().connect (sigc::mem_fun (*this, &VisibilityGroup::list_view_visible_changed));
	return v;
}

void
VisibilityGroup::list_view_visible_changed (string const & path)
{
	if (_ignore_list_view_change) {
		return;
	}
	
	Gtk::TreeModel::iterator i = _model->get_iter (path);
	if (!i) {
		return;
	}

	vector<Member>::iterator j = (*i)[_model_columns._iterator];
	j->visible = !j->visible;
	(*i)[_model_columns._visible] = j->visible;

	update ();
}
