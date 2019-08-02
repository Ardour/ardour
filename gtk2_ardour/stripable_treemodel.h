/*
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

#ifndef __gtk2_ardour_stripable_treemodel_h__
#define __gtk2_ardour_stripable_treemodel_h__

#include <set>

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <stdint.h>

#include <gtkmm/treemodel.h>

namespace ARDOUR {
	class Session;
	class Stripable;
}

class AxisView;
class AxisViewProvider;
class StripableSorter;

class StripableTreeModel : public Gtk::TreeModel, public Glib::Object
{
protected:
	StripableTreeModel (AxisViewProvider&);
	virtual ~StripableTreeModel();

public:
	static Glib::RefPtr<StripableTreeModel> create (AxisViewProvider&);
	void set_session (ARDOUR::Session&);

protected:
	Gtk::TreeModelFlags get_flags_vfunc() const;
	int   get_n_columns_vfunc() const;
	GType get_column_type_vfunc(int index) const;
	void  get_value_vfunc(const TreeModel::iterator& iter, int column, Glib::ValueBase& value) const;
	bool  iter_next_vfunc(const iterator& iter, iterator& iter_next) const;
	bool  iter_children_vfunc(const iterator& parent, iterator& iter) const;
	bool  iter_has_child_vfunc(const iterator& iter) const;
	int   iter_n_children_vfunc(const iterator& iter) const;
	int   iter_n_root_children_vfunc() const;
	bool  iter_nth_child_vfunc(const iterator& parent, int n, iterator& iter) const;
	bool  iter_nth_root_child_vfunc(int n, iterator& iter) const;
	bool  iter_parent_vfunc(const iterator& child, iterator& iter) const;
	Path  get_path_vfunc(const iterator& iter) const;
	bool  get_iter_vfunc(const Path& path, iterator& iter) const;
	bool  iter_is_valid(const iterator& iter) const;

public:
	typedef Gtk::TreeModelColumn<std::string> StringColumn;
	typedef Gtk::TreeModelColumn<bool>        BoolColumn;
	typedef Gtk::TreeModelColumn<uint32_t>    UnsignedColumn;
	typedef Gtk::TreeModelColumn<AxisView*> AVColumn;
	typedef Gtk::TreeModelColumn<boost::shared_ptr<ARDOUR::Stripable> > StripableColumn;

	struct Columns : public Gtk::TreeModel::ColumnRecord
	{
		Columns () {
			add (text);
			add (visible);
			add (rec_state);
			add (rec_safe);
			add (mute_state);
			add (solo_state);
			add (solo_visible);
			add (solo_isolate_state);
			add (solo_safe_state);
			add (is_track);
			add (av);
			add (stripable);
			add (name_editable);
			add (is_input_active);
			add (is_midi);
			add (active);
		}

		StringColumn text;
		BoolColumn visible;
		UnsignedColumn rec_state;
		UnsignedColumn rec_safe;
		UnsignedColumn mute_state;
		UnsignedColumn solo_state;
		/** true if the solo buttons are visible for this route, otherwise false */
		BoolColumn solo_visible;
		UnsignedColumn solo_isolate_state;
		UnsignedColumn solo_safe_state;
		BoolColumn is_track;
		AVColumn av;
		StripableColumn stripable;
		BoolColumn name_editable;
		BoolColumn is_input_active;
		BoolColumn is_midi;
		BoolColumn active;
	};

	Columns columns;

private:
	ARDOUR::Session* _session;
	AxisViewProvider& axis_view_provider;

	int n_columns;

	void text_value (boost::shared_ptr<ARDOUR::Stripable> stripable, Glib::ValueBase& value) const;

	struct Glue
	{
		Glue (boost::shared_ptr<ARDOUR::Stripable>);

		boost::weak_ptr<ARDOUR::Stripable> stripable;
	};

	typedef std::set<Glue*> GlueList;
	mutable GlueList glue_list;
	void remember_glue_item (Glue*) const;
};

#endif /* __gtk2_ardour_stripable_treemodel_h__ */
