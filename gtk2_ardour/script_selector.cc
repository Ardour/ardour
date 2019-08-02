/*
 * Copyright (C) 2016-2018 Robin Gareus <robin@gareus.org>
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

#include <gtkmm/frame.h>
#include <gtkmm/stock.h>
#include <gtkmm/table.h>

#include "gtkmm2ext/utils.h"

#include "script_selector.h"
#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace ARDOUR;

ScriptSelector::ScriptSelector (std::string title, LuaScriptInfo::ScriptType type)
	: ArdourDialog (title)
	, _type_label ("<b>Type:</b>", Gtk::ALIGN_END, Gtk::ALIGN_CENTER)
	, _type ("", Gtk::ALIGN_START, Gtk::ALIGN_CENTER)
	, _author_label ("<b>Author:</b>", Gtk::ALIGN_END, Gtk::ALIGN_CENTER)
	, _author ("", Gtk::ALIGN_START, Gtk::ALIGN_CENTER)
	, _description ("", Gtk::ALIGN_START, Gtk::ALIGN_START)
	, _scripts (LuaScripting::instance ().scripts (type))
	, _script_type (type)
{
	Table* t = manage (new Table (3, 2));
	t->set_spacings (6);

	int ty = 0;

	_type_label.set_use_markup ();
	t->attach (_type_label, 0, 1, ty, ty+1, FILL|EXPAND, SHRINK);
	t->attach (_type, 1, 2, ty, ty+1, FILL|EXPAND, SHRINK);
	++ty;

	_author_label.set_use_markup ();
	t->attach (_author_label, 0, 1, ty, ty+1, FILL|EXPAND, SHRINK);
	t->attach (_author, 1, 2, ty, ty+1, FILL|EXPAND, SHRINK);
	++ty;

	Frame* f = manage(new Frame (_("Description")));
	f->add (_description);
	t->attach (*f, 0, 2, ty, ty+1);
	++ty;

	_description.set_padding (5, 5);
	_description.set_line_wrap();

	get_vbox()->set_spacing (6);
	get_vbox()->pack_start (_script_combo, false, false);
	get_vbox()->pack_start (*t, true, true);

	Button *r = Gtk::manage (new Gtk::Button (Stock::REFRESH));
	r->signal_clicked().connect (sigc::mem_fun (*this, &ScriptSelector::refresh));
	get_action_area()->pack_start(*r);

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	_add = add_button (Stock::ADD, RESPONSE_ACCEPT);
	set_default_response (RESPONSE_ACCEPT);

	_add->set_sensitive (false);
	_combocon = _script_combo.signal_changed().connect (sigc::mem_fun (*this, &ScriptSelector::script_combo_changed));

	setup_list ();
	show_all ();

	script_combo_changed();
}

bool
ScriptSelector::script_separator (const Glib::RefPtr<Gtk::TreeModel> &, const Gtk::TreeModel::iterator &i)
{
	_script_combo.set_active (i);

	return _script_combo.get_active_text () == "--separator--";
}

void
ScriptSelector::setup_list ()
{
	_combocon.block();

	vector<string> script_names;
	for (LuaScriptList::const_iterator s = _scripts.begin(); s != _scripts.end(); ++s) {
		if ((*s)->name != "Shortcut" || _script_type != LuaScriptInfo::EditorAction) {
			script_names.push_back ((*s)->name);
		}
	}

	_script_combo.clear();
	_script_combo.set_row_separator_func (sigc::mem_fun (*this, &ScriptSelector::script_separator));

	if (_script_type == LuaScriptInfo::EditorAction) {
		_script_combo.append_text ("Shortcut");
		_script_combo.append_text ("--separator--");
	}

	vector<string>::const_iterator i;
	for (i = script_names.begin(); i != script_names.end(); ++i) {
		_script_combo.append_text (*i);
	}

	_script_combo.set_active(0);
	script_combo_changed ();

	_combocon.unblock();
}

void
ScriptSelector::script_combo_changed ()
{
	std::string nm = _script_combo.get_active_text();

	for (LuaScriptList::const_iterator s = _scripts.begin(); s != _scripts.end(); ++s) {
		if ((*s)->name == nm) {
			_script = (*s);
		}
	}

	if (_script) {

		if (_script->name == "Shortcut") {
			_type.hide();
			_type_label.hide();
			_author.hide();
			_author_label.hide();
			_description.set_text (_script->description);
		} else {
			_type.show();
			_type_label.show();
			_author.show();
			_author_label.show();
			_type.set_text(LuaScriptInfo::type2str (_script->type));
			_author.set_text (_script->author);
			_description.set_text (_script->description);
		}

		_add->set_sensitive (Glib::file_test(_script->path, Glib::FILE_TEST_EXISTS));
	}
}

void
ScriptSelector::refresh ()
{
	LuaScripting::instance ().refresh ();
	_script.reset ();
	_scripts = LuaScripting::instance ().scripts (_script_type);
	setup_list ();
}

///////////////////////////////////////////////////////////////////////////////

SessionScriptManager::SessionScriptManager (std::string title, const std::vector<std::string> &names)
	: ArdourDialog (title)
{
	assert (names.size() > 0);
	Gtkmm2ext::set_popdown_strings (_names_combo, names);
	_names_combo.set_active(0);

	Gtk::Label* l;
	l = manage (new Label (_("Select Script to unload")));

	get_vbox()->set_spacing (6);
	get_vbox()->pack_start (*l, false, false);
	get_vbox()->pack_start (_names_combo, false, false);

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::REMOVE, RESPONSE_ACCEPT);
	set_default_response (RESPONSE_CANCEL);

	show_all ();
}

///////////////////////////////////////////////////////////////////////////////


ScriptParameterDialog::ScriptParameterDialog (std::string title,
		const LuaScriptInfoPtr& spi,
		const std::vector<std::string> &names,
		LuaScriptParamList& lsp)
	: ArdourDialog (title)
	, _existing_names (names)
	, _lsp (lsp)
{
	Gtk::Label* l;

	Table* t = manage (new Table (4, 3));
	t->set_spacings (6);

	_name_entry.set_text (spi->name);

	for (size_t i = 0; i < _lsp.size(); ++i) {
		if (_lsp[i]->preseeded && _lsp[i]->name == "x-script-name" && !_lsp[i]->value.empty ()) {
			_name_entry.set_text (_lsp[i]->value);
		}
	}

	_name_entry.signal_changed().connect (sigc::mem_fun (*this, &ScriptParameterDialog::update_sensitivity));

	int ty = 0;

	l = manage (new Label (_("<b>Name:</b>"), Gtk::ALIGN_END, Gtk::ALIGN_CENTER, false));
	l->set_use_markup ();
	t->attach (*l, 0, 1, ty, ty+1);
	t->attach (_name_entry, 1, 2, ty, ty+1);
	++ty;

	if (_lsp.size () > 0) {
		l = manage (new Label (_("<b>Instance Parameters</b>"), Gtk::ALIGN_LEFT, Gtk::ALIGN_CENTER, false));
		l->set_use_markup ();
		t->attach (*l, 0, 2, ty, ty+1);
		++ty;
	}

	for (size_t i = 0; i < _lsp.size (); ++i) {
		Entry* e = manage (new Entry());
		if (_lsp[i]->optional) {
			CheckButton* c = manage (new CheckButton (_lsp[i]->title));
			c->set_active (!_lsp[i]->dflt.empty());
			c->signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &ScriptParameterDialog::active_changed), i, c, e));
			t->attach (*c, 0, 1, ty, ty+1);
		} else {
			Label* l = manage (new Label (_lsp[i]->title, Gtk::ALIGN_LEFT));
			t->attach (*l, 0, 1, ty, ty+1);
		}

		e->set_text (_lsp[i]->dflt);
		e->set_sensitive (!_lsp[i]->dflt.empty());
		e->signal_changed().connect (sigc::bind (sigc::mem_fun (*this, &ScriptParameterDialog::value_changed), i, e));

		t->attach (*e, 1, 2, ty, ty+1);
		++ty;
	}

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	_add = add_button (Stock::ADD, RESPONSE_ACCEPT);
	set_default_response (RESPONSE_ACCEPT);

	get_vbox()->pack_start (*t, true, true);
	show_all ();
	update_sensitivity ();
}

bool
ScriptParameterDialog::need_interation () const
{
	if (!parameters_ok ()) {
		return true;
	}
	for (size_t i = 0; i < _lsp.size(); ++i) {
		if (!_lsp[i]->optional && !_lsp[i]->preseeded) {
			return true;
		}
	}
	return false;
}

bool
ScriptParameterDialog::parameters_ok () const
{
	std::string n = _name_entry.get_text ();
	if (n.empty() || std::find (_existing_names.begin(), _existing_names.end(), n) != _existing_names.end()) {
		return false;
	}

	for (size_t i = 0; i < _lsp.size(); ++i) {
		if (!_lsp[i]->optional && _lsp[i]->value.empty()) {
			return false;
		}
	}
	return true;
}

void
ScriptParameterDialog::update_sensitivity ()
{
	_add->set_sensitive (parameters_ok ());
}

void
ScriptParameterDialog::active_changed (int i, Gtk::CheckButton* c, Gtk::Entry* e)
{
	bool en = c->get_active ();
	_lsp[i]->is_set = en;
	e->set_sensitive (en);
}

void
ScriptParameterDialog::value_changed (int i, Gtk::Entry* e)
{
	_lsp[i]->value = e->get_text ();
}
