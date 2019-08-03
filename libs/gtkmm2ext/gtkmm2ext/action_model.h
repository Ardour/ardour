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

#include <string>

#include <gtkmm/treestore.h>
#include <gtkmm/treemodelcolumn.h>

#include "gtkmm2ext/visibility.h"

namespace Gtk
{
class ComboBox;
}

/*
  The singleton ActionModel provides a Gtk::Treestore of all actions known to
  ardour.

  To be used for example by surface control editors to implement action bindings.
*/

namespace ActionManager {

class LIBGTKMM2EXT_API ActionModel
{
public:
	static const ActionModel& instance ();

	const Glib::RefPtr<Gtk::TreeStore> model () const { return _model; }

	const Gtk::TreeModelColumn<std::string>& name () const { return _columns.name; }
	const Gtk::TreeModelColumn<std::string>& path () const { return _columns.path; }

	struct Columns : public Gtk::TreeModel::ColumnRecord {
		Columns() {
			add (name);
			add (path);
		}
		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<std::string> path;
	};

	const Columns& columns() const { return _columns; }

	void build_action_combo (Gtk::ComboBox& cb, std::string const& current_action) const;
	void build_custom_action_combo (Gtk::ComboBox& cb,
					const std::vector<std::pair<std::string,std::string> >& actions,
					const std::string& current_action) const;

private:
	ActionModel ();

	bool find_action_in_model (const Gtk::TreeModel::iterator& iter, std::string const & action_path, Gtk::TreeModel::iterator* found) const;

	const Columns _columns;
	Glib::RefPtr<Gtk::TreeStore> _model;
};

}
