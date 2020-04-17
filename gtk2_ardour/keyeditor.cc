/*
 * Copyright (C) 2007-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2017 Ben Loftis <ben@harrisonconsoles.com>
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

#include <map>
#include <fstream>
#include <sstream>

#include <boost/algorithm/string.hpp>

#include <glib.h>
#include <glib/gstdio.h>

#include <gtkmm/accelkey.h>
#include <gtkmm/accelmap.h>
#include <gtkmm/label.h>
#include <gtkmm/separator.h>
#include <gtkmm/stock.h>
#include <gtkmm/treemodelsort.h>
#include <gtkmm/uimanager.h>

#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/utils.h"

#include "pbd/error.h"
#include "pbd/openuri.h"
#include "pbd/strsplit.h"

#include "ardour/filesystem_paths.h"
#include "ardour/profile.h"

#include "actions.h"
#include "keyboard.h"
#include "keyeditor.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gdk;
using namespace PBD;

using Gtkmm2ext::Keyboard;
using Gtkmm2ext::Bindings;

sigc::signal<void> KeyEditor::UpdateBindings;

static bool
bindings_collision_dialog (Gtk::Window& parent, const std::string& bound_name)
{
	ArdourDialog dialog (parent, _("Colliding keybindings"), true);
	Label label (string_compose(
				_("The key sequence is already bound to '%1'.\n\n"
				  "You can replace the existing binding or cancel this action."), bound_name));

	dialog.get_vbox()->pack_start (label, true, true);

	dialog.add_button (_("Cancel"), Gtk::RESPONSE_CANCEL);
	dialog.add_button (_("Replace"), Gtk::RESPONSE_ACCEPT);
	dialog.show_all ();

	switch (dialog.run()) {
	case RESPONSE_ACCEPT:
		return true;
	default:
		break;
	}
	return false;
}

KeyEditor::KeyEditor ()
	: ArdourWindow (_("Keyboard Shortcuts"))
	, unbind_button (_("Remove shortcut"))
	, unbind_box (BUTTONBOX_END)
	, filter_entry (_("Search..."), true)
	, filter_string("")
	, sort_column(0)
	, sort_type(Gtk::SORT_ASCENDING)
{

	notebook.signal_switch_page ().connect (sigc::mem_fun (*this, &KeyEditor::page_change));

	vpacker.pack_start (notebook, true, true);

	Glib::RefPtr<Gdk::Pixbuf> icon = ARDOUR_UI_UTILS::get_icon ("search");
	filter_entry.set_icon_from_pixbuf (icon);
	filter_entry.set_icon_tooltip_text (_("Click to reset search string"));
	filter_entry.signal_search_string_updated ().connect (sigc::mem_fun (*this, &KeyEditor::search_string_updated));
	vpacker.pack_start (filter_entry, false, false);

	Label* hint = manage (new Label (_("To remove a shortcut, select an action then press this: ")));
	hint->show ();
	unbind_box.pack_start (*hint, false, true);
	unbind_box.pack_start (unbind_button, false, false);
	unbind_button.signal_clicked().connect (sigc::mem_fun (*this, &KeyEditor::unbind));

	vpacker.set_spacing (4);
	vpacker.pack_start (unbind_box, false, false);
	unbind_box.show ();
	unbind_button.show ();

	reset_button.add (reset_label);
	reset_label.set_markup (string_compose ("  <span size=\"large\" weight=\"bold\">%1</span>  ", _("Reset Bindings to Defaults")));

	print_button.add (print_label);
	print_label.set_markup (string_compose ("  <span size=\"large\" weight=\"bold\">%1</span>  ", _("Print Bindings (to your web browser)")));

	print_button.signal_clicked().connect (sigc::mem_fun (*this, &KeyEditor::print));

	reset_box.pack_start (reset_button, true, false);
	reset_box.pack_start (print_button, true, false);
	reset_box.show ();
	reset_button.show ();
	reset_label.show ();
	print_button.show ();
	reset_button.signal_clicked().connect (sigc::mem_fun (*this, &KeyEditor::reset));
	vpacker.pack_start (*(manage (new  HSeparator())), false, false, 5);
	vpacker.pack_start (reset_box, false, false);

	add (vpacker);

	unbind_button.set_sensitive (false);
	_refresh_connection = UpdateBindings.connect (sigc::mem_fun (*this, &KeyEditor::refresh));
}

void
KeyEditor::add_tab (string const & name, Bindings& bindings)
{
	Tab* t = new Tab (*this, name, &bindings);

	if (t->populate () == 0) {
		/* no bindings */
		delete t;
		return;
	}

	tabs.push_back (t);
	t->show_all ();
	notebook.append_page (*t, name);
}


void
KeyEditor::remove_tab (string const &name)
{
	guint npages = notebook.get_n_pages ();

	for (guint n = 0; n < npages; ++n) {
		Widget* w = notebook.get_nth_page (n);
		Tab* tab = dynamic_cast<Tab*> (w);
		if (tab) {
			if (tab->name == name) {
				notebook.remove_page (*w);
				return;
			}
		}
	}
}

void
KeyEditor::unbind ()
{
	current_tab()->unbind ();
}

void
KeyEditor::page_change (GtkNotebookPage*, guint)
{
	current_tab()->view.get_selection()->unselect_all ();
	unbind_button.set_sensitive (false);
}

bool
KeyEditor::Tab::key_press_event (GdkEventKey* ev)
{
	if (view.get_selection()->count_selected_rows() != 1) {
		return false;
	}

	if (!ev->is_modifier) {
		last_keyval = ev->keyval;
	}

	/* Don't let anything else handle the key press, because navigation
	 * keys will be used by GTK to change the selection/treeview cursor
	 * position
	 */

	return true;
}

bool
KeyEditor::Tab::key_release_event (GdkEventKey* ev)
{
	if (view.get_selection()->count_selected_rows() != 1) {
		return false;
	}

	if (last_keyval == 0) {
		return false;
	}

	owner.current_tab()->bind (ev, last_keyval);

	last_keyval = 0;
	return true;
}

KeyEditor::Tab::Tab (KeyEditor& ke, string const & str, Bindings* b)
	: owner (ke)
	, name (str)
	, bindings (b)
	, last_keyval (0)
{
	data_model = TreeStore::create(columns);
	populate ();

	filter = TreeModelFilter::create(data_model);
	filter->set_visible_func (sigc::mem_fun (*this, &Tab::visible_func));

	sorted_filter = TreeModelSort::create(filter);

	view.set_model (sorted_filter);
	view.append_column (_("Action"), columns.name);
	view.append_column (_("Shortcut"), columns.binding);
	view.set_headers_visible (true);
	view.set_headers_clickable (true);
	view.get_selection()->set_mode (SELECTION_SINGLE);
	view.set_reorderable (false);
	view.set_size_request (500,300);
	view.set_enable_search (false);
	view.set_rules_hint (true);
	view.set_name (X_("KeyEditorTree"));

	view.signal_cursor_changed().connect (sigc::mem_fun (*this, &Tab::action_selected));
	view.signal_key_press_event().connect (sigc::mem_fun (*this, &Tab::key_press_event), false);
	view.signal_key_release_event().connect (sigc::mem_fun (*this, &Tab::key_release_event), false);

	view.get_column(0)->set_sort_column (columns.name);
	view.get_column(1)->set_sort_column (columns.binding);
	data_model->set_sort_column (owner.sort_column,  owner.sort_type);
	data_model->signal_sort_column_changed().connect (sigc::mem_fun (*this, &Tab::sort_column_changed));

	signal_map().connect (sigc::mem_fun (*this, &Tab::tab_mapped));

	scroller.add (view);
	scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	set_spacing (6);
	set_border_width (12);
	pack_start (scroller);
}

void
KeyEditor::Tab::action_selected ()
{
	if (view.get_selection()->count_selected_rows() == 0) {
		return;
	}

	TreeModel::const_iterator it = view.get_selection()->get_selected();

	if (!it) {
		return;
	}

	if (!(*it)[columns.bindable]) {
		owner.unbind_button.set_sensitive (false);
		return;
	}

	const string& binding = (*it)[columns.binding];

	if (!binding.empty()) {
		owner.unbind_button.set_sensitive (true);
	}
}

void
KeyEditor::Tab::unbind ()
{
	const std::string& action_path = (*view.get_selection()->get_selected())[columns.path];

	TreeModel::iterator it = find_action_path (data_model->children().begin(), data_model->children().end(),  action_path);

	if (!it || !(*it)[columns.bindable]) {
		return;
	}

	bindings->remove (Gtkmm2ext::Bindings::Press,  action_path , true);
	(*it)[columns.binding] = string ();

	owner.unbind_button.set_sensitive (false);
}

void
KeyEditor::Tab::bind (GdkEventKey* release_event, guint pressed_key)
{
	const std::string& action_path = (*view.get_selection()->get_selected())[columns.path];
	TreeModel::iterator it = find_action_path (data_model->children().begin(), data_model->children().end(),  action_path);

	/* pressed key could be upper case if Shift was used. We want all
	   single keys stored as their lower-case version, so ensure this
	*/

	pressed_key = gdk_keyval_to_lower (pressed_key);

	if (!it || !(*it)[columns.bindable]) {
		return;
	}

	GdkModifierType mod = (GdkModifierType)(Keyboard::RelevantModifierKeyMask & release_event->state);
	Gtkmm2ext::KeyboardKey new_binding (mod, pressed_key);

	std::string old_path;

	if (bindings->is_bound (new_binding, Gtkmm2ext::Bindings::Press, &old_path)) {
		if (!bindings_collision_dialog (owner, bindings->bound_name (new_binding, Gtkmm2ext::Bindings::Press))) {
			return;
		}
	}

	TreeModel::iterator oit = data_model->children().end();

	if (!old_path.empty()) {
		/* Remove the binding for the old action */
		if (!bindings->remove (Gtkmm2ext::Bindings::Press, old_path, false)) {
			return;
		}
		oit = find_action_path (data_model->children().begin(), data_model->children().end(),  old_path);
	}


	/* Add (or replace) the binding for the chosen action */
	bool result = bindings->replace (new_binding, Gtkmm2ext::Bindings::Press, action_path);

	if (result) {
		(*it)[columns.binding] = gtk_accelerator_get_label (new_binding.key(), (GdkModifierType) new_binding.state());
		if (oit != data_model->children().end()) {
			(*oit)[columns.binding] = "";
		}
		owner.unbind_button.set_sensitive (true);
	}
}

uint32_t
KeyEditor::Tab::populate ()
{
	vector<string> paths;
	vector<string> labels;
	vector<string> tooltips;
	vector<string> keys;
	vector<Glib::RefPtr<Action> > actions;
	typedef std::map<string,TreeIter> NodeMap;
	NodeMap nodes;
	NodeMap::iterator r;

	bindings->get_all_actions (paths, labels, tooltips, keys, actions);

	vector<string>::iterator k;
	vector<string>::iterator p;
	vector<string>::iterator t;
	vector<string>::iterator l;
	vector<Glib::RefPtr<Action> >::iterator a;

	data_model->clear ();

	for (a = actions.begin(), l = labels.begin(), k = keys.begin(), p = paths.begin(), t = tooltips.begin(); l != labels.end(); ++k, ++p, ++t, ++l, ++a) {

		TreeModel::Row row;
		vector<string> parts;

		split (*p, parts, '/');

		string category = parts[1];
		string action_name = parts[2];

		if (action_name.empty()) {
			continue;
		}

		//kinda kludgy way to avoid displaying menu items as mappable
		if ((action_name.find (X_("Menu")) == action_name.length() - 4) ||
		    (action_name.find (X_("menu")) == action_name.length() - 4) ||
		    (category.find (X_("Menu")) == category.length() - 4) ||
		    (category.find (X_("menu")) == category.length() - 4) ||
		    (action_name == _("RegionList"))) {
			continue;
		}

		if ((r = nodes.find (category)) == nodes.end()) {

			/* category/group is missing, so add it first */

			TreeIter rowp;
			TreeModel::Row parent;
			rowp = data_model->append();
			nodes[category] = rowp;
			parent = *(rowp);
			parent[columns.name] = category;
			parent[columns.bindable] = false;
			parent[columns.action] = *a;

			/* now set up the child row that we're about to fill
			 * out with information
			 */

			row = *(data_model->append (parent.children()));

		} else {

			/* category/group is present, so just add the child row */

			row = *(data_model->append ((*r->second)->children()));

		}

		/* add this action */

		/* use the "visible label" as the action name */

		if (l->empty ()) {
			/* no label, try using the tooltip instead */
			row[columns.name] = *t;
		} else {
			row[columns.name] = *l;
		}
		row[columns.path] = string_compose ("%1/%2", category, action_name);
		row[columns.bindable] = true;

		if (*k == ActionManager::unbound_string) {
			row[columns.binding] = string();
		} else {
			row[columns.binding] = *k;
		}
		row[columns.action] = *a;
	}

	return data_model->children().size();
}

void
KeyEditor::Tab::sort_column_changed ()
{
	int column;
	SortType type;

	if (data_model->get_sort_column_id (column, type)) {
		owner.sort_column = column;
		owner.sort_type = type;
	}
}

void
KeyEditor::Tab::tab_mapped ()
{
	data_model->set_sort_column (owner.sort_column,  owner.sort_type);
	filter->refilter ();

	if (data_model->children().size() == 1) {
		view.expand_all ();
	}
}

bool
KeyEditor::Tab::visible_func(const Gtk::TreeModel::const_iterator& iter) const
{
	if (!iter) {
		return false;
	}

	// never filter when search string is empty or item is a category
	if (owner.filter_string.empty () || !(*iter)[columns.bindable]) {
		return true;
	}

	// search name
	std::string name = (*iter)[columns.name];
	boost::to_lower (name);
	if (name.find (owner.filter_string) != std::string::npos) {
		return true;
	}

	// search binding
	std::string binding = (*iter)[columns.binding];
	boost::to_lower (binding);
	if (binding.find (owner.filter_string) != std::string::npos) {
		return true;
	}

	return false;
}

TreeModel::iterator
KeyEditor::Tab::find_action_path (TreeModel::const_iterator begin, TreeModel::const_iterator end, const std::string& action_path) const
{
	if (!begin) {
		return end;
	}

	for (TreeModel::iterator it = begin; it != end; ++it) {
		if (it->children()) {
			TreeModel::iterator jt = find_action_path (it->children().begin(), it->children().end(), action_path);
			if (jt != it->children().end()) {
				return jt;
			}
		}
		const std::string& path = (*it)[columns.path];
		if (action_path.compare(path) == 0) {
			return it;
		}
	}
	return end;
}

void
KeyEditor::reset ()
{
	Keyboard::the_keyboard().reset_bindings ();
	refresh ();
}

void
KeyEditor::refresh ()
{
	for (Tabs::iterator t = tabs.begin(); t != tabs.end(); ++t) {
		(*t)->view.get_selection()->unselect_all ();
		(*t)->populate ();
	}
}

KeyEditor::Tab*
KeyEditor::current_tab ()
{
	return dynamic_cast<Tab*> (notebook.get_nth_page (notebook.get_current_page()));
}

void
KeyEditor::search_string_updated (const std::string& filter)
{
	filter_string = boost::to_lower_copy(filter);
	KeyEditor::Tab* tab = current_tab ();
	if (tab) {
		tab->filter->refilter ();
	}
}

void
KeyEditor::print () const
{
	stringstream sstr;
	Bindings::save_all_bindings_as_html (sstr);

	if (sstr.str().empty()) {
		return;
	}


	gchar* file_name;
	GError *err = NULL;
	gint fd;

	if ((fd = g_file_open_tmp ("akprintXXXXXX.html", &file_name, &err)) < 0) {
		if (err) {
			error << string_compose (_("Could not open temporary file to print bindings (%1)"), err->message) << endmsg;
			g_error_free (err);
		}
		return;
	}

#ifdef PLATFORM_WINDOWS
	::close (fd);
#endif

	err = NULL;

	if (!g_file_set_contents (file_name, sstr.str().c_str(), sstr.str().size(), &err)) {
#ifndef PLATFORM_WINDOWS
		::close (fd);
#endif
		g_unlink (file_name);
		if (err) {
			error << string_compose (_("Could not save bindings to file (%1)"), err->message) << endmsg;
			g_error_free (err);
		}
		return;
	}

#ifndef PLATFORM_WINDOWS
	::close (fd);
#endif

	PBD::open_uri (string_compose ("file:///%1", file_name));
}
