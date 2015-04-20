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

KeyEditor::KeyEditor ()
	: ArdourWindow (_("Key Bindings"))
	, unbind_button (_("Remove shortcut"))
	, unbind_box (BUTTONBOX_END)

{
	last_keyval = 0;

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

	scroller.add (view);
	scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	vpacker.set_spacing (6);
	vpacker.set_border_width (12);
	vpacker.pack_start (scroller);

	if (!ARDOUR::Profile->get_sae()) {

		Label* hint = manage (new Label (_("Select an action, then press the key(s) to (re)set its shortcut")));
		hint->show ();
		unbind_box.set_spacing (6);
		unbind_box.pack_start (*hint, false, true);
		unbind_box.pack_start (unbind_button, false, false);
		unbind_button.signal_clicked().connect (sigc::mem_fun (*this, &KeyEditor::unbind));

		vpacker.pack_start (unbind_box, false, false);
		unbind_box.show ();
		unbind_button.show ();

	}
	
	reset_button.add (reset_label);
	reset_label.set_markup (string_compose ("<span size=\"large\" weight=\"bold\">%1</span>", _("Reset Bindings to Defaults")));
				
	reset_box.pack_start (reset_button, true, false);
	reset_box.show ();
	reset_button.show ();
	reset_label.show ();
	reset_button.signal_clicked().connect (sigc::mem_fun (*this, &KeyEditor::reset));
	vpacker.pack_start (reset_box, false, false);

	add (vpacker);

	view.show ();
	scroller.show ();
	vpacker.show ();

	unbind_button.set_sensitive (false);
}

void
KeyEditor::unbind ()
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
	ArdourWindow::on_show ();
}

void
KeyEditor::on_unmap ()
{
	ArdourWindow::on_unmap ();
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
	if (!ev->is_modifier) {
		last_keyval = ev->keyval;
	}
	return ArdourWindow::on_key_press_event (ev);
}

bool
KeyEditor::on_key_release_event (GdkEventKey* ev)
{
	if (ARDOUR::Profile->get_sae() || last_keyval == 0) {
		return false;
	}

	TreeModel::iterator i = view.get_selection()->get_selected();

	if (i != model->children().end()) {
		string path = (*i)[columns.path];

		if (!(*i)[columns.bindable]) {
			goto out;
		}

		GdkModifierType mod = (GdkModifierType)(Keyboard::RelevantModifierKeyMask & ev->state);

		Gtkmm2ext::possibly_translate_keyval_to_make_legal_accelerator (ev->keyval);
		Gtkmm2ext::possibly_translate_mod_to_make_legal_accelerator (mod);

		bool result = AccelMap::change_entry (path,
						      last_keyval,
						      Gdk::ModifierType(mod),
						      true);

		if (result) {
			AccelKey key;
			(*i)[columns.binding] = ActionManager::get_key_representation (path, key);
			unbind_button.set_sensitive (true);
		}
	}

  out:
    last_keyval = 0;
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
KeyEditor::reset ()
{
	Keyboard::the_keyboard().reset_bindings ();
	populate ();
	view.get_selection()->unselect_all ();
	populate ();
}
