/*
    Copyright (C) 2002 Paul Davis

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

#include <map>

#include <gtkmm/stock.h>
#include <gtkmm/label.h>
#include <gtkmm/accelkey.h>
#include <gtkmm/accelmap.h>
#include <gtkmm/uimanager.h>

#include "gtkmm2ext/utils.h"

#include "pbd/file_utils.h"
#include "pbd/strsplit.h"

#include "ardour/filesystem_paths.h"
#include "ardour/profile.h"

#include "actions.h"
#include "keyboard.h"
#include "keyeditor.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gdk;
using namespace PBD;

using Gtkmm2ext::Keyboard;

#if defined(PLATFORM_WINDOWS)
const char* KeyEditor::blacklist_filename = "keybindings.win.blacklist";
#elif defined(__APPLE__)
const char* KeyEditor::blacklist_filename = "keybindings.mac.blacklist";
#else
const char* KeyEditor::blacklist_filename = "keybindings.blacklist";
#endif

const char* KeyEditor::whitelist_filename = "keybindings.whitelist";

KeyEditor::KeyEditor ()
	: WavesDialog ("waves_keyeditor.xml", true, false)
	, view (get_tree_view ("view"))
	, unbind_button (get_waves_button ("unbind_button"))
	, reset_button (get_waves_button ("reset_button"))
{
	can_bind = false;
	last_state = 0;

	model = TreeStore::create(columns);

	view.set_model (model);
	view.append_column (_("Action"), columns.action);
	view.append_column (_("Shortcut"), columns.binding);
	view.set_headers_visible (true);
	view.get_selection()->set_mode (SELECTION_SINGLE);
	view.set_reorderable (false);
	view.set_size_request (500,300);
	view.set_enable_search (false);
	view.set_rules_hint (true);
	view.set_name (X_("KeyEditorTree"));

	view.get_selection()->signal_changed().connect (sigc::mem_fun (*this, &KeyEditor::action_selected));
	unbind_button.signal_clicked.connect (sigc::mem_fun (*this, &KeyEditor::unbind));
	reset_button.signal_clicked.connect (sigc::mem_fun (*this, &KeyEditor::reset));
	unbind_button.set_sensitive (false);

    load_blackwhitelists ();
}

void
KeyEditor::load_blackwhitelists ()
{
        string list;
        
        if (find_file (ARDOUR::ardour_data_search_path(), X_(blacklist_filename), list)) {
                ifstream in (list.c_str());
                
                while (in) {
                        string str;
                        in >> str;
                        action_blacklist.insert (make_pair (str, str));
                }
        }

        if (find_file (ARDOUR::ardour_data_search_path(), X_(whitelist_filename), list)) {
                ifstream in (list.c_str());
                
                while (in) {
                        string str;
                        in >> str;
                        action_whitelist.insert (make_pair (str, str));
                }
        }
}

void
KeyEditor::unbind (WavesButton*)
{
	TreeModel::iterator i = view.get_selection()->get_selected();

	unbind_button.set_sensitive (false);

	if (i != model->children().end()) {
		string path = (*i)[columns.path];

		if (!(*i)[columns.bindable]) {
			return;
		}

		bool result = AccelMap::change_entry (path,
						      0,
						      (ModifierType) 0,
						      true);
		if (result) {
			(*i)[columns.binding] = string ();
		}
	}
}

void
KeyEditor::on_show ()
{
	populate ();
	view.get_selection()->unselect_all ();
	WavesDialog::on_show ();
}

void
KeyEditor::action_selected ()
{
	if (view.get_selection()->count_selected_rows() == 0) {
		return;
	}

	TreeModel::iterator i = view.get_selection()->get_selected();

	unbind_button.set_sensitive (false);

	if (i != model->children().end()) {

		string path = (*i)[columns.path];

		if (!(*i)[columns.bindable]) {
			return;
		}

		string binding = (*i)[columns.binding];

		if (!binding.empty()) {
			unbind_button.set_sensitive (true);
		}
	}
}

bool
KeyEditor::on_key_press_event (GdkEventKey* ev)
{
	can_bind = true;
	last_state = ev->state;
	return false;
}

bool
KeyEditor::on_key_release_event (GdkEventKey* ev)
{
	if (ARDOUR::Profile->get_sae() || !can_bind || ev->state != last_state) {
		return false;
	}

	TreeModel::iterator i = view.get_selection()->get_selected();

	if (i != model->children().end()) {
		string path = (*i)[columns.path];

		if (!(*i)[columns.bindable]) {
			goto out;
		}

                Gtkmm2ext::possibly_translate_keyval_to_make_legal_accelerator (ev->keyval);


		bool result = AccelMap::change_entry (path,
						      ev->keyval,
						      ModifierType (Keyboard::RelevantModifierKeyMask & ev->state),
						      true);

		if (result) {
			AccelKey key;
			(*i)[columns.binding] = ActionManager::get_key_representation (path, key);
		}
	}

  out:
	can_bind = false;
	return true;
}

void
KeyEditor::populate ()
{
	vector<string> paths;
	vector<string> labels;
	vector<string> tooltips;
	vector<string> keys;
	vector<AccelKey> bindings;
	typedef std::map<string,TreeIter> NodeMap;
	NodeMap nodes;
	NodeMap::iterator r;

	ActionManager::get_all_actions (labels, paths, tooltips, keys, bindings);

	vector<string>::iterator k;
	vector<string>::iterator p;
	vector<string>::iterator t;
	vector<string>::iterator l;

	model->clear ();

	for (l = labels.begin(), k = keys.begin(), p = paths.begin(), t = tooltips.begin(); l != labels.end(); ++k, ++p, ++t, ++l) {

		TreeModel::Row row;
		vector<string> parts;

                /* Rip off "<Actions>" before comparing with blacklist */

                string without_action = (*p).substr (9);
                
                if (action_blacklist.find (without_action) != action_blacklist.end()) {
                        /* this action is in the blacklist */
                        continue;
                }

                if (!action_whitelist.empty() && action_whitelist.find (without_action) == action_whitelist.end()) {
                        /* there is a whitelist, and this action isn't in it */
                        continue;
                }
                
		parts.clear ();
                
		split (*p, parts, '/');

		if (parts.empty()) {
			continue;
		}

		//kinda kludgy way to avoid displaying menu items as mappable
		if ( parts[1] == _("Main_menu") )
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
			rowp = model->append();
			nodes[parts[1]] = rowp;
			parent = *(rowp);
			parent[columns.action] = parts[1];
			parent[columns.bindable] = false;

			row = *(model->append (parent.children()));

		} else {

			row = *(model->append ((*r->second)->children()));

		}

		/* add this action */

		if (l->empty ()) {
			row[columns.action] = *t;
		} else {
			row[columns.action] = *l;
		}
		row[columns.path] = (*p);
		row[columns.bindable] = true;

		if (*k == ActionManager::unbound_string) {
			row[columns.binding] = string();
		} else {
			row[columns.binding] = (*k);
		}
	}
}

void
KeyEditor::reset (WavesButton*)
{
	Keyboard::the_keyboard().reset_bindings ();
	populate ();
	view.get_selection()->unselect_all ();
	unbind_button.set_sensitive (false);
}
