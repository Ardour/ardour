#include <map>

#include <ardour/profile.h>

#include <gtkmm/stock.h>
#include <gtkmm/label.h>
#include <gtkmm/accelkey.h>
#include <gtkmm/accelmap.h>
#include <gtkmm/uimanager.h>

#include <pbd/strsplit.h>
#include <pbd/replace_all.h>

#include <ardour/profile.h>

#include "actions.h"
#include "keyboard.h"
#include "keyeditor.h"
#include "utils.h"

#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gdk;
using namespace PBD;

KeyEditor::KeyEditor ()
	: ArdourDialog (_("Keybindings"), false)
	, unbind_button (_("Remove shortcut"))
	, unbind_box (BUTTONBOX_END)
	
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
	
	view.get_selection()->signal_changed().connect (mem_fun (*this, &KeyEditor::action_selected));
	
	scroller.add (view);
	scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);


	get_vbox()->set_spacing (6);
	get_vbox()->pack_start (scroller);

	if (!ARDOUR::Profile->get_sae()) {

		Label* hint = manage (new Label (_("Select an action, then press the key(s) to (re)set its shortcut")));
		hint->show ();
		unbind_box.set_spacing (6);
		unbind_box.pack_start (*hint, false, true);
		unbind_box.pack_start (unbind_button, false, false);
		unbind_button.signal_clicked().connect (mem_fun (*this, &KeyEditor::unbind));

		get_vbox()->pack_start (unbind_box, false, false);
		unbind_box.show ();
		unbind_button.show ();
		
	}

	get_vbox()->set_border_width (12);

	view.show ();
	scroller.show ();

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
	ArdourDialog::on_show ();
}

void
KeyEditor::on_unmap ()
{
	ArdourDialog::on_unmap ();
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

		possibly_translate_keyval_to_make_legal_accelerator (ev->keyval);

		bool result = AccelMap::change_entry (path,
						      ev->keyval,
						      ModifierType (Keyboard::RelevantModifierKeyMask & ev->state),
						      true);

		if (result) {
			bool known;
			AccelKey key;

			known = ActionManager::lookup_entry (path, key);
			
			if (known) {
				(*i)[columns.binding] = ActionManager::ui_manager->get_accel_group()->get_label (key.get_key(), Gdk::ModifierType (key.get_mod()));
			} else {
				(*i)[columns.binding] = string();
			}
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
	vector<string> keys;
	vector<AccelKey> bindings;
	typedef std::map<string,TreeIter> NodeMap;
	NodeMap nodes;
	NodeMap::iterator r;
	
	ActionManager::get_all_actions (labels, paths, keys, bindings);
	
	vector<string>::iterator k;
	vector<string>::iterator p;
	vector<string>::iterator l;

	model->clear ();

	for (l = labels.begin(), k = keys.begin(), p = paths.begin(); l != labels.end(); ++k, ++p, ++l) {

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

		row[columns.action] = (*l);
		row[columns.path] = (*p);
		row[columns.bindable] = true;
		
		if (*k == ActionManager::unbound_string) {
			row[columns.binding] = string();
		} else {
			row[columns.binding] = (*k);
		}
	}
}
