/*
 * Copyright (C) 2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#include <iostream>

#include "ardour/session.h"
#include "ardour/types.h"

#include "axis_provider.h"
#include "stripable_treemodel.h"

using namespace ARDOUR;

StripableTreeModel::Glue::Glue (boost::shared_ptr<Stripable> s)
	: stripable (s)
{
}

StripableTreeModel::StripableTreeModel (AxisViewProvider& avp)
	: Glib::ObjectBase( typeid(StripableTreeModel) ) //register a custom GType.
	, Glib::Object() //The custom GType is actually registered here.
	, axis_view_provider (avp)
{
	n_columns = columns.size();
}

void
StripableTreeModel::set_session (Session& s)
{
	_session = &s;
}

StripableTreeModel::~StripableTreeModel()
{
}

Glib::RefPtr<StripableTreeModel>
StripableTreeModel::create (AxisViewProvider& avp)
{
	return Glib::RefPtr<StripableTreeModel> (new StripableTreeModel (avp));
}

Gtk::TreeModelFlags
StripableTreeModel::get_flags_vfunc() const
{
	return Gtk::TREE_MODEL_LIST_ONLY;
}

int
StripableTreeModel::get_n_columns_vfunc() const
{
	return n_columns;
}

GType
StripableTreeModel::get_column_type_vfunc (int index) const
{
	if (index <= n_columns) {
		return columns.types()[index];
	}
	return 0;
}

void
StripableTreeModel::get_value_vfunc (const TreeModel::iterator& iter, int column, Glib::ValueBase& value) const
{
	if (!_session) {
		return;
	}

	if (column > n_columns) {
		return;
	}

	const Glue* glue = (const Glue*)iter.gobj()->user_data;
	boost::shared_ptr<Stripable> iter_stripable = glue->stripable.lock();

	if (!iter_stripable) {
		return;
	}

	switch (column) {
	case 0:
		return text_value (iter_stripable, value);
	}
}

void
StripableTreeModel::text_value (boost::shared_ptr<Stripable> stripable, Glib::ValueBase& value) const
{
	StringColumn::ValueType val;
	val.set (stripable->name());
	value = val;
}

bool
StripableTreeModel::iter_next_vfunc (const iterator& iter, iterator& iter_next) const
{
	if (!_session) {
		return false;
	}

	const Glue* glue = (const Glue*)iter.gobj()->user_data;
	boost::shared_ptr<Stripable> iter_stripable = glue->stripable.lock();

	if (!iter_stripable) {
		return false;
	}

	//initialize the next iterator:
	iter_next = iterator();

	StripableList sl;
	_session->get_stripables (sl);
	if (sl.empty()) {
		return false;
	}
	sl.sort (Stripable::Sorter());

	for (StripableList::const_iterator s = sl.begin(); s != sl.end(); ++s) {

		if (*s == iter_stripable) {
			++s;
			if (s != sl.end()) {
				Glue* new_glue = new Glue (iter_stripable);
				iter_next.gobj()->user_data = (void*)new_glue;
				remember_glue_item (new_glue);
				return true; //success
			}
			break;
		}
	}

	return false; //There is no next row.
}

bool
StripableTreeModel::iter_children_vfunc(const iterator& parent, iterator& iter) const
{
	return false;
}

bool
StripableTreeModel::iter_has_child_vfunc(const iterator& iter) const
{
	return false;
}

int
StripableTreeModel::iter_n_children_vfunc(const iterator& iter) const
{
	return 0;
}

int
StripableTreeModel::iter_n_root_children_vfunc() const
{
	if (_session) {
		StripableList sl;
		_session->get_stripables (sl);
		return sl.size();
	}
	return 0;
}

bool
StripableTreeModel::iter_nth_child_vfunc(const iterator& parent, int /* n */, iterator& iter) const
{
	iter = iterator(); //Set is as invalid, as the TreeModel documentation says that it should be.
	return false; //There are no children.
}

bool
StripableTreeModel::iter_nth_root_child_vfunc(int n, iterator& iter) const
{
	iter = iterator(); //clear the input parameter.
	if (!_session) {
		return false;
	}

	StripableList sl;
	_session->get_stripables (sl);

	if (sl.empty()) {
		return false;
	}

	sl.sort (Stripable::Sorter());

	StripableList::const_iterator s;

	for (s = sl.begin(); s != sl.end() && n > 0; ++s, --n);

	if (s != sl.end()) {
		Glue* new_glue = new Glue (*s);
		iter.gobj()->user_data = new_glue;
		remember_glue_item (new_glue);
		return true;
	}

	return false; //There are no children.
}

bool
StripableTreeModel::iter_parent_vfunc(const iterator& child, iterator& iter) const
{
	iter = iterator(); //Set is as invalid, as the TreeModel documentation says that it should be.
	return false; //There are no children, so no parents.
}

Gtk::TreeModel::Path
StripableTreeModel::get_path_vfunc(const iterator& /* iter */) const
{
	//TODO:
	return Path();
}

bool
StripableTreeModel::get_iter_vfunc (const Path& path, iterator& iter) const
{
	unsigned sz = path.size();

	if (!sz || sz > 1) {
		/* path must refer to something, but not children since we
		   don't do children.
		*/
		iter = iterator(); //Set is as invalid, as the TreeModel documentation says that it should be.
		return false;
	}

	return iter_nth_root_child_vfunc (path[0], iter);
}

bool
StripableTreeModel::iter_is_valid(const iterator& iter) const
{
	const Glue* glue = (const Glue*)iter.gobj()->user_data;

	if (!glue->stripable.lock()) {
		return false;
	}

	return Gtk::TreeModel::iter_is_valid(iter);
}

void
StripableTreeModel::remember_glue_item (Glue* item) const
{
	glue_list.insert (item);
}
